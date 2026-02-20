plugins {
    id("com.android.library")
}

android {
    namespace = "com.horizon.engine"
    compileSdk = 36

    defaultConfig {
        minSdk = 31
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
}
