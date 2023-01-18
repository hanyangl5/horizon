#pragma once

#include <chrono>
#include <filesystem>
#include <iniparser.h>
#include <mutex>
#include <random>
#include <shared_mutex>

#include <argparse/argparse.hpp>

#include <runtime/core/log/log.h>
#include <runtime/core/math/math.h>
#include <runtime/core/units/Units.h>
#include <runtime/core/utils/definations.h>
#include <runtime/core/utils/functions.h>
#include <runtime/core/renderdoc/RenderDoc.h>
#include <runtime/core/window/Window.h>

#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/rhi_utils.h>
#include <runtime/function/resource/resources/mesh/Mesh.h>
#include <runtime/function/resource/resource_loader/mesh/mesh_loader.h>
#include <runtime/function/scene/light/Light.h>

#include <runtime/interface/horizon_runtime.h>

#include <runtime/system/render/RenderSystem.h>


using namespace Horizon;
using namespace Horizon::Backend;

static constexpr u32 _width = 3200, _height = 1800;

static std::filesystem::path asset_path = "C:/FILES/horizon/horizon/assets";