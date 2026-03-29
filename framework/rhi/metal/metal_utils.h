#pragma once

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
#include <objc/message.h>
#include <objc/runtime.h>

#include <rhi/rhi.h>

namespace Horizon::Backend::MetalUtils
{

NS::String *ToNSString(const std::string &value);

id ToObjCObject(void *object);
id ToObjCObject(const void *object);

template <typename Ret, typename... Args> Ret SendObjCMessage(id object, SEL selector, Args... args)
{
    using SendMessageProc = Ret (*)(id, SEL, Args...);
    return reinterpret_cast<SendMessageProc>(objc_msgSend)(object, selector, args...);
}

template <typename... Args> CGRect SendObjCGeometryMessage(id object, SEL selector, Args... args)
{
#if defined(__x86_64__)
    using SendMessageProc = void (*)(CGRect *, id, SEL, Args...);
    CGRect result{};
    reinterpret_cast<SendMessageProc>(objc_msgSend_stret)(&result, object, selector, args...);
    return result;
#else
    using SendMessageProc = CGRect (*)(id, SEL, Args...);
    return reinterpret_cast<SendMessageProc>(objc_msgSend)(object, selector, args...);
#endif
}

CGRect GetViewBounds(id view);
CGSize GetDrawableSize(id view);
CGFloat GetBackingScaleFactor(id view);
bool RespondsToSelector(id object, SEL selector);
void SetViewLayer(id view, id layer);
void SetLayerContentsScale(id layer, CGFloat scale);
void SetLayerFrame(id layer, CGRect frame);

const char *GetNSErrorDescription(NS::Error *error);

MTL::PixelFormat ToMetalPixelFormat(TextureFormat format, bool for_swap_chain = false);
MTL::VertexFormat ToMetalVertexFormat(VertexAttribFormat format, u32 portion);
MTL::PrimitiveType ToMetalPrimitiveType(PrimitiveTopology topology);
MTL::Winding ToMetalWinding(FrontFace front_face);
MTL::CullMode ToMetalCullMode(CullMode cull_mode);
MTL::TriangleFillMode ToMetalFillMode(FillMode fill_mode);
MTL::CompareFunction ToMetalCompareFunction(CompareFunc compare_function);
MTL::SamplerMinMagFilter ToMetalFilter(FilterType filter);
MTL::SamplerMipFilter ToMetalMipFilter(MipMapMode mode);
MTL::SamplerAddressMode ToMetalAddressMode(AddressMode mode);
MTL::TextureUsage ToMetalTextureUsage(DescriptorTypes descriptor_types,
                                      RenderTargetType render_target_type = RenderTargetType::UNDEFINED);
TextureCreateInfo MakeTextureCreateInfoForSwapChain(u32 width, u32 height);

} // namespace Horizon::Backend::MetalUtils
