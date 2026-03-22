#include "mesh_importer.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <core/log.h>

#include "mesh.h"
#include "mesh_import_utils.h"

#if defined(HORIZON_HAS_FBXSDK)
#include <fbxsdk.h>
#endif

namespace Horizon
{

#if defined(HORIZON_HAS_FBXSDK)
namespace
{
using namespace MeshImportUtils;

Math::float4x4 FbxMatrixToInternal(const FbxAMatrix &matrix)
{
    float external[16] = {
        static_cast<float>(matrix.Get(0, 0)), static_cast<float>(matrix.Get(1, 0)), static_cast<float>(matrix.Get(2, 0)),
        static_cast<float>(matrix.Get(3, 0)), static_cast<float>(matrix.Get(0, 1)), static_cast<float>(matrix.Get(1, 1)),
        static_cast<float>(matrix.Get(2, 1)), static_cast<float>(matrix.Get(3, 1)), static_cast<float>(matrix.Get(0, 2)),
        static_cast<float>(matrix.Get(1, 2)), static_cast<float>(matrix.Get(2, 2)), static_cast<float>(matrix.Get(3, 2)),
        static_cast<float>(matrix.Get(0, 3)), static_cast<float>(matrix.Get(1, 3)), static_cast<float>(matrix.Get(2, 3)),
        static_cast<float>(matrix.Get(3, 3)),
    };
    return ExternalToInternalMatrix(external);
}

Math::float3 ToFloat3(const FbxDouble3 &value)
{
    return Math::float3(static_cast<float>(value[0]), static_cast<float>(value[1]), static_cast<float>(value[2]));
}

Math::float4 ToFloat4(const FbxQuaternion &value)
{
    return Math::float4(static_cast<float>(value[0]), static_cast<float>(value[1]), static_cast<float>(value[2]),
                        static_cast<float>(value[3]));
}

void AssignFileTexture(Material &material, MaterialTextureType type, FbxProperty property, u32 bitmask)
{
    if (!property.IsValid())
    {
        return;
    }

    const int texture_count = property.GetSrcObjectCount<FbxFileTexture>();
    if (texture_count <= 0)
    {
        return;
    }

    FbxFileTexture *texture = property.GetSrcObject<FbxFileTexture>(0);
    if (!texture || !texture->GetFileName())
    {
        return;
    }

    material.material_textures[type] = MaterialTextureDescription(Path(texture->GetFileName()));
    material.material_params.param_bitmask |= bitmask;
}

u32 GetOrCreateMaterialIndex(Mesh &mesh, FbxSurfaceMaterial *material,
                             std::unordered_map<FbxSurfaceMaterial *, u32> &material_indices)
{
    if (!material)
    {
        if (mesh.materials.empty())
        {
            mesh.materials.emplace_back();
        }
        return 0;
    }

    const auto found = material_indices.find(material);
    if (found != material_indices.end())
    {
        return found->second;
    }

    const u32 index = static_cast<u32>(mesh.materials.size());
    material_indices[material] = index;
    mesh.materials.emplace_back();
    Material &destination = mesh.materials.back();

    if (material->GetClassId().Is(FbxSurfacePhong::ClassId))
    {
        auto *phong = static_cast<FbxSurfacePhong *>(material);
        destination.material_params.base_color_factor = ToFloat3(phong->Diffuse.Get());
        destination.material_params.emmissive_factor = ToFloat3(phong->Emissive.Get());
    }
    else if (material->GetClassId().Is(FbxSurfaceLambert::ClassId))
    {
        auto *lambert = static_cast<FbxSurfaceLambert *>(material);
        destination.material_params.base_color_factor = ToFloat3(lambert->Diffuse.Get());
        destination.material_params.emmissive_factor = ToFloat3(lambert->Emissive.Get());
    }

    const FbxProperty transparency_factor = material->FindProperty(FbxSurfaceMaterial::sTransparencyFactor);
    if (transparency_factor.IsValid() && transparency_factor.Get<FbxDouble>() > 0.0)
    {
        destination.blend_state = BlendState::BLEND_STATE_TRANSPARENT;
        destination.material_params.param_bitmask |= HAS_ALPHA;
    }

    AssignFileTexture(destination, MaterialTextureType::BASE_COLOR, material->FindProperty(FbxSurfaceMaterial::sDiffuse),
                      HAS_BASE_COLOR);
    AssignFileTexture(destination, MaterialTextureType::NORMAL, material->FindProperty(FbxSurfaceMaterial::sNormalMap),
                      HAS_NORMAL);
    if (destination.material_textures.find(MaterialTextureType::NORMAL) == destination.material_textures.end())
    {
        AssignFileTexture(destination, MaterialTextureType::NORMAL, material->FindProperty(FbxSurfaceMaterial::sBump),
                          HAS_NORMAL);
    }
    AssignFileTexture(destination, MaterialTextureType::EMISSIVE, material->FindProperty(FbxSurfaceMaterial::sEmissive),
                      HAS_EMISSIVE);

    return index;
}

void AssignControlPointWeight(std::vector<Math::float4> &joint_indices, std::vector<Math::float4> &joint_weights,
                              int control_point_index, u32 joint_index, float weight)
{
    if (control_point_index < 0 || static_cast<size_t>(control_point_index) >= joint_indices.size())
    {
        return;
    }

    Math::float4 &indices = joint_indices[control_point_index];
    Math::float4 &weights = joint_weights[control_point_index];
    float *weight_slots = &weights.x;
    float *index_slots = &indices.x;

    for (u32 slot = 0; slot < 4; ++slot)
    {
        if (weight_slots[slot] == 0.0f)
        {
            weight_slots[slot] = weight;
            index_slots[slot] = static_cast<float>(joint_index);
            return;
        }
    }

    u32 minimum_slot = 0;
    for (u32 slot = 1; slot < 4; ++slot)
    {
        if (weight_slots[slot] < weight_slots[minimum_slot])
        {
            minimum_slot = slot;
        }
    }

    if (weight > weight_slots[minimum_slot])
    {
        weight_slots[minimum_slot] = weight;
        index_slots[minimum_slot] = static_cast<float>(joint_index);
    }
}

void CollectNodeCurveTimes(FbxNode *node, FbxAnimLayer *layer, std::vector<double> &times)
{
    const auto append_curve_times = [&times](FbxAnimCurve *curve) {
        if (!curve)
        {
            return;
        }
        const int key_count = curve->KeyGetCount();
        for (int key_index = 0; key_index < key_count; ++key_index)
        {
            times.push_back(curve->KeyGetTime(key_index).GetSecondDouble());
        }
    };

    append_curve_times(node->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X));
    append_curve_times(node->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y));
    append_curve_times(node->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z));
    append_curve_times(node->LclRotation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X));
    append_curve_times(node->LclRotation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y));
    append_curve_times(node->LclRotation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z));
    append_curve_times(node->LclScaling.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X));
    append_curve_times(node->LclScaling.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y));
    append_curve_times(node->LclScaling.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z));
}

