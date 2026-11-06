#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include "Scene/WorldArchive.h"
#include "Scene/World.h"
#include "Scene/SceneComponents.h"
#include "Core/IUniquePtr.h"
#include "Core/IContainer.h"

class WorldArchiveTest: public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(initMemAlloc(nullptr));
        initial = memGetTrackingStats();
    }
    void TearDown() override
    {
        const MemoryTrackingStats after = memGetTrackingStats();
        if (after.trackingEnabled)
        {
            EXPECT_EQ(after.liveAllocationCount, initial.liveAllocationCount);
            EXPECT_EQ(after.liveRequestedBytes, initial.liveRequestedBytes);
        }
        exitMemAlloc();
    }
    static hz::Guid    id(uint64_t value) { return { .high = 1, .low = value }; }
    static std::string text(const hz::WorldArchive& archive)
    {
        hz::Array<char> json;
        EXPECT_TRUE(archive.serialize(json));
        return json.empty() ? "" : std::string(json.data(), json.size() - 1);
    }
    static bool parse(hz::WorldArchive& archive, const std::string& json) { return archive.parse({ json.data(), (uint32_t)json.size() }); }
    static std::string uuid(uint64_t value)
    {
        char text[hz::kIDStringCapacity];
        id(value).toString(text);
        return text;
    }
    static std::string envelope(const std::string& entities)
    {
        return "{\"format\":\"Horizon.World\",\"version\":1,\"id\":\"" + uuid(100) + "\",\"entities\":[" + entities + "]}";
    }
    static std::string record(uint64_t object, const std::string& components = "", uint64_t parent = 0)
    {
        return "{\"id\":\"" + uuid(object) + "\",\"parent\":" + (parent ? "\"" + uuid(parent) + "\"" : "null") +
               ",\"name\":null,\"components\":[" + components + "]}";
    }
    static std::string component(const char* type, const char* name, uint32_t version, const std::string& properties)
    {
        return "{\"type\":\"" + std::string(type) + "\",\"name\":\"" + name + "\",\"version\":" + std::to_string(version) +
               ",\"properties\":" + properties + "}";
    }
    MemoryTrackingStats initial = {};
};

TEST_F(WorldArchiveTest, RoundTripsComponentsHierarchyAndStableOrdering)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity parent = world.createPersistentEntity(id(20));
    const hz::Entity child = world.createPersistentEntity(id(10));
    const hz::Entity matrixEntity = world.createPersistentEntity(id(30));
    world.createEntity();
    ASSERT_TRUE(world.setParent(child, parent, false));
    hz::Name name = { "Parent" };
    ASSERT_TRUE(world.setComponent(parent, hz::SceneTypes::name, &name));
    hz::LocalTransform transform = { .translation = Vector3(1.2345678f, 2, 3), .scale = Vector3(-1, 0, 2) };
    ASSERT_TRUE(world.setComponent(parent, hz::SceneTypes::localTransform, &transform));
    Matrix4 matrix = Matrix4::identity();
    matrix.setElem(1, 0, 0.25f);
    matrix.setElem(3, 2, 4.0f);
    ASSERT_TRUE(world.setComponent(matrixEntity, hz::SceneTypes::localMatrix, &matrix));
    hz::MeshRenderer mesh = { .mesh = { .high = 2, .low = 1 } };
    mesh.overrides.pushBack({ .submesh = UINT64_C(18446744073709551615), .material = { .high = 2, .low = 2 } });
    ASSERT_TRUE(world.setComponent(child, hz::SceneTypes::meshRenderer, &mesh));
    hz::Light light = { .type = hz::LightType::Rect, .intensity = 100.125f };
    ASSERT_TRUE(world.setComponent(child, hz::SceneTypes::light, &light));
    ASSERT_TRUE(world.addComponent(child, hz::SceneTypes::camera));
    hz::WorldArchive archive;
    ASSERT_TRUE(archive.capture(world, id(100)));
    const std::string first = text(archive);
    EXPECT_NE(first.find("\"18446744073709551615\""), std::string::npos);
    EXPECT_LT(first.find(uuid(10)), first.find(uuid(20)));
    ASSERT_TRUE(archive.capture(world));
    EXPECT_EQ(text(archive), first);
    hz::unique_ptr<hz::World> restored;
    ASSERT_TRUE(archive.instantiate(types, restored));
    const hz::Entity loadedChild = restored->getEntity(id(10));
    const hz::Entity loadedParent = restored->getEntity(id(20));
    EXPECT_EQ(restored->getParent(loadedChild), loadedParent);
    EXPECT_EQ(((const hz::MeshRenderer*)restored->getComponent(loadedChild, hz::SceneTypes::meshRenderer))->overrides[0].submesh,
              UINT64_MAX);
    EXPECT_FLOAT_EQ(((const hz::LocalTransform*)restored->getComponent(loadedParent, hz::SceneTypes::localTransform))->translation.getX(),
                    transform.translation.getX());
    EXPECT_FLOAT_EQ((float)restored->getWorldTransform(loadedChild)->current.getElem(3, 0), transform.translation.getX());
    EXPECT_FLOAT_EQ((float)restored->getWorldTransform(loadedChild)->previous.getElem(3, 0), transform.translation.getX());
    ASSERT_TRUE(archive.capture(*restored));
    EXPECT_EQ(text(archive), first);
}

