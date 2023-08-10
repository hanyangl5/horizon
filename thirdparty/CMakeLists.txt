add_subdirectory(glfw)
add_subdirectory(spdlog)
add_subdirectory(tinygltf_impl)
add_subdirectory(glm)

set_property(TARGET tinygltf_lib spdlog glfw PROPERTY FOLDER "ThirdParty")