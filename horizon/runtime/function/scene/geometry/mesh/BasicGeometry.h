#pragma once

#include <array>
#include <vector>

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>

#include "../vertex/VertexDescription.h"

namespace Horizon::BasicGeometry {

enum class BasicGeometry { QUAD, TRIANGLE, CUBE, SPHERE, CAPSULE };

extern std::array<Vertex, 24> cube_vertices;

extern std::array<Index, 36> cube_indices;

extern std::vector<Vertex> sphere_vertices;

extern std::vector<Index> sphere_indices;

extern std::array<Vertex, 4> quad_vertices;

extern std::array<Index, 6> quad_indices;

extern std::array<Vertex, 3> triangle_vertices;

extern std::array<Index, 3> triangle_indices;

} // namespace Horizon::BasicGeometry