TEST_F(WorldArchiveTest, SavesAffineLocalMatrixAfterReparentingUnderRotatedScaledParent)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World             world(types);
    const hz::Entity      parent = world.createPersistentEntity(id(1)), child = world.createPersistentEntity(id(2));
    const Matrix4 parentMatrix = Matrix4::translation(Vector3(1.3f, -4.7f, 9.8f)) * Matrix4::rotationZYX(Vector3(0.3f, 0.4f, -0.8f)) *
                                 Matrix4::scale(Vector3(1.5f, 0.7f, -2.35f));
    const Matrix4 childMatrix = Matrix4::translation(Vector3(3.7f, -2.1f, 5.6f)) * Matrix4::rotationZYX(Vector3(0.8f, -0.3f, 0.2f));
    ASSERT_TRUE(world.setComponent(parent, hz::SceneTypes::localMatrix, &parentMatrix));
    ASSERT_TRUE(world.setComponent(child, hz::SceneTypes::localMatrix, &childMatrix));
    ASSERT_TRUE(world.setParent(child, parent, true));
    const Matrix4* local = (const Matrix4*)world.getComponent(child, hz::SceneTypes::localMatrix);
    ASSERT_NE(local, nullptr);
    for (uint32_t column = 0; column < 4; ++column)
        EXPECT_EQ((float)local->getElem(column, 3), column == 3 ? 1.0f : 0.0f);
    hz::WorldArchive archive;
    ASSERT_TRUE(archive.capture(world));
    hz::unique_ptr<hz::World> restored;
    ASSERT_TRUE(archive.instantiate(types, restored));
    const hz::Entity loaded = restored->getEntity(id(2));
    EXPECT_EQ(restored->getParent(loaded), restored->getEntity(id(1)));
    for (uint32_t column = 0; column < 4; ++column)
        for (uint32_t row = 0; row < 4; ++row)
            EXPECT_NEAR((float)restored->getWorldTransform(loaded)->current.getElem(column, row), (float)childMatrix.getElem(column, row),
                        0.00001f);
}

