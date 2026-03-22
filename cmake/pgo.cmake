option(HORIZON_PGO_GENERATE "Instrument build for PGO profiling" OFF)
option(HORIZON_PGO_USE "Optimized build using PGO profile" OFF)
set(HORIZON_PGO_PROFILE_DIR "${CMAKE_SOURCE_DIR}/pgo_profiles"
    CACHE PATH "Directory containing merged profile data")

function(horizon_copy_pgo_runtime target_name)
    if(NOT MSVC OR NOT HORIZON_PGO_GENERATE)
        return()
    endif()

    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "horizon_copy_pgo_runtime called with unknown target '${target_name}'")
    endif()

    get_filename_component(_horizon_msvc_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(_horizon_pgort_dll "${_horizon_msvc_bin_dir}/pgort140.dll")

    if(EXISTS "${_horizon_pgort_dll}")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_horizon_pgort_dll}"
                "$<TARGET_FILE_DIR:${target_name}>/pgort140.dll"
            COMMENT "Copying pgort140.dll to output directory"
        )
    else()
        message(WARNING
            "MSVC PGO runtime not found: ${_horizon_pgort_dll}. "
            "Profile binaries may fail to start until pgort140.dll is available.")
    endif()
endfunction()

if(HORIZON_PGO_GENERATE AND HORIZON_PGO_USE)
    message(FATAL_ERROR "HORIZON_PGO_GENERATE and HORIZON_PGO_USE are mutually exclusive.")
endif()

if(HORIZON_PGO_GENERATE OR HORIZON_PGO_USE)
    file(MAKE_DIRECTORY "${HORIZON_PGO_PROFILE_DIR}")

    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/GL)
        add_link_options(/LTCG)
        add_link_options(/INCREMENTAL:NO)

        if(HORIZON_PGO_GENERATE)
            add_compile_definitions(HORIZON_PGO_GENERATE=1)
            # Let MSVC emit a per-target .pgd next to each linked binary.
            # Sharing one PGD across parallel links corrupts the profile database.
            add_link_options(/GENPROFILE)
        else()
            add_link_options(/USEPROFILE)
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if(HORIZON_PGO_GENERATE)
            add_compile_definitions(HORIZON_PGO_GENERATE=1)
            add_compile_options(-fprofile-instr-generate)
            add_link_options(-fprofile-instr-generate)
        else()
            set(_horizon_profdata "${HORIZON_PGO_PROFILE_DIR}/merged.profdata")
            if(NOT EXISTS "${_horizon_profdata}")
                message(FATAL_ERROR
                    "PGO profile not found: ${_horizon_profdata}\n"
                    "Run: tools/pgo_merge.sh ${HORIZON_PGO_PROFILE_DIR}")
            endif()

            add_compile_options("-fprofile-instr-use=${_horizon_profdata}")
            if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
                add_compile_options(-fprofile-correction)
            endif()
            add_link_options("-fprofile-instr-use=${_horizon_profdata}")
        endif()
    else()
        message(FATAL_ERROR
            "PGO is enabled, but compiler '${CMAKE_CXX_COMPILER_ID}' is not supported.")
    endif()
endif()
