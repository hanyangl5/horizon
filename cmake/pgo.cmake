option(HORIZON_PGO_GENERATE "Instrument build for PGO profiling" OFF)
option(HORIZON_PGO_USE "Optimized build using PGO profile" OFF)
set(HORIZON_PGO_PROFILE_DIR "${CMAKE_SOURCE_DIR}/pgo_profiles"
    CACHE PATH "Directory containing merged profile data")

if(HORIZON_PGO_GENERATE AND HORIZON_PGO_USE)
    message(FATAL_ERROR "HORIZON_PGO_GENERATE and HORIZON_PGO_USE are mutually exclusive.")
endif()

if(HORIZON_PGO_GENERATE OR HORIZON_PGO_USE)
    file(MAKE_DIRECTORY "${HORIZON_PGO_PROFILE_DIR}")

    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/GL)
        add_link_options(/LTCG)

        if(HORIZON_PGO_GENERATE)
            add_compile_definitions(HORIZON_PGO_GENERATE=1)
            add_link_options("/GENPROFILE:PGD=${HORIZON_PGO_PROFILE_DIR}/horizon.pgd")
        else()
            add_link_options("/USEPROFILE:PGD=${HORIZON_PGO_PROFILE_DIR}/horizon.pgd")
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
