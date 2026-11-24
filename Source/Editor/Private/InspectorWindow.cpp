#include "InspectorWindow.h"

#include <d3d11.h>
#include <commctrl.h>
#include <wrl/client.h>
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "Platform/IOperatingSystem.h"
#include "Core/ILog.h"
#include "Core/IMemory.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandlerEx(HWND, UINT, WPARAM, LPARAM, ImGuiIO&);

struct InspectorWindow
{
    HWND                                           Handle = nullptr;
    ImGuiContext*                                  Context = nullptr;
    ImGuiIO*                                       IO = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Device>           Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext>    Commands;
    Microsoft::WRL::ComPtr<IDXGISwapChain>         Swapchain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> Target;
    UINT                                           Width = 0, Height = 0;
    float                                          DpiScale = 1;
};

static constexpr wchar_t ClassName[] = L"HorizonSceneInspector";

static LRESULT CALLBACK OwnerProc(HWND Owner, UINT Message, WPARAM WParam, LPARAM LParam, UINT_PTR, DWORD_PTR User)
{
    if (Message == WM_KEYDOWN && WParam == VK_F1)
    {
        HWND Tool = (HWND)User;
        if (!(LParam & (1L << 30)))
        {
            ShowWindow(Tool, IsWindowVisible(Tool) ? SW_HIDE : SW_SHOWNORMAL);
        }
        return 0;
    }
    return DefSubclassProc(Owner, Message, WParam, LParam);
}

static LRESULT CALLBACK InspectorProc(HWND Handle, UINT Message, WPARAM WParam, LPARAM LParam)
{
    InspectorWindow* Window = (InspectorWindow*)GetWindowLongPtrW(Handle, GWLP_USERDATA);
    if (Message == WM_CLOSE || (Message == WM_KEYDOWN && WParam == VK_F1 && !(LParam & (1L << 30))))
    {
        ShowWindow(Handle, SW_HIDE);
        return 0;
    }
    if (Window && Message == WM_DPICHANGED)
    {
        Window->DpiScale = (float)HIWORD(WParam) / 96.0f;
    }
    if (Window && Window->IO && ImGui_ImplWin32_WndProcHandlerEx(Handle, Message, WParam, LParam, *Window->IO))
    {
        return 1;
    }
    return DefWindowProcW(Handle, Message, WParam, LParam);
}

InspectorWindow* CreateInspectorWindow(const WindowHandle& Owner)
{
    const WNDCLASSW Class = { .lpfnWndProc = InspectorProc,
                              .hInstance = GetModuleHandleW(nullptr),
                              .hCursor = LoadCursor(nullptr, IDC_ARROW),
                              .lpszClassName = ClassName };
    if (!RegisterClassW(&Class))
    {
        return nullptr;
    }
    InspectorWindow* Window = MemNew(InspectorWindow);
    const HMONITOR   Monitor = MonitorFromWindow(Owner.window, MONITOR_DEFAULTTONEAREST);
    MONITORINFO      Info = { .cbSize = sizeof(Info) };
    GetMonitorInfoW(Monitor, &Info);
    Window->DpiScale = ImGui_ImplWin32_GetDpiScaleForMonitor(Monitor);
    const int Width = TF_MIN((int)(570 * Window->DpiScale), Info.rcWork.right - Info.rcWork.left);
    const int Height = TF_MIN((int)(800 * Window->DpiScale), Info.rcWork.bottom - Info.rcWork.top);
    Window->Handle = CreateWindowExW(WS_EX_TOOLWINDOW, ClassName, L"Scene Inspector", WS_OVERLAPPEDWINDOW, Info.rcWork.right - Width,
                                     Info.rcWork.top, Width, Height, Owner.window, nullptr, Class.hInstance, nullptr);
    DXGI_SWAP_CHAIN_DESC Desc = {};
    Desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    Desc.SampleDesc.Count = 1;
    Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    Desc.BufferCount = 2;
    Desc.OutputWindow = Window->Handle;
    Desc.Windowed = TRUE;
    Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    if (Window->Handle && SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
                                                                  D3D11_SDK_VERSION, &Desc, Window->Swapchain.GetAddressOf(),
                                                                  Window->Device.GetAddressOf(), nullptr, Window->Commands.GetAddressOf())))
    {
        ImGuiContext* Previous = ImGui::GetCurrentContext();
        Window->Context = ImGui::CreateContext();
        ImGui::SetCurrentContext(Window->Context);
        ImGuiIO& IO = ImGui::GetIO();
        IO.IniFilename = nullptr;
        IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        IO.ConfigDpiScaleViewports = true;
        const bool Ready = ImGui_ImplWin32_Init(Window->Handle) && ImGui_ImplDX11_Init(Window->Device.Get(), Window->Commands.Get()) &&
                           SetWindowSubclass(Owner.window, OwnerProc, (UINT_PTR)Window, (DWORD_PTR)Window->Handle);
        ImGui::SetCurrentContext(Previous);
        if (Ready)
        {
            Window->IO = &IO;
            SetWindowLongPtrW(Window->Handle, GWLP_USERDATA, (LONG_PTR)Window);
            ShowWindow(Window->Handle, SW_SHOWNOACTIVATE);
            return Window;
        }
    }
    DestroyInspectorWindow(Window);
    return nullptr;
}

