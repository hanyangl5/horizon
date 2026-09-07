#pragma once

#include "shared/Scene/MergeUtil.h"
#include "shared/Scene/Scene.h"
#include "shared/Scene/VtxData.h"

#include "SceneUtils.h"

#if !defined(fileNameCachedMeshes) || !defined(fileNameCachedMaterials) || !defined(fileNameCachedHierarchy)
// by default, use the precached Bistro scene
#define fileNameCachedMeshes    ".cache/ch08_bistro.meshes"
#define fileNameCachedMaterials ".cache/ch08_bistro.materials"
#define fileNameCachedHierarchy ".cache/ch08_bistro.scene"
#endif

#if !defined(BISTRO_WITH_INTERIOR)
#define BISTRO_WITH_INTERIOR 0
#endif

void loadBistro(MeshData& meshData, Scene& scene)
{
    if (!isMeshDataValid(fileNameCachedMeshes) || !isMeshHierarchyValid(fileNameCachedHierarchy) ||
        !isMeshMaterialsValid(fileNameCachedMaterials))
    {
        LLOGL("No cached mesh data found. Precaching...\n\n");

        MeshData meshData_Exterior;
        Scene    ourScene_Exterior;
#if BISTRO_WITH_INTERIOR
        MeshData meshData_Interior;
        Scene    ourScene_Interior;
#endif

        // don't generate LODs because meshoptimizer fails on the Bistro mesh
        loadMeshFile("D:/Codes/models/Bistro_v5_2/D:/Codes/models/Bistro_v5_2/BistroExterior.gltf", meshData_Exterior, ourScene_Exterior,
                     false);
#if BISTRO_WITH_INTERIOR
        loadMeshFile("D:/Codes/models/Bistro_v5_2/D:/Codes/models/Bistro_v5_2/Interior/interior.obj", meshData_Interior, ourScene_Interior,
                     false);
#endif

        // merge some meshes
        LLOGL("[Unmerged] scene items: %u\n", (uint32_t)ourScene_Exterior.hierarchy.size());
        mergeNodesWithMaterial(ourScene_Exterior, meshData_Exterior, "Foliage_Linde_Tree_Large_Orange_Leaves");
        LLOGL("[Merged orange leaves] scene items: %u\n", (uint32_t)ourScene_Exterior.hierarchy.size());
        mergeNodesWithMaterial(ourScene_Exterior, meshData_Exterior, "Foliage_Linde_Tree_Large_Green_Leaves");
        LLOGL("[Merged green leaves]  scene items: %u\n", (uint32_t)ourScene_Exterior.hierarchy.size());
        mergeNodesWithMaterial(ourScene_Exterior, meshData_Exterior, "Foliage_Linde_Tree_Large_Trunk");
        LLOGL("[Merged trunk]  scene items: %u\n", (uint32_t)ourScene_Exterior.hierarchy.size());

        // merge everything into one big scene
        MeshData meshData;
        Scene    ourScene;

#if BISTRO_WITH_INTERIOR
        mergeScenes(ourScene,
                    {
                        &ourScene_Exterior,
                        &ourScene_Interior,
                    },
                    {},
                    {
                        static_cast<uint32_t>(meshData_Exterior.meshes.size()),
                        static_cast<uint32_t>(meshData_Interior.meshes.size()),
                    });
        mergeMeshData(meshData, { &meshData_Exterior, &meshData_Interior });
        mergeMaterialLists(
            {
                &meshData_Exterior.materials,
                &meshData_Interior.materials,
            },
            {
                &meshData_Exterior.textureFiles,
                &meshData_Interior.textureFiles,
            },
            meshData.materials, meshData.textureFiles);
#else
        mergeScenes(ourScene,
                    {
                        &ourScene_Exterior,
                    },
                    {},
                    {
                        static_cast<uint32_t>(meshData_Exterior.meshes.size()),
                    });
        mergeMeshData(meshData, { &meshData_Exterior });
        mergeMaterialLists(
            {
                &meshData_Exterior.materials,
            },
            {
                &meshData_Exterior.textureFiles,
            },
            meshData.materials, meshData.textureFiles);
#endif

        ourScene.localTransform[0] = glm::scale(vec3(0.01f)); // scale the Bistro
        markAsChanged(ourScene, 0);

        recalculateBoundingBoxes(meshData);

        saveMeshData(fileNameCachedMeshes, meshData);
        saveMeshDataMaterials(fileNameCachedMaterials, meshData);
        saveScene(fileNameCachedHierarchy, ourScene);
    }

    const MeshFileHeader header = loadMeshData(fileNameCachedMeshes, meshData);
    loadMeshDataMaterials(fileNameCachedMaterials, meshData);

    loadScene(fileNameCachedHierarchy, scene);
}
