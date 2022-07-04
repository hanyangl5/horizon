#include <thread>

#define VMA_IMPLEMENTATION

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#endif

#include <vulkan/vulkan.hpp>

#include <runtime/function/rhi/vulkan/RHIVulkan.h>
#include <runtime/function/rhi/vulkan/VulkanCommandContext.h>
#include <runtime/function/rhi/vulkan/VulkanPipeline.h>
#include <runtime/function/rhi/vulkan/VulkanShaderProgram.h>

namespace Horizon::RHI {

RHIVulkan::RHIVulkan() noexcept {}

RHIVulkan::~RHIVulkan() noexcept {
    for (auto &[thread_id, context] : m_command_context_map) {
        delete context;
    }
    DestroySwapChain();
    vkDestroySurfaceKHR(m_vulkan.instance, m_vulkan.surface, nullptr);
    vmaDestroyAllocator(m_vulkan.vma_allocator);
    vkDestroyDevice(m_vulkan.device, nullptr);
    vkDestroyInstance(m_vulkan.instance, nullptr);
}

void RHIVulkan::InitializeRenderer() noexcept {
    LOG_DEBUG("using vulkan renderer");
    InitializeVulkanRenderer("vulkan renderer");
}

Resource<Buffer>
RHIVulkan::CreateBuffer(const BufferCreateInfo &buffer_create_info) noexcept {
    BufferCreateInfo create_info{};
    create_info.size = buffer_create_info.size;
    create_info.buffer_usage_flags = buffer_create_info.buffer_usage_flags |
                                     BufferUsage::BUFFER_USAGE_TRANSFER_DST;

    return std::make_unique<VulkanBuffer>(m_vulkan.vma_allocator, create_info,
                                          MemoryFlag::DEDICATE_GPU_MEMORY);
}

Resource<Texture> RHIVulkan::CreateTexture(
    const TextureCreateInfo &texture_create_info) noexcept {
    return std::make_unique<VulkanTexture>(m_vulkan.vma_allocator,
                                           texture_create_info);
}

void RHIVulkan::CreateSwapChain(Window *window) noexcept {
    // create window surface
    VkWin32SurfaceCreateInfoKHR surface_create_info{};
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hinstance = GetModuleHandle(nullptr);
    ;
    surface_create_info.hwnd = window->GetWin32Window();
    CHECK_VK_RESULT(vkCreateWin32SurfaceKHR(
        m_vulkan.instance, &surface_create_info, nullptr, &m_vulkan.surface));

    // u32 surface_format_count = 0;
    //// Get surface formats count
    // CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(m_vulkan.active_gpu,
    // m_vulkan.surface, &surface_format_count, nullptr));
    // std::vector<VkSurfaceFormatKHR> surface_formats(surface_format_count);
    // CHECK_VK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(m_vulkan.active_gpu,
    // m_vulkan.surface, &surface_format_count, surface_formats.data()));

    // VkSurfaceFormatKHR surface_format{};

    // create swap chain

    VkSwapchainCreateInfoKHR swap_chain_create_info{};
    swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_chain_create_info.surface = m_vulkan.surface;
    swap_chain_create_info.minImageCount = m_back_buffer_count;
    swap_chain_create_info.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    swap_chain_create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swap_chain_create_info.imageExtent = {window->GetWidth(),
                                          window->GetHeight()};
    swap_chain_create_info.imageArrayLayers = 1;
    swap_chain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swap_chain_create_info.preTransform =
        VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; // rotatioin/flip
    swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_chain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swap_chain_create_info.clipped = VK_TRUE;
    swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swap_chain_create_info.queueFamilyIndexCount = 0;

    CHECK_VK_RESULT(vkCreateSwapchainKHR(m_vulkan.device,
                                         &swap_chain_create_info, nullptr,
                                         &m_vulkan.swap_chain));

    // u32 image_count = 0;
    // vkGetSwapchainImagesKHR(m_vulkan.device, m_vulkan.swap_chain,
    // &image_count, nullptr);  // Get images
    m_vulkan.swap_chain_images.resize(m_back_buffer_count);
    vkGetSwapchainImagesKHR(m_vulkan.device, m_vulkan.swap_chain,
                            &m_back_buffer_count,
                            m_vulkan.swap_chain_images.data()); // Get images

    m_vulkan.swap_chain_image_views.resize(m_back_buffer_count);

    VkImageViewCreateInfo image_view_create_info{};
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    image_view_create_info.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = 1;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = 1;

    for (u32 i = 0; i < m_vulkan.swap_chain_image_views.size(); i++) {
        image_view_create_info.image = m_vulkan.swap_chain_images[i];
        CHECK_VK_RESULT(vkCreateImageView(m_vulkan.device,
                                          &image_view_create_info, nullptr,
                                          &m_vulkan.swap_chain_image_views[i]));
    }
}

ShaderProgram *RHIVulkan::CreateShaderProgram(ShaderType type,
                                              const std::string &entry_point,
                                              u32 compile_flags,
                                              std::string file_name) noexcept {
    auto spirv_blob{m_shader_compiler->CompileFromFile(
        ShaderTargetPlatform::SPIRV, type, entry_point, compile_flags,
        file_name)};

    return new VulkanShaderProgram(m_vulkan.device, type, entry_point,
                                   spirv_blob);
}

void RHIVulkan::DestroyShaderProgram(ShaderProgram *shader_program) noexcept {
    if (shader_program) {
        delete shader_program;
    } else {
        LOG_WARN("shader program is uninitialized or deleted");
    }
}

void RHIVulkan::InitializeVulkanRenderer(const std::string &app_name) noexcept {
    std::vector<const char *> instance_layers{};
    std::vector<const char *> instance_extensions{};
    std::vector<const char *> device_extensions{};
#ifndef NDEBUG
    instance_layers.emplace_back("VK_LAYER_KHRONOS_validation");
    instance_extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    instance_extensions.emplace_back("VK_KHR_surface");
    instance_extensions.emplace_back("VK_KHR_win32_surface");
    device_extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    device_extensions.emplace_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);

