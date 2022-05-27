set(ToolsFolder "Tools")

add_subdirectory(shader_compiler)

#add_subdirectory(shaders)
set_target_properties(HorizonShaderCompiler PROPERTIES FOLDER ${ToolsFolder}/shader_compiler)
#set_target_properties(ShaderFiles PROPERTIES FOLDER ${ToolsFolder}/shader_compiler)