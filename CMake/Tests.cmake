set(HORIZON_TEST_SOURCE_DIR ${ENGINE_DIR}/Tests)
set(HORIZON_TEST_CMAKE_DIR ${CMAKE_CURRENT_LIST_DIR}/Tests)

if(NOT TARGET gtest_main)
    set(HORIZON_GTEST_INCLUDE_DIR ${ENGINE_SOURCE_DIR}/ThirdParty/googletest/googletest/include)

    if(HORIZON_REBUILD_THIRDPARTY)
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

        add_subdirectory(
            ${ENGINE_SOURCE_DIR}/ThirdParty/googletest
            ${CMAKE_BINARY_DIR}/Source/ThirdParty/googletest
            EXCLUDE_FROM_ALL
        )

        if(TARGET HorizonPackageThirdParty)
            add_dependencies(HorizonPackageThirdParty gtest gtest_main)
            foreach(GTEST_PACKAGE_TARGET gtest gtest_main)
                add_custom_command(TARGET HorizonPackageThirdParty POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${HORIZON_THIRD_PARTY_PREBUILT_DIR}/$<CONFIG>/lib"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${GTEST_PACKAGE_TARGET}>" "${HORIZON_THIRD_PARTY_PREBUILT_DIR}/$<CONFIG>/lib/$<TARGET_FILE_NAME:${GTEST_PACKAGE_TARGET}>"
                    VERBATIM
                )
            endforeach()
        endif()
    else()
        horizon_import_static_third_party(gtest "gtest${CMAKE_STATIC_LIBRARY_SUFFIX}")
        target_include_directories(gtest SYSTEM INTERFACE ${HORIZON_GTEST_INCLUDE_DIR})
        add_library(GTest::gtest ALIAS gtest)

        horizon_import_static_third_party(gtest_main "gtest_main${CMAKE_STATIC_LIBRARY_SUFFIX}")
        target_link_libraries(gtest_main INTERFACE gtest)
        add_library(GTest::gtest_main ALIAS gtest_main)
    endif()
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

    if(WIN32)
        set(HORIZON_TEST_RUNTIME_DLLS
            $<TARGET_FILE:WinPixEventRuntime>
            $<TARGET_FILE:AGS>
            ${ENGINE_THIRD_PARTY_SOURCE_DIR}/DirectXShaderCompiler/bin/x64/dxcompiler.dll
            ${ENGINE_THIRD_PARTY_SOURCE_DIR}/DirectXShaderCompiler/bin/x64/dxil.dll
        )

        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${HORIZON_TEST_RUNTIME_DLLS}
                $<TARGET_FILE_DIR:${target_name}>
            COMMAND_EXPAND_LISTS
        )
    endif()

    source_group(TREE ${HORIZON_TEST_SOURCE_DIR} PREFIX "Tests" FILES ${ARGN})
    set_target_properties(${target_name} PROPERTIES FOLDER "Horizon/Tests")

    gtest_discover_tests(${target_name})
endfunction()

include(${HORIZON_TEST_CMAKE_DIR}/Core.cmake)
include(${HORIZON_TEST_CMAKE_DIR}/RHI.cmake)
include(${HORIZON_TEST_CMAKE_DIR}/Smoke.cmake)
