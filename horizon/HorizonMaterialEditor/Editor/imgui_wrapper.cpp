#include "imgui_wrapper.h"

namespace Horizon::MaterialEditor {
ImguiWrapper::ImguiWrapper() noexcept {

    if (!InitWindow()) {
    }

    if (!InitRenderBackend()) {
    }

    if (!InitImgui()) {
    }
}

ImguiWrapper::~ImguiWrapper() noexcept { Terminate(); }

void ImguiWrapper::Terminate() {

    // Cleanup
    auto err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    CleanupVulkanWindow();
    CleanupVulkan();

    glfwDestroyWindow(m_window);
    glfwTerminate();
}

bool ImguiWrapper::WindowShouldClose() { return glfwWindowShouldClose(m_window); }

void ImguiWrapper::Prepare() {
    glfwPollEvents();

    // Resize swap chain?
    if (g_SwapChainRebuild) {
        int width, height;
        glfwGetFramebufferSize(m_window, &width, &height);
        if (width > 0 && height > 0) {
            ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData,
                                                   g_QueueFamily, g_Allocator, width, height, g_MinImageCount);
            g_MainWindowData.FrameIndex = 0;
            g_SwapChainRebuild = false;
        }
    }

    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImguiWrapper::RenderUI() {
    // Rendering
    ImGui::Render();
    ImDrawData *draw_data = ImGui::GetDrawData();
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    if (!is_minimized) {
        wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
        wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
        wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
        wd->ClearValue.color.float32[3] = clear_color.w;
        FrameRender(wd, draw_data);
        FramePresent(wd);
    }
}

bool ImguiWrapper::InitWindow() {
    // Setup GLFW window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return false;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(1280, 720, "Horizon Material Editor", NULL, NULL);
    return m_window != nullptr ? true : false;
}

bool ImguiWrapper::InitRenderBackend() {
    // Setup Vulkan
    if (!glfwVulkanSupported()) {
        printf("GLFW: Vulkan Not Supported\n");
        return false;
    }
    uint32_t extensions_count = 0;
    const char **extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
    SetupVulkan(extensions, extensions_count);

    // Create Window Surface
    VkSurfaceKHR surface;
    VkResult err = glfwCreateWindowSurface(g_Instance, m_window, g_Allocator, &surface);
    check_vk_result(err);

    // Create Framebuffers
    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    wd = &g_MainWindowData;
    SetupVulkanWindow(wd, surface, w, h);
    return true;
}

bool ImguiWrapper::InitImgui() {

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    SetupTheme();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(m_window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.Subpass = 0;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = g_Allocator;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info, wd->RenderPass);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use
    // ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among
    // multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application
    // (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling
    // ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font
    // rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double
    // backslash \\ !
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL,
    // io.Fonts->GetGlyphRangesJapanese()); IM_ASSERT(font != NULL);

    // Upload Fonts
    {
        // Use any command queue
        VkCommandPool command_pool = wd->Frames[wd->FrameIndex].CommandPool;
        VkCommandBuffer command_buffer = wd->Frames[wd->FrameIndex].CommandBuffer;

        auto err = vkResetCommandPool(g_Device, command_pool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(command_buffer, &begin_info);
        check_vk_result(err);

        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &command_buffer;
        err = vkEndCommandBuffer(command_buffer);
        check_vk_result(err);
        err = vkQueueSubmit(g_Queue, 1, &end_info, VK_NULL_HANDLE);
        check_vk_result(err);

        err = vkDeviceWaitIdle(g_Device);
        check_vk_result(err);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    return true;
}

void ImguiWrapper::SetupTheme() {
    ImGuiStyle &style = ImGui::GetStyle();

    // Setup Dear ImGui style
    // ImGui::StyleColorsDark();
    ImGui::StyleColorsLight();
    // light style from Pacôme Danhiez (user itamago)
    // https://github.com/ocornut/imgui/pull/511#issuecomment-175719267
    // style.Alpha = 1.0f;
    // style.FrameRounding = 3.0f;
    // style.Colors[ImGuiCol_Text] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);

    /*
            ImGuiCol_Text,
            ImGuiCol_TextDisabled,
            ImGuiCol_WindowBg,              // Background of normal windows
            ImGuiCol_ChildBg,               // Background of child windows
            ImGuiCol_PopupBg,               // Background of popups, menus, tooltips windows
            ImGuiCol_Border,
            ImGuiCol_BorderShadow,
            ImGuiCol_FrameBg,               // Background of checkbox, radio button, plot, slider, text input
            ImGuiCol_FrameBgHovered,
            ImGuiCol_FrameBgActive,
            ImGuiCol_TitleBg,
            ImGuiCol_TitleBgActive,
            ImGuiCol_TitleBgCollapsed,
            ImGuiCol_MenuBarBg,
            ImGuiCol_ScrollbarBg,
            ImGuiCol_ScrollbarGrab,
            ImGuiCol_ScrollbarGrabHovered,
            ImGuiCol_ScrollbarGrabActive,
            ImGuiCol_CheckMark,
            ImGuiCol_SliderGrab,
            ImGuiCol_SliderGrabActive,
            ImGuiCol_Button,
            ImGuiCol_ButtonHovered,
            ImGuiCol_ButtonActive,
            ImGuiCol_Header,                // Header* colors are used for CollapsingHeader, TreeNode, Selectable,
       MenuItem ImGuiCol_HeaderHovered, ImGuiCol_HeaderActive, ImGuiCol_Separator, ImGuiCol_SeparatorHovered,
            ImGuiCol_SeparatorActive,
            ImGuiCol_ResizeGrip,            // Resize grip in lower-right and lower-left corners of windows.
            ImGuiCol_ResizeGripHovered,
            ImGuiCol_ResizeGripActive,
            ImGuiCol_Tab,                   // TabItem in a TabBar
            ImGuiCol_TabHovered,
            ImGuiCol_TabActive,
            ImGuiCol_TabUnfocused,
            ImGuiCol_TabUnfocusedActive,
            ImGuiCol_PlotLines,
            ImGuiCol_PlotLinesHovered,
            ImGuiCol_PlotHistogram,
            ImGuiCol_PlotHistogramHovered,
            ImGuiCol_TableHeaderBg,         // Table header background
            ImGuiCol_TableBorderStrong,     // Table outer and header borders (prefer using Alpha=1.0 here)
            ImGuiCol_TableBorderLight,      // Table inner borders (prefer using Alpha=1.0 here)
            ImGuiCol_TableRowBg,            // Table row background (even rows)
            ImGuiCol_TableRowBgAlt,         // Table row background (odd rows)
            ImGuiCol_TextSelectedBg,
            ImGuiCol_DragDropTarget,        // Rectangle highlighting a drop target
            ImGuiCol_NavHighlight,          // Gamepad/keyboard: current highlighted item
            ImGuiCol_NavWindowingHighlight, // Highlight window when using CTRL+TAB
            ImGuiCol_NavWindowingDimBg,     // Darken/colorize entire screen behind the CTRL+TAB window list, when
       active ImGuiCol_ModalWindowDimBg,      // Darken/colorize entire screen behind a modal window, when one is
       active ImGuiCol_COUNT
        */
}



} // namespace Horizon::MaterialEditor