TEST_F(WorldArchiveTest, FailedLoadsPreserveExistingWorldAndDocument)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::unique_ptr<hz::World> target = hz::make_unique<hz::World>(types);
    const hz::Entity          original = target->createPersistentEntity(id(50));
    hz::World*                pOriginal = target.get();
    const std::string         transform = component("0000000000000003", "LocalTransform", 1, "{}");
    const std::string         invalid[] = {
        envelope(record(1) + "," + record(1)),
        envelope(record(1, "", 99)),
        envelope(record(1, "", 2) + "," + record(2, "", 1)),
        envelope(record(1, transform + "," + component("0000000000000004", "Matrix4", 1, "{}"))),
        envelope(record(1, transform + "," + transform)),
        envelope(record(1, component("0000000000000003", "LocalTransform", 1, "{\"rotation\":[0,0,0,0]}"))),
        envelope(record(1, component("0000000000000003", "LocalTransform", 1, "{\"scale\":[1e99,1,1]}"))),
        envelope(record(1, component("0000000000000007", "Camera", 1, "{\"nearPlane\":1,\"nearPlane\":2}"))),
        envelope(record(1, component("0000000000000007", "Camera", 1, "{\"verticalFov\":0}"))),
        envelope(record(1, component("0000000000000007", "Camera", 1, "{\"verticalFov\":-1}"))),
        envelope(record(1, component("0000000000000007", "Camera", 1, "{\"verticalFov\":3.141592654}"))),
        envelope(record(1, component("0000000000000007", "Camera", 1, "{\"nearPlane\":0}"))),
        envelope(record(1, component("0000000000000007", "Camera", 1, "{\"nearPlane\":-1}"))),
        envelope(record(1, component("0000000000000007", "Camera", 1, "{\"nearPlane\":1,\"farPlane\":1}"))),
        envelope(record(1, component("0000000000000007", "Camera", 1, "{\"nearPlane\":2,\"farPlane\":1}"))),
        envelope(record(1, component("0000000000000007", "Camera", 2, "{}"))),
        envelope(record(1, component("0000000000000006", "Light", 1, "{\"type\":\"Invalid\"}"))),
    };
    hz::WorldArchive archive;
    for (const std::string& json : invalid)
    {
        ASSERT_TRUE(parse(archive, json));
        EXPECT_FALSE(archive.instantiate(types, target));
        EXPECT_EQ(target.get(), pOriginal);
        EXPECT_TRUE(target->isAlive(original));
        EXPECT_EQ(text(archive), json);
    }
    const std::string previous = text(archive);
    EXPECT_FALSE(parse(archive, "{}"));
    EXPECT_FALSE(parse(archive, envelope("") + " {}"));
    EXPECT_EQ(text(archive), previous);
}

TEST_F(WorldArchiveTest, CameraValidationAlsoAppliesToCaptureAndPreservesDocument)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::World        world(types);
    const hz::Entity entity = world.createPersistentEntity(id(1));
    const hz::Camera camera = { .verticalFov = 0.001f, .nearPlane = 0.001f, .farPlane = 0.002f };
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::camera, &camera));
    hz::WorldArchive archive;
    ASSERT_TRUE(archive.capture(world));
    const std::string         previous = text(archive);
    hz::unique_ptr<hz::World> restored;
    ASSERT_TRUE(archive.instantiate(types, restored));
    const hz::Camera* pRestored = (const hz::Camera*)restored->getComponent(restored->getEntity(id(1)), hz::SceneTypes::camera);
    ASSERT_NE(pRestored, nullptr);
    EXPECT_FLOAT_EQ(pRestored->verticalFov, camera.verticalFov);
    EXPECT_FLOAT_EQ(pRestored->nearPlane, camera.nearPlane);
    EXPECT_FLOAT_EQ(pRestored->farPlane, camera.farPlane);
    const hz::Camera invalid = { .nearPlane = 1.0f, .farPlane = 1.0f };
    ASSERT_TRUE(world.setComponent(entity, hz::SceneTypes::camera, &invalid));
    EXPECT_FALSE(archive.capture(world));
    EXPECT_EQ(text(archive), previous);
}

struct MigratingComponent
{
    int32_t  value;
    uint64_t precise;
    hz::Guid reference;
    char     label[32];
};
static const hz::PropertyDesc migratingProperties[] = {
    { .id = 4,
      .pName = "label",
      .kind = hz::PropertyKind::String,
      .offset = offsetof(MigratingComponent, label),
      .size = sizeof(MigratingComponent::label) },
    { .id = 3,
      .pName = "reference",
      .kind = hz::PropertyKind::EntityReference,
      .offset = offsetof(MigratingComponent, reference),
      .size = sizeof(hz::Guid) },
    { .id = 2,
      .pName = "precise",
      .kind = hz::PropertyKind::UnsignedInteger,
      .offset = offsetof(MigratingComponent, precise),
      .size = sizeof(uint64_t) },
    { .id = 1,
      .pName = "value",
      .kind = hz::PropertyKind::SignedInteger,
      .offset = offsetof(MigratingComponent, value),
      .size = sizeof(int32_t) },
};
static const hz::TypeDesc migratingType = { .id = 99,
                                            .pName = "Migrating",
                                            .version = 3,
                                            .size = sizeof(MigratingComponent),
                                            .alignment = alignof(MigratingComponent),
                                            .properties = migratingProperties };

