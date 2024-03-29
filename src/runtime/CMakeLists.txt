project(runtime)

if(MSVC)
 add_compile_options("/MP")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE HEADER_FILES "*.h")
file(GLOB_RECURSE SOURCE_FILES "*.cpp")

#source_group(TREE "${ENGINE_ROOT_DIR}/schema" FILES ${SCHEMA_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADER_FILES} ${SOURCE_FILES})

add_library(${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/src)

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

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Horizon")

target_link_libraries(${PROJECT_NAME} PUBLIC spdlog glm glfw tinygltf_lib)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/config)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC 
${CMAKE_SOURCE_DIR}/thirdparty/spdlog/include 
${CMAKE_SOURCE_DIR}/thirdparty/glfw/include 
${CMAKE_SOURCE_DIR}/thirdparty/tinygltf_impl
${CMAKE_SOURCE_DIR}/thirdparty/stb/include)

