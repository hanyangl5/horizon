#include <gtest/gtest.h>

#include "Scene/SceneAsset.h"
#include <string>

static const char kScene[] = R"json({
  "format": "Horizon.SceneAsset", "version": 1,
  "id": "00000000-0000-4000-8000-000000000001",
  "source": "scene.gltf", "contentHash": "fnv1a64:0123456789abcdef", "identityHash": "0123456789abcdef",
  "textures": [{"id": "00000000-0000-4000-8000-000000000002", "path": "Textures/albedo.dds", "srgb": true, "cooked": true}],
  "materials": [{
    "id": "00000000-0000-4000-8000-000000000003", "name": "Leaves",
    "baseColorTexture": "00000000-0000-4000-8000-000000000002", "normalTexture": null,
    "metallicRoughnessTexture": null, "emissiveTexture": null,
    "baseColorFactor": [1, 0.5, 0.25, 1], "metallicFactor": 0.2, "roughnessFactor": 0.8,
    "emissiveFactor": [0, 2, 0], "alphaMode": 1, "alphaCutoff": 0.5, "doubleSided": true
  }],
  "meshes": [{
    "id": "00000000-0000-4000-8000-000000000004", "name": "Tree", "geometry": "scene.bin",
    "submeshes": [
      {"id": "8000000000000001", "draw": 0, "material": "00000000-0000-4000-8000-000000000003"},
      {"id": "8000000000000002", "draw": 1, "material": null}
    ]
  }],
  "nodes": [
    {"name": "Root", "parent": -1, "translation": [10, 0, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]},
    {"name": "First", "parent": 0, "mesh": "00000000-0000-4000-8000-000000000004",
     "matrix": [1, 0, 0, 0, 0.5, 1, 0, 0, 0, 0, 1, 0, 0, 2, 3, 1]},
    {"name": "Second", "parent": 0, "mesh": "00000000-0000-4000-8000-000000000004",
     "translation": [0, 0, 3], "rotation": [0, 0, 0, 1], "scale": [-1, 0, 1]}
  ]
})json";

static std::string replaceText(const char* pFrom, const char* pTo)
{
    std::string  json(kScene);
    const size_t position = json.find(pFrom);
    EXPECT_NE(position, std::string::npos);
    if (position != std::string::npos)
        json.replace(position, strlen(pFrom), pTo);
    return json;
}

static bool parse(hz::SceneAsset& asset, const std::string& json) { return asset.parse({ json.data(), (uint32_t)json.size() }); }

class SceneAssetReadTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(initMemAlloc(nullptr));
        initialStats = memGetTrackingStats();
    }
    void TearDown() override
    {
        const MemoryTrackingStats stats = memGetTrackingStats();
        if (stats.trackingEnabled)
        {
            EXPECT_EQ(stats.liveAllocationCount, initialStats.liveAllocationCount);
            EXPECT_EQ(stats.liveRequestedBytes, initialStats.liveRequestedBytes);
        }
        exitMemAlloc();
    }
    MemoryTrackingStats initialStats = {};
};

TEST_F(SceneAssetReadTest, PreservesHierarchyLocalTransformsAndAssetReferences)
{
    hz::SceneAsset asset;
    ASSERT_TRUE(asset.parse(kScene));
    EXPECT_TRUE(asset.isValid());
    EXPECT_STREQ(asset.getSource(), "scene.gltf");
    EXPECT_STREQ(asset.getContentHash(), "fnv1a64:0123456789abcdef");
    ASSERT_EQ(asset.getNodes().count, 3u);
    const hz::SceneAssetNode* nodes = asset.getNodes().pData;
    EXPECT_EQ(nodes[0].parent, UINT32_MAX);
    EXPECT_EQ(nodes[1].parent, 0u);
    EXPECT_FLOAT_EQ((float)nodes[0].transform.translation.getX(), 10.0f);
    EXPECT_FALSE(nodes[0].hasMatrix);
    EXPECT_TRUE(nodes[1].hasMatrix);
    EXPECT_FLOAT_EQ((float)nodes[1].matrix.value.getElem(1, 0), 0.5f);
    EXPECT_FLOAT_EQ((float)nodes[1].matrix.value.getElem(3, 1), 2.0f);
    EXPECT_FLOAT_EQ((float)nodes[2].transform.scale.getX(), -1.0f);
    EXPECT_FLOAT_EQ((float)nodes[2].transform.scale.getY(), 0.0f);
    ASSERT_EQ(asset.getMeshes().count, 1u);
    const hz::SceneAssetMesh& mesh = asset.getMeshes().pData[0];
    EXPECT_EQ(nodes[1].mesh, mesh.id);
    EXPECT_EQ(nodes[2].mesh, mesh.id);
    ASSERT_EQ(mesh.submeshes.count, 2u);
    EXPECT_EQ(mesh.submeshes.pData[0].id, UINT64_C(0x8000000000000001));
    EXPECT_EQ(mesh.submeshes.pData[1].draw, 1u);
    EXPECT_FALSE(mesh.submeshes.pData[1].material.isValid());
    ASSERT_EQ(asset.getMaterials().count, 1u);
    const hz::SceneAssetMaterial& material = asset.getMaterials().pData[0];
    EXPECT_EQ(mesh.submeshes.pData[0].material, material.id);
    EXPECT_EQ(material.alphaMode, hz::MaterialAlphaMode::Mask);
    EXPECT_TRUE(material.doubleSided);
    EXPECT_FLOAT_EQ(material.alphaCutoff, 0.5f);
    EXPECT_FLOAT_EQ(material.emissiveFactor[1], 2.0f);
    ASSERT_EQ(asset.getTextures().count, 1u);
    EXPECT_EQ(material.baseColorTexture, asset.getTextures().pData[0].id);
    EXPECT_TRUE(asset.getTextures().pData[0].srgb);
    EXPECT_TRUE(asset.getTextures().pData[0].cooked);
}