static bool renameMigration(hz::Span<char> input, hz::Array<char>& output, const char* from, const char* to)
{
    std::string  json(input.pData, input.count);
    const size_t offset = json.find(from);
    if (offset == std::string::npos)
        return false;
    json.replace(offset, strlen(from), to);
    output.resize((uint32_t)json.size());
    memcpy(output.data(), json.data(), json.size());
    return true;
}
static bool migrateOne(hz::Span<char> input, hz::Array<char>& output) { return renameMigration(input, output, "old", "middle"); }
static bool migrateTwo(hz::Span<char> input, hz::Array<char>& output) { return renameMigration(input, output, "middle", "value"); }

TEST_F(WorldArchiveTest, MigratesAdjacentVersionsAndResolvesForwardEntityReferences)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    ASSERT_TRUE(types.registerTypes({ &migratingType, 1 }));
    const std::string fields = "{\"old\":-2147483648,\"precise\":\"18446744073709551615\",\"reference\":\"" + uuid(2) + "\"}";
    const std::string json = envelope(record(1, component("0000000000000063", "Migrating", 1, fields)) + "," + record(2));
    hz::WorldArchive  archive;
    ASSERT_TRUE(parse(archive, json));
    hz::unique_ptr<hz::World>    target;
    const hz::ComponentMigration migrations[] = { { .type = 99, .fromVersion = 1, .pMigrate = migrateOne },
                                                  { .type = 99, .fromVersion = 2, .pMigrate = migrateTwo } };
    EXPECT_FALSE(archive.instantiate(types, target));
    EXPECT_FALSE(archive.instantiate(types, target, { migrations, 1 }));
    EXPECT_FALSE(target);
    ASSERT_TRUE(archive.instantiate(types, target, migrations));
    const MigratingComponent* value = (const MigratingComponent*)target->getComponent(target->getEntity(id(1)), 99);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(value->value, INT32_MIN);
    EXPECT_EQ(value->precise, UINT64_MAX);
    EXPECT_EQ(value->reference, id(2));
    EXPECT_EQ(text(archive), json);
    ASSERT_TRUE(archive.capture(*target));
    const std::string current = text(archive);
    EXPECT_LT(current.find("\"value\""), current.find("\"precise\""));
    EXPECT_LT(current.find("\"precise\""), current.find("\"reference\""));
    ASSERT_TRUE(archive.instantiate(types, target));

    const std::string missing = envelope(record(1, component("0000000000000063", "Migrating", 3, "{\"reference\":\"" + uuid(9) + "\"}")));
    ASSERT_TRUE(parse(archive, missing));
    hz::World* pOriginal = target.get();
    EXPECT_FALSE(archive.instantiate(types, target));
    EXPECT_EQ(target.get(), pOriginal);
}

TEST_F(WorldArchiveTest, PreservesUnknownComponentBlobWithoutInstantiatingIt)
{
    const std::string json =
        envelope(record(1, component("000000000000ffff", "FuturePlugin", 20,
                                     "{\"big\":18446744073709551615,\"opaque\":[null,{\"newField\":1.00000000000000001}]}")));
    hz::WorldArchive archive;
    ASSERT_TRUE(parse(archive, json));
    EXPECT_EQ(text(archive), json);
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::unique_ptr<hz::World> target;
    EXPECT_FALSE(archive.instantiate(types, target));
    EXPECT_FALSE(target);
    EXPECT_EQ(text(archive), json);
    const std::string opaque = envelope(record(1, component("000000000000ffff", "FuturePlugin", 20, R"({"text":"before\u0000after"})")));
    ASSERT_TRUE(parse(archive, opaque));
    EXPECT_EQ(text(archive), opaque);
}

