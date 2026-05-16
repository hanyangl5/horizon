/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#pragma once

#ifndef FORGE_RENDERER_CONFIG_H
#define FORGE_RENDERER_CONFIG_H

// Support external config file override
#if defined(EXTERNAL_RENDERER_CONFIG_FILEPATH)
#include EXTERNAL_RENDERER_CONFIG_FILEPATH
#elif defined(EXTERNAL_RENDERER_CONFIG_FILEPATH_NO_STRING)
// When invoking clanng from FastBuild the EXTERNAL_CONFIG_FILEPATH define doesn't get expanded to a string,
// quotes are removed, that's why we add this variation of the macro that turns the define back into a valid string
#define TF_EXTERNAL_CONFIG_STRINGIFY2(x) #x
#define TF_EXTERNAL_CONFIG_STRINGIFY(x)  TF_EXTERNAL_CONFIG_STRINGIFY2(x)

#include TF_EXTERNAL_CONFIG_STRINGIFY(EXTERNAL_RENDERER_CONFIG_FILEPATH_NO_STRING)

#undef TF_EXTERNAL_CONFIG_STRINGIFY
#undef TF_EXTERNAL_CONFIG_STRINGIFY2
#else

#include "Core/IConfig.h"

// ------------------------------- renderer configuration ------------------------------- //

// Horizon only keeps the Windows Direct3D 12 backend.
#if defined(_WINDOWS)
#include "Direct3D12/Direct3D12Config.h"
#else
#error "Horizon only supports the Windows Direct3D12 renderer backend."
#endif

// Uncomment this macro to define custom rendering max options
// #define RENDERER_CUSTOM_MAX
#ifdef RENDERER_CUSTOM_MAX
enum
{
    MAX_INSTANCE_EXTENSIONS = 64,
    MAX_DEVICE_EXTENSIONS = 64,
    MAX_RENDER_TARGET_ATTACHMENTS = 8,
    MAX_VERTEX_BINDINGS = 15,
    MAX_VERTEX_ATTRIBS = 15,
    MAX_SEMANTIC_NAME_LENGTH = 128,
    MAX_DEBUG_NAME_LENGTH = 128,
    MAX_MIP_LEVELS = 0xFFFFFFFF,
    MAX_SWAPCHAIN_IMAGES = 3,
    MAX_GPU_VENDOR_STRING_LENGTH = 64, // max size for GPUVendorPreset strings
};
#endif

// Enable raytracing if available.
#if defined(D3D12_RAYTRACING_AVAILABLE)
#define ENABLE_RAYTRACING
#endif

#if defined(DIRECT3D12)
#define ENABLE_GPU_PROFILER
#endif

// Enable graphics debug if general debug is turned on
#ifdef FORGE_DEBUG
#define ENABLE_GRAPHICS_DEBUG
#endif

#ifdef NSIGHT_AFTERMATH_AVAILABLE
#ifdef FORGE_DEBUG
// #define ENABLE_NSIGHT_AFTERMATH
#endif
#endif

#if !defined(DIRECT3D12)
#error "No rendering API defined"
#endif

#if defined(ANDROID) || defined(SWITCH) || defined(TARGET_APPLE_ARM64)
#define USE_MSAA_RESOLVE_ATTACHMENTS
#endif

#ifdef FORGE_DEBUG
#define ENABLE_DEPENDENCY_TRACKER
#endif

#if defined(_WIN32)
#define FORGE_D3D12_DYNAMIC_LOADING
#endif

#endif
#endif

// ------------------------------- gpu configuration rules ------------------------------- //

struct GPUSettings;
struct GPUCapBits;

struct ExtendedSettings
{
    uint32_t     mNumSettings;
    uint32_t*    pSettings;
    const char** ppSettingNames;
};

enum GPUPresetLevel : uint32_t
{
    GPU_PRESET_NONE = 0,
    GPU_PRESET_OFFICE,  // This means unsupported
    GPU_PRESET_VERYLOW, // Mostly for mobile GPU
    GPU_PRESET_LOW,
    GPU_PRESET_MEDIUM,
    GPU_PRESET_HIGH,
    GPU_PRESET_ULTRA,
    GPU_PRESET_COUNT
};

// initialize built-in GPU selection defaults
FORGE_API void addGPUConfigurationRules(ExtendedSettings* pExtendedSettings);

// free GPU selection scratch data
FORGE_API void removeGPUConfigurationRules();

// set default value, samplerAnisotropySupported, graphicsQueueSupported, primitiveID
FORGE_API void setDefaultGPUSettings(struct GPUSettings* pGpuSettings);

// selects the best GPU for the playground's built-in adapter policy
FORGE_API uint32_t util_select_best_gpu(struct GPUSettings* availableSettings, uint32_t gpuCount);

// returns the built-in default/preset level
FORGE_API GPUPresetLevel getDefaultPresetLevel();
FORGE_API GPUPresetLevel getGPUPresetLevel(uint32_t vendorId, uint32_t modelId, const char* vendorName, const char* modelName);

// apply built-in backend defaults to a single GPUSettings
FORGE_API void applyGPUConfigurationRules(struct GPUSettings* pGpuSettings, struct GPUCapBits* pCapBits);

// kept for API compatibility; external extended settings are not changed by built-in GPU selection
FORGE_API void setupExtendedSettings(ExtendedSettings* pExtendedSettings, const struct GPUSettings* pGpuSettings);

// kept for API compatibility; driver rejection tables are not used
FORGE_API bool checkDriverRejectionSettings(const struct GPUSettings* pGpuSettings);

// ------ utilities ------
FORGE_API const char*    presetLevelToString(GPUPresetLevel preset);
FORGE_API GPUPresetLevel stringToPresetLevel(const char* presetLevel);
FORGE_API bool           gpuVendorEquals(uint32_t vendorId, const char* vendorName);
FORGE_API const char*    getGPUVendorName(uint32_t modelId);
FORGE_API uint32_t       getGPUVendorID(const char*);
