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

function(horizon_add_example TARGET_NAME)
    add_executable(${TARGET_NAME} ${ARGN})
    target_link_libraries(${TARGET_NAME} PRIVATE ${ENGINE_RUNTIME})
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(${TARGET_NAME} PRIVATE /MP)
    endif()

    if(WIN32)
        target_link_libraries(${TARGET_NAME} PRIVATE winmm)

        add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:${TARGET_NAME}>/D3D12
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${HORIZON_AGILITYSDK_RUNTIME_DLLS}
                $<TARGET_FILE_DIR:${TARGET_NAME}>/D3D12
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${HORIZON_EXAMPLE_RUNTIME_DLLS}
                $<TARGET_FILE_DIR:${TARGET_NAME}>
            COMMAND_EXPAND_LISTS
        )
    endif()

    source_group(TREE ${HORIZON_EXAMPLES_SOURCE_DIR} PREFIX "Examples" FILES ${ARGN})
    set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "Horizon/Examples")
endfunction()

set(HORIZON_HELLO_TRIANGLE_SOURCES
    ${HORIZON_EXAMPLES_SOURCE_DIR}/HelloTriangle/HelloTriangle.cpp
)

horizon_add_example(HelloTriangle ${HORIZON_HELLO_TRIANGLE_SOURCES})

set(HORIZON_GAUSSIAN_SPLATTING_SOURCES
    ${HORIZON_EXAMPLES_SOURCE_DIR}/GaussianSplatting/GaussianSplatting.cpp
)

horizon_add_example(GaussianSplatting ${HORIZON_GAUSSIAN_SPLATTING_SOURCES})
