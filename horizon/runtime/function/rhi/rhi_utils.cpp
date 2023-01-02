#include "rhi_utils.h"

namespace Horizon {

u32 GetStrideFromVertexAttributeDescription(VertexAttribFormat format, u32 portions) {
    assert(portions <= 4);
    u32 stride = 0;
    switch (format) {
    case Horizon::VertexAttribFormat::U8:
    case Horizon::VertexAttribFormat::S8:
    case Horizon::VertexAttribFormat::UN8:
    case Horizon::VertexAttribFormat::SN8:
        stride = portions * 1;
        break;
    case Horizon::VertexAttribFormat::U16:
    case Horizon::VertexAttribFormat::S16:
    case Horizon::VertexAttribFormat::UN16:
    case Horizon::VertexAttribFormat::SN16:
    case Horizon::VertexAttribFormat::F16:
        stride = portions * 2;
        break;
    case Horizon::VertexAttribFormat::U32:
    case Horizon::VertexAttribFormat::S32:
    case Horizon::VertexAttribFormat::F32:
        stride = portions * 4;
    default:
        break;
    }
    return stride;
}

u32 GetBytesFromTextureFormat(TextureFormat format) {
    switch (format) {
    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_UINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_SINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_UNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_SNORM:
        return 1;

    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_UINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_SINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_UNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_SNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_SINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_UINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_UNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_SNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_SFLOAT:
        return 2;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_UINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_SINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_UNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_SNORM:

        return 3;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_UINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_SINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_SNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_UINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_SINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_UNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_SNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_SFLOAT:

    case Horizon::TextureFormat::TEXTURE_FORMAT_R32_UINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_R32_SINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_R32_SFLOAT:

    case Horizon::TextureFormat::TEXTURE_FORMAT_R10G10B10A2_UNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_R11G11B10_UNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_R10G10B10A2_SNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_R11G11B10_SFLOAT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_R11G11B10_UFLOAT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_D32_SFLOAT:
        return 4;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_UINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_SINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_UNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_SNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_SFLOAT:
        return 6;

    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_UINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_SINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_UNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_SNORM:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_SFLOAT:

    case Horizon::TextureFormat::TEXTURE_FORMAT_RG32_UINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG32_SINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RG32_SFLOAT:
        return 8;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB32_UINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB32_SINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB32_SFLOAT:
        return 12;
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA32_UINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA32_SINT:
    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT:
        return 16;

    case Horizon::TextureFormat::TEXTURE_FORMAT_UNDEFINED:
    case Horizon::TextureFormat::TEXTURE_FORMAT_DUMMY_COLOR:
        return 0;

    default:
        LOG_ERROR("invalid format")
        return 0;
        assert(false);
    }
}
} // namespace Horizon