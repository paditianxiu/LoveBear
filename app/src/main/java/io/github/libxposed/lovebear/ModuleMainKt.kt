package io.github.libxposed.lovebear

import android.app.Activity
import android.os.Bundle
import android.util.Log
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import com.lzf.easyfloat.EasyFloat
import io.github.libxposed.api.XposedModule
import io.github.libxposed.api.XposedModuleInterface.PackageReadyParam
import io.github.libxposed.api.XposedModuleInterface.SystemServerStartingParam
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

class ModuleMainKt : XposedModule() {
    companion object {
        const val TAG = "LoveBear"
        private const val MAIN_WINDOW_TAG = "main-window"
        private const val FLOATING_BALL_TAG = "loating-ball"
    }

    override fun onPackageReady(param: PackageReadyParam) {

        if (!param.isFirstPackage) return

        val gameActivityClass =
            Class.forName("com.joym.sdk.core.GGameActivity", true, param.classLoader)
        val onCreateMethod = gameActivityClass.getDeclaredMethod("onCreate", Bundle::class.java)

        hook(onCreateMethod).intercept { chain ->
            val result = chain.proceed()

            System.loadLibrary("padi")


            val ctx: Activity = chain.thisObject as? Activity ?: return@intercept result
            val floatingRoot = FrameLayout(ctx)


            fun showMainWindow() {
                CoroutineScope(Dispatchers.IO).launch {
                    val ok = NativeFunctions.initUnityResolve()
                    if (!ok) {
                        Log.w(TAG, "initUnityResolve failed")
                        return@launch
                    }
                }

                if (EasyFloat.getFloatView(MAIN_WINDOW_TAG) != null) {
                    EasyFloat.show(MAIN_WINDOW_TAG)
                    return
                }

                val mainRoot = FrameLayout(ctx)
                mainRoot.mountCompose(
                    tag = MAIN_WINDOW_TAG,
                    lifecycleAnchor = mainRoot,
                    layoutParams = FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT
                    ),
                    configure = {
                        isClickable = true
                        isFocusable = true
                        importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
                        elevation = 100f
                    }) {
                    MainWindow(onClose = {
                        EasyFloat.hide(MAIN_WINDOW_TAG)
                        EasyFloat.show(FLOATING_BALL_TAG)
                    })
                }

                EasyFloat.with(ctx).setTag(MAIN_WINDOW_TAG).setLayout(mainRoot)
                    .setGravity(Gravity.CENTER).setLayoutChangedGravity(Gravity.CENTER)
                    .setDragEnable(false).show()
            }

            floatingRoot.mountCompose(
                tag = FLOATING_BALL_TAG,
                lifecycleAnchor = floatingRoot,
                layoutParams = FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT
                ),
                configure = {
                    isClickable = true
                    isFocusable = true
                    importantForAccessibility = View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
                    elevation = 100f
                }) {
                FloatingBall(onClick = {
                    showMainWindow()
                    EasyFloat.hide(FLOATING_BALL_TAG)
                })
            }

            EasyFloat.with(ctx).setTag(FLOATING_BALL_TAG).setLayout(floatingRoot)
                .setGravity(Gravity.END or Gravity.CENTER_VERTICAL).setDragEnable(true).show()

            result
        }


    }

    override fun onSystemServerStarting(param: SystemServerStartingParam) {
        log(Log.INFO, TAG, "onSystemServerStarting, system classloader: " + param.classLoader)
    }
}