void PopulateAnimationChannels(MeshAnimationClip &clip, FbxNode *node, FbxAnimLayer *layer,
                               const std::unordered_map<FbxNode *, u32> &node_indices)
{
    auto found = node_indices.find(node);
    if (found != node_indices.end())
    {
        std::vector<double> times;
        CollectNodeCurveTimes(node, layer, times);
        std::sort(times.begin(), times.end());
        times.erase(std::unique(times.begin(), times.end()), times.end());

        if (!times.empty())
        {
            MeshNodeAnimationChannel channel{};
            channel.node_index = found->second;
            channel.position_times.reserve(times.size());
            channel.position_values.reserve(times.size());
            channel.rotation_times.reserve(times.size());
            channel.rotation_values.reserve(times.size());
            channel.scale_times.reserve(times.size());
            channel.scale_values.reserve(times.size());

            for (double seconds : times)
            {
                FbxTime time;
                time.SetSecondDouble(seconds);

                const FbxDouble3 translation = node->LclTranslation.EvaluateValue(time);
                const FbxDouble3 rotation_euler = node->LclRotation.EvaluateValue(time);
                const FbxDouble3 scale = node->LclScaling.EvaluateValue(time);

                FbxAMatrix rotation_matrix;
                rotation_matrix.SetR(FbxVector4(rotation_euler[0], rotation_euler[1], rotation_euler[2]));
                const FbxQuaternion rotation = rotation_matrix.GetQ();

                channel.position_times.push_back(static_cast<f32>(seconds));
                channel.position_values.push_back(ToFloat3(translation));
                channel.rotation_times.push_back(static_cast<f32>(seconds));
                channel.rotation_values.push_back(ToFloat4(rotation));
                channel.scale_times.push_back(static_cast<f32>(seconds));
                channel.scale_values.push_back(ToFloat3(scale));
                clip.duration = std::max(clip.duration, static_cast<f32>(seconds));
            }

            clip.channels.emplace_back(std::move(channel));
        }
    }

    const int child_count = node->GetChildCount();
    for (int child_index = 0; child_index < child_count; ++child_index)
    {
        PopulateAnimationChannels(clip, node->GetChild(child_index), layer, node_indices);
    }
}

