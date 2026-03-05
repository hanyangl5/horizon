#pragma once

#include "scene_manager.h"
namespace Horizon
{

void AppendMeshletDataForMesh(const Mesh *mesh, u32 vertex_buffer_index,
                              const std::vector<u32> &primitive_instance_indices,
                              const std::vector<u32> &primitive_material_indices,
                              std::vector<MeshletDesc> &meshlet_descs, std::vector<u32> &meshlet_vertex_indices,
                              std::vector<u32> &meshlet_triangle_indices);

} // namespace Horizon
