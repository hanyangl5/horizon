set (ASSET_PIPELINE_FILES
    ../The-Forge/Common_3/Tools/FileSystem/IToolFileSystem.h
    ../The-Forge/Common_3/ThirdParty/OpenSource/TressFX/TressFXAsset.cpp
    ../The-Forge/Common_3/ThirdParty/OpenSource/TressFX/TressFXAsset.h
    ../The-Forge/Common_3/ThirdParty/OpenSource/TressFX/TressFXFileFormat.h
    ../The-Forge/Common_3/Tools/AssetPipeline/src/AssetPipeline.cpp
    ../The-Forge/Common_3/Tools/AssetPipeline/src/AssetPipeline.h
    ../The-Forge/Common_3/Tools/AssetPipeline/src/AssetPipelineCmd.cpp
    ../The-Forge/Common_3/ThirdParty/OpenSource/EASTL/eastl.cpp
)

if(${WINDOWS} MATCHES ON)
    set(ASSET_PIPELINE_FILES ${ASSET_PIPELINE_FILES}
        ../The-Forge/Common_3/Tools/FileSystem/WindowsToolsFileSystem.cpp
    )
endif()

add_executable(AssetPipelineCmd ${ASSET_PIPELINE_FILES})
target_link_libraries(AssetPipelineCmd The-Forge ${RENDER_LIBRARIES})
set_property(TARGET AssetPipelineCmd PROPERTY CXX_STANDARD 20)
