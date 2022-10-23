#include "TextureLoader.h"

#include <unordered_map>

#include <runtime/core/log/Log.h>

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>

#include "ddspp.h"

namespace Horizon {

TextureFormat GetTextureFormatFromDXGIForamt(ddspp::DXGIFormat format) {
    switch (format) {
    case ddspp::UNKNOWN:
        break;
    case ddspp::R32G32B32A32_TYPELESS:
        break;
    case ddspp::R32G32B32A32_FLOAT:
        return TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT;
        break;
    case ddspp::R32G32B32A32_UINT:
        break;
    case ddspp::R32G32B32A32_SINT:
        break;
    case ddspp::R32G32B32_TYPELESS:
        break;
    case ddspp::R32G32B32_FLOAT:
        break;
    case ddspp::R32G32B32_UINT:
        break;
    case ddspp::R32G32B32_SINT:
        break;
    case ddspp::R16G16B16A16_TYPELESS:
        break;
    case ddspp::R16G16B16A16_FLOAT:
        return TextureFormat::TEXTURE_FORMAT_RGBA16_SFLOAT;
        break;
    case ddspp::R16G16B16A16_UNORM:
        return TextureFormat::TEXTURE_FORMAT_RGBA16_UNORM;
        break;
    case ddspp::R16G16B16A16_UINT:
        break;
    case ddspp::R16G16B16A16_SNORM:
        break;
    case ddspp::R16G16B16A16_SINT:
        break;
    case ddspp::R32G32_TYPELESS:
        break;
    case ddspp::R32G32_FLOAT:
        break;
    case ddspp::R32G32_UINT:
        break;
    case ddspp::R32G32_SINT:
        break;
    case ddspp::R32G8X24_TYPELESS:
        break;
    case ddspp::D32_FLOAT_S8X24_UINT:
        break;
    case ddspp::R32_FLOAT_X8X24_TYPELESS:
        break;
    case ddspp::X32_TYPELESS_G8X24_UINT:
        break;
    case ddspp::R10G10B10A2_TYPELESS:
        break;
    case ddspp::R10G10B10A2_UNORM:
        break;
    case ddspp::R10G10B10A2_UINT:
        break;
    case ddspp::R11G11B10_FLOAT:
        break;
    case ddspp::R8G8B8A8_TYPELESS:
        break;
    case ddspp::R8G8B8A8_UNORM:
        break;
    case ddspp::R8G8B8A8_UNORM_SRGB:
        break;
    case ddspp::R8G8B8A8_UINT:
        break;
    case ddspp::R8G8B8A8_SNORM:
        break;
    case ddspp::R8G8B8A8_SINT:
        break;
    case ddspp::R16G16_TYPELESS:
        break;
    case ddspp::R16G16_FLOAT:
        break;
    case ddspp::R16G16_UNORM:
        break;
    case ddspp::R16G16_UINT:
        break;
    case ddspp::R16G16_SNORM:
        break;
    case ddspp::R16G16_SINT:
        break;
    case ddspp::R32_TYPELESS:
        break;
    case ddspp::D32_FLOAT:
        break;
    case ddspp::R32_FLOAT:
        break;
    case ddspp::R32_UINT:
        break;
    case ddspp::R32_SINT:
        break;
    case ddspp::R24G8_TYPELESS:
        break;
    case ddspp::D24_UNORM_S8_UINT:
        break;
    case ddspp::R24_UNORM_X8_TYPELESS:
        break;
    case ddspp::X24_TYPELESS_G8_UINT:
        break;
    case ddspp::R8G8_TYPELESS:
        break;
    case ddspp::R8G8_UNORM:
        break;
    case ddspp::R8G8_UINT:
        break;
    case ddspp::R8G8_SNORM:
        break;
    case ddspp::R8G8_SINT:
        break;
    case ddspp::R16_TYPELESS:
        break;
    case ddspp::R16_FLOAT:
        break;
    case ddspp::D16_UNORM:
        break;
    case ddspp::R16_UNORM:
        break;
    case ddspp::R16_UINT:
        break;
    case ddspp::R16_SNORM:
        break;
    case ddspp::R16_SINT:
        break;
    case ddspp::R8_TYPELESS:
        break;
    case ddspp::R8_UNORM:
        break;
    case ddspp::R8_UINT:
        break;
    case ddspp::R8_SNORM:
        break;
    case ddspp::R8_SINT:
        break;
    case ddspp::A8_UNORM:
        break;
    case ddspp::R1_UNORM:
        break;
    case ddspp::R9G9B9E5_SHAREDEXP:
        break;
    case ddspp::R8G8_B8G8_UNORM:
        break;
    case ddspp::G8R8_G8B8_UNORM:
        break;
    case ddspp::BC1_TYPELESS:
        break;
    case ddspp::BC1_UNORM:
        break;
    case ddspp::BC1_UNORM_SRGB:
        break;
    case ddspp::BC2_TYPELESS:
        break;
    case ddspp::BC2_UNORM:
        break;
    case ddspp::BC2_UNORM_SRGB:
        break;
    case ddspp::BC3_TYPELESS:
        break;
    case ddspp::BC3_UNORM:
        break;
    case ddspp::BC3_UNORM_SRGB:
        break;
    case ddspp::BC4_TYPELESS:
        break;
    case ddspp::BC4_UNORM:
        break;
    case ddspp::BC4_SNORM:
        break;
    case ddspp::BC5_TYPELESS:
        break;
    case ddspp::BC5_UNORM:
        break;
    case ddspp::BC5_SNORM:
        break;
    case ddspp::B5G6R5_UNORM:
        break;
    case ddspp::B5G5R5A1_UNORM:
        break;
    case ddspp::B8G8R8A8_UNORM:
        break;
    case ddspp::B8G8R8X8_UNORM:
        break;
    case ddspp::R10G10B10_XR_BIAS_A2_UNORM:
        break;
    case ddspp::B8G8R8A8_TYPELESS:
        break;
    case ddspp::B8G8R8A8_UNORM_SRGB:
        break;
    case ddspp::B8G8R8X8_TYPELESS:
        break;
    case ddspp::B8G8R8X8_UNORM_SRGB:
        break;
    case ddspp::BC6H_TYPELESS:
        break;
    case ddspp::BC6H_UF16:
        break;
    case ddspp::BC6H_SF16:
        break;
    case ddspp::BC7_TYPELESS:
        break;
    case ddspp::BC7_UNORM:
        break;
    case ddspp::BC7_UNORM_SRGB:
        break;
    case ddspp::AYUV:
        break;
    case ddspp::Y410:
        break;
    case ddspp::Y416:
        break;
    case ddspp::NV12:
        break;
    case ddspp::P010:
        break;
    case ddspp::P016:
        break;
    case ddspp::OPAQUE_420:
        break;
    case ddspp::YUY2:
        break;
    case ddspp::Y210:
        break;
    case ddspp::Y216:
        break;
    case ddspp::NV11:
        break;
    case ddspp::AI44:
        break;
    case ddspp::IA44:
        break;
    case ddspp::P8:
        break;
    case ddspp::A8P8:
        break;
    case ddspp::B4G4R4A4_UNORM:
        break;
    case ddspp::P208:
        break;
    case ddspp::V208:
        break;
    case ddspp::V408:
        break;
    case ddspp::ASTC_4X4_TYPELESS:
        break;
    case ddspp::ASTC_4X4_UNORM:
        break;
    case ddspp::ASTC_4X4_UNORM_SRGB:
        break;
    case ddspp::ASTC_5X4_TYPELESS:
        break;
    case ddspp::ASTC_5X4_UNORM:
        break;
    case ddspp::ASTC_5X4_UNORM_SRGB:
        break;
    case ddspp::ASTC_5X5_TYPELESS:
        break;
    case ddspp::ASTC_5X5_UNORM:
        break;
    case ddspp::ASTC_5X5_UNORM_SRGB:
        break;
    case ddspp::ASTC_6X5_TYPELESS:
        break;
    case ddspp::ASTC_6X5_UNORM:
        break;
    case ddspp::ASTC_6X5_UNORM_SRGB:
        break;
    case ddspp::ASTC_6X6_TYPELESS:
        break;
    case ddspp::ASTC_6X6_UNORM:
        break;
    case ddspp::ASTC_6X6_UNORM_SRGB:
        break;
    case ddspp::ASTC_8X5_TYPELESS:
        break;
    case ddspp::ASTC_8X5_UNORM:
        break;
    case ddspp::ASTC_8X5_UNORM_SRGB:
        break;
    case ddspp::ASTC_8X6_TYPELESS:
        break;
    case ddspp::ASTC_8X6_UNORM:
        break;
    case ddspp::ASTC_8X6_UNORM_SRGB:
        break;
    case ddspp::ASTC_8X8_TYPELESS:
        break;
    case ddspp::ASTC_8X8_UNORM:
        break;
    case ddspp::ASTC_8X8_UNORM_SRGB:
        break;
    case ddspp::ASTC_10X5_TYPELESS:
        break;
    case ddspp::ASTC_10X5_UNORM:
        break;
    case ddspp::ASTC_10X5_UNORM_SRGB:
        break;
    case ddspp::ASTC_10X6_TYPELESS:
        break;
    case ddspp::ASTC_10X6_UNORM:
        break;
    case ddspp::ASTC_10X6_UNORM_SRGB:
        break;
    case ddspp::ASTC_10X8_TYPELESS:
        break;
    case ddspp::ASTC_10X8_UNORM:
        break;
    case ddspp::ASTC_10X8_UNORM_SRGB:
        break;
    case ddspp::ASTC_10X10_TYPELESS:
        break;
    case ddspp::ASTC_10X10_UNORM:
        break;
    case ddspp::ASTC_10X10_UNORM_SRGB:
        break;
    case ddspp::ASTC_12X10_TYPELESS:
        break;
    case ddspp::ASTC_12X10_UNORM:
        break;
    case ddspp::ASTC_12X10_UNORM_SRGB:
        break;
    case ddspp::ASTC_12X12_TYPELESS:
        break;
    case ddspp::ASTC_12X12_UNORM:
        break;
    case ddspp::ASTC_12X12_UNORM_SRGB:
        break;
    case ddspp::FORCE_UINT:
        break;
    default:
        break;
    }
    return {};
}

TextureDataDesc TextureLoader::Load(const std::filesystem::path &path) {
    TextureDataDesc texture_info{};
    auto &extension = path.extension();
    if (extension == ".png") {
        LoadPNG(path, texture_info);
    } else if (extension == ".jpg" || extension == ".jpeg") {
        LoadJPG(path, texture_info);
    } else if (extension == ".dds") {
        LoadDDS(path, texture_info);

    } else if (extension == ".ktx") {
    } else {
        LOG_ERROR("{} format is not supportted", extension.string().c_str());
    }
    return texture_info;
}

void Horizon::TextureLoader::LoadJPG(const std::filesystem::path &path, TextureDataDesc &texture_info) {
    int channels;
    u8 *data = stbi_load(path.string().c_str(), (int *)&texture_info.width, (int *)&texture_info.height, &channels,
                         STBI_rgb_alpha);
    texture_info.format = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM;
    texture_info.type = TextureType::TEXTURE_TYPE_2D;
    texture_info.layer_count = 1;
    texture_info.raw_data = {data, data + texture_info.width * texture_info.height * 4};
    texture_info.mipmap_count = 1;
    texture_info.layer_count = 1;
    stbi_image_free(data);
}
void TextureLoader::LoadPNG(const std::filesystem::path &path, TextureDataDesc &texture_info) {
    int channels;
    u8 *data = stbi_load(path.string().c_str(), (int *)&texture_info.width, (int *)&texture_info.height, &channels,
                         STBI_rgb_alpha);
    texture_info.format = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM;
    texture_info.type = TextureType::TEXTURE_TYPE_2D;
    texture_info.layer_count = 1;
    texture_info.raw_data = {data, data + texture_info.width * texture_info.height * 4};
    texture_info.mipmap_count = 1;
    texture_info.layer_count = 1;
    stbi_image_free(data);
}
void TextureLoader::LoadHDR(const std::filesystem::path &path, TextureDataDesc &texture_info) {}

void TextureLoader::LoadKTX(const std::filesystem::path &path, TextureDataDesc &texture_info) {}

void TextureLoader::LoadDDS(const std::filesystem::path &path, TextureDataDesc &texture_info) {

    auto raw_data = ReadFile(path.string().c_str());
    if (raw_data.empty()) {
        return;
    }

    ddspp::Descriptor desc;
    ddspp::decode_header(reinterpret_cast<unsigned char *>(raw_data.data()), desc);

    texture_info.width = desc.width;
    texture_info.height = desc.height;
    texture_info.mipmap_count = desc.numMips;

    texture_info.type = static_cast<TextureType>(desc.type);
    texture_info.layer_count = desc.arraySize;
    if (texture_info.type == TextureType::TEXTURE_TYPE_CUBE) {
        texture_info.layer_count = 6;
    } 
    texture_info.format = GetTextureFormatFromDXGIForamt(desc.format);
    raw_data = {raw_data.begin() + desc.headerSize, raw_data.end()};
    texture_info.raw_data.swap(raw_data);

    texture_info.data_offset_map.resize(6, std::vector<u32>(desc.numMips));

    for (u32 layer = 0; layer < texture_info.layer_count; layer++) {
        for (u32 mip = 0; mip < desc.numMips; mip++) {
            texture_info.data_offset_map[layer][mip] = ddspp::get_offset(desc, mip, layer);
        }
    }
}



} // namespace Horizon