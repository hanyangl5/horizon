find_package(Doxygen QUIET)

if(NOT DOXYGEN_FOUND)
    message(STATUS "Doxygen not found. The Docs target will not be created.")
    return()
endif()

set(HORIZON_DOXYGEN_INPUT_DIRS
    "${ENGINE_DIR}/Docs"
    "${ENGINE_RUNTIME_SOURCE_DIR}/Application/Public"
    "${ENGINE_RUNTIME_SOURCE_DIR}/Core/Public"
    "${ENGINE_RUNTIME_SOURCE_DIR}/Graphics/Public"
    "${ENGINE_RUNTIME_SOURCE_DIR}/Platform/Public"
    "${ENGINE_RUNTIME_SOURCE_DIR}/Profiler/Public"
    "${ENGINE_RUNTIME_SOURCE_DIR}/Resources/Public"
    "${ENGINE_RUNTIME_SOURCE_DIR}/RHI/Public"
    "${ENGINE_RUNTIME_SOURCE_DIR}/Scripting/Public"
)

set(HORIZON_DOXYGEN_QUOTED_INPUT_DIRS ${HORIZON_DOXYGEN_INPUT_DIRS})
list(TRANSFORM HORIZON_DOXYGEN_QUOTED_INPUT_DIRS PREPEND "\"")
list(TRANSFORM HORIZON_DOXYGEN_QUOTED_INPUT_DIRS APPEND "\"")
string(JOIN " \\\n    " HORIZON_DOXYGEN_INPUTS ${HORIZON_DOXYGEN_QUOTED_INPUT_DIRS})

set(HORIZON_DOXYGEN_OUTPUT_DIR "${PROJECT_BINARY_DIR}/docs/doxygen")
set(HORIZON_DOXYGEN_WORKING_DIR "${ENGINE_DIR}")
set(HORIZON_DOXYGEN_MAINPAGE "${ENGINE_DIR}/Docs/MainPage.md")

configure_file(
    "${ENGINE_DIR}/Docs/Doxyfile.in"
    "${PROJECT_BINARY_DIR}/Doxyfile"
    @ONLY
)

add_custom_target(Docs
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${HORIZON_DOXYGEN_OUTPUT_DIR}"
    COMMAND "${DOXYGEN_EXECUTABLE}" "${PROJECT_BINARY_DIR}/Doxyfile"
    WORKING_DIRECTORY "${HORIZON_DOXYGEN_WORKING_DIR}"
    COMMENT "Generating Doxygen documentation"
    VERBATIM
)

set_target_properties(Docs PROPERTIES FOLDER "Horizon")
