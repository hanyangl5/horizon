#include <gtest/gtest.h>

#include "Scene/SceneAsset.h"
#include "Scene/AssetCatalog.h"
#include "Scene/WorldArchive.h"
#include "Core/IUniquePtr.h"
#include "Core/IContainer.h"
#include <string>

static const char kScene[] = R"json({
  "format": "Horizon.SceneAsset", "version": 1,
  "id": "00000000-0000-4000-8000-000000000001",
  "source": "scene.gltf", "contentHash": "fnv1a64:0123456789abcdef", "identityHash": "0123456789abcdef",
  "textures": [{"id": "00000000-0000-4000-8000-000000000002", "path": "Textures/albedo.dds", "source": "Textures/albedo.png", "srgb": true, "cooked": true}],
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
      {"id": "8000000000000002", "draw": 1, "material": "00000000-0000-4000-8000-000000000003"}
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
    EXPECT_EQ(mesh.submeshes.pData[1].material, mesh.submeshes.pData[0].material);
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
    EXPECT_STREQ(asset.getTextures().pData[0].pSource, "Textures/albedo.png");
}

TEST_F(SceneAssetReadTest, InstantiatesIndependentWorldEntitiesAndCopiesPayloadData)
{
    hz::SceneAsset asset;
    ASSERT_TRUE(asset.parse(kScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(asset, "scene.sceneasset.json"));
    const hz::AssetCatalogEntry* entry = catalog.findScene("scene.sceneasset.json");
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->id, asset.getID());
    EXPECT_EQ(catalog.findScene("missing.sceneasset.json"), nullptr);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World                world(types);
    const hz::Entity         parent = world.createPersistentEntity();
    const hz::LocalTransform transform = { .translation = Vector3(100, 0, 0) };
    ASSERT_TRUE(world.setComponent(parent, hz::SceneTypes::localTransform, &transform));
    const hz::AssetID mesh = asset.getMeshes().pData[0].id;
    const hz::Entity  firstRoot = asset.instantiate(world, parent);
    const hz::Entity  secondRoot = asset.instantiate(world);
    EXPECT_EQ(world.getParent(firstRoot), parent);
    EXPECT_FALSE(world.getParent(secondRoot).isValid());
    EXPECT_NE(world.getEntityGuid(firstRoot), world.getEntityGuid(secondRoot));
    asset = {};
    world.updateTransforms();
    struct Instances
    {
        hz::World*  world;
        hz::AssetID mesh;
        uint32_t    count = 0;
    } instances = { .world = &world, .mesh = mesh };
    hz::WorldQuery query(world, { hz::SceneTypes::meshRenderer, hz::SceneTypes::name });
    query.each(
        [](hz::Entity entity, hz::Span<const void*> fields, void* pUser)
        {
            Instances&              data = *(Instances*)pUser;
            const hz::MeshRenderer& renderer = *(const hz::MeshRenderer*)fields.pData[0];
            const hz::Name&         name = *(const hz::Name*)fields.pData[1];
            EXPECT_EQ(renderer.mesh, data.mesh);
            const hz::Entity nodeParent = data.world->getParent(entity);
            EXPECT_STREQ(((const hz::Name*)data.world->getComponent(nodeParent, hz::SceneTypes::name))->value, "Root");
            const hz::Entity sceneRoot = data.world->getParent(nodeParent);
            const float      x = data.world->getParent(sceneRoot).isValid() ? 110.0f : 10.0f;
            EXPECT_FLOAT_EQ((float)data.world->getWorldTransform(entity)->current.getElem(3, 0), x);
            if (!strcmp(name.value, "First"))
            {
                const hz::LocalMatrix* matrix = (const hz::LocalMatrix*)data.world->getComponent(entity, hz::SceneTypes::localMatrix);
                ASSERT(matrix);
                EXPECT_FLOAT_EQ((float)matrix->value.getElem(1, 0), 0.5f);
                EXPECT_EQ(data.world->getComponent(entity, hz::SceneTypes::localTransform), nullptr);
            }
            else
                EXPECT_STREQ(name.value, "Second");
            EXPECT_EQ(data.world->getEntity(data.world->getEntityGuid(entity)), entity);
            ++data.count;
            return true;
        },
        &instances);
    EXPECT_EQ(instances.count, 4u);
    ASSERT_TRUE(world.destroySubtree(firstRoot));
    EXPECT_TRUE(world.isAlive(secondRoot));
    EXPECT_TRUE(world.isAlive(parent));
}

