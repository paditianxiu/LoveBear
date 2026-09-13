package io.github.libxposed.lovebear

import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.compose.runtime.Composable
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.lifecycle.ViewModelStore
import androidx.lifecycle.ViewModelStoreOwner
import androidx.lifecycle.findViewTreeLifecycleOwner
import androidx.lifecycle.findViewTreeViewModelStoreOwner
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.lifecycle.setViewTreeViewModelStoreOwner
import androidx.savedstate.SavedStateRegistry
import androidx.savedstate.SavedStateRegistryController
import androidx.savedstate.SavedStateRegistryOwner
import androidx.savedstate.findViewTreeSavedStateRegistryOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import java.util.IdentityHashMap

/**
 * 在宿主 ViewGroup 中挂载一个 ComposeView。
 *
 * 返回的 Handle 必须在宿主页面销毁或不再需要时调用 dispose()。
 */
fun ViewGroup.mountCompose(
    tag: Any,
    lifecycleAnchor: View = this,
    layoutParams: ViewGroup.LayoutParams = ViewGroup.LayoutParams(
        ViewGroup.LayoutParams.MATCH_PARENT,
        ViewGroup.LayoutParams.MATCH_PARENT
    ),
    configure: ComposeView.() -> Unit = {},
    content: @Composable () -> Unit
): ComposeMountHandle {
    findViewWithTag<View>(tag)?.let { oldView ->
        (oldView.parent as? ViewGroup)?.removeView(oldView)
    }

    val handle = ComposeMountHandle(
        root = this,
        lifecycleAnchor = lifecycleAnchor,
        tag = tag,
        layoutParams = layoutParams,
        configure = configure,
        content = content
    )
    handle.attachWhenReady()
    return handle
}

class ComposeMountHandle internal constructor(
    private val root: ViewGroup,
    private val lifecycleAnchor: View,
    private val tag: Any,
    private val layoutParams: ViewGroup.LayoutParams,
    private val configure: ComposeView.() -> Unit,
    private val content: @Composable () -> Unit
) : View.OnAttachStateChangeListener {
    private var owner: HostComposeOwner? = null
    private var hostView: ComposeView? = null
    private var listening = false
    private var disposed = false

    internal fun attachWhenReady() {
        if (disposed) return
        if (!listening) {
            lifecycleAnchor.addOnAttachStateChangeListener(this)
            listening = true
        }
        if (lifecycleAnchor.isAttachedToWindow) {
            attach()
        }
    }

    override fun onViewAttachedToWindow(view: View) {
        attach()
    }

    override fun onViewDetachedFromWindow(view: View) {
        detach()
    }

    /** 主动释放注入的 Compose 内容。 */
    fun dispose() {
        if (disposed) return
        disposed = true
        detach()
        if (listening) {
            lifecycleAnchor.removeOnAttachStateChangeListener(this)
            listening = false
        }
    }

    private fun attach() {
        if (disposed) return
        detach()

        val nextOwner = HostComposeOwner()
        nextOwner.install(lifecycleAnchor)
        nextOwner.install(root)
        nextOwner.resume()
        owner = nextOwner

        val nextView = ComposeView(root.context).apply {
            tag = this@ComposeMountHandle.tag
            nextOwner.install(this)
            setViewCompositionStrategy(
                ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed
            )
            configure()
            setContent {
                content()
            }
            addOnAttachStateChangeListener(object : View.OnAttachStateChangeListener {
                override fun onViewAttachedToWindow(view: View) {
                    nextOwner.install(view)
                    nextOwner.resume()
                }

                override fun onViewDetachedFromWindow(view: View) {
                    disposeComposition()
                    removeOnAttachStateChangeListener(this)
                }
            })
        }

        hostView = nextView
        root.addView(nextView, layoutParams)
    }

    private fun detach() {
        val oldView = hostView
        hostView = null
        if (oldView != null) {
            oldView.disposeComposition()
            (oldView.parent as? ViewGroup)?.removeView(oldView)
        }
        owner?.destroy()
        owner = null
    }
}

/**
 * ComposeView 依赖的最小宿主环境。
 * 宿主 ViewTree 已经有这些 Owner 时也可以直接复用宿主 Owner；
 * Xposed 注入或普通 View 容器没有 Owner 时，则使用这个实现。
 */
private class HostComposeOwner :
    LifecycleOwner,
    SavedStateRegistryOwner,
    ViewModelStoreOwner {

    private val lifecycleRegistry = LifecycleRegistry(this)
    private val savedStateController = SavedStateRegistryController.create(this)
    private val store = ViewModelStore()
    private val boundViews = IdentityHashMap<View, PreviousOwners>()
    private var created = false
    private var destroyed = false

    override val lifecycle: Lifecycle
        get() = lifecycleRegistry

    override val savedStateRegistry: SavedStateRegistry
        get() = savedStateController.savedStateRegistry

    override val viewModelStore: ViewModelStore
        get() = store

    fun install(view: View) {
        if (destroyed) return
        create()

        var current: View? = view
        while (current != null) {
            installOnView(current)
            current = current.parent as? View
        }
    }

    fun resume() {
        if (destroyed) return
        create()
        if (!lifecycleRegistry.currentState.isAtLeast(Lifecycle.State.STARTED)) {
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_START)
        }
        if (!lifecycleRegistry.currentState.isAtLeast(Lifecycle.State.RESUMED)) {
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_RESUME)
        }
    }

    fun destroy() {
        if (destroyed) return

        if (lifecycleRegistry.currentState.isAtLeast(Lifecycle.State.RESUMED)) {
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_PAUSE)
        }
        if (lifecycleRegistry.currentState.isAtLeast(Lifecycle.State.STARTED)) {
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_STOP)
        }
        if (lifecycleRegistry.currentState.isAtLeast(Lifecycle.State.CREATED)) {
            lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_DESTROY)
        }

        boundViews.forEach { (view, previous) ->
            if (view.findViewTreeLifecycleOwner() === this) {
                view.setViewTreeLifecycleOwner(previous.lifecycleOwner)
            }
            if (view.findViewTreeSavedStateRegistryOwner() === this) {
                view.setViewTreeSavedStateRegistryOwner(previous.savedStateOwner)
            }
            if (view.findViewTreeViewModelStoreOwner() === this) {
                view.setViewTreeViewModelStoreOwner(previous.viewModelOwner)
            }
        }
        boundViews.clear()
        store.clear()
        destroyed = true
    }

    private fun create() {
        if (created) return
        savedStateController.performAttach()
        savedStateController.performRestore(null)
        lifecycleRegistry.handleLifecycleEvent(Lifecycle.Event.ON_CREATE)
        created = true
    }

    private fun installOnView(view: View) {
        if (!boundViews.containsKey(view)) {
            boundViews[view] = PreviousOwners(
                lifecycleOwner = view.findViewTreeLifecycleOwner(),
                savedStateOwner = view.findViewTreeSavedStateRegistryOwner(),
                viewModelOwner = view.findViewTreeViewModelStoreOwner()
            )
        }
        view.setViewTreeLifecycleOwner(this)
        view.setViewTreeSavedStateRegistryOwner(this)
        view.setViewTreeViewModelStoreOwner(this)
    }

    private data class PreviousOwners(
        val lifecycleOwner: LifecycleOwner?,
        val savedStateOwner: SavedStateRegistryOwner?,
        val viewModelOwner: ViewModelStoreOwner?
    )
}