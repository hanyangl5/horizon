#include "rhi_vulkan.h"

#include <core/path.h>
#include <thread>
#include <volk.h>

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include <core/memory.h>
#include <rhi/vulkan/vulkan_buffer.h>
#include <rhi/vulkan/vulkan_command_context.h>
#include <rhi/vulkan/vulkan_pipeline.h>
#include <rhi/vulkan/vulkan_render_target.h>
#include <rhi/vulkan/vulkan_sampler.h>
#include <rhi/vulkan/vulkan_semaphore.h>
#include <rhi/vulkan/vulkan_shader.h>
#include <rhi/vulkan/vulkan_shader_compiler.h>
#include <rhi/vulkan/vulkan_texture.h>

namespace Horizon::Backend
{

std::unique_ptr<RHI> CreateVulkanRenderBackend(bool offscreen) noexcept
{
    return std::make_unique<RHIVulkan>(offscreen);
}

RHIVulkan::RHIVulkan(bool offscreen) noexcept
{
    m_offscreen = offscreen;
}

RHIVulkan::~RHIVulkan() noexcept
{
    for (auto &type : fences)
    {
        if (type.empty())
            continue;
        vkWaitForFences(m_vulkan.device, (u32)type.size(), type.data(), VK_TRUE, UINT64_MAX);
        for (auto fence : type)
        {
            vkDestroyFence(m_vulkan.device, fence, nullptr);
        }
    }
    vkQueueWaitIdle(m_vulkan.command_queues[CommandQueueType::GRAPHICS]);
    // vkQueueWaitIdle(m_vulkan.command_queues[CommandQueueType::COMPUTE]);
    // vkQueueWaitIdle(m_vulkan.command_queues[CommandQueueType::TRANSFER]);

    Memory::Free(thread_command_context);
    thread_command_context = nullptr;

    Memory::Free(m_descriptor_set_allocator);
    m_descriptor_set_allocator = nullptr; // release

    for (auto &s : semaphore_ctx.render_complete_semaphore)
    {
        if (s != nullptr)
        {
            vkDestroySemaphore(m_vulkan.device, reinterpret_cast<VulkanSemaphore *>(s)->m_semaphore, nullptr);
            s = nullptr;
        }
    }
    for (auto &s : semaphore_ctx.present_complete_semaphore)
    {
        if (s != nullptr)
        {
            vkDestroySemaphore(m_vulkan.device, reinterpret_cast<VulkanSemaphore *>(s)->m_semaphore, nullptr);
            s = nullptr;
        }
    }

    vkDestroyQueryPool(m_vulkan.device, m_vulkan.gpu_query_pool, nullptr);
    vmaDestroyAllocator(m_vulkan.vma_allocator);
    vkDestroyDevice(m_vulkan.device, nullptr);
    vkDestroyInstance(m_vulkan.instance, nullptr);
}

void RHIVulkan::InitializeRenderer()
{
    LOG_DEBUG("using vulkan renderer");
    InitializeVulkanRenderer("vulkan renderer");
}

Buffer *RHIVulkan::CreateBuffer(const BufferCreateInfo &buffer_create_info)
{
    return Memory::Alloc<VulkanBuffer>(m_vulkan, buffer_create_info, MemoryFlag::DEDICATE_GPU_MEMORY);
}

Texture *RHIVulkan::CreateTexture(const TextureCreateInfo &texture_create_info)
{
    return Memory::Alloc<VulkanTexture>(m_vulkan, texture_create_info);
}

RenderTarget *RHIVulkan::CreateRenderTarget(const RenderTargetCreateInfo &render_target_create_info)
{
    return Memory::Alloc<VulkanRenderTarget>(m_vulkan, render_target_create_info);
}

SwapChain *RHIVulkan::CreateSwapChain(const SwapChainCreateInfo &create_info)
{
    SwapChain *sc = Memory::Alloc<VulkanSwapChain>(m_vulkan, create_info, m_window);
    semaphore_ctx.present_complete_semaphore.resize(create_info.back_buffer_count);
    semaphore_ctx.render_complete_semaphore.resize(create_info.back_buffer_count);
    for (u32 i = 0; i < create_info.back_buffer_count; i++)
    {
        semaphore_ctx.present_complete_semaphore[i] = CreateSemaphore1();
        semaphore_ctx.render_complete_semaphore[i] = CreateSemaphore1();
    }
    return sc;
}

Shader *RHIVulkan::CreateShader(ShaderType type, const Path &file_name, const char *entry_point)
{
    Path file_path(file_name);

    // Determine shader directory and saved shader directory
    // Try to use macros first (if available from samples), otherwise infer from file_name
    Path shader_dir;
    Path saved_shader_dir;

#ifdef SHADER_DIR
    shader_dir = Path(SHADER_DIR);
#else
    // Infer from file_name: assume file_name is relative to shader source directory
    // If absolute, use parent; if relative, we'll need to resolve it
    if (file_path.is_absolute())
    {
        shader_dir = file_path.parent_path();
    }
    else
    {
        // For relative paths, try to find the shader directory
        // Look for common patterns: .../shaders/... or .../source/shaders/...
        shader_dir = file_path.parent_path();
    }
#endif

#ifdef SAVED_SHADER_DIR
    saved_shader_dir = Path(SAVED_SHADER_DIR);
#else
    // Infer saved shader directory: look for "saved/shaders" relative to shader_dir
    // Or try "bin/VULKAN" for backward compatibility
    Path parent = shader_dir.parent_path();
    Path saved_path = parent / "saved" / "shaders";
    if (saved_path.exists())
    {
        saved_shader_dir = saved_path;
    }
    else
    {
        // Default: create saved/shaders in shader_dir's parent
        saved_shader_dir = saved_path;
    }
#endif

    auto shader_type_to_extstr = [](ShaderType type) -> std::string {
        switch (type)
        {
        case Horizon::ShaderType::VERTEX_SHADER:
            return "vs";
            break;
        case Horizon::ShaderType::PIXEL_SHADER:
            return "ps";
            break;
        case Horizon::ShaderType::COMPUTE_SHADER:
            return "cs";
            break;
        default:
            return "error";
            break;
        }
    };

    // Construct paths
    const Path hlsl_path = file_path.is_absolute() ? file_path : (shader_dir / file_path.string());
    const std::string stem = hlsl_path.stem() + "." + shader_type_to_extstr(type);
    const Path spirv_path = saved_shader_dir / (stem + ".spv");

    // Check if recompilation is needed
    if (ShaderCompiler::NeedsRecompilation(hlsl_path, spirv_path))
    {
        LOG_DEBUG("Compiling shader: {} -> {}", hlsl_path.string(), spirv_path.string());
        if (!ShaderCompiler::CompileHLSLToSPIRV(hlsl_path, spirv_path, type, shader_dir, saved_shader_dir, entry_point))
        {
            LOG_ERROR("Failed to compile shader: {}", hlsl_path.string());
            return nullptr;
        }
    }
    else
    {
        LOG_DEBUG("Using cached shader: {}", spirv_path.string());
    }

    // Load compiled SPIR-V
    auto spirv_code = ReadFile(spirv_path.generic_string().c_str());
    if (spirv_code.empty())
    {
        LOG_ERROR("Failed to load compiled shader: {}", spirv_path.string());
        return nullptr;
    }

    return new VulkanShader(m_vulkan, type, spirv_code, entry_point);
}

void RHIVulkan::DestroyShader(Shader *shader_program)
{
    if (shader_program)
    {
        delete shader_program;
    }
    else
    {
        LOG_WARN("shader program is uninitialized or deleted");
    }
}

void RHIVulkan::CreateGpuQueryPool()
{
    VkQueryPoolCreateInfo queryPoolCreateInfo = {};
    queryPoolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryPoolCreateInfo.queryCount = 64;
    CHECK_VK_RESULT(vkCreateQueryPool(m_vulkan.device, &queryPoolCreateInfo, nullptr, &m_vulkan.gpu_query_pool));
}

void RHIVulkan::InitializeVulkanRenderer(const std::string &app_name)
{

    std::vector<const char *> instance_layers;
    std::vector<const char *> instance_extensions;
    std::vector<const char *> device_extensions;
#ifndef NDEBUG
    instance_layers.emplace_back("VK_LAYER_KHRONOS_validation");
    instance_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    instance_extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#endif
    instance_extensions.emplace_back("VK_KHR_surface");

    // Platform-specific surface extensions
#ifdef _WIN32
    instance_extensions.emplace_back("VK_KHR_win32_surface");
#elif defined(__ANDROID__)
    instance_extensions.emplace_back("VK_KHR_android_surface");
#elif defined(__linux__)
    // Linux can use X11, Wayland, or both
    instance_extensions.emplace_back("VK_KHR_xlib_surface");
    // instance_extensions.emplace_back("VK_KHR_wayland_surface");
#endif

    device_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    device_extensions.emplace_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
    device_extensions.emplace_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    device_extensions.emplace_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
    // device_extensions.emplace_back(VK_GOOGLE_HLSL_FUNCTIONALITY_1_EXTENSION_NAME);
    // device_extensions.emplace_back(VK_GOOGLE_USER_TYPE_EXTENSION_NAME);

    if (volkInitialize() != VK_SUCCESS)
    {
        LOG_ERROR("volkInitialize failed");
        return;
    }
    CreateInstance(app_name, instance_layers, instance_extensions);
    volkLoadInstance(m_vulkan.instance);
    CreateDevice(device_extensions);
    volkLoadDevice(m_vulkan.device);
    InitializeVMA();
    // create sync objects
    CreateSyncObjects();
    m_descriptor_set_allocator = Memory::Alloc<VulkanDescriptorSetAllocator>(m_vulkan);

    CreateGpuQueryPool();

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(m_vulkan.active_gpu, &deviceProperties);
    m_vulkan.timestampPeriod = deviceProperties.limits.timestampPeriod;
}

void RHIVulkan::CreateInstance(const std::string &app_name, std::vector<const char *> &instance_layers,
                               std::vector<const char *> &instance_extensions)
{
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

void RHIVulkan::PickGPU(VkInstance instance, VkPhysicalDevice *gpu)
{
    u32 device_count{0};

    std::vector<VkPhysicalDevice> physical_devices;

    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    physical_devices.resize(device_count);
    if (device_count == 0)
    {
        LOG_ERROR("no available device");
    }
    vkEnumeratePhysicalDevices(instance, &device_count, physical_devices.data());

    // pick gpu

    for (const auto &physical_device : physical_devices)
    {
        u32 queue_family_count = (u32)m_vulkan.command_queues.size();

        std::vector<VkQueueFamilyProperties> queue_family_properties;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count,
                                                 nullptr); // Get queue family properties
        // if (queue_family_count < 3)
        //{
        //    LOG_ERROR("less than 3 queue");
        //    continue;
        //}
        queue_family_properties.resize(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count,
                                                 queue_family_properties.data()); // Get queue family properties

        for (u32 i = 0; i < queue_family_count; i++)
        {
            // TOOD: print gpu info, runtime swith gpu

            // graphics queue
            if (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
                queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
                queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                m_vulkan.command_queue_familiy_indices[CommandQueueType::GRAPHICS] = i;
            }

            // dedicate compute queue
            // if (!(queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            //     queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
            //     queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            // {
            //     m_vulkan.command_queue_familiy_indices[CommandQueueType::COMPUTE] = i;
            //     if (m_vulkan.command_queue_familiy_indices[CommandQueueType::GRAPHICS] !=
            //         m_vulkan.command_queue_familiy_indices[CommandQueueType::COMPUTE])
            //     {
            //         gpu_support_async_compute = true;
            //     }
            // }
            // // dedicate transfer queue
            // if (!(queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            //     queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
            //     !(queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
            // {
            //     m_vulkan.command_queue_familiy_indices[CommandQueueType::TRANSFER] = i;
            //     if (m_vulkan.command_queue_familiy_indices[CommandQueueType::GRAPHICS] !=
            //         m_vulkan.command_queue_familiy_indices[CommandQueueType::COMPUTE])
            //     {
            //         gpu_support_async_transfer = true;
            //     }
            // }
            *gpu = physical_device;
        }
        if (gpu != VK_NULL_HANDLE)
        {
            break;
        }
    }
    if (gpu == VK_NULL_HANDLE)
    {
        LOG_ERROR("no suitable gpu found");
    }
}

void RHIVulkan::CreateDevice(std::vector<const char *> &device_extensions)
{
    PickGPU(m_vulkan.instance, &m_vulkan.active_gpu);

    VkPhysicalDeviceShaderDrawParametersFeatures shader_draw_parameters_features{};
    shader_draw_parameters_features.shaderDrawParameters = true;
    shader_draw_parameters_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;

    VkPhysicalDeviceDynamicRenderingFeatures dyanmic_rendering_features{};

    dyanmic_rendering_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dyanmic_rendering_features.dynamicRendering = true;
    dyanmic_rendering_features.pNext = &shader_draw_parameters_features;

    VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features{};
    descriptor_indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    descriptor_indexing_features.pNext = &dyanmic_rendering_features;
    descriptor_indexing_features.runtimeDescriptorArray = VK_TRUE;
    descriptor_indexing_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    descriptor_indexing_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    descriptor_indexing_features.descriptorBindingPartiallyBound = VK_TRUE;

    u32 queue_count = 1;
    if (gpu_support_async_compute)
    {
        queue_count++;
    }
    if (gpu_support_async_transfer)
    {
        queue_count++;
    }
    std::vector<VkDeviceQueueCreateInfo> device_queue_create_info(queue_count);
    f32 queue_priority = 1.0f;

    for (u32 i = 0; i < device_queue_create_info.size(); i++)
    {
        device_queue_create_info[i] = {};
        device_queue_create_info[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        device_queue_create_info[i].pNext = NULL;
        device_queue_create_info[i].flags = 0;
        device_queue_create_info[i].queueFamilyIndex = m_vulkan.command_queue_familiy_indices[i];
        device_queue_create_info[i].queueCount = 1;
        device_queue_create_info[i].pQueuePriorities = &queue_priority;
    }

    VkPhysicalDeviceHostQueryResetFeaturesEXT host_query_reset_features{};
    host_query_reset_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES_EXT;
    host_query_reset_features.pNext = &descriptor_indexing_features;

    VkPhysicalDeviceFeatures2 device_features{};
    device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    // device_features.features = &requested_descriptor_indexing;
    device_features.pNext = &host_query_reset_features;
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
    if (gpu_support_async_transfer)
    {
        vkGetDeviceQueue(m_vulkan.device, m_vulkan.command_queue_familiy_indices[CommandQueueType::TRANSFER], 0,
                         &m_vulkan.command_queues[CommandQueueType::TRANSFER]);
    }
    if (gpu_support_async_compute)
    {
        vkGetDeviceQueue(m_vulkan.device, m_vulkan.command_queue_familiy_indices[CommandQueueType::COMPUTE], 0,
                         &m_vulkan.command_queues[CommandQueueType::COMPUTE]);
    }
    // TODO(hyl5): gpu don't support async compute/transfer

    LOG_DEBUG("using async compute & transfer, graphics queue: {}, compute "
              "queue: {}, transfer queue: {}",
              m_vulkan.command_queue_familiy_indices[CommandQueueType::GRAPHICS],
              m_vulkan.command_queue_familiy_indices[CommandQueueType::COMPUTE],
              m_vulkan.command_queue_familiy_indices[CommandQueueType::TRANSFER]);
}

void RHIVulkan::InitializeVMA()
{
    VmaVulkanFunctions vulkan_functions{};
    vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
#if VMA_VULKAN_VERSION >= 1003000
    vulkan_functions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
    vulkan_functions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;
#endif

    VmaAllocatorCreateInfo vma_create_info{};
    vma_create_info.device = m_vulkan.device;
    vma_create_info.physicalDevice = m_vulkan.active_gpu;
    vma_create_info.instance = m_vulkan.instance;
    vma_create_info.pVulkanFunctions = &vulkan_functions;
    vma_create_info.vulkanApiVersion = VULKAN_API_VERSION;
    CHECK_VK_RESULT(vmaCreateAllocator(&vma_create_info, &m_vulkan.vma_allocator));
}

void RHIVulkan::CreateSyncObjects()
{
}

VkFence RHIVulkan::GetFence(CommandQueueType type) noexcept
{
    VkFence fence{};
    // return an exisiting fence
    if (fence_index[type] < fences[type].size())
    {
        fence = fences[type][fence_index[type]];
    }
    else
    {
        VkFenceCreateInfo fence_create_info{};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        CHECK_VK_RESULT(vkCreateFence(m_vulkan.device, &fence_create_info, nullptr, &fence));
        fences[type].push_back(fence);
    }
    fence_index[type]++;
    return fence;
}

void RHIVulkan::SubmitCommandLists(const QueueSubmitInfo &queue_submit_info)
{
    if (queue_submit_info.wait_image_acquired && !semaphore_ctx.frame_image_acquired)
    {
        return;
    }

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
    for (u32 i = 0; i < command_lists.size(); i++)
    {
        command_buffers[i] = reinterpret_cast<VulkanCommandList *>(command_lists[i])->m_command_buffer;
        // valid command list type when submitting
    }
    submit_info.commandBufferCount = static_cast<u32>(command_buffers.size());
    submit_info.pCommandBuffers = command_buffers.data();

    u32 wait_semaphore_count = (u32)queue_submit_info.wait_semaphores.size();
    std::vector<VkSemaphore> wait_semaphores(wait_semaphore_count);
    std::vector<VkPipelineStageFlags> wait_stages(wait_semaphore_count);
    for (u32 i = 0; i < wait_semaphore_count; i++)
    {
        wait_semaphores[i] = reinterpret_cast<VulkanSemaphore *>(queue_submit_info.wait_semaphores[i])->m_semaphore;
        wait_stages[i] = queue_submit_info.wait_semaphores[i]->GetWaitStage();
    }

    // signal render complete semaphore
    if (queue_submit_info.wait_image_acquired == true)
    {
        wait_semaphores.push_back(reinterpret_cast<VulkanSemaphore *>(
                                      semaphore_ctx.present_complete_semaphore[semaphore_ctx.current_frame_index])

                                      ->m_semaphore);
        wait_stages.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    }

    u32 signal_semaphore_count = (u32)queue_submit_info.signal_semaphores.size();
    std::vector<VkSemaphore> signal_semaphores(signal_semaphore_count);

    for (u32 i = 0; i < signal_semaphore_count; i++)
    {
        signal_semaphores[i] = reinterpret_cast<VulkanSemaphore *>(queue_submit_info.signal_semaphores[i])->m_semaphore;
        queue_submit_info.signal_semaphores[i]->AddWaitStage(queue_submit_info.queue_type);
    }

    // signal render complete semaphore
    if (queue_submit_info.signal_render_complete == true)
    {
        u32 signal_index = semaphore_ctx.acquired_image_index;
        if (signal_index >= semaphore_ctx.render_complete_semaphore.size())
        {
            signal_index = semaphore_ctx.current_frame_index;
        }

        signal_semaphores.push_back(
            reinterpret_cast<VulkanSemaphore *>(semaphore_ctx.render_complete_semaphore[signal_index])->m_semaphore);
        wait_stages.push_back(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT); // correct stage?
    }

    submit_info.waitSemaphoreCount = static_cast<u32>(wait_semaphores.size());
    submit_info.pWaitSemaphores = wait_semaphores.data();

    submit_info.signalSemaphoreCount = static_cast<u32>(signal_semaphores.size());
    submit_info.pSignalSemaphores = signal_semaphores.data();

    submit_info.pWaitDstStageMask = wait_stages.data();

    auto fence = GetFence(queue_submit_info.queue_type);

    vkQueueSubmit(m_vulkan.command_queues[queue_submit_info.queue_type], 1, &submit_info, fence);
}

void RHIVulkan::Present(const QueuePresentInfo &queue_present_info)
{
    if (!semaphore_ctx.frame_image_acquired)
    {
        return;
    }

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    // u32 wait_semaphore_count = queue_present_info.wait_semaphores.size();
    // std::vector<VkSemaphore> wait_semaphores(wait_semaphore_count);

    // for (u32 i = 0; i < wait_semaphore_count; i++) {
    //     wait_semaphores[i] = reinterpret_cast<VulkanSemaphore *>(queue_present_info.wait_semaphores[i])->m_semaphore;
    // }

    auto vk_swap_chain = reinterpret_cast<VulkanSwapChain *>(queue_present_info.swap_chain);
    std::vector<VkSemaphore> wait_semaphores;
    u32 wait_index = semaphore_ctx.acquired_image_index;
    if (wait_index >= semaphore_ctx.render_complete_semaphore.size())
    {
        wait_index = semaphore_ctx.current_frame_index;
    }
    wait_semaphores.push_back(
        reinterpret_cast<VulkanSemaphore *>(semaphore_ctx.render_complete_semaphore[wait_index])->m_semaphore);

    present_info.waitSemaphoreCount = static_cast<u32>(wait_semaphores.size());
    present_info.pWaitSemaphores = wait_semaphores.data();

    VkSwapchainKHR swapChains[] = {vk_swap_chain->swap_chain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapChains;
    present_info.pImageIndices = &vk_swap_chain->image_index;
    VkResult present_result = vkQueuePresentKHR(m_vulkan.command_queues[CommandQueueType::GRAPHICS], &present_info);

    if (present_result == VK_ERROR_OUT_OF_DATE_KHR || present_result == VK_SUBOPTIMAL_KHR)
    {
        if (m_window && m_window->GetWidth() > 0 && m_window->GetHeight() > 0)
        {
            vk_swap_chain->Resize(m_window->GetWidth(), m_window->GetHeight());
        }
        semaphore_ctx.frame_image_acquired = false;
        return;
    }

    CHECK_VK_RESULT(present_result);

    vk_swap_chain->current_frame_index++;
    vk_swap_chain->current_frame_index = vk_swap_chain->current_frame_index % vk_swap_chain->m_back_buffer_count;
    semaphore_ctx.current_frame_index = vk_swap_chain->current_frame_index;
    semaphore_ctx.frame_image_acquired = false;
}

void RHIVulkan::AcquireNextFrame(SwapChain *swap_chain)
{
    auto vk_swap_chain = reinterpret_cast<VulkanSwapChain *>(swap_chain);
    semaphore_ctx.frame_image_acquired = false;

    if (m_window && (m_window->GetWidth() == 0 || m_window->GetHeight() == 0))
    {
        ResetFence(CommandQueueType::GRAPHICS);
        ResetRHIResources();
        return;
    }

    if (m_window && m_window->GetWidth() > 0 && m_window->GetHeight() > 0 &&
        (vk_swap_chain->width != m_window->GetWidth() || vk_swap_chain->height != m_window->GetHeight()))
    {
        vk_swap_chain->Resize(m_window->GetWidth(), m_window->GetHeight());
    }

    Semaphore *sm = semaphore_ctx.present_complete_semaphore[semaphore_ctx.current_frame_index];

    VkResult res = vkAcquireNextImageKHR(m_vulkan.device, vk_swap_chain->swap_chain, UINT64_MAX,
                                         reinterpret_cast<VulkanSemaphore *>(sm)->m_semaphore, nullptr,
                                         &vk_swap_chain->image_index);

    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
    {
        if (m_window && m_window->GetWidth() > 0 && m_window->GetHeight() > 0)
        {
            vk_swap_chain->Resize(m_window->GetWidth(), m_window->GetHeight());
            res = vkAcquireNextImageKHR(m_vulkan.device, vk_swap_chain->swap_chain, UINT64_MAX,
                                        reinterpret_cast<VulkanSemaphore *>(sm)->m_semaphore, nullptr,
                                        &vk_swap_chain->image_index);
        }
    }

    if (res != VK_SUCCESS)
    {
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
        {
            LOG_DEBUG("Skip frame: swapchain is out of date/suboptimal while acquiring image");
        }
        else
        {
            LOG_ERROR("failed to acquire next image, VkResult={}", static_cast<int>(res));
        }
        ResetFence(CommandQueueType::GRAPHICS);
        ResetRHIResources();
        return;
    }

    semaphore_ctx.acquired_image_index = vk_swap_chain->image_index;
    semaphore_ctx.frame_image_acquired = true;

    // RESET RHIVulkan RESOURCES
    {
        ResetFence(CommandQueueType::GRAPHICS);
        // ResetFence(CommandQueueType::COMPUTE);
        // ResetFence(CommandQueueType::TRANSFER);
        ResetRHIResources();
    }

    vkResetQueryPool(m_vulkan.device, m_vulkan.gpu_query_pool, 0, 64);
}

CommandList *RHIVulkan::GetCommandList(CommandQueueType type)
{

    if (!thread_command_context)
    {
        thread_command_context = Memory::Alloc<VulkanCommandContext>(m_vulkan);
    }

    return thread_command_context->GetCommandList(type);
}

void RHIVulkan::WaitGpuExecution(CommandQueueType queue_type)
{
    assert(fence_index[queue_type] != UINT_MAX); // no need to wait twice
    if (fence_index[queue_type] == 0)
        return;
    vkWaitForFences(m_vulkan.device, static_cast<u32>(fences[queue_type].size()), fences[queue_type].data(), VK_TRUE,
                    UINT64_MAX);
    fence_index[queue_type] = UINT_MAX;
}

void RHIVulkan::ResetFence(CommandQueueType queue_type)
{
    if (fence_index[queue_type] == 0)
        return;
    vkResetFences(m_vulkan.device, static_cast<u32>(fences[queue_type].size()), fences[queue_type].data());
    fence_index[queue_type] = 0;
}

void RHIVulkan::ResetRHIResources()
{
    if (thread_command_context)
    {
        thread_command_context->Reset();
    }
    m_descriptor_set_allocator->ResetDescriptorPool();
}

Pipeline *RHIVulkan::CreateGraphicsPipeline(const GraphicsPipelineCreateInfo &create_info)
{
    return new VulkanPipeline(m_vulkan, create_info, *m_descriptor_set_allocator);
}

Pipeline *RHIVulkan::CreateComputePipeline(const ComputePipelineCreateInfo &create_info)
{
    return new VulkanPipeline(m_vulkan, create_info, *m_descriptor_set_allocator);
}

void RHIVulkan::DestroyPipeline(Pipeline *pipeline)
{
    delete pipeline;
}

Semaphore *RHIVulkan::CreateSemaphore1()
{
    return Memory::Alloc<VulkanSemaphore>(m_vulkan);
}

Sampler *RHIVulkan::CreateSampler(const SamplerDesc &sampler_desc)
{
    return Memory::Alloc<VulkanSampler>(m_vulkan, sampler_desc);
}

void RHIVulkan::DestroyBuffer(Buffer *buffer)
{
    Memory::Free(buffer);
    buffer = nullptr;
}

void RHIVulkan::DestroyTexture(Texture *texture)
{
    Memory::Free(texture);
    texture = nullptr;
}

void RHIVulkan::DestroyRenderTarget(RenderTarget *render_target)
{
    Memory::Free(render_target);
    render_target = nullptr;
}

void RHIVulkan::DestroySwapChain(SwapChain *swap_chain)
{
    Memory::Free(swap_chain);
    swap_chain = nullptr;
}

void RHIVulkan::DestroySemaphore(Semaphore *semaphore)
{
    Memory::Free(semaphore);
    semaphore = nullptr;
}

void RHIVulkan::DestroySampler(Sampler *sampler)
{
    Memory::Free(sampler);
    sampler = nullptr;
}
} // namespace Horizon::Backend
