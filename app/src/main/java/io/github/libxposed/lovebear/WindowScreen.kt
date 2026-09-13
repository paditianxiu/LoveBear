package io.github.libxposed.lovebear

import android.util.Log
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import io.github.libxposed.lovebear.ModuleMainKt.Companion.TAG
import top.yukonga.miuix.kmp.basic.Button
import top.yukonga.miuix.kmp.basic.ButtonDefaults
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.ColorPalette
import top.yukonga.miuix.kmp.basic.FloatingNavigationBar
import top.yukonga.miuix.kmp.basic.FloatingNavigationBarItem
import top.yukonga.miuix.kmp.basic.HorizontalDivider
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.SmallTitle
import top.yukonga.miuix.kmp.basic.SmallTopAppBar
import top.yukonga.miuix.kmp.basic.Text
import top.yukonga.miuix.kmp.blur.highlight.Highlight
import top.yukonga.miuix.kmp.blur.rememberLayerBackdrop
import top.yukonga.miuix.kmp.blur.textureBlur
import top.yukonga.miuix.kmp.icon.MiuixIcons
import top.yukonga.miuix.kmp.icon.extended.Back
import top.yukonga.miuix.kmp.icon.extended.Contacts
import top.yukonga.miuix.kmp.icon.extended.Help
import top.yukonga.miuix.kmp.icon.extended.More
import top.yukonga.miuix.kmp.icon.extended.Settings
import top.yukonga.miuix.kmp.icon.extended.VerticalSplit
import top.yukonga.miuix.kmp.preference.SwitchPreference

@Composable
fun FloatingBall(onClick: () -> Unit) {
    Card(modifier = Modifier.padding(32.dp)) {
        IconButton(
            onClick
        ) {
            Icon(
                imageVector = MiuixIcons.Help, contentDescription = null
            )
        }
    }
}

@Composable
fun MainWindow(onClose: () -> Unit) {
    var selectedIndex by remember { mutableIntStateOf(0) }
    val items = listOf("Person", "Profile", "Settings")
    val icons = listOf(MiuixIcons.VerticalSplit, MiuixIcons.Contacts, MiuixIcons.Settings)
    val backdrop = rememberLayerBackdrop()
    Card(
        modifier = Modifier
            .background(Color.Transparent)
            .textureBlur(
                backdrop = backdrop,
                shape = RoundedCornerShape(24.dp),
                blurRadius = 40f,
                highlight = Highlight.GlassStrokeMiddleLight,
            )
    ) {
        Scaffold(
            modifier = Modifier
                .height(500.dp)
                .background(Color.Transparent),
            topBar = {
                SmallTopAppBar(title = "LoveBear", navigationIcon = {
                    IconButton(onClick = onClose) {
                        Icon(MiuixIcons.Back, contentDescription = "Back")
                    }
                }, actions = {
                    IconButton(onClick = onClose) {
                        Icon(MiuixIcons.More, contentDescription = "More")
                    }
                })
            },
            bottomBar = {
                FloatingNavigationBar {
                    items.forEachIndexed { index, label ->
                        FloatingNavigationBarItem(
                            selected = selectedIndex == index,
                            onClick = { selectedIndex = index },
                            icon = icons[index],
                            label = label
                        )
                    }
                }
            },
        ) { innerPadding ->
            LazyColumn(
                modifier = Modifier
                    .padding(8.dp)
                    .padding(innerPadding)
            ) {
                item {
                    SmallTitle("局内功能")
                }
                item {
                    var isChecked by remember { mutableStateOf(false) }
                    SwitchPreference(
                        title = "实体坐标绘制", checked = isChecked, onCheckedChange = {
                            isChecked = it
                            EntityOverlayController.setEnabled(it)
                        })
                }
                item {
                    var isChecked by remember { mutableStateOf(false) }
                    SwitchPreference(
                        title = "失重感", checked = isChecked, onCheckedChange = {
                            isChecked = it
                            val modifyValue = (if (it) 0F else 300F)
                            val playerControlClass = Il2Cpp.getClass(
                                assembly = "Assembly-CSharp.dll",
                                namespace = "",
                                className = "PlayerControl"
                            ) ?: run {
                                return@SwitchPreference
                            }
                            val self = playerControlClass.getStaticObject("self")
                            if (self == 0L) {
                                return@SwitchPreference
                            }
                            if (!playerControlClass.set(self, "gravity", modifyValue)) {
                                return@SwitchPreference
                            }
                        })
                }

                item {
                    var isChecked by remember { mutableStateOf(false) }
                    SwitchPreference(
                        title = "角色无敌", checked = isChecked, onCheckedChange = {
                            isChecked = it
                            val modifyValue = (if (it) 2.1474836E9F else 0F)
                            val playerControlClass = Il2Cpp.getClass(
                                assembly = "Assembly-CSharp.dll",
                                namespace = "",
                                className = "PlayerControl"
                            ) ?: run {
                                return@SwitchPreference
                            }
                            val self = playerControlClass.getStaticObject("self")
                            playerControlClass.set(self, "invincible", isChecked)
                            playerControlClass.set(self, "invincibleTime", modifyValue)

                        })
                }

                item {
                    Spacer(Modifier.height(4.dp))
                    HorizontalDivider()
                    Spacer(Modifier.height(4.dp))
                }

                item {
                    Row {
                        Button(
                            modifier = Modifier.weight(1F), onClick = {
                                val playerControlClass = Il2Cpp.getClass(
                                    assembly = "Assembly-CSharp.dll",
                                    namespace = "",
                                    className = "PlayerControl"
                                ) ?: run {
                                    return@Button
                                }
                                val self = playerControlClass.getStaticObject("self")
                                if (!playerControlClass.set(self, "coin", 114514191)) {
                                    return@Button
                                }
                            }, colors = ButtonDefaults.buttonColorsPrimary()
                        ) {
                            Text("大量金币")
                        }

                        Spacer(Modifier.width(8.dp))

                        Button(
                            modifier = Modifier.weight(1F), onClick = {
                                val playerControlClass = Il2Cpp.getClass(
                                    assembly = "Assembly-CSharp.dll",
                                    namespace = "",
                                    className = "PlayerControl"
                                ) ?: run {
                                    return@Button
                                }
                                val self = playerControlClass.getStaticObject("self")
                                if (!playerControlClass.set(self, "score", 114514191)) {
                                    return@Button
                                }
                            }, colors = ButtonDefaults.buttonColorsPrimary()
                        ) {
                            Text("大量分数")
                        }
                    }
                }
            }

        }
    }


}
