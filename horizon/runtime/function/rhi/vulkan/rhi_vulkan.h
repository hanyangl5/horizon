#pragma once

#include "vk_mem_alloc.h"

#include <runtime/core/log/log.h>
#include <runtime/core/utils/definations.h>

#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/rhi_utils.h>
#include <runtime/function/rhi/vulkan/vulkan_utils.h>

#include <runtime/function/rhi/vulkan/vulkan_buffer.h>
#include <runtime/function/rhi/vulkan/vulkan_config.h>
#include <runtime/function/rhi/vulkan/vulkan_descriptor_set_allocator.h>
#include <runtime/function/rhi/vulkan/vulkan_swap_chain.h>
#include <runtime/function/rhi/vulkan/vulkan_texture.h>

namespace Horizon::Backend {

class RHIVulkan : public RHI {
  public:
    RHIVulkan(bool offscreen) noexcept;
    virtual ~RHIVulkan() noexcept;

    RHIVulkan(const RHIVulkan &rhs) noexcept = delete;
    RHIVulkan &operator=(const RHIVulkan &rhs) noexcept = delete;
    RHIVulkan(RHIVulkan &&rhs) noexcept = delete;
    RHIVulkan &operator=(RHIVulkan &&rhs) noexcept = delete;

    void InitializeRenderer() override;

    Buffer *CreateBuffer(const BufferCreateInfo &buffer_create_info) override;

    Texture *CreateTexture(const TextureCreateInfo &texture_create_info) override;

    RenderTarget *CreateRenderTarget(const RenderTargetCreateInfo &render_target_create_info) override;

    SwapChain *CreateSwapChain(const SwapChainCreateInfo &create_info) override;

    Shader *CreateShader(ShaderType type, u32 compile_flags, const std::filesystem::path &file_name);

    void DestroyShader(Shader *shader_program) override;

    CommandList *GetCommandList(CommandQueueType type) override;

    void WaitGpuExecution(CommandQueueType queue_type) override;

    void ResetRHIResources() override;

    void ResetFence(CommandQueueType queue_type) override;

    Pipeline *CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info) override;

    Pipeline *CreateComputePipeline(const ComputePipelineCreateInfo &create_info) override;

    void DestroyPipeline(Pipeline *pipeline) override;

    Semaphore *CreateSemaphore1() override;

    Sampler *CreateSampler(const SamplerDesc &sampler_desc) override;

    void DestroyBuffer(Buffer *buffer) override;

    void DestroyTexture(Texture *texture) override;

    void DestroyRenderTarget(RenderTarget *render_target) override;

    void DestroySwapChain(SwapChain *swap_chain) override;

    void DestroySemaphore(Semaphore *semaphore) override;

    void DestroySampler(Sampler *sampler) override;

    // submit command list to command queue
    void SubmitCommandLists(const QueueSubmitInfo &queue_submit_info) override;

    void Present(const QueuePresentInfo &quue_present_info) override;
    void AcquireNextFrame(SwapChain *swap_chain) override;

  private:
    void InitializeVulkanRenderer(const Container::String &app_name);
    void CreateInstance(const Container::String &app_name, Container::Array<const char *> &instance_layers,
                        Container::Array<const char *> &instance_extensions);
    void PickGPU(VkInstance instance, VkPhysicalDevice *gpu);
    void CreateDevice(Container::Array<const char *> &device_extensions);
    void InitializeVMA();
    void CreateSyncObjects();
    void DestroySwapChain();
    VkFence GetFence(CommandQueueType type) noexcept;

  private:
    VulkanRendererContext m_vulkan{};
    SwapChainSemaphoreContext semaphore_ctx{};
    VulkanDescriptorSetAllocator *m_descriptor_set_allocator{};
    Container::FixedArray<Container::Array<VkFence>, 3> fences{};
    Container::FixedArray<u32, 3> fence_index{};
};
} // namespace Horizon::Backend
