option(CMAKE_INCLUDE_SHADERS "Enable inclusion of shaders in the project" OFF)
option(HORIZON_BUILD_TESTS "Build Horizon tests" ${PROJECT_IS_TOP_LEVEL})
option(HORIZON_BUILD_DOCS "Configure the Doxygen documentation target" ${PROJECT_IS_TOP_LEVEL})
option(HORIZON_BUILD_EXAMPLES "Build Horizon example applications" ${PROJECT_IS_TOP_LEVEL})
option(HORIZON_ENABLE_TRACY "Enable Tracy profiler integration" ON)

if(CMAKE_INCLUDE_SHADERS)
    message(STATUS "Shaders will be included in the project.")
else()
    message(STATUS "Shaders will NOT be included in the project.")
endif()

set(ENGINE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source)

set_property(GLOBAL PROPERTY USE_FOLDERS TRUE)

include(CMakeUtils)
include(ThirdParty)
add_compile_definitions(PROJECT_ROOT="${PROJECT_ROOT}")

if(${CMAKE_INCLUDE_SHADERS})
    include(Shaders)
endif()
include(Runtime)

if(HORIZON_BUILD_EXAMPLES)
    include(Examples)
endif()

if(HORIZON_BUILD_DOCS)
    include(Docs)
endif()

if(HORIZON_BUILD_TESTS)
    include(Tests)
endif()

if(PROJECT_IS_TOP_LEVEL)
else()
    message("EXPROTING ENGINE INTERFACES")
    set(ENGINE_SOURCE_DIR ${ENGINE_SOURCE_DIR} PARENT_SCOPE)
    set(RUNTIME_INTERFACE_FILES ${RUNTIME_INTERFACE_FILES} PARENT_SCOPE)
    set(RUNTIME_INCLUDE_DIR ${RUNTIME_INCLUDE_DIR} PARENT_SCOPE)
    set(ENGINE_RUNTIME_SOURCE_DIR ${ENGINE_RUNTIME_SOURCE_DIR} PARENT_SCOPE)
    set(ENGINE_RUNTIME ${ENGINE_RUNTIME} PARENT_SCOPE)
endif()
