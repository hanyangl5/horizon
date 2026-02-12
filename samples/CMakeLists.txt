include(${CMAKE_SOURCE_DIR}/cmake/compiler_args.cmake)
#find_package(Python3 REQUIRED)

macro(addproject projectdir)
    project(${projectdir})
    file(GLOB_RECURSE APP_HEADERS ${projectdir}/source/*.h)
    file(GLOB_RECURSE APP_SOURCES ${projectdir}/source/*.cpp)

    # Use PREFIX to flatten the folder structure in VS
    # This makes the path start from projectdir instead of samples/projectdir
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/${projectdir}" PREFIX "" FILES ${APP_HEADERS} ${APP_SOURCES})

        add_executable(${PROJECT_NAME} ${APP_HEADERS} ${APP_SOURCES})

    target_link_libraries(${PROJECT_NAME} PUBLIC framework)
    target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/framework)
    set(ASSET_DIR "${CMAKE_SOURCE_DIR}/assets")
    set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${projectdir}/shaders")
    set(SAVED_SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${projectdir}/saved/shaders")

    target_compile_definitions(${PROJECT_NAME} PRIVATE
        ASSET_DIR="${ASSET_DIR}"
        SHADER_DIR="${SHADER_DIR}"
        SAVED_SHADER_DIR="${SAVED_SHADER_DIR}"
    )
    set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "samples")
    configure_compile_options(${projectdir})

endmacro()

addproject(deferred)
addproject(hellotriangle)