cmake_minimum_required(VERSION 3.18)

project(vulkanapp)

set(CMAKE_CXX_STANDARD 17)

file(GLOB SOURCES ${PROJECT_SOURCE_DIR}/vulkanapp/*.cpp ${PROJECT_SOURCE_DIR}/vulkanapp/*.h)
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/vulkanapp
)

# find vulkan
find_package(Vulkan REQUIRED)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PUBLIC 
    ${Vulkan_LIBRARIES}
)

add_subdirectory(3rd_party/spdlog)
add_subdirectory(3rd_party/glfw)
add_subdirectory(3rd_party/glm)

target_include_directories(vulkanapp PUBLIC ../3rd_party)

target_link_libraries(vulkanapp PUBLIC glm::glm glfw spdlog)