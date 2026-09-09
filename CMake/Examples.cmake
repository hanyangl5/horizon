set(HORIZON_EXAMPLES_SOURCE_DIR ${ENGINE_DIR}/Examples)

if(WIN32)
    set(HORIZON_EXAMPLE_RUNTIME_DLLS
        $<TARGET_FILE:WinPixEventRuntime>
        $<TARGET_FILE:AGS>
        ${HORIZON_DIRECTSTORAGE_RUNTIME_DLLS}
        ${ENGINE_THIRD_PARTY_SOURCE_DIR}/DirectXShaderCompiler/bin/x64/dxcompiler.dll
        ${ENGINE_THIRD_PARTY_SOURCE_DIR}/DirectXShaderCompiler/bin/x64/dxil.dll
    )
endif()

function(add_example_target target_name)
    set(example_source_dir ${HORIZON_EXAMPLES_SOURCE_DIR}/${target_name})
    file(GLOB_RECURSE example_sources CONFIGURE_DEPENDS ${example_source_dir}/*)
    add_executable(${target_name} ${example_sources})
    target_link_libraries(${target_name} PRIVATE ${ENGINE_RUNTIME})
    target_compile_features(${target_name} PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /MP)
    endif()

    if(WIN32)
        target_link_libraries(${target_name} PRIVATE winmm)

        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:${target_name}>/D3D12
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${HORIZON_AGILITYSDK_RUNTIME_DLLS}
                $<TARGET_FILE_DIR:${target_name}>/D3D12
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${HORIZON_EXAMPLE_RUNTIME_DLLS}
                $<TARGET_FILE_DIR:${target_name}>
            COMMAND_EXPAND_LISTS
        )
    endif()

    set(example_code_sources ${example_sources})
    list(FILTER example_code_sources INCLUDE REGEX "\\.(c|cc|cpp|cxx|h|hh|hpp|hxx|inl)$")
    set(example_tree_sources ${example_sources})
    list(REMOVE_ITEM example_tree_sources ${example_code_sources})
    source_group(TREE ${example_source_dir} FILES ${example_tree_sources})
    source_group("Source Files" FILES ${example_code_sources})
    set(example_shader_sources ${example_sources})
    list(FILTER example_shader_sources INCLUDE REGEX "\\.(hlsl|hlsli)$")
    if(example_shader_sources)
        set_source_files_properties(${example_shader_sources} PROPERTIES HEADER_FILE_ONLY TRUE)
    endif()
    set_target_properties(${target_name} PROPERTIES FOLDER "Horizon/Examples")
endfunction()

add_example_target(HelloTriangle)
add_example_target(DeferredShading)
add_example_target(Renderer)
if(TARGET AssetPipeline)
    target_link_libraries(Renderer PRIVATE AssetPipeline)
    target_compile_definitions(Renderer PRIVATE HORIZON_RENDERER_ASSET_COOKING)
endif()
