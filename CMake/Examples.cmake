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
    add_executable(${target_name} ${ARGN})
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

    source_group(TREE ${HORIZON_EXAMPLES_SOURCE_DIR} PREFIX "Examples" FILES ${ARGN})
    set_target_properties(${target_name} PROPERTIES FOLDER "Horizon/Examples")
endfunction()

set(HORIZON_HELLO_TRIANGLE_SOURCES
    ${HORIZON_EXAMPLES_SOURCE_DIR}/HelloTriangle/HelloTriangle.cpp
)

set(HORIZON_DEFERRED_SHADING_SOURCES
    ${HORIZON_EXAMPLES_SOURCE_DIR}/DeferredShading/DeferredShading.cpp
)

set(HORIZON_RENDERER_SOURCES
    ${HORIZON_EXAMPLES_SOURCE_DIR}/Renderer/Renderer.cpp
)

add_example_target(HelloTriangle ${HORIZON_HELLO_TRIANGLE_SOURCES})
add_example_target(DeferredShading ${HORIZON_DEFERRED_SHADING_SOURCES})
add_example_target(Renderer ${HORIZON_RENDERER_SOURCES})
if(TARGET AssetPipeline)
    target_link_libraries(Renderer PRIVATE AssetPipeline)
    target_compile_definitions(Renderer PRIVATE HORIZON_RENDERER_ASSET_COOKING)
endif()

target_compile_definitions(Renderer PRIVATE
    HORIZON_RENDERER_ASSET_DIR="${CMAKE_BINARY_DIR}/RendererAssets"
)
