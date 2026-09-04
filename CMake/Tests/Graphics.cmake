set(HORIZON_GRAPHICS_TEST_SOURCES
    ${HORIZON_TEST_SOURCE_DIR}/Graphics/RenderContextTests.cpp
)

add_test_target(GraphicsTests D3D12_RESOURCE_LOCK ${HORIZON_GRAPHICS_TEST_SOURCES})

target_include_directories(GraphicsTests PRIVATE
    ${ENGINE_RUNTIME_SOURCE_DIR}/Graphics/Private
)
