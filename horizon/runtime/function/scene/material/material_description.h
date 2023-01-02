#pragma once

#include <filesystem>

#include <runtime/function/resource/resource_loader/texture/texture_loader.h>
#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/DescriptorSet.h>
#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/Texture.h>

namespace Horizon {

enum MaterialParamFlags {
    HAS_BASE_COLOR = 0x01,
    HAS_NORMAL = 0x10,
    HAS_METALLIC_ROUGHNESS = 0x100,
    HAS_EMISSIVE = 0x1000,
    HAS_ALPHA = 0x10000
};

enum class BlendState { BLEND_STATE_OPAQUE, BLEND_STATE_MASKED, BLEND_STATE_TRANSPARENT };

// each correspond a seperate pso/drawcall
enum class ShadingModel { SHADING_MODEL_LIT, SHADING_MODEL_UNLIT, SHADING_MODEL_SUBSURFACE, SHADING_MODEL_TWO_SIDE };

enum class MaterialTextureType { BASE_COLOR, NORMAL, METALLIC_ROUGHTNESS, EMISSIVE, ALPHA_MASK };

class MaterialTextureDescription {
  public:
    MaterialTextureDescription() noexcept = default;
    MaterialTextureDescription(const std::filesystem::path url) noexcept : url(url){};

    ~MaterialTextureDescription() noexcept {}
    std::filesystem::path url{};
    TextureDataDesc texture_data_desc{};
};

struct MaterialParams {
    Math::float3 base_color_factor;
    f32 roughness_factor;
    Math::float3 emmissive_factor;
    f32 metallic_factor;
    u32 param_bitmask;
    u32 shading_model_id, two_side, pad;
};

class Material {
  public:
    Material() noexcept = default;
    ~Material() noexcept { }

    Material(const Material &rhs){};
    Material &operator=(const Material &rhs) noexcept {};
    Material(Material &&rhs) noexcept {};
    Material &operator=(Material &&rhs) noexcept {};

    ShadingModel GetShadingModelID() noexcept { return shading_model; }

    void InitDescriptorSet();
    void ResetDescriptorSet();

  public:
    Container::HashMap<MaterialTextureType, MaterialTextureDescription> material_textures{};
    MaterialParams material_params{};
    ShadingModel shading_model{ShadingModel::SHADING_MODEL_LIT};
    BlendState blend_state{BlendState::BLEND_STATE_OPAQUE};
    //  Material* materials
};
} // namespace Horizon