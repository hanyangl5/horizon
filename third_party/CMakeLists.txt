project(third_party)

set(third_party_folder "third_party")

add_library(${PROJECT_NAME} STATIC 3rd_party.cpp)

# find vulkan
find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${Vulkan_LIBRARIES})

add_subdirectory(assimp)
add_subdirectory(directxtk12)
set(GLFW_LIBRARY_TYPE "STATIC")
add_subdirectory(glfw3)
add_subdirectory(nlohmann_json)

option(TBB_TEST "Enable testing" OFF)
option(TBB_EXAMPLES "Enable examples" OFF)
option(TBB_STRICT "Treat compiler warnings as errors" OFF)
option(TBB_WINDOWS_DRIVER "Build as Universal Windows Driver (UWD)" OFF)
option(TBB_NO_APPCONTAINER "Apply /APPCONTAINER:NO (for testing binaries for Windows Store)" OFF)
option(TBB4PY_BUILD "Enable tbb4py build" OFF)
option(TBB_BUILD "Enable tbb build" ON)
option(TBBMALLOC_BUILD "Enable tbbmalloc build" OFF)
cmake_dependent_option(TBBMALLOC_PROXY_BUILD "Enable tbbmalloc_proxy build" OFF "TBBMALLOC_BUILD" OFF)
option(TBB_CPF "Enable preview features of the library" OFF)
option(TBB_FIND_PACKAGE "Enable search for external oneTBB using find_package instead of build from sources" OFF)
option(TBB_DISABLE_HWLOC_AUTOMATIC_SEARCH "Disable HWLOC automatic search by pkg-config tool" OFF)
option(TBB_ENABLE_IPO "Enable Interprocedural Optimization (IPO) during the compilation" OFF)

set(BUILD_SHARED_LIBS OFF)

add_subdirectory(oneTBB)

# OPTION(SPIRV_REFLECT_EXECUTABLE "Build spirv-reflect executable" OFF)
# OPTION(SPIRV_REFLECT_EXAMPLES "Build stripper examples" OFF)
# OPTION(SPIRV_REFLECT_STRIPPER "Build stripper utility" OFF)
# OPTION(SPIRV_REFLECT_STATIC_LIB "Build a SPIRV-Reflect static library" OFF)
# OPTION(SPIRV_REFLECT_BUILD_TESTS "Build the SPIRV-Reflect test suite" OFF)
# OPTION(SPIRV_REFLECT_ENABLE_ASSERTS "Enable asserts for debugging" OFF)

# add_subdirectory(spirv-reflect)

add_subdirectory(spdlog)
add_subdirectory(VulkanMemoryAllocator)

target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json glfw DirectXTK12 spdlog GPUOpen::VulkanMemoryAllocator assimp tbb)

set_property(TARGET ${PROJECT_NAME} nlohmann_json glfw DirectXTK12 spdlog VulkanMemoryAllocator
    assimp uninstall unit zlibstatic
    update_mappings
    tbb
    PROPERTY FOLDER "ThirdParty")

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
    oneTBB/include
)