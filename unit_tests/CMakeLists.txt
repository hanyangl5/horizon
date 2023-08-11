project(unit_tests)

file(GLOB APP_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
file(GLOB APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(${PROJECT_NAME} rhi_test.cpp)

target_link_libraries(${PROJECT_NAME} PUBLIC runtime)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/src)

