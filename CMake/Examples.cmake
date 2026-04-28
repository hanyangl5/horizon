set(HORIZON_EXAMPLES_SOURCE_DIR ${ENGINE_DIR}/Examples)

set(HORIZON_HELLO_TRIANGLE_SOURCES
    ${HORIZON_EXAMPLES_SOURCE_DIR}/HelloTriangle/HelloTriangle.cpp
)

add_executable(HelloTriangle ${HORIZON_HELLO_TRIANGLE_SOURCES})
target_link_libraries(HelloTriangle PRIVATE ${ENGINE_RUNTIME})
target_compile_features(HelloTriangle PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(HelloTriangle PRIVATE /MP)
endif()

if(WIN32)
    target_link_libraries(HelloTriangle PRIVATE winmm)

    set(HORIZON_EXAMPLE_RUNTIME_DLLS
        $<TARGET_FILE:WinPixEventRuntime>
        $<TARGET_FILE:AGS>
        ${HORIZON_DIRECTSTORAGE_RUNTIME_DLLS}
        ${ENGINE_THIRD_PARTY_SOURCE_DIR}/DirectXShaderCompiler/bin/x64/dxcompiler.dll
        ${ENGINE_THIRD_PARTY_SOURCE_DIR}/DirectXShaderCompiler/bin/x64/dxil.dll
    )

    add_custom_command(TARGET HelloTriangle POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:HelloTriangle>/D3D12
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${HORIZON_AGILITYSDK_RUNTIME_DLLS}
            $<TARGET_FILE_DIR:HelloTriangle>/D3D12
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${HORIZON_EXAMPLE_RUNTIME_DLLS}
            $<TARGET_FILE_DIR:HelloTriangle>
        COMMAND_EXPAND_LISTS
    )
endif()

source_group(TREE ${HORIZON_EXAMPLES_SOURCE_DIR} PREFIX "Examples" FILES ${HORIZON_HELLO_TRIANGLE_SOURCES})
set_target_properties(HelloTriangle PROPERTIES FOLDER "Horizon/Examples")
