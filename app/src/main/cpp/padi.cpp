#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <cstring>
#include <cmath>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
#include <link.h>

#include "libs/xdl/xdl.h"

namespace {
    void *gIl2CppXdlHandle = nullptr;

    void *ResolveIl2CppSymbol(void *, const char *symbol) {
        if (gIl2CppXdlHandle == nullptr || symbol == nullptr) return nullptr;
        void *address = xdl_sym(gIl2CppXdlHandle, symbol, nullptr);
        if (address == nullptr) address = xdl_dsym(gIl2CppXdlHandle, symbol, nullptr);
        return address;
    }
}

#define dlsym ResolveIl2CppSymbol

#include "UnityResolve.hpp"

#undef dlsym

#include "libs/KittyMemory/KittyMemory.h"

namespace {
    constexpr const char *kTag = "LoveBearNative";
    bool gInitialized = false;

    std::string ToString(JNIEnv *env, jstring value) {
        if (value == nullptr) return {};
        const char *chars = env->GetStringUTFChars(value, nullptr);
        std::string result = chars != nullptr ? chars : "";
        if (chars != nullptr) env->ReleaseStringUTFChars(value, chars);
        return result;
    }

    union RuntimeValue {
        std::uint64_t u64;
        double d;
        float f;
        void *object;
    };

    void *InvokeRuntime(UnityResolve::Method *method, void *instance, const jlong *args,
                        jsize count) {
        if (method == nullptr || count < 0 ||
            static_cast<size_t>(count) != method->args.size()) {
            return nullptr;
        }

        std::vector<RuntimeValue> values(static_cast<size_t>(count));
        std::vector<void *> argumentPointers(static_cast<size_t>(count));
        for (jsize i = 0; i < count; ++i) {
            const auto *argument = method->args[static_cast<size_t>(i)];
            if (argument == nullptr || argument->pType == nullptr) return nullptr;
            const auto &type = argument->pType->name;
            values[static_cast<size_t>(i)].u64 = static_cast<std::uint64_t>(args[i]);

            if (type == "System.Single") {
                std::uint32_t bits = static_cast<std::uint32_t>(args[i]);
                std::memcpy(&values[static_cast<size_t>(i)].f, &bits, sizeof(bits));
            } else if (type == "System.Double") {
                std::memcpy(&values[static_cast<size_t>(i)].d, &args[i], sizeof(args[i]));
            } else if (type != "System.Boolean" && type != "System.Byte" &&
                       type != "System.SByte" && type != "System.Int16" &&
                       type != "System.UInt16" && type != "System.Int32" &&
                       type != "System.UInt32" && type != "System.Int64" &&
                       type != "System.UInt64" && type != "System.IntPtr" &&
                       type != "System.UIntPtr") {
                values[static_cast<size_t>(i)].object = reinterpret_cast<void *>(args[i]);
            }
            argumentPointers[static_cast<size_t>(i)] = &values[static_cast<size_t>(i)];
        }

        void *exception = nullptr;
        void *result = UnityResolve::Invoke<void *>(
                "il2cpp_runtime_invoke", method->address, instance,
                argumentPointers.empty() ? nullptr : argumentPointers.data(), &exception);
        if (exception != nullptr) {
            __android_log_print(ANDROID_LOG_WARN, kTag, "Managed method %s threw an exception",
                                method->name.c_str());
            return nullptr;
        }
        return result;
    }

    jobject Box(JNIEnv *env, const char *className, const char *methodName, const char *signature,
                jvalue value) {
        jclass clazz = env->FindClass(className);
        if (clazz == nullptr) return nullptr;
        jmethodID method = env->GetStaticMethodID(clazz, methodName, signature);
        jobject result =
                method != nullptr ? env->CallStaticObjectMethodA(clazz, method, &value) : nullptr;
        env->DeleteLocalRef(clazz);
        return result;
    }

