cmake_minimum_required(VERSION 3.18)

project(Horizon)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(thirdparty)

add_subdirectory(config)

add_subdirectory(src)

add_subdirectory(example)