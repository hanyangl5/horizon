option(HORIZON_ENABLE_LTO "Enable link-time optimization" OFF)
set(HORIZON_LTO_MODE "AUTO" CACHE STRING "LTO mode: AUTO, THIN, FULL")
set_property(CACHE HORIZON_LTO_MODE PROPERTY STRINGS AUTO THIN FULL)

if(NOT HORIZON_ENABLE_LTO)
    return()
endif()

if(NOT HORIZON_LTO_MODE MATCHES "^(AUTO|THIN|FULL)$")
    message(FATAL_ERROR
        "Invalid HORIZON_LTO_MODE='${HORIZON_LTO_MODE}'. "
        "Expected one of: AUTO, THIN, FULL.")
endif()

if(HORIZON_PGO_GENERATE OR HORIZON_PGO_USE)
    message(STATUS "LTO enabled together with PGO; keeping PGO behavior unchanged and adding LTO flags.")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(HORIZON_LTO_MODE STREQUAL "FULL")
        set(_horizon_lto_flag "-flto")
        set(_horizon_lto_mode_effective "FULL")
    else()
        set(_horizon_lto_flag "-flto=thin")
        set(_horizon_lto_mode_effective "THIN")
    endif()

    add_compile_options("${_horizon_lto_flag}")
    add_link_options("${_horizon_lto_flag}")
    message(STATUS "LTO enabled (${_horizon_lto_mode_effective}) for Clang.")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(_horizon_lto_mode_effective "${HORIZON_LTO_MODE}")
    if(HORIZON_LTO_MODE STREQUAL "THIN")
        set(_horizon_lto_mode_effective "FULL")
        message(WARNING "HORIZON_LTO_MODE=THIN is not supported on MSVC; falling back to FULL.")
    endif()

    add_compile_options(/GL)
    add_link_options(/LTCG)
    message(STATUS "LTO enabled (${_horizon_lto_mode_effective}) for MSVC via /GL + /LTCG.")
else()
    message(WARNING
        "HORIZON_ENABLE_LTO=ON, but compiler '${CMAKE_CXX_COMPILER_ID}' is not supported for LTO in Horizon."
    )
endif()
