cmake_minimum_required(VERSION 3.18)

project(horizon)

set(CMAKE_CXX_STANDARD 17)

set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/horizon)

set(THIRD_PARTY ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

file(GLOB_RECURSE HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h ${PROJECT_SOURCE_DIR}/*.hpp ${PROJECT_SOURCE_DIR}/*.ini) 

file(GLOB_RECURSE SOURCE_FILES ${PROJECT_SOURCE_DIR}/*.cpp ${PROJECT_SOURCE_DIR}/*.c)

set(AllFile ${HEADER_FILES} ${SOURCE_FILES})

add_executable(${PROJECT_NAME} ${AllFile})

target_include_directories(${PROJECT_NAME} PUBLIC
    ${HEADER_FILES}
)

# find vulkan
find_package(Vulkan REQUIRED)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PUBLIC 
    ${Vulkan_LIBRARIES}
)

#add_subdirectory(${THIRD_PARTY}/glm)
#add_subdirectory(${THIRD_PARTY}/spdlog)
#add_subdirectory(${THIRD_PARTY}/imgui)
#add_subdirectory(${THIRD_PARTY}/glfw)
#add_subdirectory(${THIRD_PARTY}/tiny_gltf)