    const std::string &FieldType(UnityResolve::Field *field) {
        static const std::string empty;
        return field != nullptr && field->type != nullptr ? field->type->name : empty;
    }

    std::string JsonEscape(const std::string &value) {
        std::string escaped;
        escaped.reserve(value.size() + 8);
        for (const char character : value) {
            switch (character) {
                case '\\': escaped += "\\\\"; break;
                case '"': escaped += "\\\""; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped += character; break;
            }
        }
        return escaped;
    }

    jobject
    ReadField(JNIEnv *env, UnityResolve::Class *clazz, void *instance, UnityResolve::Field *field) {
        if (field == nullptr || field->offset < 0) return nullptr;
        const auto &type = FieldType(field);
        const auto offset = static_cast<unsigned int>(field->offset);
        if (type == "System.Boolean") {
            jvalue value{};
            value.z = clazz->GetValue<bool>(instance, offset);
            return Box(env, "java/lang/Boolean", "valueOf", "(Z)Ljava/lang/Boolean;", value);
        }
        if (type == "System.Byte" || type == "System.SByte" || type == "System.Int16" ||
            type == "System.UInt16" || type == "System.Int32" || type == "System.UInt32") {
            jvalue value{};
            value.i = clazz->GetValue<jint>(instance, offset);
            return Box(env, "java/lang/Integer", "valueOf", "(I)Ljava/lang/Integer;", value);
        }
        if (type == "System.Int64" || type == "System.UInt64") {
            jvalue value{};
            value.j = clazz->GetValue<jlong>(instance, offset);
            return Box(env, "java/lang/Long", "valueOf", "(J)Ljava/lang/Long;", value);
        }
        if (type == "System.Single") {
            jvalue value{};
            value.f = clazz->GetValue<float>(instance, offset);
            return Box(env, "java/lang/Float", "valueOf", "(F)Ljava/lang/Float;", value);
        }
        if (type == "System.Double") {
            jvalue value{};
            value.d = clazz->GetValue<double>(instance, offset);
            return Box(env, "java/lang/Double", "valueOf", "(D)Ljava/lang/Double;", value);
        }
        if (type == "System.String") {
            auto *value = clazz->GetValue<UnityResolve::UnityType::String *>(instance, offset);
            if (value == nullptr) return nullptr;
            return env->NewStringUTF(value->ToString().c_str());
        }
        jvalue value{};
        value.j = reinterpret_cast<jlong>(clazz->GetValue<void *>(instance, offset));
        return Box(env, "java/lang/Long", "valueOf", "(J)Ljava/lang/Long;", value);
    }

