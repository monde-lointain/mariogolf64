# Packs libmain.so into an Android app, an APK, with Gradle, which runs
# from SDL's own Android project: its wrapper, and its Java sources, which
# match the SDL linked in. Gradle needs a Java between 17 and 23, which
# MG64_JAVA_HOME names, and downloads its Android plugin on its first run.
#
#   cmake --build --preset android-arm64 --target mg64_apk
#
# builds a debug APK, signed with the SDK's debug key.

if(MG64_USE_SYSTEM_SDL)
    message(FATAL_ERROR "The Android app needs SDL's sources, which MG64_USE_SYSTEM_SDL lacks")
endif()

set(MG64_JAVA_HOME "$ENV{JAVA_HOME}" CACHE PATH "The Java, 17 to 23, that Gradle runs on")
if(NOT IS_DIRECTORY "${MG64_JAVA_HOME}")
    message(WARNING "MG64_JAVA_HOME names no Java, so mg64_apk can't run Gradle")
endif()

# The NDK is the SDK's ndk/<version>
cmake_path(GET CMAKE_ANDROID_NDK PARENT_PATH ndk_versions)
cmake_path(GET ndk_versions PARENT_PATH android_sdk)

set(sdl_project "${sdl3_SOURCE_DIR}/android-project")
if(CMAKE_HOST_WIN32)
    set(gradle "${sdl_project}/gradlew.bat")
else()
    set(gradle "${sdl_project}/gradlew")
endif()

set(native_library "${CMAKE_CURRENT_BINARY_DIR}/jniLibs/${CMAKE_ANDROID_ARCH_ABI}/libmain.so")
set(gradle_dir "${CMAKE_CURRENT_BINARY_DIR}/gradle")
set(apk "${gradle_dir}/app/outputs/apk/debug/app-debug.apk")

set(gradle_sources
    settings.gradle
    build.gradle
    gradle.properties
    app/build.gradle
    app/src/main/AndroidManifest.xml
    app/src/main/java/org/mariogolf64/MainActivity.java
    app/src/main/res/values/strings.xml
)
list(TRANSFORM gradle_sources PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")

# Gradle takes each ABI's libraries from a directory of the ABI's name
add_custom_command(
    OUTPUT "${native_library}"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:mariogolf64>" "${native_library}"
    DEPENDS mariogolf64
    COMMENT "Copying libmain.so for Gradle"
    VERBATIM
)

add_custom_command(
    OUTPUT "${apk}"
    COMMAND "${CMAKE_COMMAND}" -E env
        "ANDROID_HOME=${android_sdk}"
        "JAVA_HOME=${MG64_JAVA_HOME}"
        "${gradle}" --quiet
            --project-dir "${CMAKE_CURRENT_SOURCE_DIR}"
            --project-cache-dir "${gradle_dir}/cache"
            "-Pmg64.sdlJavaDir=${sdl_project}/app/src/main/java"
            "-Pmg64.nativeLibDir=${CMAKE_CURRENT_BINARY_DIR}/jniLibs"
            "-Pmg64.buildDir=${gradle_dir}"
            "-Pmg64.versionName=${PROJECT_VERSION}"
            "-Pmg64.minSdk=${ANDROID_PLATFORM_LEVEL}"
            assembleDebug
    DEPENDS "${native_library}" ${gradle_sources}
    COMMENT "Packing the Android app"
    VERBATIM
    USES_TERMINAL
)

add_custom_target(mg64_apk DEPENDS "${apk}")
