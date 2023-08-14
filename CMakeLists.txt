cmake_minimum_required(VERSION 3.18)

project(Horizon)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(PROJECT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/horizon)
set(SOLUTION_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options("/MP")
endif()

add_subdirectory(third_party)
add_subdirectory(src)
add_subdirectory(unit_tests)
add_subdirectory(samples)