TEST_F(SceneAssetReadTest, TruncatesInstanceNamesAtUtf8CodepointBoundaries)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    const char*    codepoints[] = { "\xC2\xA2", "\xE4\xB8\xAD", "\xF0\x9F\x8C\xB3" };
    const uint32_t limit = sizeof(hz::Name::value) - 1;
    for (const char* pCodepoint : codepoints)
    {
        for (uint32_t prefix = limit - 4; prefix <= limit + 1; ++prefix)
        {
            SCOPED_TRACE(strlen(pCodepoint));
            SCOPED_TRACE(prefix);
            const std::string input = std::string(prefix, 'a') + pCodepoint + ".gltf";
            std::string       expected(prefix < limit ? prefix : limit, 'a');
            if (prefix + strlen(pCodepoint) <= limit)
            {
                expected += pCodepoint;
                expected.append(".gltf", limit - expected.size() < 5 ? limit - expected.size() : 5);
            }
            std::string json = replaceText("scene.gltf", input.c_str());
            json.replace(json.find("\"Root\""), strlen("\"Root\""), "\"" + input + "\"");
            hz::SceneAsset asset;
            ASSERT_TRUE(parse(asset, json));
            hz::World        world(types);
            const hz::Entity root = asset.instantiate(world);
            struct Names
            {
                hz::World*  pWorld;
                hz::Entity  root;
                const char* pExpected;
                uint32_t    count = 0;
            } names = { .pWorld = &world, .root = root, .pExpected = expected.c_str() };
            hz::WorldQuery query(world, { hz::SceneTypes::name });
            query.each(
                [](hz::Entity entity, hz::Span<const void*> fields, void* pUser)
                {
                    Names& names = *(Names*)pUser;
                    if (entity == names.root || names.pWorld->getParent(entity) == names.root)
                    {
                        EXPECT_STREQ(((const hz::Name*)fields.pData[0])->value, names.pExpected);
                        ++names.count;
                    }
                    return true;
                },
                &names);
            EXPECT_EQ(names.count, 2u);
            hz::WorldArchive archive;
            ASSERT_TRUE(archive.capture(world));
            hz::unique_ptr<hz::World> restored;
            ASSERT_TRUE(archive.instantiate(types, restored));
            const hz::Entity restoredRoot = restored->getEntity(world.getEntityGuid(root));
            const hz::Name*  pName = (const hz::Name*)restored->getComponent(restoredRoot, hz::SceneTypes::name);
            ASSERT_NE(pName, nullptr);
            EXPECT_STREQ(pName->value, expected.c_str());
        }
    }
}

