/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IMath.h"

#define IM_ASSERT(Expression) ASSERT(static_cast<bool>(Expression))
#define IMGUI_API             FORGE_API
#define IMGUI_DISABLE_DEFAULT_ALLOCATORS
#define ImTextureID_Invalid ((ImTextureID) - 1)

#define IM_VEC2_CLASS_EXTRA                                \
    ImVec2(const float2& Value): x(Value.x), y(Value.y) {} \
    operator float2() const { return float2(x, y); }
#define IM_VEC4_CLASS_EXTRA                                                        \
    ImVec4(const float4& Value): x(Value.x), y(Value.y), z(Value.z), w(Value.w) {} \
    operator float4() const { return float4(x, y, z, w); }

#ifdef FORGE_UI_USE_32BIT_INDEXES
#define ImDrawIdx unsigned int
#endif

#define IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
struct FileStream;
using ImFileHandle = FileStream*;
ImFileHandle       ImFileOpen(const char* Filename, const char* Mode);
bool               ImFileClose(ImFileHandle File);
unsigned long long ImFileGetSize(ImFileHandle File);
unsigned long long ImFileRead(void* Data, unsigned long long Size, unsigned long long Count, ImFileHandle File);
unsigned long long ImFileWrite(const void* Data, unsigned long long Size, unsigned long long Count, ImFileHandle File);
