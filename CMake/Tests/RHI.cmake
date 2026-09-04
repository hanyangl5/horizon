set(RHI_TEST_FILES
    ${HORIZON_TEST_SOURCE_DIR}/RHI/GraphicsConfigTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/RHI/RingBufferTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/RHI/ShaderReflectionTests.cpp
)

if(WIN32)
    list(APPEND RHI_TEST_FILES
        ${HORIZON_TEST_SOURCE_DIR}/RHI/IGraphicsApiTests.cpp
    )
endif()

add_test_target(RHITests D3D12_RESOURCE_LOCK ${RHI_TEST_FILES})

target_include_directories(RHITests PRIVATE
    ${ENGINE_RUNTIME_SOURCE_DIR}/Graphics/Private
)
