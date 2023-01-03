//
//inline D3D12_COMMAND_LIST_TYPE ToDX12CommandQueueType(CommandQueueType type) {
//    switch (type) {
//    case Horizon::GRAPHICS:
//        return D3D12_COMMAND_LIST_TYPE_DIRECT;
//    case Horizon::COMPUTE:
//        return D3D12_COMMAND_LIST_TYPE_COMPUTE;
//    case Horizon::TRANSFER:
//        return D3D12_COMMAND_LIST_TYPE_COPY;
//    default:
//        LOG_ERROR("invalid command queue type")
//        return {};
//    }
//}
//
//inline DXGI_FORMAT ToDx12TextureFormat(TextureFormat format) {
//    switch (format) {
//    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_UINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_UINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_UINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_UINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_UINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_UINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_UINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_UINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_R32_UINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RG32_UINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB32_UINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA32_UINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_UNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_UNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_UNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_UNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_UNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_UNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_UNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_SINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_SINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_SINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_SINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_SINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_SINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_SINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_SINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_R32_SINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RG32_SINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB32_SINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA32_SINT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_R8_SNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RG8_SNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB8_SNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA8_SNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_SNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_SNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_SNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_SNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_R32_SNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RG32_SNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB32_SNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA32_SNORM:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_R16_SFLOAT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RG16_SFLOAT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB16_SFLOAT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA16_SFLOAT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_R32_SFLOAT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RG32_SFLOAT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGB32_SFLOAT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_RGBA32_SFLOAT:
//        break;
//    case Horizon::TextureFormat::TEXTURE_FORMAT_D32_SFLOAT:
//        break;
//    default:
//        LOG_ERROR("invalid texture format, use rgba8 format as default");
//        return DXGI_FORMAT_R8G8B8A8_UNORM;
//        break;
//    }
//    return DXGI_FORMAT_R8G8B8A8_UNORM;
//}
//
//inline D3D12_RESOURCE_DIMENSION ToDX12TextureDimension(TextureType type) {
//    switch (type) {
//    case Horizon::TextureType::TEXTURE_TYPE_1D:
//        return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
//        break;
//    case Horizon::TextureType::TEXTURE_TYPE_2D:
//        return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
//        break;
//    case Horizon::TextureType::TEXTURE_TYPE_3D:
//        return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
//        break;
//    default:
//        LOG_ERROR("invalid image type, use texture2D as default");
//        return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
//        break;
//    }
//}