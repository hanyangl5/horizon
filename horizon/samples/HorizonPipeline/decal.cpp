#include "decal.h"

DecalData::DecalData(Backend::RHI *rhi, SceneManager *scene_manager) noexcept {
    decal_vs = rhi->CreateShader(ShaderType::VERTEX_SHADER, 0, asset_path / "shaders/decal.vert.hsl");
    decal_ps = rhi->CreateShader(ShaderType::VERTEX_SHADER, 0, asset_path / "shaders/decal.frag.hsl");
    decal_pass = rhi->CreateGraphicsPipeline(GraphicsPipelineCreateInfo{});
}