TEST_F(SceneAssetReadTest, OwnsStringsAndSupportsMoveAndReplacement)
{
    hz::SceneAsset asset;
    {
        std::string json(kScene);
        ASSERT_TRUE(parse(asset, json));
        json.assign(json.size(), '!');
    }
    hz::SceneAsset moved(std::move(asset));
    EXPECT_FALSE(asset.isValid());
    EXPECT_EQ(asset.getNodes().count, 0u);
    EXPECT_FALSE(asset.getID().isValid());
    EXPECT_STREQ(moved.getNodes().pData[1].pName, "First");
    EXPECT_STREQ(moved.getMeshes().pData[0].pGeometry, "scene.bin");
    EXPECT_STREQ(moved.getMaterials().pData[0].pName, "Leaves");
    EXPECT_STREQ(moved.getTextures().pData[0].pPath, "Textures/albedo.dds");
    ASSERT_TRUE(asset.parse(kScene));
    asset = std::move(moved);
    EXPECT_FALSE(moved.isValid());
    ASSERT_TRUE(parse(asset, replaceText("First", "Replaced")));
    EXPECT_STREQ(asset.getNodes().pData[1].pName, "Replaced");
}

TEST_F(SceneAssetReadTest, RejectsInvalidIdentityAndReferencesWithoutChangingLoadedData)
{
    const char* replacements[][2] = {
        { "00000000-0000-4000-8000-000000000001", "00000000-0000-0000-0000-000000000000" },
        { "00000000-0000-4000-8000-000000000001", "00000000-0000-4000-8000-000000000002" },
        { "00000000-0000-4000-8000-000000000002", "bad-id" },
        { "\"baseColorTexture\": \"00000000-0000-4000-8000-000000000002\"",
          "\"baseColorTexture\": \"00000000-0000-4000-8000-000000000009\"" },
        { "\"baseColorTexture\": \"00000000-0000-4000-8000-000000000002\"",
          "\"baseColorTexture\": \"00000000-0000-4000-8000-000000000003\"" },
        { "\"material\": \"00000000-0000-4000-8000-000000000003\"", "\"material\": \"00000000-0000-4000-8000-000000000002\"" },
        { "\"mesh\": \"00000000-0000-4000-8000-000000000004\"", "\"mesh\": \"00000000-0000-4000-8000-000000000003\"" },
        { "8000000000000002", "8000000000000001" },
        { "8000000000000002", "0000000000000000" },
        { "8000000000000002", "+000000000000001" },
    };
    hz::SceneAsset asset;
    ASSERT_TRUE(asset.parse(kScene));
    const hz::SceneAssetNode* nodes = asset.getNodes().pData;
    for (const char* const* replacement : replacements)
    {
        SCOPED_TRACE(replacement[1]);
        EXPECT_FALSE(parse(asset, replaceText(replacement[0], replacement[1])));
        EXPECT_EQ(asset.getNodes().pData, nodes);
        EXPECT_STREQ(asset.getNodes().pData[0].pName, "Root");
    }
}

