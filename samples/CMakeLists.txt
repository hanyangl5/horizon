include(${CMAKE_SOURCE_DIR}/cmake/compiler_args.cmake)

macro(addproject projectdir)
    project(${projectdir})
    file(GLOB APP_HEADERS ${projectdir}/*.h)
    file(GLOB APP_SOURCES ${projectdir}/*.cpp)

    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${APP_HEADERS} ${APP_SOURCES})

    add_executable(${PROJECT_NAME} ${APP_HEADERS} ${APP_SOURCES})

    target_link_libraries(${PROJECT_NAME} PUBLIC runtime)
    target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/src)

    set(ASSET_DIR ${CMAKE_SOURCE_DIR}/assets)
    set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/${projectdir}/shaders)

    configure_file(${projectdir}/config.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/${projectdir}/config.hpp)
    set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "samples")
    configure_compile_options(${projectdir})
endmacro()

addproject(renderer_v1)
addproject(hbao)