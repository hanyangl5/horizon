#include "BasicGeometry.h"

#include <array>
#include <vector>

#include <runtime/core/math/Math.h>
#include <runtime/core/utils/definations.h>

#include "VertexDescription.h"

namespace Horizon::BasicGeometry {

std::array<Vertex, 24> cube_vertices{
    // Top
    Vertex{Math::float3(-1, 1, -1)}, // 0
    Vertex{Math::float3(1, 1, -1)},  // 1
    Vertex{Math::float3(-1, 1, 1)},  // 2
    Vertex{Math::float3(1, 1, 1)},   // 3

    // Bottom
    Vertex{Math::float3(-1, -1, -1)}, // 4
    Vertex{Math::float3(1, -1, -1)},  // 5
    Vertex{Math::float3(-1, -1, 1)},  // 6
    Vertex{Math::float3(1, -1, 1)},   // 7

    // Front
    Vertex{Math::float3(-1, 1, 1)},  // 8
    Vertex{Math::float3(1, 1, 1)},   // 9
    Vertex{Math::float3(-1, -1, 1)}, // 10
    Vertex{Math::float3(1, -1, 1)},  // 11

    // Back
    Vertex{Math::float3(-1, 1, -1)},  // 12
    Vertex{Math::float3(1, 1, -1)},   // 13
    Vertex{Math::float3(-1, -1, -1)}, // 14
    Vertex{Math::float3(1, -1, -1)},  // 15

    // Left
    Vertex{Math::float3(-1, 1, 1)},   // 16
    Vertex{Math::float3(-1, 1, -1)},  // 17
    Vertex{Math::float3(-1, -1, 1)},  // 18
    Vertex{Math::float3(-1, -1, -1)}, // 19

    // Right
    Vertex{Math::float3(1, 1, 1)},  // 20
    Vertex{Math::float3(1, 1, -1)}, // 21
    Vertex{Math::float3(1, -1, 1)}, // 22
    Vertex{Math::float3(1, -1, -1)} // 23

};

std::array<Index, 36> cube_indices{// Top
                                   0, 1, 2, 2, 3, 1,
                                   // Bottom
                                   4, 5, 6, 6, 7, 5,
                                   // Front
                                   8, 9, 10, 10, 11, 9,
                                   // Back
                                   12, 13, 14, 14, 15, 13,
                                   // Left
                                   16, 17, 18, 18, 19, 17,
                                   // Right
                                   20, 21, 22, 22, 23, 21};

std::vector<Vertex> sphere_vertices;
std::vector<Index> sphere_indices;

std::array<Vertex, 4> quad_vertices = {
    Vertex{Math::float3(-1.0, -1.0, 0.0)}, Vertex{Math::float3(-1.0, 1.0, 0.0)},
    Vertex{Math::float3(1.0, 1.0, 0.0)}, Vertex{Math::float3(1.0, -1.0, 0.0)}};

std::array<Index, 6> quad_indices = {0, 1, 2, 1, 3, 2};

std::array<Vertex, 3> triangle_vertices = {};

std::array<Index, 3> triangle_indices = {};
} // namespace Horizon::BasicGeometry