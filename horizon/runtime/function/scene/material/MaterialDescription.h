#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/Texture.h>
#include <runtime/function/rhi/DescriptorSet.h>

namespace Horizon {

enum class MaterialTextureType { BASE_COLOR, NORMAL, METALLIC_ROUGHTNESS, EMISSIVE };

class MaterialTextureDescription {
  public:
    MaterialTextureDescription() noexcept = default;
    MaterialTextureDescription(const std::filesystem::path url) noexcept : url(url){};

    ~MaterialTextureDescription() noexcept { texture = nullptr; }

    MaterialTextureDescription(const MaterialTextureDescription &rhs) { url = rhs.url; };
    MaterialTextureDescription &operator=(const MaterialTextureDescription &rhs) noexcept { url = rhs.url; };
    MaterialTextureDescription(MaterialTextureDescription &&rhs) noexcept { url = rhs.url; };
    MaterialTextureDescription &operator=(MaterialTextureDescription &&rhs) noexcept { url = rhs.url; };

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

class Material {
  public:
    Material() noexcept = default;
    ~Material() noexcept = default;

  public:
    RHI::DescriptorSet *material_descriptor_set{};
    std::unordered_map<MaterialTextureType, MaterialTextureDescription> material_textures{};
    //MaterialParams material_params;
    //RHI::Buffer *param_buffer{};
    // Material* materials
};
} // namespace Horizon