TEST_F(WorldArchiveTest, RejectsEmbeddedNullsWithoutReplacingWorldOrTruncatingDocument)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    ASSERT_TRUE(types.registerTypes({ &migratingType, 1 }));
    hz::unique_ptr<hz::World> target = hz::make_unique<hz::World>(types);
    hz::World*                pOriginal = target.get();
    const hz::Entity          original = target->createPersistentEntity(id(50));
    const std::string         named = "{\"id\":\"" + uuid(1) + "\",\"parent\":null,\"name\":\"before\\u0000after\",\"components\":[]}";
    const std::string         invalid[] = {
        envelope(named),
        envelope(record(1, component("0000000000000063", "Migrating", 3, R"({"label":"before\u0000after"})"))),
        envelope(record(1, component("0000000000000063", "Migrating", 3, R"({"precise":"42\u0000suffix"})"))),
        envelope(record(1, component("0000000000000006", "Light", 1, R"({"type":"Point\u0000suffix"})"))),
        envelope(record(1, component("0000000000000007", "Camera", 1, R"({"nearPlane\u0000suffix":1})"))),
        envelope(record(1, component("0000000000000005", "MeshRenderer", 1, "{\"mesh\":\"" + uuid(2) + "\\u0000suffix\"}"))),
        envelope(record(1, component("0000000000000063\\u0000suffix", "Migrating", 3, "{}"))),
    };
    hz::WorldArchive archive;
    for (const std::string& json : invalid)
    {
        ASSERT_TRUE(parse(archive, json));
        EXPECT_FALSE(archive.instantiate(types, target));
        EXPECT_EQ(target.get(), pOriginal);
        EXPECT_TRUE(target->isAlive(original));
        EXPECT_EQ(text(archive), json);
    }
    // Escaped backslashes and quotes must not be confused with a null escape.
    const std::string escaped = envelope(record(1, component("0000000000000063", "Migrating", 3, R"({"label":"a\"b\\u0000c\\"})")));
    ASSERT_TRUE(parse(archive, escaped));
    ASSERT_TRUE(archive.instantiate(types, target));
    EXPECT_STREQ(((const MigratingComponent*)target->getComponent(target->getEntity(id(1)), 99))->label, "a\"b\\u0000c\\");
    ASSERT_TRUE(archive.capture(*target));
    ASSERT_TRUE(archive.instantiate(types, target));
    EXPECT_STREQ(((const MigratingComponent*)target->getComponent(target->getEntity(id(1)), 99))->label, "a\"b\\u0000c\\");
}

static bool migrateNull(hz::Span<char> input, hz::Array<char>& output)
{
    return renameMigration(input, output, "value", "value\\u0000suffix");
}

TEST_F(WorldArchiveTest, RejectsEmbeddedNullsProducedByMigration)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    ASSERT_TRUE(types.registerTypes({ &migratingType, 1 }));
    hz::unique_ptr<hz::World> target = hz::make_unique<hz::World>(types);
    hz::World*                pOriginal = target.get();
    const hz::Entity          original = target->createPersistentEntity(id(50));
    const std::string         json = envelope(record(1, component("0000000000000063", "Migrating", 2, R"({"value":7})")));
    hz::WorldArchive          archive;
    ASSERT_TRUE(parse(archive, json));
    const hz::ComponentMigration migration = { .type = 99, .fromVersion = 2, .pMigrate = migrateNull };
    EXPECT_FALSE(archive.instantiate(types, target, { &migration, 1 }));
    EXPECT_EQ(target.get(), pOriginal);
    EXPECT_TRUE(target->isAlive(original));
    EXPECT_EQ(text(archive), json);
}

