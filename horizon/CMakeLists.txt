if(MSVC)
 add_compile_options("/MP")
endif()

set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "")

add_subdirectory(3rd_party)
add_subdirectory(runtime)
add_subdirectory(app)
add_subdirectory(unit_tests)

