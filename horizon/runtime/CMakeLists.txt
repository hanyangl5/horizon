project(runtime)

file(GLOB_RECURSE HEADER_FILES "*.h")
file(GLOB_RECURSE SOURCE_FILES "*.cpp")

#source_group(TREE "${ENGINE_ROOT_DIR}/schema" FILES ${SCHEMA_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADER_FILES} ${SOURCE_FILES})

add_library(${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/horizon)

# find vulkan
find_package(Vulkan REQUIRED)

if(Vulkan_FOUND)
    target_include_directories(${PROJECT_NAME} PUBLIC
        ${Vulkan_INCLUDE_DIRS}
    )

    target_link_libraries(${PROJECT_NAME} PUBLIC 
        ${Vulkan_LIBRARIES}
    )
    message("vulkan found")
else(Vulkan_FOUND)
    message("error: cannot find vulkan")
endif(Vulkan_FOUND)

#target_link_libraries(${PROJECT_NAME} PUBLIC HorizonShaderCompiler)

target_link_libraries(${PROJECT_NAME} PUBLIC VulkanMemoryAllocator D3D12MemoryAllocator spdlog tinygltf glm glfw dxclib dxcompiler)

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Horizon")
