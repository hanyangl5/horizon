#pragma once

#include <filesystem>

#include <runtime/core/utils/definations.h>
#include <runtime/rhi/Enums.h>

namespace Horizon {

class TextureLoader {
  public:
    static TextureDataDesc Load(const std::filesystem::path &path);
    // jpg, png
  private:
    static void LoadJPG(const std::filesystem::path &path, TextureDataDesc &texture_info);

    static void LoadPNG(const std::filesystem::path &path, TextureDataDesc &texture_info);

    static void LoadHDR(const std::filesystem::path &path, TextureDataDesc &texture_info);
    // ktx
    static void LoadKTX(const std::filesystem::path &path, TextureDataDesc &texture_info);
    // dds
    static void LoadDDS(const std::filesystem::path &path, TextureDataDesc &texture_info);

    static void LoadTGA(const std::filesystem::path &path, TextureDataDesc &texture_info);
};
} // namespace Horizon
