package io.github.libxposed.lovebear

import android.util.Log
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import io.github.libxposed.lovebear.ModuleMainKt.Companion.TAG
import top.yukonga.miuix.kmp.basic.Button
import top.yukonga.miuix.kmp.basic.Card
import top.yukonga.miuix.kmp.basic.ColorPalette
import top.yukonga.miuix.kmp.basic.FloatingNavigationBar
import top.yukonga.miuix.kmp.basic.FloatingNavigationBarItem
import top.yukonga.miuix.kmp.basic.Icon
import top.yukonga.miuix.kmp.basic.IconButton
import top.yukonga.miuix.kmp.basic.Scaffold
import top.yukonga.miuix.kmp.basic.SmallTopAppBar
import top.yukonga.miuix.kmp.basic.Text
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

    var selectedIndex by remember { mutableStateOf(0) }
    val items = listOf("Home", "Profile", "Settings")
    val icons = listOf(MiuixIcons.VerticalSplit, MiuixIcons.Contacts, MiuixIcons.Settings)

    Card {
        Scaffold(
            modifier = Modifier.height(500.dp),
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
                    Spacer(Modifier.height(8.dp))
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
                        title = "局内大量金币", checked = isChecked, onCheckedChange = {
                            isChecked = it
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
                            if (!playerControlClass.set(self, "coin", 114514191)) {
                                return@SwitchPreference
                            }
                        })
                }

                item {
                    var isChecked by remember { mutableStateOf(false) }
                    SwitchPreference(
                        title = "局内大量分数", checked = isChecked, onCheckedChange = {
                            isChecked = it
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
                            if (!playerControlClass.set(self, "score", 114514191)) {
                                return@SwitchPreference
                            }
                        })
                }
            }

        }
    }


}
