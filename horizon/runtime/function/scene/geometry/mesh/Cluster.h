#pragma once

#include <array>
#include <memory>
#include <thread>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>

#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/Texture.h>
#include <runtime/function/rhi/Semaphore.h>
#include <runtime/function/rhi/Pipeline.h>

#include <runtime/function/scene/material/MaterialDescription.h>

#include "BasicGeometry.h"
#include "../vertex/VertexDescription.h"
#include "../aabb/AABB.h"

namespace Horizon {

class Cluster {};
} // namespace Horizon