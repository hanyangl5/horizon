project(HorizonMaterialEditor)

file(GLOB_RECURSE HEADER_FILES "*.h")
file(GLOB_RECURSE SOURCE_FILES "*.cpp")

#source_group(TREE "${ENGINE_ROOT_DIR}/schema" FILES ${SCHEMA_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADER_FILES} ${SOURCE_FILES})

add_executable(${PROJECT_NAME} ${HEADER_FILES} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} PUBLIC third_party)

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Horizon/MaterialEditor")

find_package(unofficial-nativefiledialog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::nativefiledialog::nfd)