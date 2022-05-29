if(MSVC)
 add_compile_options("/MP")
endif()

add_subdirectory(runtime)
add_subdirectory(app)
add_subdirectory(third_party)
