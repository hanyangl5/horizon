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

set(TBB_TEST OFF)
set(TBB_EXAMPLESOFF)
set(TBB_STRICT OFF)
set(TBB_WINDOWS_DRIVEROFF)
set(TBB_NO_APPCONTAINER OFF)
set(TBB4PY_BUILD OFF)
set(TBB_BUILD ON)
set(TBBMALLOC_BUILD OFF)
set(TBB_CPF OFF)
set(TBB_FIND_PACKAGE OFF)
set(TBB_DISABLE_HWLOC_AUTOMATIC_SEARCH OFF)
set(TBB_ENABLE_IPO OFF)

set(BUILD_SHARED_LIBS OFF)

add_subdirectory(oneTBB)
set(SPIRV_REFLECT_EXECUTABLE OFF)
set(SPIRV_REFLECT_EXAMPLES OFF)

# set(SPIRV_REFLECT_STRIPPER OFF)
# set(SPIRV_REFLECT_STATIC_LIB ON)
add_subdirectory(spirv-reflect)

add_subdirectory(spdlog)
add_subdirectory(VulkanMemoryAllocator)

target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json glfw DirectXTK12 spdlog GPUOpen::VulkanMemoryAllocator assimp tbb)

set_property(TARGET ${PROJECT_NAME} nlohmann_json glfw DirectXTK12 spdlog VulkanMemoryAllocator
    assimp uninstall unit zlibstatic
    docs
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