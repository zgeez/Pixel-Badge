import java.util.Properties

plugins {
    id("com.android.application")
    id("kotlin-android")
    // The Flutter Gradle Plugin must be applied after the Android and Kotlin Gradle plugins.
    id("dev.flutter.flutter-gradle-plugin")
}

// Owner-supplied release credentials are local and excluded from Git.
val releaseKeys = Properties()
val releaseKeysFile = rootProject.file("key.properties")
if (releaseKeysFile.exists()) {
    releaseKeysFile.inputStream().use { releaseKeys.load(it) }
}

android {
    namespace = "com.ziadg.badge_app"
    compileSdk = flutter.compileSdkVersion
    buildToolsVersion = "36.1.0"
    ndkVersion = flutter.ndkVersion

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = JavaVersion.VERSION_17.toString()
    }

    defaultConfig {
        applicationId = "com.ziadg.badge_app"
        minSdk = flutter.minSdkVersion
        targetSdk = flutter.targetSdkVersion
        versionCode = flutter.versionCode
        versionName = flutter.versionName
    }

    signingConfigs {
        if (releaseKeysFile.exists()) {
            create("ownerRelease") {
                keyAlias = releaseKeys.getProperty("keyAlias") ?: error("Missing keyAlias")
                keyPassword = releaseKeys.getProperty("keyPassword") ?: error("Missing keyPassword")
                storeFile = rootProject.file(releaseKeys.getProperty("storeFile") ?: error("Missing storeFile"))
                storePassword = releaseKeys.getProperty("storePassword") ?: error("Missing storePassword")
            }
        }
    }
    buildTypes {
        release {
            // Without owner credentials the release artifact remains unsigned.
            signingConfig = signingConfigs.findByName("ownerRelease")
        }
    }
}

flutter {
    source = "../.."
}

dependencies {
    testImplementation("junit:junit:4.13.2")
}
