set(HORIZON_RESOURCES_TEST_SOURCES
    ${HORIZON_TEST_SOURCE_DIR}/Resources/TextureContainersTests.cpp
)

add_test_target(ResourcesTests ${HORIZON_RESOURCES_TEST_SOURCES})
target_include_directories(ResourcesTests PRIVATE ${ENGINE_RUNTIME_SOURCE_DIR}/Resources/Private/ResourceLoader)

add_test_target(AssetPipelineContractTests
    ${HORIZON_TEST_SOURCE_DIR}/Resources/AssetPipelineContractTests.cpp
)

if(TARGET AssetPipelineCmd)
    set(HORIZON_ASSET_PIPELINE_FIXTURE_DIR ${CMAKE_BINARY_DIR}/AssetPipelineFixture)

    add_test(NAME AssetPipelineFixtureCook
        COMMAND $<TARGET_FILE:AssetPipelineCmd>
            -pgltf
            --input-file Tests/Resources/Fixtures/triangle.gltf
            --output ${HORIZON_ASSET_PIPELINE_FIXTURE_DIR}
            --force
            --meshlets
            --optimize
    )
    set_tests_properties(AssetPipelineFixtureCook PROPERTIES
        WORKING_DIRECTORY ${ENGINE_DIR}
        FIXTURES_SETUP HorizonCookedScene
    )

    add_test(NAME AssetPipelineFixtureContract
        COMMAND $<TARGET_FILE:AssetPipelineContractTests>
            --gtest_filter=SceneAssetCookerContractTest.*
    )
    set_tests_properties(AssetPipelineFixtureContract PROPERTIES
        ENVIRONMENT "HORIZON_COOKED_SCENE_ROOT=${HORIZON_ASSET_PIPELINE_FIXTURE_DIR}"
        FIXTURES_REQUIRED HorizonCookedScene
    )

    add_test(NAME AssetPipelineIncrementalCook
        COMMAND ${CMAKE_COMMAND}
            -DASSET_PIPELINE=$<TARGET_FILE:AssetPipelineCmd>
            -DSOURCE_FILE=${ENGINE_DIR}/Tests/Resources/Fixtures/triangle.gltf
            -DOUTPUT_DIR=${CMAKE_BINARY_DIR}/AssetPipelineIncrementalFixture
            -P ${ENGINE_DIR}/Tests/Resources/VerifyIncrementalCook.cmake
    )
endif()
