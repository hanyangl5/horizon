set(ASSET_PIPELINE_SOURCE_DIR ${ENGINE_SOURCE_DIR}/Tools/AssetPipeline/src)

if(WIN32)
    function(add_asset_pipeline_directxtex)
        set(BUILD_TOOLS OFF)
        set(BUILD_SAMPLE OFF)
        set(BUILD_DX11 OFF)
        set(BUILD_DX12 OFF)
        set(BC_USE_OPENMP OFF)
        set(BUILD_SHARED_LIBS OFF)
        add_subdirectory(${ENGINE_THIRD_PARTY_SOURCE_DIR}/DirectXTex ${CMAKE_BINARY_DIR}/DirectXTex EXCLUDE_FROM_ALL)
        set_target_properties(DirectXTex PROPERTIES FOLDER "Horizon/ThirdParty")
    endfunction()
    add_asset_pipeline_directxtex()
endif()

set (ASSET_PIPELINE_FILES
    ${ASSET_PIPELINE_SOURCE_DIR}/AssetPipeline.cpp
    ${ASSET_PIPELINE_SOURCE_DIR}/AssetPipeline.h
    ${ASSET_PIPELINE_SOURCE_DIR}/AssetPipelineConfig.h
    ${ASSET_PIPELINE_SOURCE_DIR}/AssetPipelineUnsupported.cpp
    ${ASSET_PIPELINE_SOURCE_DIR}/SceneAssetCooker.cpp
    ${ASSET_PIPELINE_SOURCE_DIR}/SceneAssetCooker.h
    ${ASSET_PIPELINE_SOURCE_DIR}/SceneTextureCooker.cpp
    ${ASSET_PIPELINE_SOURCE_DIR}/SceneTextureCooker.h
)

add_library(AssetPipeline STATIC ${ASSET_PIPELINE_FILES})
target_link_libraries(AssetPipeline PUBLIC Runtime)
target_include_directories(AssetPipeline PUBLIC ${ENGINE_SOURCE_DIR}/Tools/AssetPipeline/Public)
target_compile_features(AssetPipeline PRIVATE cxx_std_20)

add_executable(AssetPipelineCmd ${ASSET_PIPELINE_SOURCE_DIR}/AssetPipelineCmd.cpp)
target_link_libraries(AssetPipelineCmd PRIVATE AssetPipeline)
target_compile_features(AssetPipelineCmd PRIVATE cxx_std_20)

if(WIN32)
    target_link_libraries(AssetPipeline PUBLIC winmm PRIVATE DirectXTex windowscodecs ole32)
endif()

if(MSVC)
    target_compile_options(AssetPipeline PRIVATE /MP)
    target_compile_options(AssetPipelineCmd PRIVATE /MP)
endif()

source_group(TREE ${ASSET_PIPELINE_SOURCE_DIR} PREFIX "Source" FILES ${ASSET_PIPELINE_FILES})
set_target_properties(AssetPipeline AssetPipelineCmd PROPERTIES FOLDER "Horizon/Tools")
