set(flecs_source_dir ${ENGINE_THIRD_PARTY_SOURCE_DIR}/flecs)
file(GLOB flecs_sources CONFIGURE_DEPENDS
    ${flecs_source_dir}/src/*.c
    ${flecs_source_dir}/src/datastructures/*.c
    ${flecs_source_dir}/src/storage/*.c
    ${flecs_source_dir}/src/addons/os_api_impl/*.c)
list(APPEND flecs_sources ${flecs_source_dir}/src/addons/log.c)
add_library(HorizonFlecs STATIC ${flecs_sources})
target_include_directories(HorizonFlecs PUBLIC ${flecs_source_dir}/include)
target_compile_definitions(HorizonFlecs PUBLIC flecs_STATIC FLECS_CUSTOM_BUILD FLECS_OS_API_IMPL)
target_compile_features(HorizonFlecs PRIVATE c_std_99)
if(WIN32)
    target_link_libraries(HorizonFlecs PRIVATE ws2_32)
endif()
set_target_properties(HorizonFlecs PROPERTIES FOLDER "Horizon/ThirdParty")
source_group(TREE ${flecs_source_dir}/src PREFIX "Source Files" FILES ${flecs_sources})
