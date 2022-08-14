#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <runtime/core/log/Log.h>
#include <runtime/core/utils/Definations.h>
#include <runtime/function/rhi/RHIInterface.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/vulkan/VulkanBuffer.h>
#include <runtime/function/rhi/vulkan/VulkanConfig.h>
#include <runtime/function/rhi/vulkan/VulkanDescriptorSetManager.h>
#include <runtime/function/rhi/vulkan/VulkanTexture.h>
#include <runtime/function/rhi/vulkan/VulkanUtils.h>


#include "vk_mem_alloc.h"

namespace Horizon::RHI {

class RHIVulkan : public RHIInterface {
  public:
    RHIVulkan() noexcept;
    virtual ~RHIVulkan() noexcept;

    void InitializeRenderer() noexcept override;

    Resource<Buffer>
    CreateBuffer(const BufferCreateInfo &buffer_create_info) noexcept override;

    Resource<Texture> CreateTexture(
        const TextureCreateInfo &texture_create_info) noexcept override;

    void CreateSwapChain(Window *window) noexcept override;

    ShaderProgram *CreateShaderProgram(ShaderType type,
                                       const std::string &entry_point,
                                       u32 compile_flags,
                                       std::string file_name) noexcept override;

    void DestroyShaderProgram(ShaderProgram *shader_program) noexcept override;

    CommandList *GetCommandList(CommandQueueType type) noexcept override;

    void WaitGpuExecution(CommandQueueType queue_type) noexcept override;

    void ResetCommandResources() noexcept override;

    Pipeline *CreatePipeline(
        const PipelineCreateInfo &pipeline_create_info) noexcept override;

    // submit command list to command queue
    virtual void SubmitCommandLists(
        CommandQueueType queue_type,
        std::vector<CommandList *> &command_lists) noexcept override;
    void SetResource(Buffer *buffer, Pipeline *pipeline, u32 set,
                     u32 binding) noexcept override;
    void SetResource(Texture *texture) noexcept override;

    void UpdateDescriptors() noexcept override;

  private:
    void InitializeVulkanRenderer(const std::string &app_name) noexcept;
    void
    CreateInstance(const std::string &app_name,
                   std::vector<const char *> &instance_layers,
                   std::vector<const char *> &instance_extensions) noexcept;
    void PickGPU(VkInstance instance, VkPhysicalDevice *gpu) noexcept;
    void CreateDevice(std::vector<const char *> &device_extensions) noexcept;
    void InitializeVMA() noexcept;
    void CreateSyncObjects() noexcept;
    void DestroySwapChain() noexcept;

  private:
    VulkanRendererContext m_vulkan{};
    std::unique_ptr<VulkanDescriptorSetManager> m_descriptor_set_manager = nullptr;
    // pipeline map
    // resource manager, auto
};
} // namespace Horizon::RHI
