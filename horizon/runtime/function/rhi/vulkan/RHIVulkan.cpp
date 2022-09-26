#include <filesystem>
#include <memory>
#include <thread>

#define VMA_IMPLEMENTATION

#include <vulkan/vulkan.hpp>

#include <runtime/function/rhi/vulkan/RHIVulkan.h>

#include <runtime/function/rhi/vulkan/VulkanBuffer.h>
#include <runtime/function/rhi/vulkan/VulkanCommandContext.h>
#include <runtime/function/rhi/vulkan/VulkanPipeline.h>
#include <runtime/function/rhi/vulkan/VulkanRenderTarget.h>
#include <runtime/function/rhi/vulkan/VulkanSampler.h>
#include <runtime/function/rhi/vulkan/VulkanSemaphore.h>
#include <runtime/function/rhi/vulkan/VulkanShader.h>
#include <runtime/function/rhi/vulkan/VulkanTexture.h>

namespace Horizon::RHI {

RHIVulkan::RHIVulkan() noexcept {}

RHIVulkan::~RHIVulkan() noexcept {

    vkWaitForFences(m_vulkan.device, m_vulkan.fences.size(), m_vulkan.fences.data(), VK_TRUE, UINT64_MAX);

    for (auto &fence : m_vulkan.fences) {
        vkDestroyFence(m_vulkan.device, fence, nullptr);
    }

    thread_command_context = nullptr;

    m_descriptor_set_allocator = nullptr; // release

    vmaDestroyAllocator(m_vulkan.vma_allocator);
    vkDestroyDevice(m_vulkan.device, nullptr);
    vkDestroyInstance(m_vulkan.instance, nullptr);
}

void RHIVulkan::InitializeRenderer() {
    LOG_DEBUG("using vulkan renderer");
    InitializeVulkanRenderer("vulkan renderer");
}

Resource<Buffer> RHIVulkan::CreateBuffer(const BufferCreateInfo &buffer_create_info) {

    return std::make_unique<VulkanBuffer>(m_vulkan, buffer_create_info, MemoryFlag::DEDICATE_GPU_MEMORY);
}

Resource<Texture> RHIVulkan::CreateTexture(const TextureCreateInfo &texture_create_info) {
    return std::make_unique<VulkanTexture>(m_vulkan, texture_create_info);
}

Resource<RenderTarget> RHIVulkan::CreateRenderTarget(const RenderTargetCreateInfo &render_target_create_info) {
    return std::make_unique<VulkanRenderTarget>(m_vulkan, render_target_create_info);
}

Resource<SwapChain> RHIVulkan::CreateSwapChain(const SwapChainCreateInfo& create_info) {
    return std::make_unique<VulkanSwapChain>(m_vulkan, create_info, m_window);
}

Shader *RHIVulkan::CreateShader(ShaderType type, u32 compile_flags, const std::filesystem::path &file_name) {
    auto _file_name = file_name.parent_path() / "bin" / "VULKAN" / file_name.filename();
    auto spirv_code = ReadFile(_file_name.generic_string().c_str());
    return new VulkanShader(m_vulkan, type, spirv_code);
}

void RHIVulkan::DestroyShader(Shader *shader_program) {
    if (shader_program) {
        delete shader_program;
    } else {
        LOG_WARN("shader program is uninitialized or deleted");
    }
}

void RHIVulkan::InitializeVulkanRenderer(const std::string &app_name) {
    std::vector<const char *> instance_layers{};
    std::vector<const char *> instance_extensions{};
    std::vector<const char *> device_extensions{};
#ifndef NDEBUG
    instance_layers.emplace_back("VK_LAYER_KHRONOS_validation");
    instance_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    instance_extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif
    instance_extensions.emplace_back("VK_KHR_surface");
    instance_extensions.emplace_back("VK_KHR_win32_surface");
    device_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    device_extensions.emplace_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
    device_extensions.emplace_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    device_extensions.emplace_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);

