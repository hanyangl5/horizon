include(${CMAKE_SOURCE_DIR}/cmake/compiler_args.cmake)

option(BUILD_ANDROID_APK "Generate Gradle projects and APK packaging targets" OFF)

set(ANDROID_APK_DIR "${CMAKE_BINARY_DIR}/android")
set(ALL_SAMPLE_NAMES "")
set(ANDROID_TEMPLATE_DIR "${CMAKE_SOURCE_DIR}/framework/app_framework/android")

macro(addproject projectdir)
    project(${projectdir})
    file(GLOB_RECURSE APP_HEADERS ${projectdir}/source/*.h)
    file(GLOB_RECURSE APP_SOURCES ${projectdir}/source/*.cpp)

    # Use PREFIX to flatten the folder structure in VS
    # This makes the path start from projectdir instead of samples/projectdir
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/${projectdir}" PREFIX "" FILES ${APP_HEADERS} ${APP_SOURCES})

    if(ANDROID)
        add_library(${PROJECT_NAME} SHARED ${APP_HEADERS} ${APP_SOURCES})
        set_target_properties(${PROJECT_NAME} PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/${projectdir}/${ANDROID_ABI}")
    else()
        add_executable(${PROJECT_NAME} ${APP_HEADERS} ${APP_SOURCES})
    endif()

    target_link_libraries(${PROJECT_NAME} PUBLIC framework)
    target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/framework)

    # Absolute sample root for SetRuntimeSampleRoot (works when assets symlink is missing, e.g. on Windows).
    get_filename_component(SAMPLE_ROOT_ABS "${CMAKE_CURRENT_SOURCE_DIR}/${projectdir}" ABSOLUTE)
    string(REPLACE "\\" "/" RUNTIME_SAMPLE_ROOT "${SAMPLE_ROOT_ABS}")
    set(SAMPLE_CONFIG_DIR "${CMAKE_BINARY_DIR}/samples/${projectdir}")
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.hpp.in ${SAMPLE_CONFIG_DIR}/config.hpp @ONLY)
    target_include_directories(${PROJECT_NAME} PRIVATE ${SAMPLE_CONFIG_DIR})

    # Development-time sample assets path: create samples/<name>/assets symlink to repo assets.
    set(SAMPLE_ASSETS_LINK "${CMAKE_CURRENT_SOURCE_DIR}/${projectdir}/assets")
    if(NOT EXISTS "${SAMPLE_ASSETS_LINK}")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_SOURCE_DIR}/assets" "${SAMPLE_ASSETS_LINK}"
            RESULT_VARIABLE _assets_link_result)
        if(NOT _assets_link_result EQUAL 0)
            message(WARNING "Failed to create assets symlink for ${projectdir}: ${SAMPLE_ASSETS_LINK}")
        endif()
    endif()

    set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "samples")
    configure_compile_options(${projectdir})

    if(BUILD_ANDROID_APK)
        list(APPEND ALL_SAMPLE_NAMES ${projectdir})

        set(SAMPLE_PACKAGE "com.horizon.${projectdir}")
        set(SAMPLE_LABEL "${projectdir}")
        set(SAMPLE_LIB_NAME "${projectdir}")
        set(SAMPLE_NAME "${projectdir}")
        set(NATIVE_LIB_DIR "${CMAKE_BINARY_DIR}/lib/${projectdir}")

        string(REPLACE "." "/" SAMPLE_PACKAGE_PATH "${SAMPLE_PACKAGE}")

        configure_file(
            ${ANDROID_TEMPLATE_DIR}/template/AndroidManifest.xml.in
            ${ANDROID_APK_DIR}/${projectdir}/src/main/AndroidManifest.xml
            @ONLY)
        configure_file(
            ${ANDROID_TEMPLATE_DIR}/template/MainActivity.java.in
            ${ANDROID_APK_DIR}/${projectdir}/src/main/java/${SAMPLE_PACKAGE_PATH}/MainActivity.java
            @ONLY)
        configure_file(
            ${ANDROID_TEMPLATE_DIR}/template/build.gradle.kts.in
            ${ANDROID_APK_DIR}/${projectdir}/build.gradle.kts
            @ONLY)
    endif()
endmacro()

addproject(deferred)
addproject(hellotriangle)

# Generate root Gradle project and APK packaging targets
if(BUILD_ANDROID_APK)
    # Build settings.gradle.kts include lines from collected sample names
    set(SAMPLE_MODULE_INCLUDES "")
    foreach(sample ${ALL_SAMPLE_NAMES})
        string(APPEND SAMPLE_MODULE_INCLUDES "include(\":${sample}\")\n")
    endforeach()

    configure_file(
        ${ANDROID_TEMPLATE_DIR}/root/settings.gradle.kts.in
        ${ANDROID_APK_DIR}/settings.gradle.kts
        @ONLY)

    # Copy static (non-templated) files into the build directory
    file(COPY ${ANDROID_TEMPLATE_DIR}/root/build.gradle.kts    DESTINATION ${ANDROID_APK_DIR})
    file(COPY ${ANDROID_TEMPLATE_DIR}/root/gradle.properties    DESTINATION ${ANDROID_APK_DIR})
    file(COPY ${ANDROID_TEMPLATE_DIR}/engine/build.gradle.kts   DESTINATION ${ANDROID_APK_DIR}/engine)
    file(COPY ${ANDROID_TEMPLATE_DIR}/engine/src/               DESTINATION ${ANDROID_APK_DIR}/engine/src)
    file(COPY ${ANDROID_TEMPLATE_DIR}/shared/HorizonActivity.java
         DESTINATION ${ANDROID_APK_DIR}/engine/src/main/java/com/horizon/engine/)
    file(COPY ${ANDROID_TEMPLATE_DIR}/gradle/gradlew            DESTINATION ${ANDROID_APK_DIR})
    file(COPY ${ANDROID_TEMPLATE_DIR}/gradle/gradlew.bat        DESTINATION ${ANDROID_APK_DIR})
    file(COPY ${ANDROID_TEMPLATE_DIR}/gradle/wrapper/           DESTINATION ${ANDROID_APK_DIR}/gradle/wrapper)

    # Write local.properties pointing to the Android SDK
    if(DEFINED ENV{ANDROID_SDK_ROOT})
        set(_SDK_DIR "$ENV{ANDROID_SDK_ROOT}")
    elseif(DEFINED ENV{ANDROID_HOME})
        set(_SDK_DIR "$ENV{ANDROID_HOME}")
    elseif(DEFINED ANDROID_NDK)
        # NDK is typically at <sdk>/ndk/<version>; walk up two levels
        get_filename_component(_SDK_DIR "${ANDROID_NDK}" DIRECTORY)
        get_filename_component(_SDK_DIR "${_SDK_DIR}" DIRECTORY)
    else()
        set(_SDK_DIR "")
    endif()
    if(_SDK_DIR)
        string(REPLACE "\\" "/" _SDK_DIR_ESCAPED "${_SDK_DIR}")
        file(WRITE ${ANDROID_APK_DIR}/local.properties "sdk.dir=${_SDK_DIR_ESCAPED}\n")
    endif()

    # Per-sample APK targets
    foreach(sample ${ALL_SAMPLE_NAMES})
        if(CMAKE_HOST_WIN32)
            # Host is Windows: use gradlew.bat, no chmod needed
            add_custom_target(apk_${sample}
                COMMAND ${CMAKE_COMMAND} -E chdir ${ANDROID_APK_DIR} gradlew.bat :${sample}:assembleDebug
                DEPENDS ${sample}
                COMMENT "Packaging APK for ${sample}"
            )
        else()
            # Host is Unix-like: use gradlew, need chmod
            add_custom_target(apk_${sample}
                COMMAND chmod +x ${ANDROID_APK_DIR}/gradlew
                COMMAND ${CMAKE_COMMAND} -E chdir ${ANDROID_APK_DIR} ./gradlew :${sample}:assembleDebug
                DEPENDS ${sample}
                COMMENT "Packaging APK for ${sample}"
            )
        endif()
    endforeach()

    # Umbrella target that builds all APKs
    set(ALL_APK_TARGETS "")
    foreach(sample ${ALL_SAMPLE_NAMES})
        list(APPEND ALL_APK_TARGETS apk_${sample})
    endforeach()
    add_custom_target(apk_all DEPENDS ${ALL_APK_TARGETS})
endif()
