configure_file(
    ${CMAKE_MODULE_PATH}/Config/ProjectConfig.ini.in
    ${PROJECT_PATH}/Config/Config.ini
)

file(COPY ${CMAKE_MODULE_PATH}/Config/gpu.cfg 
    DESTINATION ${PROJECT_PATH}/Config/)

configure_file(
    ${CMAKE_MODULE_PATH}/Config/ProjectConfig.ini.in
    ${PROJECT_PATH}/Config/Config.ini
)

configure_file(
    ${CMAKE_MODULE_PATH}/Config/Path.h.in
    ${PROJECT_PATH}/Config/Path.h
)


