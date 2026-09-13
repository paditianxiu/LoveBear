package io.github.libxposed.lovebear

/** Kotlin wrapper around UnityResolve class and instance handles. */
object Il2Cpp {
    fun getClass(assembly: String, namespace: String, className: String): Il2CppClass? {
        val assemblyHandle = NativeFunctions.findAssembly(assembly)
        if (assemblyHandle == 0L) return null

        val classHandle = NativeFunctions.findClass(assemblyHandle, className, namespace)
        return classHandle.takeIf { it != 0L }?.let(::Il2CppClass)
    }
}

class Il2CppClass internal constructor(
    @PublishedApi internal val handle: Long
) {
    fun field(name: String): Il2CppField? {
        val offset = NativeFunctions.getFieldOffset(handle, name)
        return offset.takeIf { it >= 0 }?.let { Il2CppField(this, name, it) }
    }

    fun getObject(instance: Long, fieldName: String): Long =
        NativeFunctions.getObjectField(instance, handle, fieldName)

    inline fun <reified T> get(instance: Long, fieldName: String): T? =
        NativeFunctions.getFieldValue(instance, handle, fieldName) as? T

    fun set(instance: Long, fieldName: String, value: Any?): Boolean =
        NativeFunctions.setFieldValue(instance, handle, fieldName, value)

    operator fun get(fieldName: String): Il2CppValue = Il2CppValue(this, fieldName)

    fun getStaticObject(fieldName: String): Long =
        NativeFunctions.getStaticObjectField(handle, fieldName)
}

class Il2CppField internal constructor(
    @PublishedApi internal val owner: Il2CppClass, val name: String, val offset: Int
) {
    fun getObject(instance: Long): Long = owner.getObject(instance, name)

    inline fun <reified T> get(instance: Long): T? = owner.get(instance, name)

    fun set(instance: Long, value: Any?): Boolean = owner.set(instance, name, value)
}

class Il2CppValue internal constructor(
    @PublishedApi internal val owner: Il2CppClass, @PublishedApi internal val name: String
) {
    inline fun <reified T> get(instance: Long): T? = owner.get(instance, name)

    fun set(instance: Long, value: Any?): Boolean = owner.set(instance, name, value)
}
