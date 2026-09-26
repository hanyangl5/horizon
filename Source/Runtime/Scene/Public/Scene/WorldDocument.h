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
class unique_ptr;
class World;
class WorldDocumentData;

struct ComponentMigration
{
    TypeID   type = 0;
    uint32_t fromVersion = 0;
    // Transform the properties JSON object from v to v+1. Output excludes or includes one terminating null.
    bool (*pMigrate)(Span<char> properties, Array<char>& output) = nullptr;
};

class WorldDocument
{
public:
    WorldDocument() = default;
    ~WorldDocument();
    WorldDocument(const WorldDocument&) = delete;
    WorldDocument& operator=(const WorldDocument&) = delete;

    // Parsing retains original JSON, including unknown component blobs. Failure preserves this document.
    bool     parse(Span<char> json);
    bool     load(ResourceDirectory directory, const char* pFileName);
    bool     save(ResourceDirectory directory, const char* pFileName) const;
    // Output includes a terminating null. Parsed documents retain their original text exactly.
    bool     serialize(Array<char>& json) const;
    ObjectID getID() const;
    // Captures authoring objects only; an existing document ID is retained unless explicitly replaced.
    bool     capture(World& world, ObjectID id = {});
    // Replaces target only after complete validation. Destroy its queries/scopes before calling.
    bool     instantiate(const TypeRegistry& types, unique_ptr<World>& target, Span<ComponentMigration> migrations = {},
                         const IDGenerator& objectIDs = {}) const;

private:
    WorldDocumentData* pData = nullptr;
};
} // namespace hz
