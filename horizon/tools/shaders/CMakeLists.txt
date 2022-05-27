project(ShaderFiles)

file(GLOB_RECURSE SHADER_FILES "*.vert" "*.frag")
message(${SHADER_FILES})
add_custom_target(${PROJECT_NAME} ${SHADER_FILES})