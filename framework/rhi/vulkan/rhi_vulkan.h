#pragma once

#include <core/definations.h>
#include <core/path.h>

#include <rhi/enums.h>
#include <rhi/rhi.h>
#include <rhi/vulkan/vulkan_utils.h>

#include <rhi/vulkan/vulkan_buffer.h>
#include <rhi/vulkan/vulkan_config.h>
#include <rhi/vulkan/vulkan_descriptor_set_allocator.h>
#include <rhi/vulkan/vulkan_swap_chain.h>
#include <rhi/vulkan/vulkan_texture.h>

namespace Horizon::Backend
{

class RHIVulkan : public RHI
{
  public:
    RHIVulkan(bool offscreen) noexcept;
    virtual ~RHIVulkan() noexcept;

    RHIVulkan(const RHIVulkan &rhs) noexcept = delete;
    RHIVulkan &operator=(const RHIVulkan &rhs) noexcept = delete;
    RHIVulkan(RHIVulkan &&rhs) noexcept = delete;
    RHIVulkan &operator=(RHIVulkan &&rhs) noexcept = delete;

    void InitializeRenderer() override;
    bool IsInitialized() const override
    {
        return m_initialized;
    }

    Buffer *CreateBuffer(const BufferCreateInfo &buffer_create_info) override;

    Texture *CreateTexture(const TextureCreateInfo &texture_create_info) override;

    RenderTarget *CreateRenderTarget(const RenderTargetCreateInfo &render_target_create_info) override;

    SwapChain *CreateSwapChain(const SwapChainCreateInfo &create_info) override;

    Shader *CreateShader(ShaderType type, const Path &file_name, const char *entry_point) override;

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
    bool SupportsMeshShader() const noexcept override
    {
        return m_mesh_shader_supported;
    }

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

    double QueryResult() override
    {

        std::vector<uint64_t> time_stamps(2);
        vkGetQueryPoolResults(m_vulkan.device, m_vulkan.gpu_query_pool, 0, 2, time_stamps.size() * sizeof(uint64_t),
                              time_stamps.data(), sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
        return float(time_stamps[1] - time_stamps[0]) * m_vulkan.timestampPeriod;
    }

  private:
    VulkanRendererContext m_vulkan{};
    bool m_initialized{false};
    SwapChainSemaphoreContext semaphore_ctx{};
    VulkanDescriptorSetAllocator *m_descriptor_set_allocator{};
    std::array<std::vector<VkFence>, 3> fences{};
    std::array<u32, 3> fence_index{};
    bool m_mesh_shader_supported{false};
};

extern std::unique_ptr<RHI> CreateVulkanRenderBackend(bool offscreen) noexcept;
} // namespace Horizon::Backend