    bool WriteField(JNIEnv *env, UnityResolve::Class *clazz, void *instance,
                    UnityResolve::Field *field, jobject value) {
        if (field == nullptr || field->offset < 0 || value == nullptr) return false;
        const auto &type = FieldType(field);
        const auto offset = static_cast<unsigned int>(field->offset);
        if (type == "System.String") {
            auto *string = UnityResolve::UnityType::String::New(
                    ToString(env, static_cast<jstring>(value)));
            clazz->SetValue<UnityResolve::UnityType::String *>(instance, offset, string);
            return true;
        }
        if (type == "System.Boolean") {
            jclass clazzBoolean = env->FindClass("java/lang/Boolean");
            jmethodID method = env->GetMethodID(clazzBoolean, "booleanValue", "()Z");
            clazz->SetValue<bool>(instance, offset, env->CallBooleanMethod(value, method));
            env->DeleteLocalRef(clazzBoolean);
            return true;
        }
        if (type == "System.Single") {
            jclass valueClass = env->FindClass("java/lang/Number");
            jmethodID method = env->GetMethodID(valueClass, "floatValue", "()F");
            clazz->SetValue<float>(instance, offset, env->CallFloatMethod(value, method));
            env->DeleteLocalRef(valueClass);
            return true;
        }
        if (type == "System.Double") {
            jclass valueClass = env->FindClass("java/lang/Number");
            jmethodID method = env->GetMethodID(valueClass, "doubleValue", "()D");
            clazz->SetValue<double>(instance, offset, env->CallDoubleMethod(value, method));
            env->DeleteLocalRef(valueClass);
            return true;
        }
        if (type == "System.Int64" || type == "System.UInt64") {
            jclass valueClass = env->FindClass("java/lang/Number");
            jmethodID method = env->GetMethodID(valueClass, "longValue", "()J");
            clazz->SetValue<jlong>(instance, offset, env->CallLongMethod(value, method));
            env->DeleteLocalRef(valueClass);
            return true;
        }
        if (type == "System.Byte" || type == "System.SByte" || type == "System.Int16" ||
            type == "System.UInt16" || type == "System.Int32" || type == "System.UInt32") {
            jclass valueClass = env->FindClass("java/lang/Number");
            jmethodID method = env->GetMethodID(valueClass, "intValue", "()I");
            clazz->SetValue<jint>(instance, offset, env->CallIntMethod(value, method));
            env->DeleteLocalRef(valueClass);
            return true;
        }
        if (env->IsInstanceOf(value, env->FindClass("java/lang/Number"))) {
            auto object = reinterpret_cast<void *>(env->CallLongMethod(value,
                                                                       env->GetMethodID(
                                                                               env->FindClass(
                                                                                       "java/lang/Number"),
                                                                               "longValue",
                                                                               "()J")));
            clazz->SetValue<void *>(instance, offset, object);
            return true;
        }
        return false;
    }
}

