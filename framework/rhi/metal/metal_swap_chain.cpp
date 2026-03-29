#include "metal_swap_chain.h"

#include <core/log.h>
#include <core/memory.h>
#include <rhi/metal/metal_render_target.h>
#include <rhi/metal/metal_texture.h>

namespace Horizon::Backend
{

using namespace MetalUtils;

namespace
{

SEL DisplaySyncSelector()
{
    static SEL kSelector = sel_registerName("setDisplaySyncEnabled:");
    return kSelector;
}

} // namespace

MetalSwapChain::MetalSwapChain(const SwapChainCreateInfo &swap_chain_create_info, Window *window) noexcept
    : SwapChain(swap_chain_create_info, window)
{
}

MetalSwapChain::~MetalSwapChain() noexcept
{
    if (m_current_drawable != nil)
    {
        m_current_drawable->release();
        m_current_drawable = nil;
    }
    if (m_layer != nil)
    {
        m_layer->release();
        m_layer = nil;
    }
}

bool MetalSwapChain::Initialize(MTL::Device *device, Window *window) noexcept
{
    id view = ToObjCObject(window->GetNativeView());
    if (view == nil)
    {
        LOG_ERROR("Cannot create Metal swap chain without a native view");
        return false;
    }

    CA::MetalLayer *layer = CA::MetalLayer::layer();
    if (layer == nil)
    {
        LOG_ERROR("Failed to create CAMetalLayer");
        return false;
    }

    layer->retain();
    layer->setDevice(device);
    layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    layer->setFramebufferOnly(true);
    id native_layer = ToObjCObject(layer);
    const CGFloat backing_scale = GetBackingScaleFactor(view);
    SetLayerContentsScale(native_layer, backing_scale);
    layer->setDrawableSize(GetDrawableSize(view));
    if (RespondsToSelector(native_layer, DisplaySyncSelector()))
    {
        layer->setDisplaySyncEnabled(m_enable_vsync);
    }

    SetViewLayer(view, native_layer);
    m_layer = layer;
    width = window->GetWidth();
    height = window->GetHeight();

    render_targets.resize(m_back_buffer_count);
    for (u32 index = 0; index < m_back_buffer_count; ++index)
    {
        auto texture_create_info = MakeTextureCreateInfoForSwapChain(width, height);
        auto *texture = Memory::Alloc<MetalTexture>(texture_create_info, nil, false, true);
        render_targets[index] = Memory::Alloc<MetalRenderTarget>(texture);
    }
    return true;
}

void MetalSwapChain::SetVSyncEnabled(bool enabled) noexcept
{
    SwapChain::SetVSyncEnabled(enabled);
    if (m_layer != nil && RespondsToSelector(ToObjCObject(m_layer), DisplaySyncSelector()))
    {
        m_layer->setDisplaySyncEnabled(enabled);
    }
}

void MetalSwapChain::UpdateDrawableSize(Window *window) noexcept
{
    id view = ToObjCObject(window->GetNativeView());
    if (m_layer == nil || view == nil)
    {
        return;
    }

    const CGSize drawable_size = GetDrawableSize(view);
    SetLayerFrame(ToObjCObject(m_layer), GetViewBounds(view));
    m_layer->setDrawableSize(drawable_size);
    width = static_cast<u32>(drawable_size.width > 0.0 ? drawable_size.width : 0.0);
    height = static_cast<u32>(drawable_size.height > 0.0 ? drawable_size.height : 0.0);
}

} // namespace Horizon::Backend
