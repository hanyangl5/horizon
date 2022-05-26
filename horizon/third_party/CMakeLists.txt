set(third_party_folder "ThirdParty")

add_subdirectory(VulkanMemoryAllocator)
add_subdirectory(D3D12MemoryAllocator)
add_subdirectory(SPIRV-Cross)
#add_subdirectory(DirectXShaderCompiler)
add_subdirectory(spdlog)
add_subdirectory(tinygltf)
add_subdirectory(glm)
add_subdirectory(GLFW)
set_target_properties(VulkanMemoryAllocator PROPERTIES FOLDER ${third_party_folder}/VulkanMemoryAllocator)
set_target_properties(D3D12MemoryAllocator PROPERTIES FOLDER ${third_party_folder}/D3D12MemoryAllocator)
set_target_properties(	
    spirv-cross
    spirv-cross-glsl
    spirv-cross-hlsl
    spirv-cross-c
    spirv-cross-cpp
    spirv-cross-reflect
    spirv-cross-msl
    spirv-cross-util
    spirv-cross-core PROPERTIES FOLDER ${third_party_folder}/SPIRV-Cross)

#set_target_properties(DirectXShaderCompiler PROPERTIES FOLDER ${third_party_folder}/DirectXShaderCompiler)
set_target_properties(spdlog PROPERTIES FOLDER ${third_party_folder}/spdlog)
set_target_properties(tinygltf PROPERTIES FOLDER ${third_party_folder}/tinygltf)
set_target_properties(glm PROPERTIES FOLDER ${third_party_folder}/glm)
set_target_properties(glfw PROPERTIES FOLDER ${third_party_folder}/GLFW3)