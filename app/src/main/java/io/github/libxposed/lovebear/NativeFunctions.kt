package io.github.libxposed.lovebear

object NativeFunctions {
    external fun initUnityResolve(): Boolean

    external fun threadAttach()

    external fun threadDetach()

    external fun findAssembly(name: String): Long

    external fun findClass(assembly: Long, name: String, namespace: String = "*"): Long

    external fun getFieldOffset(clazz: Long, name: String): Int

    external fun getObjectField(instance: Long, clazz: Long, name: String): Long

    external fun getFieldValue(instance: Long, clazz: Long, name: String): Any?

    external fun setFieldValue(instance: Long, clazz: Long, name: String, value: Any?): Boolean

    external fun getStaticObjectField(clazz: Long, name: String): Long

    external fun findMethod(clazz: Long, name: String, parameterTypes: Array<String> = emptyArray()): Long

    external fun getMethodFunction(method: Long): Long

    external fun invokeVoid(method: Long, instance: Long = 0, arguments: IntArray = intArrayOf())


    external fun invokeVoidObjects(method: Long, instance: Long = 0, arguments: LongArray = longArrayOf())

    /** Calls any managed method and returns its IL2CPP object/boxed result handle. */
    external fun invoke(method: Long, instance: Long = 0, arguments: LongArray = longArrayOf()): Long

    external fun newString(value: String): Long

    /** Returns active MonoBehaviour instances as a JSON snapshot. */
    external fun getEntitySnapshot(): String
}
