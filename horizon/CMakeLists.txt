if(MSVC)
 add_compile_options("/MP")
endif()

# # https://github.com/izenecloud/cmake/blob/master/SetCompilerWarningAll.cmake
# if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
#   # Use the highest warning level for Visual Studio.
#   set(CMAKE_CXX_WARNING_LEVEL 4)
#   if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
#     string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
#   else(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
#   endif(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
#   # Disable C++ exceptions.
#   string(REGEX REPLACE "/EH[a-z]+" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
#   # Disable RTTI.
#   string(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")
# else(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
#   # Use -Wall for clang and gcc.
#   if(NOT CMAKE_CXX_FLAGS MATCHES "-Wall")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
#   endif(NOT CMAKE_CXX_FLAGS MATCHES "-Wall")
#   # Use -Wextra for clang and gcc.
#   if(NOT CMAKE_CXX_FLAGS MATCHES "-Wextra")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
#   endif(NOT CMAKE_CXX_FLAGS MATCHES "-Wextra")
#   # Use -Werror for clang and gcc.
#   if(NOT CMAKE_CXX_FLAGS MATCHES "-Werror")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
#   endif(NOT CMAKE_CXX_FLAGS MATCHES "-Werror")
#   # Disable C++ exceptions.
#   string(REGEX REPLACE "-fexceptions" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
#   # Disable RTTI.
#   string(REGEX REPLACE "-frtti" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
# endif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "")

add_subdirectory(3rd_party)
add_subdirectory(runtime)
add_subdirectory(app)
add_subdirectory(unit_tests)
add_subdirectory(tools)
add_subdirectory(samples)