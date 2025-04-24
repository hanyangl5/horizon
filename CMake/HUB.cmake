file(GLOB_RECURSE HUB_SOURCE_FILES ${ENGINE_SOURCE_DIR}/Hub/*.cpp)
add_executable(HUB ${HUB_SOURCE_FILES})
set_target_properties(HUB PROPERTIES FOLDER "Horizon")
