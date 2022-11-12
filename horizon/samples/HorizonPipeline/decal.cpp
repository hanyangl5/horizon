#include "decal.h"

DecalData::DecalData(RHI *rhi, ResourceManager *resource_manager) noexcept {
    decal_vs = rhi->CreateShader(ShaderType::VERTEX_SHADER, 0, asset_path / "shaders/post_process.comp.hsl");
    decal_ps = rhi->CreateShader(ShaderType::PIXEL_SHADER, 0, asset_path / "shaders/post_process.comp.hsl");

    decal_buffer =
        resource_manager->CreateGpuBuffer(BufferCreateInfo{DescriptorType::DESCRIPTOR_TYPE_CONSTANT_BUFFER,
                                           ResourceState::RESOURCE_STATE_SHADER_RESOURCE, sizeof(decal_constant)});
    auto tex = TextureLoader::Load("");
    tex.width;

    decal_texture = resource_manager->CreateGpuTexture();
    decal_pass->SetGraphicsShader(decal_vs, decal_ps);
}
