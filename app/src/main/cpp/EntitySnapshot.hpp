#ifndef LOVE_BEAR_ENTITY_SNAPSHOT_HPP
#define LOVE_BEAR_ENTITY_SNAPSHOT_HPP

#include <android/log.h>
#include <jni.h>

#include <cmath>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "UnityResolve.hpp"
#include "libs/dobby/dobby.h"

namespace EntitySnapshot {
    namespace {
        constexpr const char *kLogTag = "LoveBearNative";
        using MonoBehaviour = UnityResolve::UnityType::MonoBehaviour;
        using MonoBehaviourArray = UnityResolve::UnityType::Array<MonoBehaviour *>;
        using Camera = UnityResolve::UnityType::Camera;
        using CoinOnEnableFunction = void (*)(MonoBehaviour *, void *);

        std::mutex coinMutex;
        std::unordered_map<MonoBehaviour *, std::uint32_t> trackedCoins;
        CoinOnEnableFunction coinOnEnableOriginal = nullptr;
        bool coinHookInstalled = false;

        std::vector<MonoBehaviour *> entityCache;
        std::uint32_t entityArrayHandle = 0;
        Camera *entityCacheCamera = nullptr;
        bool entityCacheInitialized = false;

        std::string JsonEscape(const std::string &value) {
            std::string escaped;
            escaped.reserve(value.size() + 8);
            for (const char character: value) {
                switch (character) {
                    case '\\':
                        escaped += "\\\\";
                        break;
                    case '"':
                        escaped += "\\\"";
                        break;
                    case '\n':
                        escaped += "\\n";
                        break;
                    case '\r':
                        escaped += "\\r";
                        break;
                    case '\t':
                        escaped += "\\t";
                        break;
                    default:
                        escaped += character;
                        break;
                }
            }
            return escaped;
        }

        void CoinOnEnableHook(MonoBehaviour *coin, void *methodInfo) {
            if (coinOnEnableOriginal != nullptr) coinOnEnableOriginal(coin, methodInfo);
            if (coin == nullptr || coin->m_CachedPtr == nullptr) return;

            std::lock_guard<std::mutex> lock(coinMutex);
            if (trackedCoins.find(coin) != trackedCoins.end()) return;
            const auto handle = UnityResolve::Invoke<std::uint32_t, void *, bool>(
                    "il2cpp_gchandle_new", coin, false);
            if (handle != 0) trackedCoins.emplace(coin, handle);
        }

        std::vector<MonoBehaviour *> GetTrackedCoins() {
            std::vector<MonoBehaviour *> coins;
            std::lock_guard<std::mutex> lock(coinMutex);
            coins.reserve(trackedCoins.size());
            for (auto iterator = trackedCoins.begin(); iterator != trackedCoins.end();) {
                auto *coin = iterator->first;
                if (coin == nullptr || coin->m_CachedPtr == nullptr) {
                    if (iterator->second != 0) {
                        UnityResolve::Invoke<void, std::uint32_t>("il2cpp_gchandle_free",
                                                                  iterator->second);
                    }
                    iterator = trackedCoins.erase(iterator);
                } else {
                    coins.push_back(coin);
                    ++iterator;
                }
            }
            return coins;
        }

        void RefreshEntityCacheIfNeeded(Camera *camera) {
            if (entityCacheInitialized && entityCacheCamera == camera) return;
            entityCacheInitialized = true;
            entityCacheCamera = camera;
            entityCache.clear();
            if (entityArrayHandle != 0) {
                UnityResolve::Invoke<void, std::uint32_t>("il2cpp_gchandle_free",
                                                          entityArrayHandle);
                entityArrayHandle = 0;
            }

            auto *core = UnityResolve::Get("UnityEngine.CoreModule.dll");
            auto *monoClass = core != nullptr ? core->Get("MonoBehaviour", "UnityEngine") : nullptr;
            if (monoClass == nullptr && core != nullptr) monoClass = core->Get("MonoBehaviour");
            auto *objectClass = core != nullptr ? core->Get("Object", "UnityEngine") : nullptr;
            if (objectClass == nullptr && core != nullptr) objectClass = core->Get("Object");
            auto *findObjects = objectClass != nullptr
                                ? objectClass->Get<UnityResolve::Method>(
                            "FindObjectsOfType", {"System.Type"})
                                : nullptr;
            if (monoClass == nullptr || findObjects == nullptr ||
                findObjects->function == nullptr) {
                __android_log_write(ANDROID_LOG_WARN, kLogTag,
                                    "Entity cache lookup is unavailable");
                return;
            }

            const auto *nativeType = UnityResolve::Invoke<void *, void *>(
                    "il2cpp_class_get_type", monoClass->address);
            auto *managedType = nativeType != nullptr
                                ? UnityResolve::Invoke<void *, const void *>(
                            "il2cpp_type_get_object", nativeType)
                                : nullptr;
            if (managedType == nullptr) return;

            using FindObjectsFunction = MonoBehaviourArray *(*)(void *, void *);
            auto function = reinterpret_cast<FindObjectsFunction>(findObjects->function);
            auto *array = function(managedType, findObjects->address);
            if (array == nullptr) return;

            entityArrayHandle = UnityResolve::Invoke<std::uint32_t, void *, bool>(
                    "il2cpp_gchandle_new", array, false);
            if (entityArrayHandle != 0) entityCache = array->ToVector();
            __android_log_print(ANDROID_LOG_INFO, kLogTag, "Entity cache initialized: count=%zu",
                                entityCache.size());
        }
    }

