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
cJSON* ParseWorldJson(Span<char> json);
bool   ValidWorldJsonStrings(Span<char> json);
bool   UniqueWorldFields(const cJSON* pObject);
bool   WorldVersion(const cJSON* pValue, uint32_t& version);
cJSON* WriteWorldProperties(const TypeDesc& Type, const void* pValue);
bool   ReadWorldProperties(const TypeDesc& Type, const cJSON* pObject, void* pValue);
bool   ValidateWorldReferences(const TypeDesc& Type, const void* pValue, const World& world);
bool   NormalizeWorldComponent(TypeID Type, void* pValue, bool sortOverrides);
} // namespace hz
