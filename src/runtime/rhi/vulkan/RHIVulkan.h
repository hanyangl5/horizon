#pragma once

#include <runtime/core/log/Log.h>
#include <runtime/core/utils/definations.h>

#include <runtime/rhi/RHI.h>
#include <runtime/rhi/RHIUtils.h>
#include <runtime/rhi/vulkan/VulkanUtils.h>

#include <runtime/rhi/vulkan/VulkanBuffer.h>
#include <runtime/rhi/vulkan/VulkanConfig.h>
#include <runtime/rhi/vulkan/VulkanDescriptorSetAllocator.h>
#include <runtime/rhi/vulkan/VulkanSwapChain.h>
#include <runtime/rhi/vulkan/VulkanTexture.h>

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

    Shader *CreateShader(ShaderType type, u32 compile_flags, const std::filesystem::path &file_name) override;

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
    void InitializeVulkanRenderer(const std::string &app_name);
    void CreateInstance(const std::string &app_name, std::vector<const char *> &instance_layers,
                        std::vector<const char *> &instance_extensions);
    void PickGPU(VkInstance instance, VkPhysicalDevice *gpu);
    void CreateDevice(std::vector<const char *> &device_extensions);
    void InitializeVMA();
    void CreateSyncObjects();
    void DestroySwapChain();
    VkFence GetFence(CommandQueueType type) noexcept;
    void CreateGpuQueryPool();

    double QueryResult() override {

        std::vector<uint64_t> time_stamps(2);
        vkGetQueryPoolResults(m_vulkan.device, m_vulkan.gpu_query_pool, 0, 2, time_stamps.size() * sizeof(uint64_t),
                              time_stamps.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        return float(time_stamps[1] - time_stamps[0]) * m_vulkan.timestampPeriod;
    }

  private:
    VulkanRendererContext m_vulkan{};
    SwapChainSemaphoreContext semaphore_ctx{};
    VulkanDescriptorSetAllocator *m_descriptor_set_allocator{};
    std::array<std::vector<VkFence>, 3> fences{};
    std::array<u32, 3> fence_index{};
};
} // namespace Horizon::Backend
