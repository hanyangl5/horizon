project(framework)

# 定义选项来控制是否编译相应的后端
# 可以通过命令行参数设置：-DUSE_VULKAN=ON 或 -DUSE_DX12=OFF
option(USE_VULKAN "Enable Vulkan backend" ON)
option(USE_DX12 "Enable DirectX 12 backend" ON)

# 收集头文件
file(GLOB_RECURSE HEADER_FILES "*.h" "*.hpp")

# 收集除了 rhi/dx12 和 rhi/vulkan 之外的所有源文件
file(GLOB_RECURSE SOURCE_FILES "*.cpp" "*.c")

# 排除 rhi/dx12 和 rhi/vulkan 文件夹中的源文件（稍后根据条件添加）
list(FILTER SOURCE_FILES EXCLUDE REGEX ".*rhi/dx12/.*")
list(FILTER SOURCE_FILES EXCLUDE REGEX ".*rhi/vulkan/.*")

# 根据变量决定是否添加相应的源文件
if(USE_VULKAN)
    file(GLOB_RECURSE VULKAN_SOURCES "rhi/vulkan/*.cpp" "rhi/vulkan/*.c")
    list(APPEND SOURCE_FILES ${VULKAN_SOURCES})
endif()

if(USE_DX12)
    file(GLOB_RECURSE DX12_SOURCES "rhi/dx12/*.cpp" "rhi/dx12/*.c")
    list(APPEND SOURCE_FILES ${DX12_SOURCES})
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HEADER_FILES} ${SOURCE_FILES})

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR} / framework)

# 根据变量设置编译定义
if(USE_VULKAN)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_VULKAN)
endif()

if(USE_DX12)
    target_compile_definitions(${PROJECT_NAME} PUBLIC USE_DX12)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC third_party)

configure_compile_options(${PROJECT_NAME})