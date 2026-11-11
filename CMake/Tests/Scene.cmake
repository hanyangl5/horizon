add_test_target(SceneTests D3D12_RESOURCE_LOCK
    ${HORIZON_TEST_SOURCE_DIR}/Scene/AssetManagerTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/Scene/SceneAssetTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/Scene/SceneIDTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/Scene/SceneReflectionTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/Scene/WorldTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/Scene/WorldArchiveTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/Scene/WorldTransformTests.cpp
)

target_sources(SceneTests PRIVATE
    ${ENGINE_DIR}/Examples/Renderer/Source/GpuDrawList.cpp
    ${ENGINE_DIR}/Examples/Renderer/Source/RayTracingScene.cpp
)
