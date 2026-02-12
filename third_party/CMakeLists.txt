project(third_party)

set(third_party_folder "third_party")

add_library(${PROJECT_NAME} STATIC 3rd_party.cpp)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fms-extensions )
endif()
# find vulkan
find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${Vulkan_LIBRARIES})

OPTION(BUILD_SHARED_LIBS
    "Build package with shared libraries."
    OFF
)
OPTION(ASSIMP_BUILD_TESTS
    "If the test suite for Assimp is built in addition to the library."
    OFF
)
OPTION(ASSIMP_INSTALL
    "Disable this if you want to use assimp as a submodule."
    OFF
)
OPTION(ASSIMP_WARNINGS_AS_ERRORS
    "Treat all warnings as errors."
    OFF
)
add_subdirectory(assimp)

option(BUILD_DXIL_SHADERS "Use DXC Shader Model 6 for shaders" OFF)

# add_subdirectory(directxtk12)

# SimpleMath depends on DirectXMath which is Windows-specific
# On Android, we need to use an alternative math library (e.g., GLM)
if(NOT ANDROID)
    add_library(simplemath STATIC directxtk12/src/SimpleMath.cpp)
    target_include_directories(simplemath PUBLIC directxtk12/Inc)
endif()

# GLFW is not supported on Android, skip building it
if(NOT ANDROID)
    set(GLFW_LIBRARY_TYPE "STATIC")
    add_subdirectory(glfw3)
endif()

add_subdirectory(spdlog)
add_subdirectory(VulkanMemoryAllocator)

target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/spirv-reflect/spirv_reflect.c)
set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/spirv-reflect/spirv_reflect.c PROPERTIES LANGUAGE C)

# Link libraries - glfw and simplemath are not available on Android
if(ANDROID)
    target_link_libraries(${PROJECT_NAME} PRIVATE spdlog GPUOpen::VulkanMemoryAllocator assimp)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE glfw simplemath spdlog GPUOpen::VulkanMemoryAllocator assimp)
endif()

# Set folder property - glfw and simplemath are not available on Android
if(ANDROID)
    set_property(TARGET ${PROJECT_NAME} spdlog VulkanMemoryAllocator
        assimp zlibstatic
        PROPERTY FOLDER "third_party")
else()
    set_property(TARGET ${PROJECT_NAME} glfw simplemath spdlog VulkanMemoryAllocator
        assimp uninstall zlibstatic
        update_mappings
        PROPERTY FOLDER "third_party")
endif()
    
IF(MSVC12 OR MSVC14 OR MSVC15 )
    set_property(TARGET UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTY FOLDER "third_party")
endif()

# Include directories - glfw3/include and directxtk12/Inc are not available on Android
if(ANDROID)
    target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
        assimp/include
        spdlog/include
        VulkanMemoryAllocator/include
        spirv-reflect
        spirv-reflect/include
        nlohmann_json/include
        ${CMAKE_BINARY_DIR}/third_party/assimp/include
        include
        dxc_2025_07_14/inc
    )
else()
    target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
        assimp/include
        directxtk12/Inc
        glfw3/include
        spdlog/include
        VulkanMemoryAllocator/include
        spirv-reflect
        spirv-reflect/include
        nlohmann_json/include
        ${CMAKE_BINARY_DIR}/third_party/assimp/include
        include
        dxc_2025_07_14/inc
    )
endif()

# Add DXC library path and link
if(WIN32)
    # Determine architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|aarch64")
            set(DXC_ARCH "arm64")
        else()
            set(DXC_ARCH "x64")
        endif()
    else()
        set(DXC_ARCH "x86")
    endif()
    
    # Add DXC library directory
    target_link_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/dxc_2025_07_14/lib/${DXC_ARCH})
    
    # Link DXC library
    target_link_libraries(${PROJECT_NAME} PUBLIC dxcompiler)
endif()