    CreateInstance(app_name, instance_layers, instance_extensions);
    CreateDevice(device_extensions);
    InitializeVMA();
    // create sync objects
    CreateSyncObjects();
    m_descriptor_set_allocator = std::make_unique<VulkanDescriptorSetAllocator>(m_vulkan);
}

void RHIVulkan::CreateInstance(const std::string &app_name, std::vector<const char *> &instance_layers,
                               std::vector<const char *> &instance_extensions) {

    u32 layer_count{0}, extension_count{0};
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    std::vector<VkExtensionProperties> available_extensions(extension_count);

    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions.data());

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = app_name.data();
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Horizon Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VULKAN_API_VERSION;

    VkInstanceCreateInfo instance_create_info{};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;
    instance_create_info.flags = 0;
    instance_create_info.enabledExtensionCount = static_cast<u32>(instance_extensions.size());
    instance_create_info.ppEnabledExtensionNames = instance_extensions.data();
    instance_create_info.enabledLayerCount = static_cast<u32>(instance_layers.size());
    instance_create_info.ppEnabledLayerNames = instance_layers.data();

    CHECK_VK_RESULT(vkCreateInstance(&instance_create_info, nullptr, &(m_vulkan.instance)));
}

void RHIVulkan::PickGPU(VkInstance instance, VkPhysicalDevice *gpu) {
    u32 device_count{0};
    std::vector<VkPhysicalDevice> physical_devices{};
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    physical_devices.resize(device_count);
    if (device_count == 0) {
        LOG_ERROR("no available device");
    }
    vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data());

    // pick gpu

    for (const auto &physical_device : physical_devices) {
        u32 queue_family_count = m_vulkan.command_queues.size();
        std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count,
                                                 nullptr); // Get queue family properties
        if (queue_family_count != 3) {
            continue;
        }
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count,
                                                 queue_family_properties.data()); // Get queue family properties

        for (u32 i = 0; i < queue_family_count; i++) {
            // TOOD: print gpu info, runtime swith gpu

            if (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
                queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
                queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                *gpu = physical_device;
                m_vulkan.command_queue_familiy_indices[CommandQueueType::GRAPHICS] = i;
                // break;
            }
            // dedicate compute queue
            if (!(queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
                queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                m_vulkan.command_queue_familiy_indices[CommandQueueType::COMPUTE] = i;
            }
            // dedicate transfer queue
            if (!(queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
                queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
                !(queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                m_vulkan.command_queue_familiy_indices[CommandQueueType::TRANSFER] = i;
            }
        }
        if (gpu != VK_NULL_HANDLE) {
            break;
        }
    }
    if (gpu == VK_NULL_HANDLE) {
        LOG_ERROR("no suitable gpu found");
    }
}

void RHIVulkan::CreateDevice(std::vector<const char *> &device_extensions) {
    PickGPU(m_vulkan.instance, &m_vulkan.active_gpu);

    VkPhysicalDeviceDynamicRenderingFeatures dyanmic_rendering_features{};
    dyanmic_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dyanmic_rendering_features.dynamicRendering = true;

    VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features{};
    descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    descriptor_indexing_features.pNext = &dyanmic_rendering_features;
    descriptor_indexing_features.runtimeDescriptorArray = VK_TRUE;
    descriptor_indexing_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    descriptor_indexing_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

    std::vector<VkDeviceQueueCreateInfo> device_queue_create_info(m_vulkan.command_queues.size());

    f32 queue_priority = 1.0f;

    for (u32 i = 0; i < device_queue_create_info.size(); i++) {
        device_queue_create_info[i] = {};
        device_queue_create_info[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        device_queue_create_info[i].pNext = NULL;
        device_queue_create_info[i].flags = 0;
        device_queue_create_info[i].queueFamilyIndex = m_vulkan.command_queue_familiy_indices[i];
        device_queue_create_info[i].queueCount = 1;
        device_queue_create_info[i].pQueuePriorities = &queue_priority;
    }

    VkPhysicalDeviceFeatures2 device_features{};
    device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    // device_features.features = &requested_descriptor_indexing;
    device_features.pNext = &descriptor_indexing_features;
    vkGetPhysicalDeviceFeatures2(m_vulkan.active_gpu, &device_features);
    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos = device_queue_create_info.data();
    device_create_info.queueCreateInfoCount = static_cast<u32>(device_queue_create_info.size());
    device_create_info.enabledExtensionCount = static_cast<u32>(device_extensions.size());
    device_create_info.ppEnabledExtensionNames = device_extensions.data();
    device_create_info.pNext = &device_features;

    CHECK_VK_RESULT(vkCreateDevice(m_vulkan.active_gpu, &device_create_info, nullptr, &m_vulkan.device));

    // retrive command queue

    vkGetDeviceQueue(m_vulkan.device, m_vulkan.command_queue_familiy_indices[CommandQueueType::GRAPHICS], 0,
                     &m_vulkan.command_queues[CommandQueueType::GRAPHICS]);
    // vkGetDeviceQueue(m_vulkan.device, m_vulkan.graphics_queue_family_index,
    // 0, &m_vulkan.m_present_queue);
    vkGetDeviceQueue(m_vulkan.device, m_vulkan.command_queue_familiy_indices[CommandQueueType::TRANSFER], 0,
                     &m_vulkan.command_queues[CommandQueueType::TRANSFER]);
    vkGetDeviceQueue(m_vulkan.device, m_vulkan.command_queue_familiy_indices[CommandQueueType::COMPUTE], 0,
                     &m_vulkan.command_queues[CommandQueueType::COMPUTE]);

#ifdef USE_ASYNC_COMPUTE_TRANSFER
    LOG_DEBUG("using async compute & transfer, graphics queue: {}, compute "
              "queue: {}, transfer queue: {}",
              m_vulkan.command_queue_familiy_indices[CommandQueueType::GRAPHICS],
              m_vulkan.command_queue_familiy_indices[CommandQueueType::COMPUTE],
              m_vulkan.command_queue_familiy_indices[CommandQueueType::TRANSFER]);
#endif // USE_ASYNC_COMPUTE
}

void RHIVulkan::InitializeVMA() {
    VmaVulkanFunctions vulkan_functions{};
    vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo vma_create_info = {0};
    vma_create_info.device = m_vulkan.device;
    vma_create_info.physicalDevice = m_vulkan.active_gpu;
    vma_create_info.instance = m_vulkan.instance;
    vma_create_info.pVulkanFunctions = &vulkan_functions;
    vma_create_info.vulkanApiVersion = VULKAN_API_VERSION;
    CHECK_VK_RESULT(vmaCreateAllocator(&vma_create_info, &m_vulkan.vma_allocator));
}

void RHIVulkan::CreateSyncObjects() {
    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    for (auto &fence : m_vulkan.fences) {
        CHECK_VK_RESULT(vkCreateFence(m_vulkan.device, &fence_create_info, nullptr, &fence));
    }
}



void RHIVulkan::SubmitCommandLists(const QueueSubmitInfo &queue_submit_info) {

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    // submit command buffers
    // VkCommandBufferBeginInfo begin_info{};
    // begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    // begin_info.pInheritanceInfo = nullptr;

    // vkBeginCommandBuffer(m_vulkan.command_buffers[CommandQueueType::GRAPHICS],
    // &begin_info); for (auto command_list : *command_list)
    //{
    //	command_list->Execute(m_vulkan.command_buffers[CommandQueueType::GRAPHICS]);
    // }
    // vkEndCommandBuffer(m_vulkan.command_buffers[CommandQueueType::GRAPHICS]);
    auto &command_lists = queue_submit_info.command_lists;
    std::vector<VkCommandBuffer> command_buffers(command_lists.size());
    for (u32 i = 0; i < command_lists.size(); i++) {
        command_buffers[i] = reinterpret_cast<VulkanCommandList *>(command_lists[i])->m_command_buffer;
        // valid command list type when submitting
    }
    submit_info.commandBufferCount = static_cast<u32>(command_buffers.size());
    submit_info.pCommandBuffers = command_buffers.data();


    u32 wait_semaphore_count = queue_submit_info.wait_semaphores.size();
    std::vector<VkSemaphore> wait_semaphores(wait_semaphore_count);
    std::vector<VkPipelineStageFlags> wait_stages(wait_semaphore_count);
    for (u32 i = 0; i < wait_semaphore_count; i++) {
        wait_semaphores[i] = reinterpret_cast<VulkanSemaphore *>(queue_submit_info.wait_semaphores[i])->m_semaphore;
        wait_stages[i] = queue_submit_info.wait_semaphores[i]->GetWaitStage();
    }

    // signal render complete semaphore
    if (queue_submit_info.wait_image_acquired == true) {
        wait_semaphores.push_back(
            std::reinterpret_pointer_cast<VulkanSemaphore>(semaphore_ctx.swap_chain_acquire_semaphore)
                .get()
                ->m_semaphore);
        wait_stages.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }

    u32 signal_semaphore_count = queue_submit_info.signal_semaphores.size();
    std::vector<VkSemaphore> signal_semaphores(signal_semaphore_count);

    for (u32 i = 0; i < signal_semaphore_count; i++) {
        signal_semaphores[i] = reinterpret_cast<VulkanSemaphore *>(queue_submit_info.signal_semaphores[i])->m_semaphore;
        queue_submit_info.signal_semaphores[i]->AddWaitStage(queue_submit_info.queue_type);
    }

    // signal render complete semaphore
    if (queue_submit_info.signal_render_complete == true) {

        if (!semaphore_ctx.swap_chain_release_semaphore) {
            semaphore_ctx.swap_chain_release_semaphore = std::move(GetSemaphore());
        }

        signal_semaphores.push_back(
            std::reinterpret_pointer_cast<VulkanSemaphore>(semaphore_ctx.swap_chain_release_semaphore)
                .get()
                ->m_semaphore);
        wait_stages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT); // correct stage?
    }

    submit_info.waitSemaphoreCount = static_cast<u32>(wait_semaphores.size());
    submit_info.pWaitSemaphores = wait_semaphores.data();

    submit_info.signalSemaphoreCount = static_cast<u32>(signal_semaphores.size());
    submit_info.pSignalSemaphores = signal_semaphores.data();

    submit_info.pWaitDstStageMask = wait_stages.data();
    auto &fence = m_vulkan.fences[queue_submit_info.queue_type];

    vkQueueSubmit(m_vulkan.command_queues[queue_submit_info.queue_type], 1, &submit_info, fence);
}

void RHIVulkan::Present(const QueuePresentInfo &queue_present_info) {

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    //u32 wait_semaphore_count = queue_present_info.wait_semaphores.size();
    //std::vector<VkSemaphore> wait_semaphores(wait_semaphore_count);

    //for (u32 i = 0; i < wait_semaphore_count; i++) {
    //    wait_semaphores[i] = reinterpret_cast<VulkanSemaphore *>(queue_present_info.wait_semaphores[i])->m_semaphore;
    //}
    std::vector<VkSemaphore> wait_semaphores{};
    wait_semaphores.push_back(
        std::reinterpret_pointer_cast<VulkanSemaphore>(semaphore_ctx.swap_chain_release_semaphore).get()->m_semaphore);

    present_info.waitSemaphoreCount = static_cast<u32>(wait_semaphores.size());
    present_info.pWaitSemaphores = wait_semaphores.data();

    auto vk_swap_chain = reinterpret_cast<VulkanSwapChain *>(queue_present_info.swap_chain);

    VkSwapchainKHR swapChains[] = {vk_swap_chain->swap_chain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapChains;
    present_info.pImageIndices = &vk_swap_chain->image_index;

    CHECK_VK_RESULT(vkQueuePresentKHR(m_vulkan.command_queues[CommandQueueType::GRAPHICS], &present_info));
    vk_swap_chain->current_frame_index++;
    vk_swap_chain->current_frame_index = vk_swap_chain->current_frame_index % vk_swap_chain->m_back_buffer_count;
}

void RHIVulkan::AcquireNextFrame(SwapChain *swap_chain) {

    auto vk_swap_chain = reinterpret_cast<VulkanSwapChain *>(swap_chain);

    std::shared_ptr<Semaphore> sm;
    if (semaphore_ctx.recycled_semaphores.empty()) {
        sm = std::move(GetSemaphore());

    } else {
        sm = semaphore_ctx.recycled_semaphores.back();
        semaphore_ctx.recycled_semaphores.pop_back();
    }

    VkResult res = vkAcquireNextImageKHR(m_vulkan.device, vk_swap_chain->swap_chain, UINT64_MAX,
                                         std::reinterpret_pointer_cast<VulkanSemaphore>(sm)->m_semaphore, nullptr,
                                         &vk_swap_chain->image_index);

    if (res != VK_SUCCESS) {
        semaphore_ctx.recycled_semaphores.push_back(sm);
        LOG_ERROR("failed to acqurei next image");
        if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
            //resize(context.swapchain_dimensions.width, context.swapchain_dimensions.height);
            //res = acquire_next_image(context, &index);
        }

        if (res != VK_SUCCESS) {
            //vkQueueWaitIdle(context.queue);
            //return;
        }
    }

    // RESET RHI RESOURCES
    {

        WaitGpuExecution(CommandQueueType::GRAPHICS);
        WaitGpuExecution(CommandQueueType::COMPUTE);
        WaitGpuExecution(CommandQueueType::TRANSFER);
        ResetFence(CommandQueueType::GRAPHICS);
        ResetFence(CommandQueueType::COMPUTE);
        ResetFence(CommandQueueType::TRANSFER);
        ResetRHIResources();
    }


    // Recycle the old semaphore back into the semaphore manager.
    auto old_semaphore = std::reinterpret_pointer_cast<VulkanSemaphore>(semaphore_ctx.swap_chain_acquire_semaphore);

    if (old_semaphore != nullptr && old_semaphore->m_semaphore != VK_NULL_HANDLE) {
        semaphore_ctx.recycled_semaphores.push_back(old_semaphore);
    }

    semaphore_ctx.swap_chain_acquire_semaphore = sm;
}

CommandList *RHIVulkan::GetCommandList(CommandQueueType type) {

    if (!thread_command_context) {
        thread_command_context = std::make_unique<VulkanCommandContext>(m_vulkan);
    }

    return thread_command_context->GetCommandList(type);
}

void RHIVulkan::WaitGpuExecution(CommandQueueType queue_type) {
    vkWaitForFences(m_vulkan.device, 1, &m_vulkan.fences[queue_type], VK_TRUE, UINT64_MAX);
}

void RHIVulkan::ResetFence(CommandQueueType queue_type) {
    vkResetFences(m_vulkan.device, 1, &m_vulkan.fences[queue_type]);
}

void RHIVulkan::ResetRHIResources() {
    if (thread_command_context) {
        thread_command_context->Reset();
    }
    m_descriptor_set_allocator->ResetDescriptorPool();
}

Pipeline *RHIVulkan::CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info) {
    return new VulkanPipeline(m_vulkan, create_info, *m_descriptor_set_allocator.get());
}

Pipeline *RHIVulkan::CreateComputePipeline(const ComputePipelineCreateInfo &create_info) {
    return new VulkanPipeline(m_vulkan, create_info, *m_descriptor_set_allocator.get());
}

void RHIVulkan::DestroyPipeline(Pipeline *pipeline) { delete pipeline; }

Resource<Semaphore> RHIVulkan::GetSemaphore() { return std::make_unique<VulkanSemaphore>(m_vulkan); }

Resource<Sampler> RHIVulkan::GetSampler(const SamplerDesc &sampler_desc) {
    return std::make_unique<VulkanSampler>(m_vulkan, sampler_desc);
}

} // namespace Horizon::RHI
