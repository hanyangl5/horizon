project(HorizonShaderCompiler)

set(BUILD_SAMPLE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADERS} ${SOURCES})

if(BUILD_SAMPLE)
	add_executable(${PROJECT_NAME} ${HEADERS} ${SOURCES})
else()
	add_library(${PROJECT_NAME} ${HEADERS} ${SOURCES})
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC     
    spirv-cross-glsl
    spirv-cross-hlsl
    spirv-cross-c
    spirv-cross-cpp
    spirv-cross-reflect
    spirv-cross-msl
    spirv-cross-util
    spirv-cross-core)


