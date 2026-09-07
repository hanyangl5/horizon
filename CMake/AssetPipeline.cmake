set(ASSET_PIPELINE_SOURCE_DIR ${ENGINE_SOURCE_DIR}/Tools/AssetPipeline/src)

set (ASSET_PIPELINE_FILES
    ${ASSET_PIPELINE_SOURCE_DIR}/AssetPipeline.cpp
    ${ASSET_PIPELINE_SOURCE_DIR}/AssetPipeline.h
    ${ASSET_PIPELINE_SOURCE_DIR}/AssetPipelineConfig.h
    ${ASSET_PIPELINE_SOURCE_DIR}/AssetPipelineUnsupported.cpp
)

add_library(AssetPipeline STATIC ${ASSET_PIPELINE_FILES})
target_link_libraries(AssetPipeline PUBLIC Runtime)
target_include_directories(AssetPipeline PUBLIC ${ENGINE_SOURCE_DIR}/Tools/AssetPipeline/Public)
target_compile_features(AssetPipeline PRIVATE cxx_std_20)

add_executable(AssetPipelineCmd ${ASSET_PIPELINE_SOURCE_DIR}/AssetPipelineCmd.cpp)
target_link_libraries(AssetPipelineCmd PRIVATE AssetPipeline)
target_compile_features(AssetPipelineCmd PRIVATE cxx_std_20)

if(WIN32)
    target_link_libraries(AssetPipeline PUBLIC winmm)
endif()

if(MSVC)
    target_compile_options(AssetPipeline PRIVATE /MP)
    target_compile_options(AssetPipelineCmd PRIVATE /MP)
endif()

source_group(TREE ${ASSET_PIPELINE_SOURCE_DIR} PREFIX "Source" FILES ${ASSET_PIPELINE_FILES})
set_target_properties(AssetPipeline AssetPipelineCmd PROPERTIES FOLDER "Horizon/Tools")
