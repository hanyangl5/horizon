set(SHADER_SOURCE_DIR ${ENGINE_SOURCE_DIR}/Shaders)
file(GLOB_RECURSE SHADER_SOURCE_FILES ${SHADER_SOURCE_DIR}/*)

# Organize shaders into a source group for Visual Studio
source_group(TREE ${SHADER_SOURCE_DIR} PREFIX "Shaders" FILES ${SHADER_SOURCE_FILES})

# Add a dummy target to make shaders appear in Visual Studio
add_custom_target(Shaders ALL
    COMMENT "Compiling Shaders [${TARGET_NAME}]"
)
set_property(TARGET Shaders APPEND PROPERTY SOURCES ${SHADER_SOURCE_FILES})

set_target_properties(Shaders PROPERTIES FOLDER "Horizon")
