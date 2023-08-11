project(runtime)

file(GLOB_RECURSE HEADER_FILES "*.h")
file(GLOB_RECURSE SOURCE_FILES "*.cpp")

#source_group(TREE "${ENGINE_ROOT_DIR}/schema" FILES ${SCHEMA_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADER_FILES} ${SOURCE_FILES})

add_library(${PROJECT_NAME} STATIC  ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(${PROJECT_NAME} PUBLIC third_party)