TEST_F(SceneAssetReadTest, FailedGuidGenerationRollsBackOnlyTheNewInstance)
{
    hz::SceneAsset asset;
    ASSERT_TRUE(asset.parse(kScene));
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    for (bool collision : { false, true })
    {
        for (uint32_t failureAt : { 0u, 2u })
        {
            SCOPED_TRACE(collision);
            SCOPED_TRACE(failureAt);
            struct Source
            {
                uint32_t calls = 0;
                uint32_t failureAt;
                bool     collision;
            } source = { .failureAt = failureAt, .collision = collision };
            const hz::IDGenerator generator = {
                .pGenerate =
                    [](void* pUser, uint8_t (&pBytes)[16])
                {
                    Source&        source = *(Source*)pUser;
                    const uint32_t index = source.calls++;
                    if (index == source.failureAt && !source.collision)
                        return false;
                    memset(pBytes, 0, 16);
                    pBytes[15] = index == source.failureAt ? 0x10 : (uint8_t)(0x20 + index);
                    return true;
                },
                .pUserData = &source,
            };
            hz::World        world(types, generator);
            const hz::Guid   parentID = { .high = 0x4000, .low = UINT64_C(0x8000000000000010) };
            const hz::Guid   childID = { .high = 0x4000, .low = UINT64_C(0x8000000000000011) };
            const hz::Entity parent = world.createPersistentEntity(parentID);
            const hz::Entity child = world.createPersistentEntity(childID);
            ASSERT_TRUE(world.setParent(child, parent, false));
            const hz::LocalTransform transform = { .translation = Vector3(3, 0, 0) };
            ASSERT_TRUE(world.setComponent(parent, hz::SceneTypes::localTransform, &transform));
            EXPECT_FALSE(asset.instantiate(world, parent).isValid());
            EXPECT_EQ(source.calls, failureAt + 1);
            EXPECT_EQ(world.getEntity(parentID), parent);
            EXPECT_EQ(world.getEntity(childID), child);
            EXPECT_EQ(world.getParent(child), parent);
            for (uint32_t i = 0; i < failureAt; ++i)
                EXPECT_FALSE(world.getEntity({ .high = 0x4000, .low = UINT64_C(0x8000000000000020) + i }).isValid());
            uint32_t       count = 0;
            hz::WorldQuery query(world, {});
            query.each(
                [](hz::Entity, hz::Span<const void*>, void* pUser)
                {
                    ++*(uint32_t*)pUser;
                    return true;
                },
                &count);
            EXPECT_EQ(count, 2u);
            world.updateTransforms();
            EXPECT_FLOAT_EQ((float)world.getWorldTransform(child)->current.getElem(3, 0), 3.0f);
            source.failureAt = UINT32_MAX;
            const hz::Entity root = asset.instantiate(world, parent);
            ASSERT_TRUE(root.isValid());
            EXPECT_EQ(world.getParent(root), parent);
            ASSERT_TRUE(world.destroySubtree(root));
            EXPECT_EQ(world.getEntity(parentID), parent);
            EXPECT_EQ(world.getParent(child), parent);
        }
    }
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
        { "\"material\": \"00000000-0000-4000-8000-000000000003\"", "\"material\": null" },
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
        { "Textures/albedo.png", "../albedo.png" },
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
    for (bool parentFirst : { true, false })
    {
        SCOPED_TRACE(parentFirst);
        std::string json = R"({"format":"Horizon.SceneAsset","version":1,"id":"00000000-0000-4000-8000-000000000001",
            "source":"deep.gltf","contentHash":"fnv1a64:0123456789abcdef","identityHash":"0123456789abcdef",
            "textures":[],"materials":[],"meshes":[],"nodes":[)";
        for (uint32_t i = 0; i < count; ++i)
        {
            if (i)
                json += ',';
            const int parent = parentFirst ? (i ? (int)i - 1 : -1) : (i + 1 == count ? -1 : (int)i + 1);
            json += "{\"name\":\"Node" + std::to_string(i) + "\",\"parent\":" + std::to_string(parent) +
                    ",\"translation\":[1,0,0],\"rotation\":[0,0,0,1],\"scale\":[1,1,1]}";
        }
        json += "]}";
        hz::SceneAsset asset;
        ASSERT_TRUE(parse(asset, json));
        ASSERT_EQ(asset.getNodes().count, count);
        hz::TypeRegistry types;
        ASSERT_TRUE(types.registerBuiltins());
        hz::World                world(types);
        const hz::Entity         parent = world.createPersistentEntity();
        const hz::LocalTransform transform = { .translation = Vector3(3, 0, 0) };
        ASSERT_TRUE(world.setComponent(parent, hz::SceneTypes::localTransform, &transform));
        const hz::Entity root = asset.instantiate(world, parent);
        EXPECT_EQ(world.getParent(root), parent);
        asset = {};
        world.updateTransforms();
        struct Nodes
        {
            hz::World* world;
            hz::Entity root;
            uint32_t   count;
            bool       parentFirst;
            uint32_t   found = 0;
        } nodes = { .world = &world, .root = root, .count = count, .parentFirst = parentFirst };
        hz::WorldQuery query(world, { hz::SceneTypes::name, hz::SceneTypes::localTransform });
        query.each(
            [](hz::Entity entity, hz::Span<const void*> fields, void* pUser)
            {
                Nodes&          nodes = *(Nodes*)pUser;
                const hz::Name& name = *(const hz::Name*)fields.pData[0];
                uint32_t        index = 0;
                EXPECT_EQ(sscanf(name.value, "Node%u", &index), 1);
                EXPECT_LT(index, nodes.count);
                const uint32_t depth = nodes.parentFirst ? index + 1 : nodes.count - index;
                EXPECT_FLOAT_EQ((float)nodes.world->getWorldTransform(entity)->current.getElem(3, 0), (float)depth + 3.0f);
                const hz::Entity parent = nodes.world->getParent(entity);
                if (depth == 1)
                    EXPECT_EQ(parent, nodes.root);
                else
                {
                    const hz::Name* pParent = (const hz::Name*)nodes.world->getComponent(parent, hz::SceneTypes::name);
                    EXPECT_NE(pParent, nullptr);
                    if (pParent)
                    {
                        const std::string expected = "Node" + std::to_string(nodes.parentFirst ? index - 1 : index + 1);
                        EXPECT_STREQ(pParent->value, expected.c_str());
                    }
                }
                ++nodes.found;
                return true;
            },
            &nodes);
        EXPECT_EQ(nodes.found, count);
    }
}