TEST_F(WorldArchiveTest, NormalizesOverridesOnLoadAndRejectsDuplicateKeys)
{
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    const std::string material = uuid(7);
    const std::string second = "{\"submesh\":\"2\",\"material\":\"" + material + "\"}";
    const std::string first = "{\"submesh\":\"1\",\"material\":\"" + material + "\"}";
    hz::WorldArchive  archive;
    ASSERT_TRUE(parse(
        archive, envelope(record(1, component("0000000000000005", "MeshRenderer", 1, "{\"overrides\":[" + second + "," + first + "]}")))));
    hz::unique_ptr<hz::World> target;
    ASSERT_TRUE(archive.instantiate(types, target));
    const hz::MeshRenderer* mesh = (const hz::MeshRenderer*)target->getComponent(target->getEntity(id(1)), hz::SceneTypes::meshRenderer);
    ASSERT_NE(mesh, nullptr);
    ASSERT_EQ(mesh->overrides.size(), 2u);
    EXPECT_EQ(mesh->overrides[0].submesh, 1u);
    EXPECT_EQ(mesh->overrides[1].submesh, 2u);
    ASSERT_TRUE(archive.capture(*target));
    ASSERT_TRUE(parse(
        archive, envelope(record(1, component("0000000000000005", "MeshRenderer", 1, "{\"overrides\":[" + first + "," + first + "]}")))));
    EXPECT_FALSE(archive.instantiate(types, target));
    EXPECT_EQ(((const hz::MeshRenderer*)target->getComponent(target->getEntity(id(1)), hz::SceneTypes::meshRenderer))->overrides.size(),
              2u);
}

TEST_F(WorldArchiveTest, LoadsDeepHierarchyWithoutRecursiveTraversal)
{
    std::string        entities;
    constexpr uint32_t count = 5000;
    for (uint32_t i = 1; i <= count; ++i)
    {
        if (i != 1)
            entities += ',';
        entities += record(i, "", i == count ? 0 : i + 1);
    }
    hz::WorldArchive archive;
    ASSERT_TRUE(parse(archive, envelope(entities)));
    hz::TypeRegistry types;
    ASSERT_TRUE(types.registerBuiltins());
    hz::unique_ptr<hz::World> world;
    ASSERT_TRUE(archive.instantiate(types, world));
    EXPECT_EQ(world->getParent(world->getEntity(id(1))), world->getEntity(id(2)));
    EXPECT_FALSE(world->getParent(world->getEntity(id(count))).isValid());
    ASSERT_NE(world->getWorldTransform(world->getEntity(id(1))), nullptr);
}

TEST_F(WorldArchiveTest, SavesLoadsAndRecoversInterruptedFileReplacement)
{
    FileSystemInitDesc desc = { .pAppName = "WorldArchiveTest" };
    ASSERT_TRUE(initFileSystem(&desc));
    {
        char name[hz::kIDStringCapacity];
        hz::Guid::create().toString(name);
        const std::filesystem::path directory = std::filesystem::temp_directory_path() / name;
        std::filesystem::create_directory(directory);
        IFileSystem io = *pSystemFileIO;
        io.GetResourceMount = nullptr;
        fsSetPathForResourceDir(&io, RM_PROJECT, RD_OTHER_FILES, directory.string().c_str());
        hz::WorldArchive original, loaded;
        ASSERT_TRUE(parse(original, envelope(record(1))));
        ASSERT_TRUE(original.save(RD_OTHER_FILES, "test.world.json"));
        ASSERT_TRUE(loaded.load(RD_OTHER_FILES, "test.world.json"));
        EXPECT_EQ(text(loaded), text(original));
        std::filesystem::create_directory(directory / "test.world.json.tmp");
        ASSERT_TRUE(parse(original, envelope(record(2))));
        EXPECT_FALSE(original.save(RD_OTHER_FILES, "test.world.json"));
        ASSERT_TRUE(loaded.load(RD_OTHER_FILES, "test.world.json"));
        EXPECT_EQ(text(loaded), envelope(record(1)));
        std::filesystem::remove(directory / "test.world.json.tmp");
        std::filesystem::rename(directory / "test.world.json", directory / "test.world.json.bak");
        ASSERT_TRUE(original.save(RD_OTHER_FILES, "test.world.json"));
        ASSERT_TRUE(loaded.load(RD_OTHER_FILES, "test.world.json"));
        EXPECT_EQ(text(loaded), envelope(record(2)));
        std::filesystem::remove_all(directory);
    }
    exitFileSystem();
}