    CreateInstance(app_name, instance_layers, instance_extensions);
    CreateDevice(device_extensions);
    InitializeVMA();
}

void RHIVulkan::CreateInstance(
    const std::string &app_name, std::vector<const char *> &instance_layers,
    std::vector<const char *> &instance_extensions) noexcept {

    u32 layer_count{0}, extension_count{0};
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    std::vector<VkExtensionProperties> available_extensions(extension_count);

    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                           available_extensions.data());

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
    instance_create_info.enabledExtensionCount =
        static_cast<u32>(instance_extensions.size());
    instance_create_info.ppEnabledExtensionNames = instance_extensions.data();
    instance_create_info.enabledLayerCount =
        static_cast<u32>(instance_layers.size());
    instance_create_info.ppEnabledLayerNames = instance_layers.data();

    CHECK_VK_RESULT(
        vkCreateInstance(&instance_create_info, nullptr, &(m_vulkan.instance)));
}

void RHIVulkan::PickGPU(VkInstance instance, VkPhysicalDevice *gpu) noexcept {
    u32 device_count{0};
    std::vector<VkPhysicalDevice> physical_devices{};
    vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
    physical_devices.resize(device_count);
    if (device_count == 0) {
        LOG_ERROR("no available device");
    }
    vkEnumeratePhysicalDevices(instance, &device_count,
                               physical_devices.data());

    // pick gpu

    u32 graphics_queue_family_index{};
    u32 compute_queue_family_index{};
    u32 transfer_queue_family_index{};

    for (const auto &physical_device : physical_devices) {
        u32 queue_family_count{0};
        vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device, &queue_family_count, nullptr); // Get count
        std::vector<VkQueueFamilyProperties> queue_family_properties(
            queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(
            physical_device, &queue_family_count,
            queue_family_properties.data()); // Get queue family properties

        for (u32 i = 0; i < queue_family_count; i++) {
            // TOOD: print gpu info, runtime swith gpu
            // TODO: support async compute, async transfer

            if (queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
                queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
                queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                *gpu = physical_device;
                assert(CommandQueueType::GRAPHICS == i);
                // break;
            }
            // dedicate compute queue
            if (!(queue_family_properties[i].queueFlags &
                  VK_QUEUE_GRAPHICS_BIT) &&
                queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
                queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
                compute_queue_family_index = i;
                assert(CommandQueueType::COMPUTE == i);
            }
            // dedicate transfer queue
            if (!(queue_family_properties[i].queueFlags &
                  VK_QUEUE_GRAPHICS_BIT) &&
                queue_family_properties[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
                !(queue_family_properties[i].queueFlags &
                  VK_QUEUE_COMPUTE_BIT)) {
                transfer_queue_family_index = i;
                assert(CommandQueueType::TRANSFER == i);
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

void RHIVulkan::CreateDevice(
    std::vector<const char *> &device_extensions) noexcept {

    PickGPU(m_vulkan.instance, &m_vulkan.active_gpu);

    std::vector<VkDeviceQueueCreateInfo> device_queue_create_info{};

    f32 queue_priority{1.0f};

    device_queue_create_info.emplace_back(VkDeviceQueueCreateInfo{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0,
        CommandQueueType::GRAPHICS, 1, &queue_priority});
#ifdef USE_ASYNC_COMPUTE_TRANSFER
    device_queue_create_info.emplace_back(VkDeviceQueueCreateInfo{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0,
        CommandQueueType::COMPUTE, 1, &queue_priority});
    device_queue_create_info.emplace_back(VkDeviceQueueCreateInfo{
        VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0,
        CommandQueueType::TRANSFER, 1, &queue_priority});
#endif // USE_ASYNC_TRANSFER

    // bindless extension
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexing_features{};
    indexing_features.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    indexing_features.pNext = nullptr;
    indexing_features.runtimeDescriptorArray = VK_TRUE;
    indexing_features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    indexing_features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

    VkPhysicalDeviceFeatures2 device_features{};
    device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    device_features.pNext = &indexing_features;
    vkGetPhysicalDeviceFeatures2(m_vulkan.active_gpu, &device_features);

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos = device_queue_create_info.data();
    device_create_info.queueCreateInfoCount =
        static_cast<u32>(device_queue_create_info.size());
    device_create_info.enabledExtensionCount =
        static_cast<u32>(device_extensions.size());
    device_create_info.ppEnabledExtensionNames = device_extensions.data();
    device_create_info.pNext = &device_features;

    CHECK_VK_RESULT(vkCreateDevice(m_vulkan.active_gpu, &device_create_info,
                                   nullptr, &m_vulkan.device));

    // retrive command queue

    vkGetDeviceQueue(m_vulkan.device, CommandQueueType::GRAPHICS, 0,
                     &m_vulkan.command_queues[CommandQueueType::GRAPHICS]);
    // vkGetDeviceQueue(m_vulkan.device, m_vulkan.graphics_queue_family_index,
    // 0, &m_vulkan.m_present_queue);
    vkGetDeviceQueue(m_vulkan.device, CommandQueueType::TRANSFER, 0,
                     &m_vulkan.command_queues[CommandQueueType::TRANSFER]);
    vkGetDeviceQueue(m_vulkan.device, CommandQueueType::COMPUTE, 0,
                     &m_vulkan.command_queues[CommandQueueType::COMPUTE]);

#ifdef USE_ASYNC_COMPUTE_TRANSFER
    LOG_DEBUG("using async compute & transfer, graphics queue: {}, compute "
              "queue: {}, transfer queue: {}",
              CommandQueueType::GRAPHICS, CommandQueueType::COMPUTE,
              CommandQueueType::TRANSFER);
#endif // USE_ASYNC_COMPUTE
}

void RHIVulkan::InitializeVMA() noexcept {
    VmaVulkanFunctions vulkan_functions{};
    vulkan_functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo vma_create_info = {0};
    vma_create_info.device = m_vulkan.device;
    vma_create_info.physicalDevice = m_vulkan.active_gpu;
    vma_create_info.instance = m_vulkan.instance;
    vma_create_info.pVulkanFunctions = &vulkan_functions;
    vma_create_info.vulkanApiVersion = VULKAN_API_VERSION;
    vmaCreateAllocator(&vma_create_info, &m_vulkan.vma_allocator);
}

void RHIVulkan::DestroySwapChain() noexcept {
    for (u32 i = 0; i < m_vulkan.swap_chain_images.size(); i++) {
        vkDestroyImageView(m_vulkan.device, m_vulkan.swap_chain_image_views[i],
                           nullptr);
        // vkDestroyImage(m_vulkan.device, m_vulkan.swap_chain_images[i],
        // nullptr);
    }
    vkDestroySwapchainKHR(m_vulkan.device, m_vulkan.swap_chain, nullptr);
}

void RHIVulkan::SubmitCommandLists(
    CommandQueueType queue_type,
    std::vector<CommandList *> &command_lists) noexcept {
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

    std::vector<VkCommandBuffer> command_buffers(command_lists.size());
    for (u32 i = 0; i < command_lists.size(); i++) {
        command_buffers[i] = dynamic_cast<VulkanCommandList *>(command_lists[i])
                                 ->m_command_buffer;
        // valid command list type when submitting
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = static_cast<u32>(command_buffers.size());
    submit_info.pCommandBuffers = command_buffers.data();
    vkQueueSubmit(m_vulkan.command_queues[queue_type], 1, &submit_info,
                  VK_NULL_HANDLE);
    vkQueueWaitIdle(m_vulkan.command_queues[queue_type]);
}

CommandList *RHIVulkan::GetCommandList(CommandQueueType type) noexcept {
    auto key{std::this_thread::get_id()};

    if (!m_command_context_map[key]) {
        m_command_context_map[key] = new VulkanCommandContext(m_vulkan.device);
    }

    auto vk_command_context =
        dynamic_cast<VulkanCommandContext *>(m_command_context_map[key]);
    return vk_command_context->GetVulkanCommandList(type);
}

void RHIVulkan::ResetCommandResources() noexcept {
    for (auto &[thred_id, context] : m_command_context_map) {
        context->Reset();
    }
}

Pipeline *RHIVulkan::CreatePipeline(
    const PipelineCreateInfo &pipeline_create_info) noexcept {
    if (!m_global_descriptor) {
        m_global_descriptor = new VulkanDescriptor(m_vulkan.device);
        m_global_descriptor->AllocateDescriptors();
    }
    return new VulkanPipeline(m_vulkan.device, pipeline_create_info,
                              m_global_descriptor);
}
} // namespace Horizon::RHI