TEST_F(SceneAssetReadTest, CatalogLocatesSubassetsAndDeduplicatesDependencies)
{
    hz::SceneAsset scene;
    std::string    json = replaceText("\"emissiveTexture\": null", "\"emissiveTexture\": \"00000000-0000-4000-8000-000000000002\"");
    json.insert(
        json.find("\"textures\": [") + strlen("\"textures\": ["),
        R"({"id":"00000000-0000-4000-8000-000000000005","path":"Textures/normal.dds","source":"Textures/normal.png","srgb":false,"cooked":true},)");
    json.replace(json.find("\"normalTexture\": null"), strlen("\"normalTexture\": null"),
                 "\"normalTexture\": \"00000000-0000-4000-8000-000000000005\"");
    ASSERT_TRUE(parse(scene, json));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(scene, "Scenes/scene.sceneasset.json"));
    EXPECT_EQ(catalog.getCount(), 5u);
    EXPECT_EQ(catalog.find({}), nullptr);
    EXPECT_EQ(catalog.find({ .high = 123 }), nullptr);
    const hz::AssetCatalogEntry* root = catalog.find(scene.getID());
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->type, hz::AssetType::Scene);
    EXPECT_STREQ(root->pSourcePath, "scene.gltf");
    EXPECT_EQ(root->location.elementIndex, UINT32_MAX);
    ASSERT_EQ(root->dependencies.count, 1u);
    const hz::AssetCatalogEntry* mesh = catalog.find(root->dependencies.pData[0]);
    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->type, hz::AssetType::Mesh);
    EXPECT_EQ(mesh->location.elementIndex, 0u);
    EXPECT_STREQ(mesh->location.pPath, "Scenes/scene.sceneasset.json");
    EXPECT_EQ(mesh->location.root, hz::AssetPathRoot::Cooked);
    ASSERT_EQ(mesh->dependencies.count, 1u);
    const hz::AssetCatalogEntry* material = catalog.find(mesh->dependencies.pData[0]);
    ASSERT_NE(material, nullptr);
    EXPECT_EQ(material->type, hz::AssetType::Material);
    EXPECT_EQ(material->location.elementIndex, 0u);
    ASSERT_EQ(material->dependencies.count, 2u);
    EXPECT_EQ(material->dependencies.pData[0], scene.getMaterials().pData[0].baseColorTexture);
    EXPECT_EQ(material->dependencies.pData[1], scene.getMaterials().pData[0].normalTexture);
    const hz::AssetCatalogEntry* texture = catalog.find(material->dependencies.pData[0]);
    ASSERT_NE(texture, nullptr);
    EXPECT_EQ(texture->type, hz::AssetType::Texture);
    EXPECT_EQ(texture->dependencies.count, 0u);
    EXPECT_STREQ(texture->pSourcePath, "Textures/albedo.png");
    EXPECT_STREQ(texture->location.pPath, "Textures/albedo.dds");
    EXPECT_EQ(texture->location.root, hz::AssetPathRoot::Cooked);
    EXPECT_EQ(texture->location.elementIndex, UINT32_MAX);
    EXPECT_TRUE(texture->srgb);
    EXPECT_TRUE(catalog.addScene(scene, "Scenes/scene.sceneasset.json"));
    EXPECT_EQ(catalog.getCount(), 5u);
}

