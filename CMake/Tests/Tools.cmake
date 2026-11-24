if(TARGET AssetPipelineCmd)
    # The cooker regressions drive AssetPipelineCmd as a subprocess; they need only the CPU.
    add_test(NAME SceneAssetCookerTests
        COMMAND "${Python3_EXECUTABLE}" -I ${HORIZON_TEST_SOURCE_DIR}/Tools/SceneAssetCookerTests.py $<TARGET_FILE:AssetPipelineCmd>
    )
endif()
