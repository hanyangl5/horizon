set(HORIZON_RESOURCES_TEST_SOURCES
    ${HORIZON_TEST_SOURCE_DIR}/Resources/TextureContainersTests.cpp
)

add_test_target(ResourcesTests ${HORIZON_RESOURCES_TEST_SOURCES})
target_include_directories(ResourcesTests PRIVATE ${ENGINE_RUNTIME_SOURCE_DIR}/Resources/Private/ResourceLoader)