TEST_F(SceneAssetReadTest, CatalogOwnsMetadataAcrossSceneDestructionMoveAndClear)
{
    hz::AssetCatalog catalog;
    hz::AssetID      id, textureID;
    {
        hz::SceneAsset scene;
        ASSERT_TRUE(parse(scene, replaceText("\"cooked\": true", "\"cooked\": false")));
        std::string path = "Scenes/scene.sceneasset.json";
        ASSERT_TRUE(catalog.addScene(scene, path.c_str()));
        id = scene.getID();
        textureID = scene.getTextures().pData[0].id;
        path.assign(path.size(), '!');
    }
    hz::AssetCatalog moved(std::move(catalog));
    EXPECT_EQ(catalog.getCount(), 0u);
    EXPECT_EQ(catalog.find(id), nullptr);
    ASSERT_NE(moved.find(id), nullptr);
    EXPECT_STREQ(moved.find(id)->location.pPath, "Scenes/scene.sceneasset.json");
    EXPECT_EQ(moved.find(textureID)->location.root, hz::AssetPathRoot::Source);
    EXPECT_NE(moved.find(moved.find(id)->dependencies.pData[0]), nullptr);
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.parse(kScene));
    ASSERT_TRUE(catalog.addScene(scene, "replacement.sceneasset.json"));
    catalog = std::move(moved);
    EXPECT_EQ(moved.getCount(), 0u);
    EXPECT_STREQ(catalog.find(id)->location.pPath, "Scenes/scene.sceneasset.json");
    catalog.clear();
    EXPECT_EQ(catalog.getCount(), 0u);
    EXPECT_EQ(catalog.find(id), nullptr);
    EXPECT_TRUE(catalog.addScene(scene, "scene.sceneasset.json"));
}