u32 ImportNode(Mesh &mesh, FbxNode *node, u32 parent_index, std::unordered_map<FbxNode *, u32> &node_indices,
               std::unordered_map<FbxSurfaceMaterial *, u32> &material_indices, std::unordered_map<FbxNode *, u32> &joint_nodes)
{
    if (!node)
    {
        return INVALID_NODE_INDEX;
    }

    Node destination{};
    destination.parent = parent_index;
    destination.name = node->GetName() ? node->GetName() : "";

    const FbxAMatrix local_transform = node->EvaluateLocalTransform();
    const FbxVector4 translation = local_transform.GetT();
    const FbxQuaternion rotation = local_transform.GetQ();
    const FbxVector4 scale = local_transform.GetS();

    destination.translation =
        Math::float3(static_cast<float>(translation[0]), static_cast<float>(translation[1]), static_cast<float>(translation[2]));
    destination.rotation = ToFloat4(rotation);
    destination.scale = Math::float3(static_cast<float>(scale[0]), static_cast<float>(scale[1]), static_cast<float>(scale[2]));
    destination.local_matrix = ComposeMatrix(destination.translation, destination.rotation, destination.scale);

    const u32 node_index = static_cast<u32>(mesh.m_nodes.size());
    mesh.m_nodes.emplace_back(std::move(destination));
    node_indices[node] = node_index;
    joint_nodes[node] = node_index;

    FbxMesh *fbx_mesh = node->GetMesh();
    if (fbx_mesh)
    {
        const u32 primitive_index = static_cast<u32>(mesh.m_mesh_primitives.size());
        mesh.m_mesh_primitives.emplace_back();
        MeshPrimitive &primitive = mesh.m_mesh_primitives.back();
        primitive.node_index = node_index;
        primitive.index_offset = static_cast<u32>(mesh.m_indices.size());
        primitive.material_id = GetOrCreateMaterialIndex(mesh, node->GetMaterialCount() > 0 ? node->GetMaterial(0) : nullptr,
                                                         material_indices);

        i32 skin_index = -1;
        const int control_point_count = fbx_mesh->GetControlPointsCount();
        std::vector<Math::float4> control_point_joint_indices(control_point_count, Math::float4(0.0f, 0.0f, 0.0f, 0.0f));
        std::vector<Math::float4> control_point_joint_weights(control_point_count, Math::float4(0.0f, 0.0f, 0.0f, 0.0f));

        const int skin_deformer_count = fbx_mesh->GetDeformerCount(FbxDeformer::eSkin);
        if (skin_deformer_count > 0)
        {
            skin_index = static_cast<i32>(mesh.m_skins.size());
            MeshSkin &skin = mesh.m_skins.emplace_back();
            skin.name = fbx_mesh->GetName() ? fbx_mesh->GetName() : "";

            int cluster_count = 0;
            for (int deformer_index = 0; deformer_index < skin_deformer_count; ++deformer_index)
            {
                auto *skin_deformer = static_cast<FbxSkin *>(fbx_mesh->GetDeformer(deformer_index, FbxDeformer::eSkin));
                if (skin_deformer)
                {
                    cluster_count += skin_deformer->GetClusterCount();
                }
            }

            skin.joint_node_indices.resize(cluster_count, INVALID_NODE_INDEX);
            skin.inverse_bind_matrices.resize(cluster_count, Math::float4x4::Identity);
            skin.joint_matrices.resize(cluster_count, Math::float4x4::Identity);

            int joint_cursor = 0;
            for (int deformer_index = 0; deformer_index < skin_deformer_count; ++deformer_index)
            {
                auto *skin_deformer = static_cast<FbxSkin *>(fbx_mesh->GetDeformer(deformer_index, FbxDeformer::eSkin));
                if (!skin_deformer)
                {
                    continue;
                }

                const int cluster_total = skin_deformer->GetClusterCount();
                for (int cluster_index = 0; cluster_index < cluster_total; ++cluster_index, ++joint_cursor)
                {
                    FbxCluster *cluster = skin_deformer->GetCluster(cluster_index);
                    if (!cluster)
                    {
                        continue;
                    }

                    FbxNode *link = cluster->GetLink();
                    if (link)
                    {
                        const auto node_it = node_indices.find(link);
                        if (node_it != node_indices.end())
                        {
                            skin.joint_node_indices[joint_cursor] = node_it->second;
                        }
                    }

                    FbxAMatrix mesh_transform;
                    FbxAMatrix link_transform;
                    cluster->GetTransformMatrix(mesh_transform);
                    cluster->GetTransformLinkMatrix(link_transform);
                    skin.inverse_bind_matrices[joint_cursor] = FbxMatrixToInternal(link_transform.Inverse() * mesh_transform);

                    const int *control_point_indices = cluster->GetControlPointIndices();
                    const double *control_point_weights = cluster->GetControlPointWeights();
                    const int weight_count = cluster->GetControlPointIndicesCount();
                    for (int weight_index = 0; weight_index < weight_count; ++weight_index)
                    {
                        AssignControlPointWeight(control_point_joint_indices, control_point_joint_weights,
                                                 control_point_indices[weight_index], static_cast<u32>(joint_cursor),
                                                 static_cast<float>(control_point_weights[weight_index]));
                    }
                }
            }

            for (Math::float4 &weights : control_point_joint_weights)
            {
                NormalizeJointWeights(weights);
            }
        }

        if (!fbx_mesh->GetElementTangentCount())
        {
            fbx_mesh->GenerateTangentsDataForAllUVSets();
        }

        const char *uv_set_name = nullptr;
        if (fbx_mesh->GetElementUVCount() > 0 && fbx_mesh->GetElementUV(0))
        {
            uv_set_name = fbx_mesh->GetElementUV(0)->GetName();
        }

        const FbxVector4 *control_points = fbx_mesh->GetControlPoints();
        const int polygon_count = fbx_mesh->GetPolygonCount();
        for (int polygon_index = 0; polygon_index < polygon_count; ++polygon_index)
        {
            const int polygon_size = fbx_mesh->GetPolygonSize(polygon_index);
            for (int vertex_in_polygon = 0; vertex_in_polygon < polygon_size; ++vertex_in_polygon)
            {
                const int control_point_index = fbx_mesh->GetPolygonVertex(polygon_index, vertex_in_polygon);
                const FbxVector4 position = control_points[control_point_index];

                Vertex vertex{};
                vertex.pos = Math::float3(static_cast<float>(position[0]), static_cast<float>(position[1]),
                                          static_cast<float>(position[2]));

                FbxVector4 normal;
                if (fbx_mesh->GetPolygonVertexNormal(polygon_index, vertex_in_polygon, normal))
                {
                    vertex.normal = Math::float3(static_cast<float>(normal[0]), static_cast<float>(normal[1]),
                                                 static_cast<float>(normal[2]));
                }

                if (uv_set_name)
                {
                    FbxVector2 uv;
                    bool unmapped = false;
                    if (fbx_mesh->GetPolygonVertexUV(polygon_index, vertex_in_polygon, uv_set_name, uv, unmapped) && !unmapped)
                    {
                        vertex.uv0 = Math::float2(static_cast<float>(uv[0]), 1.0f - static_cast<float>(uv[1]));
                    }
                }

                if (skin_index >= 0)
                {
                    vertex.joint_indices = control_point_joint_indices[control_point_index];
                    vertex.joint_weights = control_point_joint_weights[control_point_index];
                }

                mesh.m_vertices.emplace_back(vertex);
                mesh.m_indices.emplace_back(static_cast<u32>(mesh.m_vertices.size() - 1));
            }
        }

        primitive.index_count = static_cast<u32>(mesh.m_indices.size()) - primitive.index_offset;
        primitive.skin_index = skin_index;
        mesh.m_nodes[node_index].mesh_primitives.push_back(primitive_index);
    }

    const int child_count = node->GetChildCount();
    mesh.m_nodes[node_index].childs.reserve(child_count);
    for (int child_index = 0; child_index < child_count; ++child_index)
    {
        const u32 imported_child = ImportNode(mesh, node->GetChild(child_index), node_index, node_indices, material_indices, joint_nodes);
        if (imported_child != INVALID_NODE_INDEX)
        {
            mesh.m_nodes[node_index].childs.push_back(imported_child);
        }
    }

    return node_index;
}
} // namespace
#endif

