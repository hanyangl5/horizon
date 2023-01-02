#pragma once
#include <chrono>
#include <mutex>
#include <shared_mutex>



#include <iniparser.h>

#include <argparse/argparse.hpp>

#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

#include <runtime/core/log/Log.h>
#include <runtime/core/utils/renderdoc/RenderDoc.h>
#include <runtime/core/window/Window.h>
#include <runtime/function/rhi/rhi_utils.h>
#include <runtime/function/rhi/RHI.h>
#include <runtime/function/resource/mesh/Mesh.h>
#include <runtime/interface/Engine.h>
#include <runtime/system/input/InputSystem.h>
#include <runtime/system/render/RenderSystem.h>
#include <runtime/function/texture_loader/texture_loader.h>