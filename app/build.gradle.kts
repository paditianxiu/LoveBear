plugins {
    alias(libs.plugins.agp.app)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "me.paditianxiu.lovebear"
    compileSdk = 37
    buildToolsVersion = "37.0.0"

    defaultConfig {
        minSdk = 33
        targetSdk = 37
        versionCode = 1
        versionName = "1.0"
        ndk {
            abiFilters += listOf("arm64-v8a", "armeabi-v7a")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            isShrinkResources = false
            proguardFiles("proguard-rules.pro")
            signingConfig = signingConfigs["debug"]
        }
    }

    buildFeatures {
        viewBinding = true
        compose = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_21
        targetCompatibility = JavaVersion.VERSION_21
    }

    packaging {
        resources {
            merges += "META-INF/xposed/*"
            excludes += "**"
        }
    }

    lint {
        abortOnError = true
        checkReleaseBuilds = false
    }
}

dependencies {
    implementation(libs.material3)
    compileOnly(libs.libxposed.api)
    implementation(libs.libxposed.service)

    implementation("com.github.princekin-f:EasyFloat:2.0.4")

    implementation(platform(libs.compose.bom))
    implementation(libs.compose.foundation.layout)
    implementation(libs.compose.material3)
    implementation(libs.compose.ui)
    implementation("androidx.lifecycle:lifecycle-runtime-android:2.11.0")
    implementation("androidx.lifecycle:lifecycle-viewmodel-android:2.11.0")
    implementation("androidx.savedstate:savedstate-android:1.5.0")

    val miuix = "0.9.4-rc01"

    implementation("top.yukonga.miuix.kmp:miuix-ui-android:$miuix")
    // Optional: Add miuix-preference for preference components
    implementation("top.yukonga.miuix.kmp:miuix-preference-android:$miuix")
    // Optional: Add miuix-icons for more icons
    implementation("top.yukonga.miuix.kmp:miuix-icons-android:$miuix")
    // Optional: Add miuix-squircle for squircle (smooth rounded corner) shapes
    implementation("top.yukonga.miuix.kmp:miuix-squircle-android:$miuix")

    implementation("top.yukonga.miuix.kmp:miuix-blur-android:$miuix")

}
