#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <runtime/core/log/Log.h>
#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/vulkan/VulkanBuffer.h>
#include <runtime/function/rhi/vulkan/VulkanConfig.h>
#include <runtime/function/rhi/vulkan/VulkanDescriptorSetManager.h>
#include <runtime/function/rhi/vulkan/VulkanTexture.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>

#include "vk_mem_alloc.h"

namespace Horizon::RHI {

class RHIVulkan : public RHI {
  public:
    RHIVulkan() noexcept;
    virtual ~RHIVulkan() noexcept;
    RHIVulkan(const RHIVulkan &rhs) noexcept = delete;
    RHIVulkan &operator=(const RHIVulkan &rhs) noexcept = delete;
    RHIVulkan(RHIVulkan &&rhs) noexcept = delete;
    RHIVulkan &operator=(RHIVulkan &&rhs) noexcept = delete;

    void InitializeRenderer() override;

    Resource<Buffer> CreateBuffer(const BufferCreateInfo &buffer_create_info) override;

    Resource<Texture> CreateTexture(const TextureCreateInfo &texture_create_info) override;

    Resource<RenderTarget> CreateRenderTarget(const RenderTargetCreateInfo &render_target_create_info) override;

    void CreateSwapChain(Window *window) override;

    Shader *CreateShader(ShaderType type, u32 compile_flags, const std::filesystem::path& file_name) override;

    void DestroyShader(Shader *shader_program) override;

    CommandList *GetCommandList(CommandQueueType type) override;

    void WaitGpuExecution(CommandQueueType queue_type) override;

    void ResetRHIResources() override;

    void ResetFence(CommandQueueType queue_type) override;

    Pipeline *CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info) override;

    Pipeline *CreateComputePipeline(const ComputePipelineCreateInfo &create_info) override;

    void DestroyPipeline(Pipeline *pipeline) override;

    Resource<Semaphore> GetSemaphore() override;

    Resource<Sampler> GetSampler(const SamplerDesc &sampler_desc) override;

    // submit command list to command queue
    void SubmitCommandLists(const QueueSubmitInfo &queue_submit_info) override;

    void AcquireNextImage(Semaphore* image_acquired_semaphore, u32 swap_chain_image_index) override;
    void Present(const QueuePresentInfo& quue_present_info) override;

  private:
    void InitializeVulkanRenderer(const std::string &app_name);
    void CreateInstance(const std::string &app_name, std::vector<const char *> &instance_layers,
                        std::vector<const char *> &instance_extensions);
    void PickGPU(VkInstance instance, VkPhysicalDevice *gpu);
    void CreateDevice(std::vector<const char *> &device_extensions);
    void InitializeVMA();
    void CreateSyncObjects();
    void DestroySwapChain();

  private:
    VulkanRendererContext m_vulkan{};
    std::unique_ptr<VulkanDescriptorSetManager> m_descriptor_set_manager = nullptr;
    // pipeline map
    // resource manager, auto
};
} // namespace Horizon::RHI
