file(GLOB_RECURSE HUB_SOURCE_FILES ${ENGINE_SOURCE_DIR}/Hub/*.cpp)
file(GLOB_RECURSE HUB_HEADER_FILES ${ENGINE_SOURCE_DIR}/Hub/*.h)
add_executable(Hub ${HUB_SOURCE_FILES} ${HUB_HEADER_FILES})
set_target_properties(Hub PROPERTIES FOLDER "Horizon")
source_group(TREE ${ENGINE_SOURCE_DIR}/Hub PREFIX "Sources" FILES ${HUB_SOURCE_FILES} ${HUB_HEADER_FILES})

# win32 app
target_compile_definitions(Hub PRIVATE GUI_MODE)
set_target_properties(Hub PROPERTIES WIN32_EXECUTABLE TRUE)