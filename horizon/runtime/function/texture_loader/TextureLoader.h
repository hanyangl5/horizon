#pragma once

#include <filesystem>

#include <DXGIFormat.h>

#include <runtime/core/utils/definations.h>
#include <runtime/function/rhi/RHIUtils.h>

namespace Horizon {

struct TextureDataInfo {
    u32 width;
    u32 height;
    u32 depth;
    u32 array_size;
    u32 mipmap_count;
    TextureFormat format;
    TextureType type;
    std::vector<char> data;
};

class TextureLoader {
  public:
    static TextureDataInfo Load(const std::filesystem::path &path);
    // jpg, png
  private:
    static void LoadJPG(const std::filesystem::path &path, TextureDataInfo &texture_info);

    static void LoadPNG(const std::filesystem::path &path, TextureDataInfo &texture_info);

    static void LoadHDR(const std::filesystem::path &path, TextureDataInfo &texture_info);
    // ktx
    static void LoadKTX(const std::filesystem::path &path, TextureDataInfo &texture_info);
    // dds
    static void LoadDDS(const std::filesystem::path &path, TextureDataInfo &texture_info);
};
} // namespace Horizon
