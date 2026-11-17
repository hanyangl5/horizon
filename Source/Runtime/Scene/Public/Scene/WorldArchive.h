/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IFileSystem.h"
#include "Scene/SceneReflection.h"
#include "Scene/SceneID.h"

namespace hz
{
template<typename T>
class Array;
template<typename T>
class UniquePtr;
class World;
class WorldArchiveData;

struct ComponentMigration
{
    TypeID   Type = 0;
    uint32_t fromVersion = 0;
    // Transform the properties JSON object from v to v+1. Output excludes or includes one terminating null.
    bool (*pMigrate)(Span<char> properties, Array<char>& output) = nullptr;
};

class WorldArchive
{
public:
    WorldArchive() = default;
    ~WorldArchive();
    WorldArchive(const WorldArchive&) = delete;
    WorldArchive& operator=(const WorldArchive&) = delete;

    // Parsing retains original JSON, including unknown component blobs. Failure preserves this archive.
    bool Parse(Span<char> json);
    bool Load(ResourceDirectory directory, const char* pFileName);
    bool Save(ResourceDirectory directory, const char* pFileName) const;
    // Output includes a terminating null. Parsed archives retain their original text exactly.
    bool Serialize(Array<char>& json) const;
    Guid GetID() const;
    // Captures persistent entities only; an existing archive ID is retained unless explicitly replaced.
    bool Capture(World& world, Guid id = {});
    // Replaces target only after complete validation. Fails while target has live queries or scopes.
    bool Instantiate(const TypeRegistry& types, UniquePtr<World>& target, Span<ComponentMigration> migrations = {},
                     const IDGenerator& entityGuids = {}) const;

private:
    WorldArchiveData* Data = nullptr;
};
} // namespace hz
