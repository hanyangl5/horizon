project(SpirvTools)

add_library(${PROJECT_NAME} SpirvTools.h SpirvTools.cpp)

target_link_libraries(${PROJECT_NAME} PUBLIC third_party)

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Horizon/Tools")