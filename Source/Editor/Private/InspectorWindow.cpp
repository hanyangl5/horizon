#include "InspectorWindow.h"
#include "TransformGizmo.h"
#include "Editor/ViewportPicking.h"

#include <d3d11.h>
#include <commctrl.h>
#include <wrl/client.h>
#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "Platform/IOperatingSystem.h"
#include "Core/ILog.h"
#include "Core/ITime.h"
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
    TransformGizmo*                                Gizmo = nullptr;
    ViewportPicking*                                Picking = nullptr;
    POINT                                          Press{}, Click{};
    RECT                                           ClickRect{};
    bool                                           Pressed = false, Clicked = false, GizmoCapture = false;
    float                                          DpiScale = 1;
    int64_t                                        SampleStart = -1;
    uint32_t                                       FrameIntervals = 0;
    double                                         FrameMs = 0;
};

static constexpr wchar_t ClassName[] = L"HorizonSceneInspector";

static LRESULT CALLBACK OwnerProc(HWND Owner, UINT Message, WPARAM WParam, LPARAM LParam, UINT_PTR, DWORD_PTR User)
{
    InspectorWindow& Window = *reinterpret_cast<InspectorWindow*>(User);
    if (Message == WM_SIZE && WParam == SIZE_MINIMIZED)
    {
        Window.SampleStart = -1;
    }
    if (Message == WM_KEYDOWN && WParam == VK_F1)
    {
        if (!(LParam & (1L << 30)))
        {
            ShowWindow(Window.Handle, IsInspectorVisible(&Window) ? SW_HIDE : SW_SHOWNOACTIVATE);
        }
        return 0;
    }
    if (!IsInspectorVisible(&Window))
    {
        return DefSubclassProc(Owner, Message, WParam, LParam);
    }
    const POINT      Position = { (short)LOWORD(LParam), (short)HIWORD(LParam) };
    TransformGizmo&  Gizmo = *Window.Gizmo;
    const float2     Mouse((float)Position.x, (float)Position.y);
    if (Message == WM_KEYDOWN && !(LParam & (1L << 30)) && !(GetKeyState(VK_RBUTTON) & 0x8000) && !(GetKeyState(VK_CONTROL) & 0x8000) &&
        !(GetKeyState(VK_MENU) & 0x8000))
    {
        switch (WParam)
        {
        case 'W':
            Gizmo.SetTool(TransformGizmo::Tool::Translate);
            break;
        case 'E':
            Gizmo.SetTool(TransformGizmo::Tool::Rotate);
            break;
        case 'R':
            Gizmo.SetTool(TransformGizmo::Tool::Scale);
            break;
        }
    }
    if (Message == WM_LBUTTONDOWN && !(WParam & MK_RBUTTON) && Gizmo.Press(Mouse))
    {
        Window.GizmoCapture = true;
        SetCapture(Owner);
        Window.Pressed = Window.Clicked = false;
        return 0;
    }
    if (Message == WM_MOUSEMOVE && Window.GizmoCapture)
    {
        Gizmo.Move(Mouse);
        return 0;
    }
    if (Message == WM_LBUTTONUP && Window.GizmoCapture)
    {
        Gizmo.Release(Mouse);
        Window.GizmoCapture = false;
        ReleaseCapture();
        return 0;
    }
    const bool CancelGesture = Message == WM_SIZE || Message == WM_KILLFOCUS || Message == WM_CANCELMODE || Message == WM_DPICHANGED ||
                               Message == WM_RBUTTONDOWN || (Message == WM_KEYDOWN && WParam == VK_ESCAPE);
    if (CancelGesture || (Message == WM_CAPTURECHANGED && Window.GizmoCapture))
    {
        const bool Captured = Window.GizmoCapture && GetCapture() == Owner;
        Window.GizmoCapture = false;
        Gizmo.Cancel();
        if (Captured)
        {
            ReleaseCapture();
        }
    }
    if (Message == WM_LBUTTONDOWN)
    {
        Window.Pressed = !(WParam & MK_RBUTTON);
        Window.Press = Position;
    }
    else if (Window.Pressed && (Message == WM_MOUSEMOVE || Message == WM_LBUTTONUP))
    {
        if (abs(Position.x - Window.Press.x) >= GetSystemMetrics(SM_CXDRAG) ||
            abs(Position.y - Window.Press.y) >= GetSystemMetrics(SM_CYDRAG) || (WParam & MK_RBUTTON))
        {
            Window.Pressed = false;
        }
        if (Message == WM_LBUTTONUP)
        {
            if (Window.Pressed && GetClientRect(Owner, &Window.ClickRect) &&
                PtInRect(&Window.ClickRect, Position))
            {
                Window.Click = Position;
                Window.Clicked = true;
            }
            Window.Pressed = false;
        }
    }
    else if (CancelGesture || Message == WM_CAPTURECHANGED)
    {
        Window.Pressed = false;
        if (Message == WM_SIZE || Message == WM_DPICHANGED)
        {
            Window.Clicked = false;
        }
    }
    return DefSubclassProc(Owner, Message, WParam, LParam);
}

