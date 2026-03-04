#include "scene_manager_meshlet.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <type_traits>
#include <unordered_map>

#include <core/log.h>
#include <core/path.h>

namespace Horizon
{

namespace
{
constexpr u32 k_meshlet_max_vertices = 64;
constexpr u32 k_meshlet_max_triangles = 124;
constexpr u32 k_meshlet_cache_version = 1;
constexpr char k_meshlet_cache_magic[8] = {'H', 'M', 'S', 'H', 'L', 'T', '0', '1'};

struct MeshletScratch
{
    std::vector<u32> vertices{};
    std::vector<u32> packed_triangles{};
    std::unordered_map<u32, u32> global_to_local{};
};

struct PrimitiveMeshletRange
{
    u32 first_meshlet{};
    u32 meshlet_count{};
};

struct CachedMeshletDesc
{
    f32 bounding_sphere[4]{};
    f32 cone_axis_cutoff[4]{};
    u32 vertex_offset{};
    u32 vertex_count{};
    u32 triangle_offset{};
    u32 triangle_count{};
};

struct MeshletCacheHeader
{
    char magic[8]{};
    u32 version{};
    i64 source_timestamp{};
    u32 vertex_count{};
    u32 index_count{};
    u32 primitive_count{};
    u32 meshlet_count{};
    u32 vertex_index_count{};
    u32 triangle_index_count{};
};

struct MeshMeshletCache
{
    std::vector<MeshletDesc> descs{};
    std::vector<u32> vertex_indices{};
    std::vector<u32> triangle_indices{};
    std::vector<PrimitiveMeshletRange> primitive_ranges{};
};

Path GetMeshletCachePath(const Mesh *mesh)
{
    (void)mesh;
    return Path("tesemesh.meshlet_cache.bin");
}

i64 GetMeshFileTimestamp(const Mesh *mesh)
{
    std::error_code ec{};
    const auto t = std::filesystem::last_write_time(std::filesystem::path(mesh->m_path), ec);
    if (ec)
    {
        return 0;
    }
    return static_cast<i64>(t.time_since_epoch().count());
}

bool SaveMeshletCache(const Mesh *mesh, const MeshMeshletCache &cache)
{
    static_assert(std::is_trivially_copyable_v<PrimitiveMeshletRange>, "PrimitiveMeshletRange must be POD");
    static_assert(std::is_trivially_copyable_v<CachedMeshletDesc>, "CachedMeshletDesc must be POD");

    const Path cache_path = GetMeshletCachePath(mesh);
    const std::filesystem::path fs_path(cache_path.string());
    const std::filesystem::path parent = fs_path.parent_path();
    std::error_code ec{};
    if (!parent.empty())
    {
        std::filesystem::create_directories(parent, ec);
        if (ec)
        {
            LOG_WARN("Failed to create meshlet cache directory: {}", cache_path.string());
            return false;
        }
    }

    std::ofstream out(cache_path.string(), std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        LOG_WARN("Failed to open meshlet cache for write: {}", cache_path.string());
        return false;
    }

    MeshletCacheHeader header{};
    std::copy(std::begin(k_meshlet_cache_magic), std::end(k_meshlet_cache_magic), std::begin(header.magic));
    header.version = k_meshlet_cache_version;
    header.source_timestamp = GetMeshFileTimestamp(mesh);
    header.vertex_count = static_cast<u32>(mesh->m_vertices.size());
    header.index_count = static_cast<u32>(mesh->m_indices.size());
    header.primitive_count = static_cast<u32>(mesh->m_mesh_primitives.size());
    header.meshlet_count = static_cast<u32>(cache.descs.size());
    header.vertex_index_count = static_cast<u32>(cache.vertex_indices.size());
    header.triangle_index_count = static_cast<u32>(cache.triangle_indices.size());

    std::vector<CachedMeshletDesc> serialized_descs(cache.descs.size());
    for (size_t i = 0; i < cache.descs.size(); ++i)
    {
        const auto &src = cache.descs[i];
        auto &dst = serialized_descs[i];
        dst.bounding_sphere[0] = src.bounding_sphere.x;
        dst.bounding_sphere[1] = src.bounding_sphere.y;
        dst.bounding_sphere[2] = src.bounding_sphere.z;
        dst.bounding_sphere[3] = src.bounding_sphere.w;
        dst.cone_axis_cutoff[0] = src.cone_axis_cutoff.x;
        dst.cone_axis_cutoff[1] = src.cone_axis_cutoff.y;
        dst.cone_axis_cutoff[2] = src.cone_axis_cutoff.z;
        dst.cone_axis_cutoff[3] = src.cone_axis_cutoff.w;
        dst.vertex_offset = src.vertex_offset;
        dst.vertex_count = src.vertex_count;
        dst.triangle_offset = src.triangle_offset;
        dst.triangle_count = src.triangle_count;
    }

    out.write(reinterpret_cast<const char *>(&header), sizeof(header));
    if (!cache.primitive_ranges.empty())
    {
        out.write(reinterpret_cast<const char *>(cache.primitive_ranges.data()),
                  cache.primitive_ranges.size() * sizeof(PrimitiveMeshletRange));
    }
    if (!serialized_descs.empty())
    {
        out.write(reinterpret_cast<const char *>(serialized_descs.data()),
                  serialized_descs.size() * sizeof(CachedMeshletDesc));
    }
    if (!cache.vertex_indices.empty())
    {
        out.write(reinterpret_cast<const char *>(cache.vertex_indices.data()), cache.vertex_indices.size() * sizeof(u32));
    }
    if (!cache.triangle_indices.empty())
    {
        out.write(reinterpret_cast<const char *>(cache.triangle_indices.data()),
                  cache.triangle_indices.size() * sizeof(u32));
    }

    if (!out.good())
    {
        LOG_WARN("Failed to save meshlet cache: {}", cache_path.string());
        return false;
    }

    return true;
}

bool LoadMeshletCache(const Mesh *mesh, MeshMeshletCache &cache)
{
    const Path cache_path = GetMeshletCachePath(mesh);
    std::ifstream in(cache_path.string(), std::ios::binary);
    if (!in.is_open())
    {
        return false;
    }

    MeshletCacheHeader header{};
    in.read(reinterpret_cast<char *>(&header), sizeof(header));
    if (!in.good())
    {
        return false;
    }

    if (!std::equal(std::begin(header.magic), std::end(header.magic), std::begin(k_meshlet_cache_magic)))
    {
        return false;
    }
    if (header.version != k_meshlet_cache_version)
    {
        return false;
    }

    if (header.source_timestamp != GetMeshFileTimestamp(mesh) || header.vertex_count != mesh->m_vertices.size() ||
        header.index_count != mesh->m_indices.size() || header.primitive_count != mesh->m_mesh_primitives.size())
    {
        return false;
    }

    cache.primitive_ranges.resize(header.primitive_count);
    std::vector<CachedMeshletDesc> serialized_descs(header.meshlet_count);
    cache.vertex_indices.resize(header.vertex_index_count);
    cache.triangle_indices.resize(header.triangle_index_count);

    if (!cache.primitive_ranges.empty())
    {
        in.read(reinterpret_cast<char *>(cache.primitive_ranges.data()),
                cache.primitive_ranges.size() * sizeof(PrimitiveMeshletRange));
    }
    if (!serialized_descs.empty())
    {
        in.read(reinterpret_cast<char *>(serialized_descs.data()), serialized_descs.size() * sizeof(CachedMeshletDesc));
    }
    if (!cache.vertex_indices.empty())
    {
        in.read(reinterpret_cast<char *>(cache.vertex_indices.data()), cache.vertex_indices.size() * sizeof(u32));
    }
    if (!cache.triangle_indices.empty())
    {
        in.read(reinterpret_cast<char *>(cache.triangle_indices.data()), cache.triangle_indices.size() * sizeof(u32));
    }
    if (!in.good())
    {
        return false;
    }

    cache.descs.resize(serialized_descs.size());
    for (size_t i = 0; i < serialized_descs.size(); ++i)
    {
        const auto &src = serialized_descs[i];
        auto &dst = cache.descs[i];
        dst.bounding_sphere =
            Math::float4(src.bounding_sphere[0], src.bounding_sphere[1], src.bounding_sphere[2], src.bounding_sphere[3]);
        dst.cone_axis_cutoff = Math::float4(src.cone_axis_cutoff[0], src.cone_axis_cutoff[1], src.cone_axis_cutoff[2],
                                            src.cone_axis_cutoff[3]);
        dst.vertex_offset = src.vertex_offset;
        dst.vertex_count = src.vertex_count;
        dst.triangle_offset = src.triangle_offset;
        dst.triangle_count = src.triangle_count;
        dst.vertex_buffer_index = 0;
        dst.instance_index = 0;
        dst.material_index = 0;
    }

    for (const auto &range : cache.primitive_ranges)
    {
        if (range.first_meshlet + range.meshlet_count > cache.descs.size())
        {
            return false;
        }
    }

    return true;
}

Math::float4 ComputeMeshletBoundingSphere(const Mesh *mesh, const std::vector<u32> &meshlet_vertices)
{
    if (meshlet_vertices.empty())
    {
        return Math::float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    Math::float3 center(0.0f, 0.0f, 0.0f);
    for (u32 global_vertex_index : meshlet_vertices)
    {
        center += mesh->m_vertices[global_vertex_index].pos;
    }
    center /= static_cast<f32>(meshlet_vertices.size());

    f32 max_distance_sq = 0.0f;
    for (u32 global_vertex_index : meshlet_vertices)
    {
        const Math::float3 delta = mesh->m_vertices[global_vertex_index].pos - center;
        max_distance_sq = std::max(max_distance_sq, delta.LengthSquared());
    }

    return Math::float4(center.x, center.y, center.z, std::sqrt(max_distance_sq));
}

Math::float4 ComputeMeshletNormalCone(const Mesh *mesh, const std::vector<u32> &meshlet_vertices,
                                      const std::vector<u32> &packed_triangles)
{
    if (meshlet_vertices.empty() || packed_triangles.empty())
    {
        return Math::float4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    std::vector<Math::float3> face_normals{};
    face_normals.reserve(packed_triangles.size());
    Math::float3 normal_sum(0.0f, 0.0f, 0.0f);

    for (u32 packed_triangle : packed_triangles)
    {
        const u32 i0 = packed_triangle & 0xFFu;
        const u32 i1 = (packed_triangle >> 8u) & 0xFFu;
        const u32 i2 = (packed_triangle >> 16u) & 0xFFu;

        if (i0 >= meshlet_vertices.size() || i1 >= meshlet_vertices.size() || i2 >= meshlet_vertices.size())
        {
            continue;
        }

        const Math::float3 &p0 = mesh->m_vertices[meshlet_vertices[i0]].pos;
        const Math::float3 &p1 = mesh->m_vertices[meshlet_vertices[i1]].pos;
        const Math::float3 &p2 = mesh->m_vertices[meshlet_vertices[i2]].pos;

        Math::float3 normal = Math::Cross(p1 - p0, p2 - p0);
        const f32 normal_length_sq = normal.LengthSquared();
        if (normal_length_sq <= 1.0e-12f)
        {
            continue;
        }

        normal /= std::sqrt(normal_length_sq);
        face_normals.push_back(normal);
        normal_sum += normal;
    }

    if (face_normals.empty())
    {
        return Math::float4(0.0f, 0.0f, 1.0f, 1.0f);
    }

    Math::float3 axis = normal_sum;
    if (axis.LengthSquared() <= 1.0e-12f)
    {
        axis = face_normals.front();
    }
    else
    {
        axis.Normalize();
    }

    f32 cutoff = 1.0f;
    for (const Math::float3 &normal : face_normals)
    {
        cutoff = std::min(cutoff, axis.Dot(normal));
    }

    return Math::float4(axis.x, axis.y, axis.z, cutoff);
}

void FlushMeshlet(const Mesh *mesh, u32 instance_index, u32 material_index, u32 vertex_buffer_index, MeshletScratch &scratch,
                  std::vector<MeshletDesc> &out_descs, std::vector<u32> &out_vertex_indices,
                  std::vector<u32> &out_triangle_indices)
{
    if (scratch.vertices.empty() || scratch.packed_triangles.empty())
    {
        return;
    }

    MeshletDesc desc{};
    desc.vertex_offset = static_cast<u32>(out_vertex_indices.size());
    desc.vertex_count = static_cast<u32>(scratch.vertices.size());
    desc.triangle_offset = static_cast<u32>(out_triangle_indices.size());
    desc.triangle_count = static_cast<u32>(scratch.packed_triangles.size());
    desc.vertex_buffer_index = vertex_buffer_index;
    desc.instance_index = instance_index;
    desc.material_index = material_index;
    desc.bounding_sphere = ComputeMeshletBoundingSphere(mesh, scratch.vertices);
    desc.cone_axis_cutoff = ComputeMeshletNormalCone(mesh, scratch.vertices, scratch.packed_triangles);

    out_vertex_indices.insert(out_vertex_indices.end(), scratch.vertices.begin(), scratch.vertices.end());
    out_triangle_indices.insert(out_triangle_indices.end(), scratch.packed_triangles.begin(), scratch.packed_triangles.end());
    out_descs.push_back(desc);

    scratch.vertices.clear();
    scratch.packed_triangles.clear();
    scratch.global_to_local.clear();
}

void BuildPrimitiveMeshlets(const Mesh *mesh, const MeshPrimitive &primitive, u32 instance_index, u32 material_index,
                            u32 vertex_buffer_index, std::vector<MeshletDesc> &out_descs,
                            std::vector<u32> &out_vertex_indices, std::vector<u32> &out_triangle_indices)
{
    MeshletScratch scratch{};

    const u32 first_index = primitive.index_offset;
    const u32 last_index = primitive.index_offset + primitive.index_count;
    const u32 safe_last_index = std::min(last_index, static_cast<u32>(mesh->m_indices.size()));

    for (u32 i = first_index; i + 2 < safe_last_index; i += 3)
    {
        const u32 tri_global[3] = {mesh->m_indices[i], mesh->m_indices[i + 1], mesh->m_indices[i + 2]};
        if (tri_global[0] >= mesh->m_vertices.size() || tri_global[1] >= mesh->m_vertices.size() ||
            tri_global[2] >= mesh->m_vertices.size())
        {
            continue;
        }

        u32 new_vertex_count = 0;
        for (u32 global_index : tri_global)
        {
            if (scratch.global_to_local.find(global_index) == scratch.global_to_local.end())
            {
                ++new_vertex_count;
            }
        }

        if (!scratch.packed_triangles.empty() &&
            (scratch.vertices.size() + new_vertex_count > k_meshlet_max_vertices ||
             scratch.packed_triangles.size() + 1 > k_meshlet_max_triangles))
        {
            FlushMeshlet(mesh, instance_index, material_index, vertex_buffer_index, scratch, out_descs, out_vertex_indices,
                         out_triangle_indices);
        }

        u32 tri_local[3]{};
        for (u32 lane = 0; lane < 3; ++lane)
        {
            const u32 global_index = tri_global[lane];
            auto local_iter = scratch.global_to_local.find(global_index);
            if (local_iter == scratch.global_to_local.end())
            {
                const u32 local_index = static_cast<u32>(scratch.vertices.size());
                scratch.vertices.push_back(global_index);
                scratch.global_to_local.emplace(global_index, local_index);
                tri_local[lane] = local_index;
            }
            else
            {
                tri_local[lane] = local_iter->second;
            }
        }

        scratch.packed_triangles.push_back((tri_local[0] & 0xFFu) | ((tri_local[1] & 0xFFu) << 8u) |
                                           ((tri_local[2] & 0xFFu) << 16u));
    }

    FlushMeshlet(mesh, instance_index, material_index, vertex_buffer_index, scratch, out_descs, out_vertex_indices,
                 out_triangle_indices);
}

MeshMeshletCache BuildMeshletCacheRuntime(const Mesh *mesh)
{
    MeshMeshletCache cache{};
    cache.primitive_ranges.resize(mesh->m_mesh_primitives.size());
    for (u32 primitive_index = 0; primitive_index < mesh->m_mesh_primitives.size(); ++primitive_index)
    {
        auto &range = cache.primitive_ranges[primitive_index];
        range.first_meshlet = static_cast<u32>(cache.descs.size());
        BuildPrimitiveMeshlets(mesh, mesh->m_mesh_primitives[primitive_index], 0, 0, 0, cache.descs, cache.vertex_indices,
                               cache.triangle_indices);
        range.meshlet_count = static_cast<u32>(cache.descs.size()) - range.first_meshlet;
    }
    return cache;
}

MeshMeshletCache LoadOrBuildMeshletCache(const Mesh *mesh)
{
    MeshMeshletCache cache{};
    if (LoadMeshletCache(mesh, cache))
    {
        LOG_INFO("Loaded meshlet cache: {}", GetMeshletCachePath(mesh).string());
        return cache;
    }

    cache = BuildMeshletCacheRuntime(mesh);
    if (SaveMeshletCache(mesh, cache))
    {
        LOG_INFO("Saved meshlet cache: {}", GetMeshletCachePath(mesh).string());
    }
    return cache;
}
} // namespace

void AppendMeshletDataForMesh(const Mesh *mesh, u32 vertex_buffer_index, const std::vector<u32> &primitive_instance_indices,
                              const std::vector<u32> &primitive_material_indices, std::vector<MeshletDesc> &meshlet_descs,
                              std::vector<u32> &meshlet_vertex_indices, std::vector<u32> &meshlet_triangle_indices)
{
    MeshMeshletCache mesh_cache = LoadOrBuildMeshletCache(mesh);
    bool cache_layout_valid = (mesh_cache.primitive_ranges.size() == mesh->m_mesh_primitives.size());
    if (cache_layout_valid)
    {
        for (const auto &range : mesh_cache.primitive_ranges)
        {
            if (range.first_meshlet + range.meshlet_count > mesh_cache.descs.size())
            {
                cache_layout_valid = false;
                break;
            }
        }
    }
    if (!cache_layout_valid)
    {
        LOG_WARN("Meshlet cache layout invalid for '{}', rebuilding runtime.", mesh->m_path);
        mesh_cache = BuildMeshletCacheRuntime(mesh);
        SaveMeshletCache(mesh, mesh_cache);
    }

    if (primitive_instance_indices.size() != mesh_cache.primitive_ranges.size() ||
        primitive_material_indices.size() != mesh_cache.primitive_ranges.size())
    {
        LOG_WARN("Primitive metadata size mismatch for '{}', skip meshlet append.", mesh->m_path);
        return;
    }

    const u32 vertex_index_base = static_cast<u32>(meshlet_vertex_indices.size());
    const u32 triangle_index_base = static_cast<u32>(meshlet_triangle_indices.size());
    meshlet_vertex_indices.insert(meshlet_vertex_indices.end(), mesh_cache.vertex_indices.begin(), mesh_cache.vertex_indices.end());
    meshlet_triangle_indices.insert(meshlet_triangle_indices.end(), mesh_cache.triangle_indices.begin(),
                                    mesh_cache.triangle_indices.end());

    for (u32 primitive_index = 0; primitive_index < mesh_cache.primitive_ranges.size(); ++primitive_index)
    {
        const auto &range = mesh_cache.primitive_ranges[primitive_index];
        for (u32 i = 0; i < range.meshlet_count; ++i)
        {
            MeshletDesc desc = mesh_cache.descs[range.first_meshlet + i];
            desc.vertex_offset += vertex_index_base;
            desc.triangle_offset += triangle_index_base;
            desc.vertex_buffer_index = vertex_buffer_index;
            desc.instance_index = primitive_instance_indices[primitive_index];
            desc.material_index = primitive_material_indices[primitive_index];
            meshlet_descs.push_back(desc);
        }
    }
}

} // namespace Horizon

