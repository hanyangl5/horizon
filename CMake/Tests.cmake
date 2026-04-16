set(HORIZON_TEST_SOURCE_DIR ${ENGINE_DIR}/Tests)
set(HORIZON_TEST_CMAKE_DIR ${CMAKE_CURRENT_LIST_DIR}/Tests)

if(NOT TARGET gtest_main)
    set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    add_subdirectory(
        ${ENGINE_SOURCE_DIR}/ThirdParty/googletest
        ${CMAKE_BINARY_DIR}/Source/ThirdParty/googletest
        EXCLUDE_FROM_ALL
    )
endif()

enable_testing()
include(GoogleTest)

foreach(GTEST_TARGET gtest gtest_main)
    if(TARGET ${GTEST_TARGET})
        set_target_properties(${GTEST_TARGET} PROPERTIES FOLDER "Horizon/ThirdParty")
    endif()
endforeach()

function(add_test_target target_name)
    add_executable(${target_name} ${ARGN})

    target_link_libraries(${target_name} PRIVATE ${ENGINE_RUNTIME} GTest::gtest_main)
    target_compile_features(${target_name} PRIVATE cxx_std_20)

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /MP)
    endif()

    if(WIN32)
        target_link_libraries(${target_name} PRIVATE winmm)
    endif()

    source_group(TREE ${HORIZON_TEST_SOURCE_DIR} PREFIX "Tests" FILES ${ARGN})
    set_target_properties(${target_name} PROPERTIES FOLDER "Horizon/Tests")

    gtest_discover_tests(${target_name})
endfunction()

include(${HORIZON_TEST_CMAKE_DIR}/Core.cmake)
include(${HORIZON_TEST_CMAKE_DIR}/Smoke.cmake)