TEST_F(SceneAssetReadTest, CatalogSharesExternalTexturesAndRejectsConflictsAtomically)
{
    hz::SceneAsset first, second;
    ASSERT_TRUE(first.parse(kScene));
    std::string json(kScene);
    for (const char* suffix : { "0001", "0003", "0004" })
    {
        const std::string from = std::string("8000-00000000") + suffix;
        const std::string to = std::string("8000-10000000") + suffix;
        for (size_t pos = json.find(from); pos != std::string::npos; pos = json.find(from, pos + to.size()))
            json.replace(pos, from.size(), to);
    }
    json.replace(json.find("scene.gltf"), strlen("scene.gltf"), "second.gltf");
    ASSERT_TRUE(parse(second, json));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(first, "first.sceneasset.json"));
    ASSERT_TRUE(catalog.addScene(second, "second.sceneasset.json"));
    EXPECT_EQ(catalog.getCount(), 7u);
    const hz::AssetCatalogEntry* original = catalog.find(first.getID());
    EXPECT_FALSE(catalog.addScene(first, "different.sceneasset.json"));
    EXPECT_EQ(catalog.find(first.getID()), original);
    EXPECT_EQ(catalog.getCount(), 7u);

    // A new texture preceding a conflicting existing Scene ID must not be inserted.
    hz::SceneAsset    conflict;
    std::string       changed(kScene);
    const std::string oldID = "00000000-0000-4000-8000-000000000002";
    const std::string newID = "00000000-0000-4000-8000-000000000009";
    for (size_t pos = changed.find(oldID); pos != std::string::npos; pos = changed.find(oldID, pos + newID.size()))
        changed.replace(pos, oldID.size(), newID);
    ASSERT_TRUE(parse(conflict, changed));
    EXPECT_FALSE(catalog.addScene(conflict, "first.sceneasset.json"));
    EXPECT_EQ(catalog.getCount(), 7u);
    EXPECT_EQ(catalog.find(conflict.getTextures().pData[0].id), nullptr);
    hz::AssetCatalog reverse;
    ASSERT_TRUE(reverse.addScene(second, "second.sceneasset.json"));
    ASSERT_TRUE(reverse.addScene(first, "first.sceneasset.json"));
    EXPECT_EQ(reverse.getCount(), catalog.getCount());
    EXPECT_STREQ(reverse.find(first.getTextures().pData[0].id)->pSourcePath, "Textures/albedo.png");
    hz::AssetCatalog firstOnly, secondOnly;
    ASSERT_TRUE(firstOnly.addScene(first, "first.sceneasset.json"));
    ASSERT_TRUE(secondOnly.addScene(second, "second.sceneasset.json"));
    const hz::AssetCatalogEntry* pShared = firstOnly.find(first.getTextures().pData[0].id);
    ASSERT_TRUE(firstOnly.merge(secondOnly));
    EXPECT_EQ(firstOnly.getCount(), 7u);
    EXPECT_EQ(firstOnly.find(first.getTextures().pData[0].id), pShared);
    ASSERT_TRUE(
        secondOnly.replace({ .id = first.getID(), .pSourcePath = "different.gltf", .location = { .pPath = "different.sceneasset.json" } }));
    EXPECT_FALSE(firstOnly.merge(secondOnly));
    EXPECT_EQ(firstOnly.getCount(), 7u);
}

TEST_F(SceneAssetReadTest, CatalogRejectsInvalidScenesAndPaths)
{
    hz::AssetCatalog catalog;
    hz::SceneAsset   scene;
    EXPECT_FALSE(catalog.addScene(scene, "scene.sceneasset.json"));
    ASSERT_TRUE(scene.parse(kScene));
    for (const char* path :
         { "", "../scene.json", "D:/scene.json", "/scene.json", "dir/../scene.json", "dir\\scene.json", "dir//scene.json" })
        EXPECT_FALSE(catalog.addScene(scene, path));
    EXPECT_FALSE(catalog.addScene(scene, nullptr));
    EXPECT_EQ(catalog.getCount(), 0u);
}

TEST_F(SceneAssetReadTest, CatalogSerializationIsDeterministicAndRoundTrips)
{
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.parse(kScene));
    hz::AssetCatalog catalog, reverse, decoded;
    ASSERT_TRUE(catalog.addScene(scene, "scene.sceneasset.json"));
    ASSERT_TRUE(reverse.add(*catalog.find(scene.getID())));
    ASSERT_TRUE(reverse.add(*catalog.find(scene.getMeshes().pData[0].id)));
    ASSERT_TRUE(reverse.add(*catalog.find(scene.getMaterials().pData[0].id)));
    ASSERT_TRUE(reverse.add(*catalog.find(scene.getTextures().pData[0].id)));
    hz::Array<char> first, second, roundtrip;
    ASSERT_TRUE(catalog.serialize(first));
    ASSERT_TRUE(reverse.serialize(second));
    EXPECT_STREQ(first.data(), second.data());
    ASSERT_TRUE(decoded.parse({ first.data(), first.size() }));
    ASSERT_TRUE(decoded.serialize(roundtrip));
    EXPECT_STREQ(first.data(), roundtrip.data());
    EXPECT_EQ(decoded.getCount(), catalog.getCount());
    EXPECT_EQ(decoded.find(scene.getMeshes().pData[0].id)->location.elementIndex, 0u);
    EXPECT_TRUE(decoded.find(scene.getTextures().pData[0].id)->srgb);
    EXPECT_EQ(decoded.find(scene.getID())->dependencies.pData[0], scene.getMeshes().pData[0].id);
}

