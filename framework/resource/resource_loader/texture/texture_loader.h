#pragma once

#include <core/definations.h>
#include <rhi/enums.h>

namespace Horizon
{

class TextureLoader
{
  public:
    static TextureDataDesc Load(const char *path);
    static TextureDataDesc LoadFromMemory(const void *data, u64 size);
    // jpg, png
  private:
    static void LoadJPG(const char *path, TextureDataDesc &texture_info);

    static void LoadPNG(const char *path, TextureDataDesc &texture_info);

    static void LoadHDR(const char *path, TextureDataDesc &texture_info);
    // ktx
    static void LoadKTX(const char *path, TextureDataDesc &texture_info);
    // dds
    static void LoadDDS(const char *path, TextureDataDesc &texture_info);

    static void LoadTGA(const char *path, TextureDataDesc &texture_info);
};
} // namespace Horizon