static void CancelViewportInput(InspectorWindow& Window)
{
    Window.Pressed = Window.Clicked = false;
    Window.Gizmo->Cancel();
    Window.Gizmo->Started = false;
    Window.Picking->Cancel();
    const HWND Captured = GetCapture();
    if (Captured == Window.Handle && Window.IO)
    {
        for (int Button = 0; Button < ImGuiMouseButton_COUNT; ++Button)
        {
            Window.IO->AddMouseButtonEvent(Button, false);
        }
    }
    if (Captured == Window.Handle || (Window.GizmoCapture && Captured == GetWindow(Window.Handle, GW_OWNER)))
    {
        ReleaseCapture();
    }
    Window.GizmoCapture = false;
}

bool IsInspectorVisible(const InspectorWindow* Window)
{
    return Window && IsWindowVisible(Window->Handle) && !IsIconic(Window->Handle);
}

void OnViewportPresented(InspectorWindow* Window)
{
    if (!IsInspectorVisible(Window))
    {
        return;
    }
    const int64_t Now = GetUSec(true);
    if (Window->SampleStart < 0)
    {
        Window->SampleStart = Now;
        Window->FrameIntervals = 0;
        Window->FrameMs = 0;
        return;
    }
    ++Window->FrameIntervals;
    const int64_t Elapsed = Now - Window->SampleStart;
    if (Elapsed >= 500000)
    {
        Window->FrameMs = double(Elapsed) / (1000.0 * Window->FrameIntervals);
        Window->SampleStart = Now;
        Window->FrameIntervals = 0;
    }
}

bool ConsumeViewportClick(InspectorWindow* Window, uint32_t Width, uint32_t Height, uint32_t& X, uint32_t& Y)
{
    if (!Window || !Window->Clicked)
    {
        return false;
    }
    Window->Clicked = false;
    if (!Width || !Height || !IsInspectorVisible(Window))
    {
        return false;
    }
    X = uint32_t(uint64_t(Window->Click.x) * Width / Window->ClickRect.right);
    Y = uint32_t(uint64_t(Window->Click.y) * Height / Window->ClickRect.bottom);
    return true;
}

static LRESULT CALLBACK InspectorProc(HWND Handle, UINT Message, WPARAM WParam, LPARAM LParam)
{
    InspectorWindow* Window = (InspectorWindow*)GetWindowLongPtrW(Handle, GWLP_USERDATA);
    if (Message == WM_CLOSE || (Message == WM_KEYDOWN && WParam == VK_F1 && !(LParam & (1L << 30))))
    {
        ShowWindow(Handle, SW_HIDE);
        return 0;
    }
    if (Window && ((Message == WM_SHOWWINDOW && !WParam) || (Message == WM_SIZE && WParam == SIZE_MINIMIZED)))
    {
        Window->SampleStart = -1;
        CancelViewportInput(*Window);
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

float2 GetViewportSize(const InspectorWindow* Window, float& Dpi)
{
    RECT Rect = {};
    if (IsInspectorVisible(Window))
    {
        const HWND Owner = GetWindow(Window->Handle, GW_OWNER);
        GetClientRect(Owner, &Rect);
        Dpi = (float)GetDpiForWindow(Owner) / 96.0f;
    }
    return float2((float)Rect.right, (float)Rect.bottom);
}

InspectorWindow* CreateInspectorWindow(const WindowHandle& Owner, TransformGizmo& Gizmo, ViewportPicking& Picking)
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
    Window->Gizmo = &Gizmo;
    Window->Picking = &Picking;
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
                           SetWindowSubclass(Owner.window, OwnerProc, (UINT_PTR)Window, (DWORD_PTR)Window);
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
    if (Window->SampleStart >= 0 && Window->FrameMs > 0)
    {
        ImGui::Text("Viewport  %.1f FPS  |  %.2f ms/frame", 1000.0 / Window->FrameMs, Window->FrameMs);
    }
    else
    {
        ImGui::TextDisabled("Viewport  -- FPS");
    }
    Draw(User);
    ImGui::End();
    ImGui::Render();
    Window->Commands->OMSetRenderTargets(1, Window->Target.GetAddressOf(), nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    Window->Swapchain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
    ImGui::SetCurrentContext(Previous);
}
