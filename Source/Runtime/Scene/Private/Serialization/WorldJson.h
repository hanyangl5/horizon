/* Copyright (c) 2026 Horizon */
#pragma once
#include "Scene/WorldArchive.h"
#include "Scene/World.h"
#include "Core/IContainer.h"
#include <ThirdParty/cJSON/cJSON.h>

namespace hz
{
class JsonValue
{
public:
    explicit JsonValue(cJSON* pValue = nullptr): pValue(pValue) {}
    ~JsonValue() { cJSON_Delete(pValue); }
    JsonValue(const JsonValue&) = delete;
    JsonValue& operator=(const JsonValue&) = delete;
    cJSON*     pValue;
};
cJSON* parseWorldJson(Span<char> json);
bool   validWorldJsonStrings(Span<char> json);
bool   uniqueWorldFields(const cJSON* pObject);
bool   worldVersion(const cJSON* pValue, uint32_t& version);
cJSON* writeWorldProperties(const TypeDesc& type, const void* pValue);
bool   readWorldProperties(const TypeDesc& type, const cJSON* pObject, void* pValue);
bool   validateWorldReferences(const TypeDesc& type, const void* pValue, const World& world);
bool   normalizeWorldComponent(TypeID type, void* pValue, bool sortOverrides);
} // namespace hz
