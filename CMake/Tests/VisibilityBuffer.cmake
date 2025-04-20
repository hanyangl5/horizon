cmake_minimum_required(VERSION 3.16)

set(VB_PROJECT_DIR ${ENGINE_DIR}/Tests/Visibility_Buffer2)

# Project name
set(PROJECT_NAME VisbilityBuffer)

# Set C++ standard

file(GLOB_RECURSE VB_INCLUDE_FILES ${VB_PROJECT_DIR}/Sources/*.h)
file(GLOB_RECURSE VB_SOURCE_FILES ${VB_PROJECT_DIR}/Sources/*.cpp)
file(GLOB_RECURSE VB_SHADER_FILES ${VB_PROJECT_DIR}/Shaders/*)

#source_group(TREE ${VB_PROJECT_DIR} FILES ${VB_INCLUDE_FILES} ${VB_SOURCE_FILES})
source_group(TREE ${VB_PROJECT_DIR} FILES ${VB_SHADER_FILES})

source_group(TREE ${VB_PROJECT_DIR} FILES ${VB_INCLUDE_FILES} ${VB_SOURCE_FILES})

# Add executable
add_executable(${PROJECT_NAME} ${VB_SOURCE_FILES} ${VB_INCLUDE_FILES})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${RUNTIME_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${ENGINE_RUNTIME})

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Tests/VisbilityBuffer")

if(${CMAKE_INCLUDE_SHADERS})
    # Organize shaders into a source group for Visual Studio

    # Add a dummy target to make shaders appear in Visual Studio
    add_custom_target(VBShaders ALL
        COMMENT "Compiling Shaders [${TARGET_NAME}]"
    )
    set_property(TARGET VBShaders APPEND PROPERTY SOURCES ${VB_SHADER_FILES})

    set_target_properties(VBShaders PROPERTIES FOLDER "Tests/VisbilityBuffer")

endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:${PROJECT_NAME}> $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND_EXPAND_LISTS
)