TEST_F(SceneAssetReadTest, CatalogRejectsInvalidIndexWithoutChangingPublishedEntries)
{
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.parse(kScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(scene, "scene.sceneasset.json"));
    hz::Array<char> json;
    ASSERT_TRUE(catalog.serialize(json));
    const hz::AssetCatalogEntry* original = catalog.find(scene.getID());
    const char*                  replacements[][2] = {
        { "\"version\":1", "\"version\":2" },
        { "\"version\":1", "\"version\":1,\"version\":1" },
        { "\"type\":0", "\"type\":99" },
        { "\"type\":0", "\"type\":0.5" },
        { "\"type\":0", "\"type\":1" },
        { "\"root\":1", "\"root\":9" },
        { "\"source\":\"scene.gltf\"", "\"source\":null" },
        { "\"path\":\"scene.sceneasset.json\"", "\"path\":\"../scene.sceneasset.json\"" },
        { "\"srgb\":false", "\"srgb\":0" },
        { "\"elementIndex\":0", "\"elementIndex\":-1" },
    };
    for (const auto& replacement : replacements)
    {
        SCOPED_TRACE(replacement[1]);
        std::string  invalid(json.data());
        const size_t offset = invalid.find(replacement[0]);
        ASSERT_NE(offset, std::string::npos);
        invalid.replace(offset, strlen(replacement[0]), replacement[1]);
        EXPECT_FALSE(catalog.parse({ invalid.data(), (uint32_t)invalid.size() }));
        EXPECT_EQ(catalog.find(scene.getID()), original);
        EXPECT_EQ(catalog.getCount(), 4u);
    }
    std::string trailing(json.data());
    trailing += "{}";
    EXPECT_FALSE(catalog.parse({ trailing.data(), (uint32_t)trailing.size() }));
    EXPECT_FALSE(catalog.parse({}));
    EXPECT_EQ(catalog.find(scene.getID()), original);
}

TEST_F(SceneAssetReadTest, CatalogReplacementRemovalAndFullReloadPreserveOldMetadataViews)
{
    hz::SceneAsset scene;
    ASSERT_TRUE(scene.parse(kScene));
    hz::AssetCatalog catalog;
    ASSERT_TRUE(catalog.addScene(scene, "scene.sceneasset.json"));
    const hz::AssetID            id = scene.getID();
    const hz::AssetCatalogEntry* original = catalog.find(id);
    hz::AssetCatalogEntry        replacement = *original;
    replacement.location.pPath = "new.sceneasset.json";
    ASSERT_TRUE(catalog.replace(replacement));
    const hz::AssetCatalogEntry* updated = catalog.find(id);
    EXPECT_STREQ(updated->location.pPath, "new.sceneasset.json");
    EXPECT_STREQ(original->location.pPath, "scene.sceneasset.json");
    replacement.type = hz::AssetType::Texture;
    EXPECT_FALSE(catalog.replace(replacement));
    EXPECT_EQ(catalog.find(id), updated);
    EXPECT_TRUE(catalog.remove(id));
    EXPECT_FALSE(catalog.remove(id));
    EXPECT_EQ(catalog.find(id), nullptr);
    EXPECT_STREQ(updated->location.pPath, "new.sceneasset.json");
    EXPECT_EQ(original->dependencies.pData[0], scene.getMeshes().pData[0].id);
    hz::AssetCatalog empty;
    hz::Array<char>  json;
    ASSERT_TRUE(empty.serialize(json));
    ASSERT_TRUE(catalog.parse({ json.data(), json.size() }));
    EXPECT_EQ(catalog.getCount(), 0u);
    EXPECT_STREQ(original->pSourcePath, "scene.gltf");
    ASSERT_TRUE(catalog.addScene(scene, "scene.sceneasset.json"));
    ASSERT_TRUE(catalog.serialize(json));
    ASSERT_TRUE(catalog.parse({ json.data(), json.size() }));
    EXPECT_EQ(catalog.getCount(), 4u);
}
