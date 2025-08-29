option(CMAKE_INCLUDE_SHADERS "Enable inclusion of shaders in the project" ON)

if(CMAKE_INCLUDE_SHADERS)
    message(STATUS "Shaders will be included in the project.")
else()
    message(STATUS "Shaders will NOT be included in the project.")
endif()

set(ENGINE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Source)

set_property(GLOBAL PROPERTY USE_FOLDERS TRUE)

include(CMakeUtils)
include(ThirdParty)

if(NOT PROJECT_IS_TOP_LEVEL)
    include(Config)
endif()

if(${CMAKE_INCLUDE_SHADERS})
    include(Shaders)
endif()
include(Runtime)

add_definitions(-DPROJECT_ROOT="${PROJECT_ROOT}")

if(PROJECT_IS_TOP_LEVEL)
else()
    message("EXPROTING ENGINE INTERFACES")
    set(ENGINE_SOURCE_DIR ${ENGINE_SOURCE_DIR} PARENT_SCOPE)
    set(RUNTIME_INTERFACE_FILES ${RUNTIME_INTERFACE_FILES} PARENT_SCOPE)
    set(RUNTIME_INCLUDE_DIR ${RUNTIME_INCLUDE_DIR} PARENT_SCOPE)
    set(ENGINE_RUNTIME_SOURCE_DIR ${ENGINE_RUNTIME_SOURCE_DIR} PARENT_SCOPE)
    set(ENGINE_RUNTIME ${ENGINE_RUNTIME} PARENT_SCOPE)
endif()
