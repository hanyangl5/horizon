#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/Texture.h>
#include <runtime/function/rhi/DescriptorSet.h>
#include <runtime/function/texture_loader/TextureLoader.h>

namespace Horizon {

enum MaterialParamFlags {
    HAS_BASE_COLOR = 0x01,
    HAS_NORMAL = 0x10,
    HAS_METALLIC_ROUGHNESS = 0x100,
    HAS_EMISSIVE = 0x1000,
    HAS_ALPHA = 0x10000
};

enum class ShadingModel {
    SHADING_MODEL_UNLIT,
    SHADING_MODEL_OPAQUE,
    SHADING_MODEL_MASKED
};

enum class MaterialTextureType { BASE_COLOR, NORMAL, METALLIC_ROUGHTNESS, EMISSIVE, ALPHA_MASK };

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
    Resource<Backend::Texture> texture{};
    TextureDataDesc texture_data_desc{};
};

struct MaterialParams {
    Math::float3 base_color_factor;
    f32 roughness_factor;
    Math::float3 emmissive_factor;
    f32 metallic_factor;
    u32 param_bitmask;
    u32 shading_model_id, pad1, pad2;
};

class Material {
  public:
    Material() noexcept = default;
    ~Material() noexcept { material_descriptor_set = nullptr; }

    Material(const Material &rhs){};
    Material &operator=(const Material &rhs) noexcept {};
    Material(Material &&rhs) noexcept {};
    Material &operator=(Material &&rhs) noexcept {};

    ShadingModel GetShadingModelID() noexcept { return shading_model; }

    void InitDescriptorSet();
    void ResetDescriptorSet();
  public:
    Backend::DescriptorSet* material_descriptor_set{};
    std::unordered_map<MaterialTextureType, MaterialTextureDescription> material_textures{};
    MaterialParams material_params{};
    ShadingModel shading_model{ShadingModel::SHADING_MODEL_OPAQUE};
    Resource<Backend::Buffer> param_buffer{};
    //  Material* materials
};
} // namespace Horizon