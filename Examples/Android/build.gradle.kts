// The Carbon Gallery as an Android app: a GameActivity whose native code (CMakeLists.txt next to this file) runs
// the shared Gallery on Carbon's OpenGL ES backend.
//
// The sources sit in this folder (the activity in Java/) rather than in Gradle's src/main tree. Everything Gradle
// and the NDK produce goes to Build/Android in the repository, next to the other build trees.
plugins {
    id("com.android.application") version "8.13.2"
}

layout.buildDirectory.set(file("../../Build/Android/Gradle"))

android {
    namespace = "io.github.eliasertl.carbon.gallery"
    compileSdk = 36
    ndkVersion = "27.3.13750724"

    defaultConfig {
        applicationId = "io.github.eliasertl.carbon.gallery"
        // Android 9: the first with display cutouts.
        minSdk = 28
        targetSdk = 36
        versionCode = 1
        versionName = "0.1.0"
        ndk {
            // Phones and tablets (arm64), and the emulator on a PC (x86_64).
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf("-DANDROID_STL=c++_static")
            }
        }
    }

    sourceSets {
        getByName("main") {
            manifest.srcFile("AndroidManifest.xml")
            java.setSrcDirs(listOf("Java"))
            res.setSrcDirs(emptyList<String>())
            assets.setSrcDirs(emptyList<String>())
        }
    }

    buildTypes {
        getByName("release") {
            isMinifyEnabled = false
            // Signed with the debug key, so that a release build installs without a keystore of its own.
            signingConfig = signingConfigs.getByName("debug")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        prefab = true
    }

    externalNativeBuild {
        cmake {
            path = file("CMakeLists.txt")
            version = "3.31.6"
            buildStagingDirectory = file("../../Build/Android/Native")
        }
    }
}

dependencies {
    implementation("androidx.games:games-activity:4.4.2")
    implementation("androidx.appcompat:appcompat:1.7.1")
    implementation("androidx.core:core:1.17.0")
}
