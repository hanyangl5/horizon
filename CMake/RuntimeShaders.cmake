set(runtime_shader_dir ${ENGINE_SOURCE_DIR}/Shaders)
file(GLOB_RECURSE runtime_shaders CONFIGURE_DEPENDS
    ${runtime_shader_dir}/UI/*.vert.hlsl ${runtime_shader_dir}/UI/*.frag.hlsl
    ${runtime_shader_dir}/Fonts/*.vert.hlsl ${runtime_shader_dir}/Fonts/*.frag.hlsl
    ${runtime_shader_dir}/AnimationSystem/*.vert.hlsl ${runtime_shader_dir}/AnimationSystem/*.frag.hlsl)
file(GLOB_RECURSE runtime_shader_headers CONFIGURE_DEPENDS
    ${runtime_shader_dir}/UI/*.h.hlsl ${runtime_shader_dir}/Fonts/*.h.hlsl ${runtime_shader_dir}/AnimationSystem/*.h.hlsl)
set(runtime_shader_outputs)
foreach(shader IN LISTS runtime_shaders)
    get_filename_component(name ${shader} NAME_WLE)
    if(name MATCHES "\\.vert$")
        set(profile vs_6_6)
        set(entry VS_MAIN)
    else()
        set(profile ps_6_6)
        set(entry PS_MAIN)
    endif()
    set(samples 1)
    if(name STREQUAL "imgui.frag")
        set(samples 1 2 4 8 16)
    endif()
    foreach(sample IN LISTS samples)
        set(output_name ${name})
        if(name STREQUAL "imgui.frag")
            set(output_name imgui_SAMPLE_COUNT_${sample}.frag)
        endif()
        set(output_dir ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/CompiledShaders/DIRECT3D12)
        set(output ${output_dir}/${output_name})
        add_custom_command(OUTPUT ${output}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${output_dir}
            COMMAND ${ENGINE_THIRD_PARTY_SOURCE_DIR}/DirectXShaderCompiler/bin/x64/dxc.exe
                -T ${profile} -E ${entry} -D SAMPLE_COUNT=${sample} -Fo ${output} ${shader}
            DEPENDS ${shader} ${runtime_shader_headers}
                ${ENGINE_THIRD_PARTY_SOURCE_DIR}/DirectXShaderCompiler/bin/x64/dxc.exe
            VERBATIM)
        list(APPEND runtime_shader_outputs ${output})
    endforeach()
endforeach()
add_custom_target(RuntimeShaders DEPENDS ${runtime_shader_outputs})
set_target_properties(RuntimeShaders PROPERTIES FOLDER "Horizon")
add_dependencies(${ENGINE_RUNTIME} RuntimeShaders)
