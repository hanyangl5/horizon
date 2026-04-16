set(CORE_TEST_FILES
    ${HORIZON_TEST_SOURCE_DIR}/Core/AlgorithmsTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/Core/FileSystemTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/Core/LogTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/Core/MathTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/Core/MemoryTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/Core/RandomTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/Core/ThreadTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/Core/TimeTests.cpp
    ${HORIZON_TEST_SOURCE_DIR}/Core/ToolFileSystemTests.cpp
)

add_test_target(CoreTests ${CORE_TEST_FILES})
