/*****************************************************************//**
 * \file   Decal.cpp
 * \brief  
 * 
 * \author hylu
 * \date   December 2022
 *********************************************************************/

#include "Decal.h"

#include <nlohmann/json.hpp>

#include "../../resource_loader/texture/TextureLoader.h"

Horizon::Decal::Decal(const std::filesystem::path &path, std::pmr::polymorphic_allocator<std::byte> allocator) noexcept
    : m_path(path) {
    auto decal_material_desc = ReadFile(path.generic_string().c_str());
    nlohmann::json json_data = nlohmann::json::parse(decal_material_desc);

    decal_material = Memory::Alloc<Material>();

    if (json_data["blend_state"] == "OPAQUE") {
        decal_material->blend_state = BlendState::BLEND_STATE_OPAQUE;
    } else if (json_data["blend_state"] == "MASKED") {
        decal_material->blend_state = BlendState::BLEND_STATE_MASKED;
    } else if (json_data["blend_state"] == "TRANSPAREN") {
        decal_material->blend_state = BlendState::BLEND_STATE_TRANSPARENT;
    } else {
        LOG_ERROR("invalid blend state for decal");
    }

    if (json_data["shading_model"] == "lit") {
        decal_material->shading_model = ShadingModel::SHADING_MODEL_LIT;
    } else if (json_data["shading_model"] == "unlit") {
        decal_material->shading_model = ShadingModel::SHADING_MODEL_UNLIT;
    } else {
        LOG_ERROR("invalid shading model for decal");
    }
    decal_material->material_params.shading_model_id = static_cast<u32>(decal_material->shading_model);

    auto &texs = json_data["textures"];
    for (auto &tex : texs.items()) {
        if (tex.key() == "base_color") {
            std::filesystem::path bc_path = path.parent_path() / tex.value();
            decal_material->material_textures[MaterialTextureType::BASE_COLOR].texture_data_desc =
                TextureLoader::Load(bc_path.string().c_str());
            decal_material->material_params.param_bitmask |= HAS_BASE_COLOR;
            
        } else if (tex.key() == "normal") {
            std::filesystem::path normal_path = path.parent_path() / tex.value();
            decal_material->material_textures[MaterialTextureType::NORMAL].texture_data_desc =
                TextureLoader::Load(normal_path.string().c_str());
            decal_material->material_params.param_bitmask |= HAS_NORMAL;
        }
    }
}

Horizon::Decal::~Decal() noexcept {}
