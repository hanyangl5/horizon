#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/Texture.h>

namespace Horizon {

enum class MaterialTextureType { BASE_COLOR, NORMAL, METALLIC_ROUGHTNESS, EMISSIVE };

class MaterialTextureDescription {
  public:
    MaterialTextureDescription(MaterialTextureType type, const std::filesystem::path url) noexcept
        : type(type), url(url){};

    ~MaterialTextureDescription() noexcept { texture = nullptr; }

    MaterialTextureDescription(const MaterialTextureDescription &rhs) {
        type = rhs.type;
        url = rhs.url;
    };
    MaterialTextureDescription &operator=(const MaterialTextureDescription &rhs) {
        type = rhs.type;
        url = rhs.url;
    };
    MaterialTextureDescription(MaterialTextureDescription &&rhs) noexcept {
        type = rhs.type;
        url = rhs.url;
    };
    MaterialTextureDescription &operator=(MaterialTextureDescription &&rhs) {
        type = rhs.type;
        url = rhs.url;
    };

    MaterialTextureType type{};
    std::filesystem::path url{};
    u32 width{}, height{}, channels{};
    void *data{};
    Resource<RHI::Texture> texture{};
};

struct MaterialParams {
    // Math::float3 base_color_factor;
    // Math::float3 emmissive_factor;
    // f32 roughness_factor;
    // f32 metallic_factor;
};

class MaterialDescription {
  public:
    MaterialDescription() noexcept = default;
    ~MaterialDescription() noexcept = default;

  public:
    void UploadTextures(RHI::RHI *rhi);

    std::vector<MaterialTextureDescription> material_textures{};
    MaterialParams material_params;
    RHI::Buffer *param_buffer{};
    // Material* materials
};
} // namespace Horizon