#pragma once

struct WindowHandle;
struct InspectorWindow;

InspectorWindow* CreateInspectorWindow(const WindowHandle& Owner);
void             DestroyInspectorWindow(InspectorWindow* Window);
void             DrawInspectorWindow(InspectorWindow* Window, void (*Draw)(void*), void* User);