void DestroyInspectorWindow(InspectorWindow* Window)
{
    if (!Window)
    {
        return;
    }
    if (Window->Context)
    {
        ImGuiContext* Previous = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(Window->Context);
        Window->IO = nullptr;
        if (ImGui::GetIO().BackendRendererUserData)
        {
            ImGui_ImplDX11_Shutdown();
        }
        if (ImGui::GetIO().BackendPlatformUserData)
        {
            ImGui_ImplWin32_Shutdown();
        }
        ImGui::DestroyContext(Window->Context);
        ImGui::SetCurrentContext(Previous);
    }
    if (Window->Handle)
    {
        RemoveWindowSubclass(GetWindow(Window->Handle, GW_OWNER), OwnerProc, (UINT_PTR)Window);
        DestroyWindow(Window->Handle);
    }
    UnregisterClassW(ClassName, GetModuleHandleW(nullptr));
    MemDelete(Window);
}

static bool PrepareTarget(InspectorWindow& Window)
{
    RECT Bounds;
    GetClientRect(Window.Handle, &Bounds);
    const UINT Width = Bounds.right, Height = Bounds.bottom;
    if (!Width || !Height)
    {
        return false;
    }
    if (Window.Target && Width == Window.Width && Height == Window.Height)
    {
        return true;
    }
    Window.Commands->OMSetRenderTargets(0, nullptr, nullptr);
    Window.Target.Reset();
    Microsoft::WRL::ComPtr<ID3D11Texture2D> Buffer;
    if (FAILED(Window.Swapchain->ResizeBuffers(0, Width, Height, DXGI_FORMAT_UNKNOWN, 0)) ||
        FAILED(Window.Swapchain->GetBuffer(0, IID_PPV_ARGS(Buffer.GetAddressOf()))) ||
        FAILED(Window.Device->CreateRenderTargetView(Buffer.Get(), nullptr, Window.Target.GetAddressOf())))
    {
        return false;
    }
    Window.Width = Width;
    Window.Height = Height;
    return true;
}

void DrawInspectorWindow(InspectorWindow* Window, void (*Draw)(void*), void* User)
{
    if (!Window)
    {
        return;
    }
    if (!IsWindowVisible(Window->Handle) || IsIconic(Window->Handle) || !PrepareTarget(*Window))
    {
        return;
    }

    ImGuiContext* Previous = ImGui::GetCurrentContext();
    ImGui::SetCurrentContext(Window->Context);
    if (ImGui::GetStyle().FontScaleDpi != Window->DpiScale)
    {
        ImGui::GetStyle() = ImGuiStyle();
        ImGui::GetStyle().ScaleAllSizes(Window->DpiScale);
        ImGui::GetStyle().FontScaleDpi = Window->DpiScale;
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(Window->IO->DisplaySize);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::Begin("Scene Inspector", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    Draw(User);
    ImGui::End();
    ImGui::Render();
    Window->Commands->OMSetRenderTargets(1, Window->Target.GetAddressOf(), nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    Window->Swapchain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
    ImGui::SetCurrentContext(Previous);
}