    void Install() {
        if (coinHookInstalled) return;
        auto *assembly = UnityResolve::Get("Assembly-CSharp.dll");
        auto *coinsClass = assembly != nullptr ? assembly->Get("Coins", "") : nullptr;
        if (coinsClass == nullptr && assembly != nullptr) coinsClass = assembly->Get("Coins");
        auto *onEnable = coinsClass != nullptr
                         ? coinsClass->Get<UnityResolve::Method>("OnEnable")
                         : nullptr;
        if (onEnable == nullptr || onEnable->function == nullptr) {
            __android_log_write(ANDROID_LOG_WARN, kLogTag, "Coins.OnEnable hook target not found");
            return;
        }

        const int result = DobbyHook(
                onEnable->function,
                reinterpret_cast<dobby_dummy_func_t>(CoinOnEnableHook),
                reinterpret_cast<dobby_dummy_func_t *>(&coinOnEnableOriginal));
        coinHookInstalled = result == 0 && coinOnEnableOriginal != nullptr;
        __android_log_print(coinHookInstalled ? ANDROID_LOG_INFO : ANDROID_LOG_WARN, kLogTag,
                            "Coin tracking hook %s: OnEnable=%p original=%p result=%d",
                            coinHookInstalled ? "installed" : "failed", onEnable->function,
                            reinterpret_cast<void *>(coinOnEnableOriginal), result);
    }

    jstring Get(JNIEnv *env, bool initialized, jboolean coinsOnly) {
        if (!initialized) {
            return env->NewStringUTF("{\"width\":0,\"height\":0,\"entities\":[]}");
        }

        UnityResolve::ThreadAttach();
        auto *camera = Camera::GetMain();
        const auto width = UnityResolve::UnityType::Screen::get_width();
        const auto height = UnityResolve::UnityType::Screen::get_height();
        std::ostringstream json;
        json << "{\"width\":" << width << ",\"height\":" << height << ",\"entities\":[";

        bool first = true;
        if (camera != nullptr && width > 0 && height > 0) {
            std::vector<MonoBehaviour *> objects;
            const auto coins = GetTrackedCoins();
            if (coinsOnly == JNI_TRUE) {
                objects = coins;
            } else {
                RefreshEntityCacheIfNeeded(camera);
                objects = entityCache;
                for (auto *coin: coins) {
                    bool alreadyCached = false;
                    for (auto *object: objects) {
                        if (object == coin) {
                            alreadyCached = true;
                            break;
                        }
                    }
                    if (!alreadyCached) objects.push_back(coin);
                }
            }

            for (auto *object: objects) {
                if (object == nullptr || object->m_CachedPtr == nullptr) continue;
                auto *gameObject = object->GetGameObject();
                if (gameObject == nullptr || gameObject->m_CachedPtr == nullptr ||
                    !gameObject->GetActiveInHierarchy())
                    continue;
                auto *transform = object->GetTransform();
                if (transform == nullptr || transform->m_CachedPtr == nullptr) continue;
                const auto screen = camera->WorldToScreenPoint(transform->GetPosition());
                if (!std::isfinite(screen.x) || !std::isfinite(screen.y) ||
                    !std::isfinite(screen.z) || screen.z <= 0.0f ||
                    screen.x < 0.0f || screen.x > static_cast<float>(width) ||
                    screen.y < 0.0f || screen.y > static_cast<float>(height))
                    continue;

                std::string type = coinsOnly == JNI_TRUE ? "Coins" : "UnityEngine.MonoBehaviour";
                if (coinsOnly != JNI_TRUE) {
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
                }

                if (!first) json << ',';
                first = false;
                json << "{\"type\":\"" << JsonEscape(type)
                     << "\",\"name\":\"" << JsonEscape(type)
                     << "\",\"x\":" << screen.x
                     << ",\"y\":" << screen.y
                     << ",\"z\":" << screen.z << '}';
            }
        }
        json << "]}";
        return env->NewStringUTF(json.str().c_str());
    }
}

#endif
