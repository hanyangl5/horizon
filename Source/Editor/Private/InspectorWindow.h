#pragma once

#include <stdint.h>

#include "Core/IMath.h"

class TransformGizmo;
struct WindowHandle;
struct InspectorWindow;

class ViewportPicking;

InspectorWindow* CreateInspectorWindow(const WindowHandle& Owner, TransformGizmo& Gizmo, ViewportPicking& Picking);
float2           GetViewportSize(const InspectorWindow* Window, float& Dpi);
bool             IsInspectorVisible(const InspectorWindow* Window);
void             OnViewportPresented(InspectorWindow* Window);
bool             ConsumeViewportClick(InspectorWindow* Window, uint32_t Width, uint32_t Height, uint32_t& X, uint32_t& Y);
void             DestroyInspectorWindow(InspectorWindow* Window);
void             DrawInspectorWindow(InspectorWindow* Window, void (*Draw)(void*), void* User);
