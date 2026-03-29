#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include "metal_utils.h"

#include <core/log.h>

namespace Horizon::Backend::MetalUtils
{

NS::String *ToNSString(const std::string &value)
{
    return NS::String::string(value.c_str(), NS::UTF8StringEncoding);
}

id ToObjCObject(void *object)
{
    return reinterpret_cast<id>(object);
}

id ToObjCObject(const void *object)
{
    return reinterpret_cast<id>(const_cast<void *>(object));
}

CGRect GetViewBounds(id view)
{
    static SEL kBoundsSelector = sel_registerName("bounds");
    return SendObjCGeometryMessage(view, kBoundsSelector);
}

CGSize GetDrawableSize(id view)
{
    if (view == nil)
    {
        return CGSizeZero;
    }

    static SEL kConvertRectToBackingSelector = sel_registerName("convertRectToBacking:");
    return SendObjCGeometryMessage(view, kConvertRectToBackingSelector, GetViewBounds(view)).size;
}

CGFloat GetBackingScaleFactor(id view)
{
    static SEL kWindowSelector = sel_registerName("window");
    static SEL kBackingScaleFactorSelector = sel_registerName("backingScaleFactor");

    id window = SendObjCMessage<id>(view, kWindowSelector);
    return window != nil ? SendObjCMessage<CGFloat>(window, kBackingScaleFactorSelector) : 1.0;
}

bool RespondsToSelector(id object, SEL selector)
{
    static SEL kRespondsToSelector = sel_registerName("respondsToSelector:");
    return object != nil && SendObjCMessage<BOOL>(object, kRespondsToSelector, selector) == YES;
}

void SetViewLayer(id view, id layer)
{
    static SEL kSetWantsLayerSelector = sel_registerName("setWantsLayer:");
    static SEL kSetLayerSelector = sel_registerName("setLayer:");

    SendObjCMessage<void>(view, kSetWantsLayerSelector, YES);
    SendObjCMessage<void>(view, kSetLayerSelector, layer);
}

void SetLayerContentsScale(id layer, CGFloat scale)
{
    static SEL kSetContentsScaleSelector = sel_registerName("setContentsScale:");
    SendObjCMessage<void>(layer, kSetContentsScaleSelector, scale);
}

void SetLayerFrame(id layer, CGRect frame)
{
    static SEL kSetFrameSelector = sel_registerName("setFrame:");
    SendObjCMessage<void>(layer, kSetFrameSelector, frame);
}

const char *GetNSErrorDescription(NS::Error *error)
{
    if (error == nullptr)
    {
        return "unknown error";
    }

    NS::String *description = error->localizedDescription();
    return description != nullptr ? description->utf8String() : "unknown error";
}

MTL::PixelFormat ToMetalPixelFormat(TextureFormat format, bool for_swap_chain)
{
    if (for_swap_chain)
    {
        return MTL::PixelFormatBGRA8Unorm;
    }

    switch (format)
    {
    case TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM:
        return MTL::PixelFormatRGBA8Unorm;
    case TextureFormat::TEXTURE_FORMAT_D32_SFLOAT:
        return MTL::PixelFormatDepth32Float;
    case TextureFormat::TEXTURE_FORMAT_R32_SFLOAT:
        return MTL::PixelFormatR32Float;
    default:
        LOG_ERROR("Unsupported Metal texture format {}", static_cast<u32>(format));
        return MTL::PixelFormatInvalid;
    }
}

MTL::VertexFormat ToMetalVertexFormat(VertexAttribFormat format, u32 portion)
{
    if (format != VertexAttribFormat::F32)
    {
        LOG_ERROR("Unsupported Metal vertex format {} with portion {}", static_cast<u32>(format), portion);
        return MTL::VertexFormatInvalid;
    }

    switch (portion)
    {
    case 1:
        return MTL::VertexFormatFloat;
    case 2:
        return MTL::VertexFormatFloat2;
    case 3:
        return MTL::VertexFormatFloat3;
    case 4:
        return MTL::VertexFormatFloat4;
    default:
        LOG_ERROR("Unsupported Metal vertex attribute portion {}", portion);
        return MTL::VertexFormatInvalid;
    }
}

MTL::PrimitiveType ToMetalPrimitiveType(PrimitiveTopology topology)
{
    switch (topology)
    {
    case PrimitiveTopology::POINT_LIST:
        return MTL::PrimitiveTypePoint;
    case PrimitiveTopology::LINE_LIST:
        return MTL::PrimitiveTypeLine;
    case PrimitiveTopology::TRIANGLE_LIST:
        return MTL::PrimitiveTypeTriangle;
    default:
        return MTL::PrimitiveTypeTriangle;
    }
}

MTL::Winding ToMetalWinding(FrontFace front_face)
{
    return front_face == FrontFace::CW ? MTL::WindingClockwise : MTL::WindingCounterClockwise;
}

MTL::CullMode ToMetalCullMode(CullMode cull_mode)
{
    switch (cull_mode)
    {
    case CullMode::NONE:
        return MTL::CullModeNone;
    case CullMode::FRONT:
        return MTL::CullModeFront;
    case CullMode::BACK:
        return MTL::CullModeBack;
    default:
        return MTL::CullModeNone;
    }
}

MTL::TriangleFillMode ToMetalFillMode(FillMode fill_mode)
{
    switch (fill_mode)
    {
    case FillMode::LINE:
        return MTL::TriangleFillModeLines;
    case FillMode::POINT:
    case FillMode::TRIANGLE:
    default:
        return MTL::TriangleFillModeFill;
    }
}

MTL::CompareFunction ToMetalCompareFunction(CompareFunc compare_function)
{
    switch (compare_function)
    {
    case CompareFunc::NEVER:
        return MTL::CompareFunctionNever;
    case CompareFunc::LESS:
        return MTL::CompareFunctionLess;
    case CompareFunc::L_EQUAL:
        return MTL::CompareFunctionLessEqual;
    case CompareFunc::EQUAL:
        return MTL::CompareFunctionEqual;
    case CompareFunc::GREATER:
        return MTL::CompareFunctionGreater;
    case CompareFunc::G_EQUAL:
        return MTL::CompareFunctionGreaterEqual;
    case CompareFunc::ALWAYS:
    default:
        return MTL::CompareFunctionAlways;
    }
}

MTL::SamplerMinMagFilter ToMetalFilter(FilterType filter)
{
    return filter == FilterType::FILTER_NEAREST ? MTL::SamplerMinMagFilterNearest : MTL::SamplerMinMagFilterLinear;
}

MTL::SamplerMipFilter ToMetalMipFilter(MipMapMode mode)
{
    return mode == MipMapMode::MIPMAP_MODE_NEAREST ? MTL::SamplerMipFilterNearest : MTL::SamplerMipFilterLinear;
}

MTL::SamplerAddressMode ToMetalAddressMode(AddressMode mode)
{
    switch (mode)
    {
    case AddressMode::ADDRESS_MODE_MIRROR:
        return MTL::SamplerAddressModeMirrorRepeat;
    case AddressMode::ADDRESS_MODE_REPEAT:
        return MTL::SamplerAddressModeRepeat;
    case AddressMode::ADDRESS_MODE_CLAMP_TO_BORDER:
        return MTL::SamplerAddressModeClampToBorderColor;
    case AddressMode::ADDRESS_MODE_CLAMP_TO_EDGE:
    default:
        return MTL::SamplerAddressModeClampToEdge;
    }
}

MTL::TextureUsage ToMetalTextureUsage(DescriptorTypes descriptor_types, RenderTargetType render_target_type)
{
    MTL::TextureUsage usage = MTL::TextureUsageUnknown;
    if ((descriptor_types & DescriptorType::DESCRIPTOR_TYPE_TEXTURE) != 0 ||
        (descriptor_types & DescriptorType::DESCRIPTOR_TYPE_TEXTURE_CUBE) != 0)
    {
        usage |= MTL::TextureUsageShaderRead;
    }
    if ((descriptor_types & DescriptorType::DESCRIPTOR_TYPE_RW_TEXTURE) != 0)
    {
        usage |= MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite;
    }
    if ((descriptor_types & DescriptorType::DESCRIPTOR_TYPE_RENDER_TARGET_MIP_SLICES) != 0 ||
        (descriptor_types & DescriptorType::DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) != 0 ||
        (descriptor_types & DescriptorType::DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES) != 0 ||
        render_target_type == RenderTargetType::COLOR)
    {
        usage |= MTL::TextureUsageRenderTarget;
    }
    if (render_target_type == RenderTargetType::DEPTH_STENCIL)
    {
        usage |= MTL::TextureUsageRenderTarget;
    }
    if (usage == MTL::TextureUsageUnknown)
    {
        usage = MTL::TextureUsageShaderRead;
    }
    return usage;
}

TextureCreateInfo MakeTextureCreateInfoForSwapChain(u32 width, u32 height)
{
    TextureCreateInfo texture_create_info{};
    texture_create_info.descriptor_types = DescriptorType::DESCRIPTOR_TYPE_COLOR_ATTACHMENT;
    texture_create_info.initial_state = ResourceState::RESOURCE_STATE_PRESENT;
    texture_create_info.texture_type = TextureType::TEXTURE_TYPE_2D;
    texture_create_info.texture_format = TextureFormat::TEXTURE_FORMAT_RGBA8_UNORM;
    texture_create_info.width = width;
    texture_create_info.height = height;
    texture_create_info.depth = 1;
    texture_create_info.array_layer = 1;
    texture_create_info.debug_name = "MetalSwapChainTexture";
    return texture_create_info;
}

} // namespace Horizon::Backend::MetalUtils