uintptr_t get_base_address(const char *name) {
    return KittyMemory::getAbsoluteAddress(name, 0);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_initUnityResolve(JNIEnv *, jobject) {
    if (gInitialized) return JNI_TRUE;

    const auto libraryMap = KittyMemory::getLibraryMap("libil2cpp.so");
    const uintptr_t base = reinterpret_cast<uintptr_t>(libraryMap.startAddr);
    __android_log_print(ANDROID_LOG_INFO, kTag, "libil2cpp base = 0x%llx",
                        static_cast<unsigned long long>(base));

    if (base == 0 || libraryMap.pathname.empty()) {
        __android_log_write(ANDROID_LOG_WARN, kTag, "libil2cpp.so is not loaded");
        return JNI_FALSE;
    }

    __android_log_print(ANDROID_LOG_INFO, kTag, "libil2cpp path = %s",
                        libraryMap.pathname.c_str());

    gIl2CppXdlHandle = xdl_open(libraryMap.pathname.c_str(), XDL_DEFAULT);
    if (gIl2CppXdlHandle == nullptr) {
        __android_log_write(ANDROID_LOG_WARN, kTag,
                            "xdl_open failed for the loaded libil2cpp.so");
        return JNI_FALSE;
    }
    void *domainGet = ResolveIl2CppSymbol(gIl2CppXdlHandle, "il2cpp_domain_get");
    if (domainGet == nullptr) {
        __android_log_write(ANDROID_LOG_WARN, kTag,
                            "il2cpp_domain_get was not found in libil2cpp ELF symbols");
        xdl_close(gIl2CppXdlHandle);
        gIl2CppXdlHandle = nullptr;
        return JNI_FALSE;
    }
    __android_log_print(ANDROID_LOG_INFO, kTag, "il2cpp_domain_get = %p", domainGet);

    gInitialized = UnityResolve::Init(RTLD_DEFAULT, UnityResolve::Mode::Il2Cpp);
    if (!gInitialized) {
        __android_log_write(ANDROID_LOG_WARN, kTag, "Unity runtime is not ready yet");
        return JNI_FALSE;
    }
    __android_log_write(ANDROID_LOG_INFO, kTag, "UnityResolve initialized in Il2Cpp mode");
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_threadAttach(JNIEnv *, jobject) {
    if (gInitialized) UnityResolve::ThreadAttach();
}

extern "C" JNIEXPORT void JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_threadDetach(JNIEnv *, jobject) {
    // UnityResolve::ThreadDetach passes the domain to il2cpp_thread_detach,
    // which expects the Il2CppThread returned by il2cpp_thread_attach.
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_findAssembly(JNIEnv *env, jobject, jstring name) {
    if (!gInitialized) return 0;
    UnityResolve::ThreadAttach();
    auto *assembly = UnityResolve::Get(ToString(env, name));
    return reinterpret_cast<jlong>(assembly);
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_findClass(
        JNIEnv *env, jobject, jlong assemblyHandle, jstring name, jstring namespaceName) {
    if (!gInitialized || assemblyHandle == 0) return 0;
    UnityResolve::ThreadAttach();
    auto *assembly = reinterpret_cast<UnityResolve::Assembly *>(assemblyHandle);
    auto *clazz = assembly->Get(ToString(env, name), ToString(env, namespaceName));
    return reinterpret_cast<jlong>(clazz);
}

extern "C" JNIEXPORT jint JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_getFieldOffset(
        JNIEnv *env, jobject, jlong classHandle, jstring name) {
    if (!gInitialized || classHandle == 0) return -1;
    UnityResolve::ThreadAttach();
    auto *clazz = reinterpret_cast<UnityResolve::Class *>(classHandle);
    auto *field = clazz->Get<UnityResolve::Field>(ToString(env, name));
    const jint offset = field != nullptr ? field->offset : -1;
    return offset;
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_getObjectField(
        JNIEnv *env, jobject, jlong instanceHandle, jlong classHandle, jstring name) {
    if (!gInitialized || instanceHandle == 0 || classHandle == 0) return 0;
    UnityResolve::ThreadAttach();
    auto *clazz = reinterpret_cast<UnityResolve::Class *>(classHandle);
    auto *field = clazz->Get<UnityResolve::Field>(ToString(env, name));
    void *value = nullptr;
    if (field != nullptr && field->offset >= 0) {
        value = clazz->GetValue<void *>(reinterpret_cast<void *>(instanceHandle),
                                        static_cast<unsigned int>(field->offset));
    }
    return reinterpret_cast<jlong>(value);
}

extern "C" JNIEXPORT jobject JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_getFieldValue(
        JNIEnv *env, jobject, jlong instanceHandle, jlong classHandle, jstring name) {
    if (!gInitialized || instanceHandle == 0 || classHandle == 0) return nullptr;
    UnityResolve::ThreadAttach();
    auto *clazz = reinterpret_cast<UnityResolve::Class *>(classHandle);
    auto *field = clazz->Get<UnityResolve::Field>(ToString(env, name));
    jobject result = ReadField(env, clazz, reinterpret_cast<void *>(instanceHandle), field);
    return result;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_setFieldValue(
        JNIEnv *env, jobject, jlong instanceHandle, jlong classHandle, jstring name,
        jobject value) {
    if (!gInitialized || instanceHandle == 0 || classHandle == 0) return JNI_FALSE;
    UnityResolve::ThreadAttach();
    auto *clazz = reinterpret_cast<UnityResolve::Class *>(classHandle);
    auto *field = clazz->Get<UnityResolve::Field>(ToString(env, name));
    const bool result = WriteField(env, clazz, reinterpret_cast<void *>(instanceHandle), field,
                                   value);
    return result ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_getStaticObjectField(
        JNIEnv *env, jobject, jlong classHandle, jstring name) {
    if (!gInitialized || classHandle == 0) return 0;
    UnityResolve::ThreadAttach();
    auto *clazz = reinterpret_cast<UnityResolve::Class *>(classHandle);
    auto *field = clazz->Get<UnityResolve::Field>(ToString(env, name));
    void *value = nullptr;
    if (field != nullptr && field->static_field) field->GetStaticValue(&value);
    return reinterpret_cast<jlong>(value);
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_findMethod(
        JNIEnv *env, jobject, jlong classHandle, jstring name, jobjectArray parameterTypes) {
    if (!gInitialized || classHandle == 0) return 0;
    std::vector<std::string> types;
    if (parameterTypes != nullptr) {
        const jsize count = env->GetArrayLength(parameterTypes);
        types.reserve(count);
        for (jsize i = 0; i < count; ++i) {
            auto value = static_cast<jstring>(env->GetObjectArrayElement(parameterTypes, i));
            types.push_back(ToString(env, value));
            env->DeleteLocalRef(value);
        }
    }
    UnityResolve::ThreadAttach();
    auto *clazz = reinterpret_cast<UnityResolve::Class *>(classHandle);
    auto *method = clazz->Get<UnityResolve::Method>(ToString(env, name), types);
    return reinterpret_cast<jlong>(method);
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_getMethodFunction(
        JNIEnv *, jobject, jlong methodHandle) {
    if (!gInitialized || methodHandle == 0) return 0;
    UnityResolve::ThreadAttach();
    auto *method = reinterpret_cast<UnityResolve::Method *>(methodHandle);
    method->Compile();
    const jlong function = reinterpret_cast<jlong>(method->function);
    return function;
}


extern "C" JNIEXPORT void JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_invokeVoid(
        JNIEnv *env, jobject, jlong methodHandle, jlong instanceHandle, jintArray arguments) {
    if (!gInitialized || methodHandle == 0) return;
    const jsize count = arguments == nullptr ? 0 : env->GetArrayLength(arguments);
    std::vector<jlong> values(static_cast<size_t>(count));
    if (count > 0) {
        std::vector<jint> intValues(static_cast<size_t>(count));
        env->GetIntArrayRegion(arguments, 0, count, intValues.data());
        for (jsize i = 0; i < count; ++i) values[static_cast<size_t>(i)] = intValues[i];
    }
    UnityResolve::ThreadAttach();
    InvokeRuntime(reinterpret_cast<UnityResolve::Method *>(methodHandle),
                  reinterpret_cast<void *>(instanceHandle), values.data(), count);
}


extern "C" JNIEXPORT void JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_invokeVoidObjects(
        JNIEnv *env, jobject, jlong methodHandle, jlong instanceHandle, jlongArray arguments) {
    if (!gInitialized || methodHandle == 0) return;
    const jsize count = arguments == nullptr ? 0 : env->GetArrayLength(arguments);
    std::vector<jlong> values(static_cast<size_t>(count));
    if (count > 0) env->GetLongArrayRegion(arguments, 0, count, values.data());
    UnityResolve::ThreadAttach();
    InvokeRuntime(reinterpret_cast<UnityResolve::Method *>(methodHandle),
                  reinterpret_cast<void *>(instanceHandle), values.data(), count);
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_invoke(
        JNIEnv *env, jobject, jlong methodHandle, jlong instanceHandle, jlongArray arguments) {
    if (!gInitialized || methodHandle == 0) return 0;
    const jsize count = arguments == nullptr ? 0 : env->GetArrayLength(arguments);
    std::vector<jlong> values(static_cast<size_t>(count));
    if (count > 0) env->GetLongArrayRegion(arguments, 0, count, values.data());
    UnityResolve::ThreadAttach();
    void *result = InvokeRuntime(reinterpret_cast<UnityResolve::Method *>(methodHandle),
                                 reinterpret_cast<void *>(instanceHandle), values.data(), count);
    return reinterpret_cast<jlong>(result);
}

extern "C" JNIEXPORT jlong JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_newString(JNIEnv *env, jobject, jstring value) {
    if (!gInitialized) return 0;
    UnityResolve::ThreadAttach();
    auto *string = UnityResolve::UnityType::String::New(ToString(env, value));
    return reinterpret_cast<jlong>(string);
}

extern "C" JNIEXPORT jstring JNICALL
Java_io_github_libxposed_lovebear_NativeFunctions_getEntitySnapshot(JNIEnv *env, jobject) {
    if (!gInitialized) return env->NewStringUTF("{\"width\":0,\"height\":0,\"entities\":[]}");

    UnityResolve::ThreadAttach();
    auto *camera = UnityResolve::UnityType::Camera::GetMain();
    const auto width = UnityResolve::UnityType::Screen::get_width();
    const auto height = UnityResolve::UnityType::Screen::get_height();
    std::ostringstream json;
    json << "{\"width\":" << width << ",\"height\":" << height << ",\"entities\":[";

    bool first = true;
    if (camera != nullptr && width > 0 && height > 0) {
        auto *core = UnityResolve::Get("UnityEngine.CoreModule.dll");
        auto *monoClass = core != nullptr ? core->Get("MonoBehaviour", "UnityEngine") : nullptr;
        if (monoClass == nullptr && core != nullptr) monoClass = core->Get("MonoBehaviour");
        if (monoClass != nullptr) {
            using MonoBehaviour = UnityResolve::UnityType::MonoBehaviour;
            using MonoBehaviourArray = UnityResolve::UnityType::Array<MonoBehaviour *>;

            std::vector<MonoBehaviour *> objects;
            auto *objectClass = core->Get("Object", "UnityEngine");
            if (objectClass == nullptr) objectClass = core->Get("Object");
            auto *findObjects = objectClass != nullptr
                                ? objectClass->Get<UnityResolve::Method>(
                                        "FindObjectsOfType", {"System.Type"})
                                : nullptr;
            auto *monoType = monoClass->GetType();
            if (findObjects != nullptr && monoType != nullptr) {
                using FindObjectsFunction = MonoBehaviourArray *(*)(void *, void *);
                auto function = reinterpret_cast<FindObjectsFunction>(findObjects->function);
                auto *array = function != nullptr
                              ? function(monoType, findObjects->address)
                              : nullptr;
                if (array != nullptr) objects = array->ToVector();
            }

            for (auto *object : objects) {
                if (object == nullptr || object->m_CachedPtr == nullptr) continue;
                auto *gameObject = object->GetGameObject();
                if (gameObject == nullptr || gameObject->m_CachedPtr == nullptr ||
                    !gameObject->GetActiveInHierarchy()) continue;
                auto *transform = object->GetTransform();
                if (transform == nullptr) continue;
                const auto screen = camera->WorldToScreenPoint(transform->GetPosition());
                if (!std::isfinite(screen.x) || !std::isfinite(screen.y) ||
                    !std::isfinite(screen.z) || screen.z <= 0.0f ||
                    screen.x < 0.0f || screen.x > static_cast<float>(width) ||
                    screen.y < 0.0f || screen.y > static_cast<float>(height)) continue;

                // Do not call System.Type methods here: some IL2CPP builds expose
                // metadata-only MethodInfo entries whose function slot is not executable.
                std::string type = "UnityEngine.MonoBehaviour";
                const auto klass = object->Il2CppClass.klass;
                if (klass != nullptr) {
                    const auto className = UnityResolve::Invoke<const char *>(
                            "il2cpp_class_get_name", klass);
                    const auto namespaceName = UnityResolve::Invoke<const char *>(
                            "il2cpp_class_get_namespace", klass);
                    if (className != nullptr && className[0] != '\0') {
                        type = (namespaceName != nullptr && namespaceName[0] != '\0')
                               ? std::string(namespaceName) + "." + className
                               : className;
                    }
                }
                const std::string &name = type;

                if (!first) json << ',';
                first = false;
                json << "{\"type\":\"" << JsonEscape(type)
                     << "\",\"name\":\"" << JsonEscape(name)
                     << "\",\"x\":" << screen.x
                     << ",\"y\":" << screen.y
                     << ",\"z\":" << screen.z << '}';
            }
        }
    }
    json << "]}";
    return env->NewStringUTF(json.str().c_str());
}