TEST_F(SceneAssetReadTest, RejectsInvalidHierarchyAndTransforms)
{
    const char* replacements[][2] = {
        { "\"parent\": -1", "\"parent\": 1" },  { "\"parent\": 0", "\"parent\": 1" },
        { "\"parent\": 0", "\"parent\": 3" },   { "\"parent\": 0", "\"parent\": -2" },
        { "\"parent\": 0", "\"parent\": 0.5" }, { "\"matrix\":", "\"translation\": [0, 0, 0], \"matrix\":" },
        { "0, 2, 3, 1]", "0, 2, 3, 0]" },       { "[10, 0, 0]", "[1e100, 0, 0]" },
        { "[10, 0, 0]", "[1e999, 0, 0]" },      { "[10, 0, 0]", "[0, 0]" },
        { "[0, 0, 0, 1]", "[0, 0, 0, 0]" },     { "[0, 0, 0, 1]", "[0, 0, 0, 2]" },
    };
    hz::SceneAsset asset;
    for (const char* const* replacement : replacements)
    {
        SCOPED_TRACE(replacement[1]);
        EXPECT_FALSE(parse(asset, replaceText(replacement[0], replacement[1])));
        EXPECT_FALSE(asset.isValid());
    }
    EXPECT_TRUE(parse(asset, replaceText("\"parent\": 0", "\"parent\": 2")));
}

TEST_F(SceneAssetReadTest, RejectsUnsupportedFormatAndVersion)
{
    hz::SceneAsset asset;
    EXPECT_FALSE(parse(asset, replaceText("\"version\": 1", "\"version\": 0")));
    EXPECT_FALSE(parse(asset, replaceText("\"version\": 1", "\"version\": 2")));
    EXPECT_FALSE(parse(asset, replaceText("\"version\": 1", "\"version\": 1.5")));
    EXPECT_FALSE(parse(asset, replaceText("Horizon.SceneAsset", "OtherFormat")));
    EXPECT_FALSE(asset.isValid());
}

TEST_F(SceneAssetReadTest, RejectsMalformedJsonPathsAndMaterialFields)
{
    hz::SceneAsset asset;
    EXPECT_FALSE(asset.parse({}));
    EXPECT_FALSE(asset.parse("[]"));
    EXPECT_FALSE(parse(asset, std::string(kScene) + " garbage"));
    EXPECT_FALSE(parse(asset, std::string(kScene) + '\0' + "garbage"));
    EXPECT_FALSE(parse(asset, std::string(kScene).substr(0, sizeof(kScene) - 3)));
    const char* replacements[][2] = {
        { "Textures/albedo.dds", "../albedo.dds" },
        { "scene.bin", "D:/scene.bin" },
        { "scene.bin", "/scene.bin" },
        { "scene.bin", "dir/../scene.bin" },
        { "\"metallicFactor\": 0.2", "\"metallicFactor\": 2" },
        { "\"roughnessFactor\": 0.8", "\"roughnessFactor\": -1" },
        { "\"alphaMode\": 1", "\"alphaMode\": 3" },
        { "\"alphaMode\": 1", "\"alphaMode\": 0.5" },
        { "\"doubleSided\": true", "\"doubleSided\": 1" },
        { "\"cooked\": true", "\"cooked\": 1" },
        { "\"draw\": 1", "\"draw\": -1" },
        { "\"draw\": 1", "\"draw\": 4294967296" },
        { "\"draw\": 1", "\"draw\": 0.5" },
        { "fnv1a64:0123456789abcdef", "fnv1a64:0123456789abcdeg" },
    };
    for (const char* const* replacement : replacements)
    {
        SCOPED_TRACE(replacement[1]);
        EXPECT_FALSE(parse(asset, replaceText(replacement[0], replacement[1])));
    }
    EXPECT_TRUE(parse(asset, replaceText("\"alphaCutoff\": 0.5", "\"alphaCutoff\": 2")));
    EXPECT_FLOAT_EQ(asset.getMaterials().pData[0].alphaCutoff, 2.0f);
}

TEST_F(SceneAssetReadTest, HandlesDeepHierarchyWithoutRecursiveTraversal)
{
    const uint32_t count = 10000;
    std::string    json = R"({"format":"Horizon.SceneAsset","version":1,"id":"00000000-0000-4000-8000-000000000001",
        "source":"deep.gltf","contentHash":"fnv1a64:0123456789abcdef","identityHash":"0123456789abcdef",
        "textures":[],"materials":[],"meshes":[],"nodes":[)";
    for (uint32_t i = 0; i < count; ++i)
    {
        if (i)
            json += ',';
        json += "{\"name\":\"Node\",\"parent\":" + std::to_string(i + 1 == count ? -1 : (int)i + 1) +
                ",\"translation\":[0,0,0],\"rotation\":[0,0,0,1],\"scale\":[1,1,1]}";
    }
    json += "]}";
    hz::SceneAsset asset;
    ASSERT_TRUE(parse(asset, json));
    EXPECT_EQ(asset.getNodes().count, count);
    EXPECT_EQ(asset.getNodes().pData[0].parent, 1u);
    EXPECT_EQ(asset.getNodes().pData[count - 1].parent, UINT32_MAX);
}
