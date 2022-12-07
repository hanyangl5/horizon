/*****************************************************************//**
 * \file   Decal.cpp
 * \brief  
 * 
 * \author hylu
 * \date   December 2022
 *********************************************************************/

#include "Decal.h"

#include "../../resource_loader/texture/TextureLoader.h"

Horizon::Decal::Decal(const std::filesystem::path &path, std::pmr::polymorphic_allocator<std::byte> allocator) noexcept
    : m_path(path) {
    texture_data_desc = TextureLoader::Load(path);
}

Horizon::Decal::~Decal() noexcept {}