bool LoadMeshWithFbxSdk(Mesh &mesh)
{
#if !defined(HORIZON_HAS_FBXSDK)
    LOG_ERROR("FBX SDK support is not configured; set FBXSDK_ROOT or install Autodesk FBX SDK to load '{}'",
              mesh.m_asset_path.string());
    return false;
#else
    FbxManager *manager = FbxManager::Create();
    if (!manager)
    {
        LOG_ERROR("failed to create FBX SDK manager");
        return false;
    }

    FbxIOSettings *io_settings = FbxIOSettings::Create(manager, IOSROOT);
    manager->SetIOSettings(io_settings);

    FbxImporter *importer = FbxImporter::Create(manager, "");
    const std::string asset_path = mesh.m_asset_path.string();
    if (!importer->Initialize(asset_path.c_str(), -1, manager->GetIOSettings()))
    {
        LOG_ERROR("failed to initialize FBX importer for '{}': {}", asset_path, importer->GetStatus().GetErrorString());
        importer->Destroy();
        manager->Destroy();
        return false;
    }

    FbxScene *scene = FbxScene::Create(manager, "horizon_fbx_scene");
    if (!importer->Import(scene))
    {
        LOG_ERROR("failed to import FBX scene '{}': {}", asset_path, importer->GetStatus().GetErrorString());
        importer->Destroy();
        scene->Destroy();
        manager->Destroy();
        return false;
    }
    importer->Destroy();

    FbxGeometryConverter geometry_converter(manager);
    geometry_converter.Triangulate(scene, true);

    std::unordered_map<FbxNode *, u32> node_indices;
    std::unordered_map<FbxSurfaceMaterial *, u32> material_indices;
    std::unordered_map<FbxNode *, u32> joint_nodes;

    FbxNode *root = scene->GetRootNode();
    if (!root)
    {
        LOG_ERROR("FBX scene '{}' has no root node", asset_path);
        scene->Destroy();
        manager->Destroy();
        return false;
    }

    ImportNode(mesh, root, INVALID_NODE_INDEX, node_indices, material_indices, joint_nodes);

    const int animation_stack_count = scene->GetSrcObjectCount<FbxAnimStack>();
    mesh.m_animations.reserve(animation_stack_count);
    for (int stack_index = 0; stack_index < animation_stack_count; ++stack_index)
    {
        FbxAnimStack *stack = scene->GetSrcObject<FbxAnimStack>(stack_index);
        if (!stack || stack->GetMemberCount<FbxAnimLayer>() <= 0)
        {
            continue;
        }

        FbxAnimLayer *layer = stack->GetMember<FbxAnimLayer>(0);
        MeshAnimationClip clip{};
        clip.name = stack->GetName() ? stack->GetName() : "";
        clip.ticks_per_second = 1.0f;
        clip.current_time = 0.0f;

        FbxTimeSpan time_span;
        stack->GetLocalTimeSpan(time_span);
        clip.duration = static_cast<f32>(time_span.GetDuration().GetSecondDouble());

        PopulateAnimationChannels(clip, root, layer, node_indices);
        if (!clip.channels.empty() && clip.duration > 0.0f)
        {
            mesh.m_animations.emplace_back(std::move(clip));
        }
    }

    if (mesh.materials.empty())
    {
        mesh.materials.emplace_back();
    }

    scene->Destroy();
    manager->Destroy();
    return !mesh.m_mesh_primitives.empty();
#endif
}

} // namespace Horizon
