#pragma once


#include <thread>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>

#include <runtime/function/rhi/Buffer.h>
#include <runtime/function/rhi/Pipeline.h>
#include <runtime/function/rhi/RHI.h>
#include <runtime/function/rhi/Semaphore.h>
#include <runtime/function/rhi/Texture.h>

#include <runtime/function/scene/material/MaterialDescription.h>

#include "../aabb/AABB.h"
#include "../vertex/VertexDescription.h"

namespace Horizon {

class Cluster {};
} // namespace Horizon