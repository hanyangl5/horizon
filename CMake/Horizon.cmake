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

if(${CMAKE_INCLUDE_SHADERS})
    include(Shaders)
endif()
include(Runtime)
