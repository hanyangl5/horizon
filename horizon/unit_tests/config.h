#pragma once
#include <chrono>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_set>

#include <iniparser.h>

#include <argparse/argparse.hpp>

#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

#include <runtime/core/log/Log.h>
#include <runtime/core/utils/renderdoc/RenderDoc.h>
#include <runtime/core/window/Window.h>
#include <runtime/function/rhi/RHIUtils.h>
#include <runtime/function/rhi/RHI.h>
#include <runtime/function/scene/geometry/mesh/Mesh.h>
#include <runtime/interface/Engine.h>
#include <runtime/system/input/InputSystem.h>
#include <runtime/system/render/RenderSystem.h>
