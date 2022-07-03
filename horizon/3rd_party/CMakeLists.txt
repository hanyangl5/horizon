include(D:/vcpkg/scripts/buildsystems/vcpkg.cmake)

project(third_party_libs)

add_library(${PROJECT_NAME} 3rd_party.cpp)

# find vulkan
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(directxtk12 REQUIRED)
find_package(directx-dxc REQUIRED)
find_package(spdlog REQUIRED)
find_package(unofficial-vulkan-memory-allocator REQUIRED)
find_package(d3d12-memory-allocator REQUIRED)

target_link_libraries(${PROJECT_NAME} PUBLIC unofficial::vulkan-memory-allocator::vulkan-memory-allocator)
target_link_libraries(${PROJECT_NAME} PUBLIC Microsoft::DirectXTK12)
target_link_libraries(${PROJECT_NAME} PUBLIC Microsoft::DirectXShaderCompiler)
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog spdlog::spdlog_header_only)
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)
target_link_libraries(${PROJECT_NAME} PUBLIC unofficial::d3d12-memory-allocator)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${Vulkan_LIBRARIES})