project(third_party)

set(third_party_folder "third_party")

add_library(${PROJECT_NAME} 3rd_party.cpp)

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Horizon")



# inih

# find_path(INIH_INCLUDE_DIRS "ini.h")
# get_filename_component(INIH_LIB_DIR ${INIH_INCLUDE_DIRS} DIRECTORY)
# set(INIH_LIB "${INIH_LIB_DIR}/lib/inih.lib")

# target_link_libraries(${PROJECT_NAME} PUBLIC ${INIH_LIB})

# directx 12

#target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/dx12)

# ini parser

find_package(unofficial-iniparser CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC unofficial::iniparser::iniparser)

# find vulkan
find_package(Vulkan REQUIRED)
target_include_directories(${PROJECT_NAME} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${Vulkan_LIBRARIES})

# glfw
find_package(glfw3 REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC glfw)


# directxtk
find_package(directxtk12 REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Microsoft::DirectXTK12)


# directx shader compiler
find_package(directx-dxc REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Microsoft::DirectXShaderCompiler)

# spdlog
find_package(spdlog REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog::spdlog spdlog::spdlog_header_only)

# vma
find_package(unofficial-vulkan-memory-allocator REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC unofficial::vulkan-memory-allocator::vulkan-memory-allocator)

# d3dma
find_package(d3d12-memory-allocator REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC unofficial::d3d12-memory-allocator)

# doctest
find_package(doctest CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC doctest::doctest)

#argparse
find_package(argparse CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC argparse::argparse)

find_path(STB_INCLUDE_DIRS "stb_c_lexer.h")
target_include_directories(${PROJECT_NAME} PUBLIC ${STB_INCLUDE_DIRS})

find_package(assimp CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE assimp::assimp)

find_package(unofficial-spirv-reflect CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::spirv-reflect::spirv-reflect)

find_path(BSHOSHANY_THREAD_POOL_INCLUDE_DIRS "BS_thread_pool.hpp")
target_include_directories(${PROJECT_NAME} PRIVATE ${BSHOSHANY_THREAD_POOL_INCLUDE_DIRS})