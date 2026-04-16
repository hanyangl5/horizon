set(SMOKE_TEST_FILES
    ${HORIZON_TEST_SOURCE_DIR}/Smoke/SmokeTests.cpp
)

add_test_target(SmokeTests ${SMOKE_TEST_FILES})
