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
    //texture_data_desc = TextureLoader::Load(path);
    
    decal_material->blend_state;
}

Horizon::Decal::~Decal() noexcept {}
