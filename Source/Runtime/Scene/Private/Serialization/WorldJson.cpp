/* Copyright (c) 2026 Horizon */
#include "WorldJson.h"
#include "Scene/SceneComponents.h"
#include <charconv>
#include <math.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

namespace hz
{
bool validWorldJsonStrings(Span<char> json)
{
    // cJSON loses decoded string lengths, so check for embedded nulls before consuming its strings.
    bool inString = false;
    for (uint32_t i = 0; i < json.count; ++i)
    {
        const char value = json.pData[i];
        if (value == '"')
            inString = !inString;
        else if (inString)
        {
            if (!value || (value == '\\' && json.count - i >= 6 && !memcmp(json.pData + i + 1, "u0000", 5)))
            {
                LOGF(eERROR, "World JSON strings cannot contain embedded nulls");
                return false;
            }
            if (value == '\\')
                ++i;
        }
    }
    return true;
}
cJSON* parseWorldJson(Span<char> json)
{
    if (!json.pData || !json.count || json.count > 64 * 1024 * 1024)
        return nullptr;
    const char* pEnd = nullptr;
    cJSON*      pValue = cJSON_ParseWithLengthOpts(json.pData, json.count, &pEnd, false);
    if (!pValue)
        return nullptr;
    while (pEnd < json.pData + json.count && isspace((unsigned char)*pEnd))
        ++pEnd;
    if (pEnd < json.pData + json.count && pEnd + 1 == json.pData + json.count && !*pEnd)
        ++pEnd;
    if (pEnd != json.pData + json.count)
    {
        cJSON_Delete(pValue);
        return nullptr;
    }
    return pValue;
}
bool uniqueWorldFields(const cJSON* pObject)
{
    if (!cJSON_IsObject(pObject))
        return false;
    for (const cJSON* pA = pObject->child; pA; pA = pA->next)
        for (const cJSON* pB = pA->next; pB; pB = pB->next)
            if (!strcmp(pA->string, pB->string))
                return false;
    return true;
}
bool worldVersion(const cJSON* pValue, uint32_t& version)
{
    if (!cJSON_IsNumber(pValue) || !(pValue->valuedouble >= 1 && pValue->valuedouble <= UINT32_MAX) ||
        floor(pValue->valuedouble) != pValue->valuedouble)
        return false;
    version = (uint32_t)pValue->valuedouble;
    return true;
}
static bool serialized(const PropertyDesc& property) { return (property.flags & PROPERTY_SERIALIZED) != 0; }
static int  compareProperties(const void* pA, const void* pB)
{
    const PropertyID a = (*(const PropertyDesc* const*)pA)->id;
    const PropertyID b = (*(const PropertyDesc* const*)pB)->id;
    return (a > b) - (a < b);
}
static cJSON* number(double value, int precision)
{
    if (!isfinite(value))
        return nullptr;
    char                       text[64];
    const std::to_chars_result result =
        std::to_chars(text, text + sizeof(text) - 1, value == 0 ? 0.0 : value, std::chars_format::general, precision);
    if (result.ec != std::errc{})
        return nullptr;
    *result.ptr = 0;
    return cJSON_CreateRaw(text);
}
static uint64_t unsignedValue(const void* pValue, uint32_t size)
{
    uint64_t value = 0;
    memcpy(&value, pValue, size);
    return value;
}
static int64_t signedValue(const void* pValue, uint32_t size)
{
    const uint64_t value = unsignedValue(pValue, size);
    return (int64_t)(value << (64 - size * 8)) >> (64 - size * 8);
}
static uint32_t floatCount(PropertyKind kind)
{
    switch (kind)
    {
    case PropertyKind::Vector2:
        return 2;
    case PropertyKind::Vector3:
    case PropertyKind::Color:
        return 3;
    case PropertyKind::Vector4:
    case PropertyKind::Quaternion:
        return 4;
    case PropertyKind::Matrix4:
        return 16;
    default:
        return 0;
    }
}
static cJSON* writeProperty(const PropertyDesc& property, const void* pValue)
{
    switch (property.kind)
    {
    case PropertyKind::Boolean:
        return cJSON_CreateBool(*(const bool*)pValue);
    case PropertyKind::SignedInteger:
    case PropertyKind::UnsignedInteger:
    {
        const bool sign = property.kind == PropertyKind::SignedInteger;
        if (property.size < 8)
            return cJSON_CreateNumber(sign ? (double)signedValue(pValue, property.size) : (double)unsignedValue(pValue, property.size));
        char                       text[32];
        const std::to_chars_result result = sign ? std::to_chars(text, text + sizeof(text) - 1, signedValue(pValue, 8))
                                                 : std::to_chars(text, text + sizeof(text) - 1, unsignedValue(pValue, 8));
        *result.ptr = 0;
        return cJSON_CreateString(text);
    }
    case PropertyKind::Float:
    {
        if (property.size == sizeof(float))
        {
            float value;
            memcpy(&value, pValue, sizeof(value));
            return number(value, 9);
        }
        double value;
        memcpy(&value, pValue, sizeof(value));
        return number(value, 17);
    }
    case PropertyKind::String:
        return memchr(pValue, 0, property.size) ? cJSON_CreateString((const char*)pValue) : nullptr;
    case PropertyKind::Enum:
        for (const EnumValue& entry : property.enumValues)
            if (entry.value == signedValue(pValue, property.size))
                return cJSON_CreateString(entry.pName);
        return nullptr;
    case PropertyKind::AssetReference:
    {
        const AssetID& id = *(const AssetID*)pValue;
        char           text[kIDStringCapacity];
        id.toString(text);
        return id.isValid() ? cJSON_CreateString(text) : cJSON_CreateNull();
    }
    case PropertyKind::ObjectReference:
    {
        const ObjectID& id = *(const ObjectID*)pValue;
        char            text[kIDStringCapacity];
        id.toString(text);
        return id.isValid() ? cJSON_CreateString(text) : cJSON_CreateNull();
    }
    case PropertyKind::Array:
    {
        JsonValue      array(cJSON_CreateArray());
        const uint8_t* pElements = (const uint8_t*)property.pArrayOperations->pData(pValue);
        for (uint32_t i = 0; i < property.pArrayOperations->pCount(pValue); ++i)
        {
            cJSON* pElement = writeWorldProperties(*property.pElementType, pElements + (size_t)i * property.pElementType->size);
            if (!pElement)
                return nullptr;
            cJSON_AddItemToArray(array.pValue, pElement);
        }
        cJSON* pResult = array.pValue;
        array.pValue = nullptr;
        return pResult;
    }
    default:
    {
        const uint32_t count = floatCount(property.kind);
        float          values[16] = {};
        if (!count)
            return nullptr;
        if (property.kind == PropertyKind::Matrix4)
            for (uint32_t i = 0; i < 16; ++i)
                values[i] = (float)((const Matrix4*)pValue)->getElem(i / 4, i % 4);
        else
            memcpy(values, pValue, count * sizeof(float));
        JsonValue array(cJSON_CreateArray());
        for (uint32_t i = 0; i < count; ++i)
        {
            cJSON* pNumber = number(values[i], 9);
            if (!pNumber)
                return nullptr;
            cJSON_AddItemToArray(array.pValue, pNumber);
        }
        cJSON* pResult = array.pValue;
        array.pValue = nullptr;
        return pResult;
    }
    }
}
cJSON* writeWorldProperties(const TypeDesc& type, const void* pValue)
{
    Array<const PropertyDesc*> properties;
    for (const PropertyDesc& property : type.properties)
        if (serialized(property))
            properties.pushBack(&property);
    if (!properties.empty())
        qsort(properties.data(), properties.size(), sizeof(properties[0]), compareProperties);
    JsonValue object(cJSON_CreateObject());
    for (const PropertyDesc* pProperty : properties)
    {
        cJSON* pField = writeProperty(*pProperty, (const uint8_t*)pValue + pProperty->offset);
        if (!pField)
        {
            LOGF(eERROR, "Cannot serialize property '%s.%s'", type.pName, pProperty->pName);
            return nullptr;
        }
        cJSON_AddItemToObject(object.pValue, pProperty->pName, pField);
    }
    cJSON* pResult = object.pValue;
    object.pValue = nullptr;
    return pResult;
}
static bool readProperty(const PropertyDesc& property, const cJSON* pJson, void* pValue)
{
    switch (property.kind)
    {
    case PropertyKind::Boolean:
        if (!cJSON_IsBool(pJson))
            return false;
        *(bool*)pValue = cJSON_IsTrue(pJson);
        return true;
    case PropertyKind::SignedInteger:
    case PropertyKind::UnsignedInteger:
    {
        const bool sign = property.kind == PropertyKind::SignedInteger;
        if (property.size == 8)
        {
            if (!cJSON_IsString(pJson))
                return false;
            const char*                  pEnd = pJson->valuestring + strlen(pJson->valuestring);
            uint64_t                     unsignedNumber = 0;
            int64_t                      signedNumber = 0;
            const std::from_chars_result result =
                sign ? std::from_chars(pJson->valuestring, pEnd, signedNumber) : std::from_chars(pJson->valuestring, pEnd, unsignedNumber);
            if (result.ec != std::errc{} || result.ptr != pEnd)
                return false;
            memcpy(pValue, sign ? (const void*)&signedNumber : (const void*)&unsignedNumber, 8);
            return true;
        }
        const double minimum = sign ? -(double)(UINT64_C(1) << (property.size * 8 - 1)) : 0;
        const double maximum = (double)((UINT64_C(1) << (property.size * 8 - (sign ? 1 : 0))) - 1);
        if (!cJSON_IsNumber(pJson) || !(pJson->valuedouble >= minimum && pJson->valuedouble <= maximum) ||
            floor(pJson->valuedouble) != pJson->valuedouble)
            return false;
        const uint64_t value = sign ? (uint64_t)(int64_t)pJson->valuedouble : (uint64_t)pJson->valuedouble;
        memcpy(pValue, &value, property.size);
        return true;
    }
    case PropertyKind::Float:
        if (!cJSON_IsNumber(pJson) || !isfinite(pJson->valuedouble))
            return false;
        if (property.size == sizeof(float))
        {
            if (fabs(pJson->valuedouble) > FLT_MAX)
                return false;
            const float value = (float)pJson->valuedouble;
            memcpy(pValue, &value, sizeof(value));
        }
        else
            memcpy(pValue, &pJson->valuedouble, sizeof(double));
        return true;
    case PropertyKind::String:
        if (!cJSON_IsString(pJson) || strlen(pJson->valuestring) >= property.size)
            return false;
        memset(pValue, 0, property.size);
        memcpy(pValue, pJson->valuestring, strlen(pJson->valuestring));
        return true;
    case PropertyKind::Enum:
        if (!cJSON_IsString(pJson))
            return false;
        for (const EnumValue& entry : property.enumValues)
            if (!strcmp(entry.pName, pJson->valuestring))
            {
                memcpy(pValue, &entry.value, sizeof(entry.value));
                return true;
            }
        return false;
    case PropertyKind::AssetReference:
        if (cJSON_IsNull(pJson))
        {
            *(AssetID*)pValue = {};
            return true;
        }
        return cJSON_IsString(pJson) && ((AssetID*)pValue)->parse(pJson->valuestring);
    case PropertyKind::ObjectReference:
        if (cJSON_IsNull(pJson))
        {
            *(ObjectID*)pValue = {};
            return true;
        }
        return cJSON_IsString(pJson) && ((ObjectID*)pValue)->parse(pJson->valuestring);
    case PropertyKind::Array:
    {
        if (!cJSON_IsArray(pJson))
            return false;
        property.pArrayOperations->pResize(pValue, (uint32_t)cJSON_GetArraySize(pJson));
        uint8_t* pElement = (uint8_t*)property.pArrayOperations->pData(pValue);
        for (const cJSON* pItem = pJson->child; pItem; pItem = pItem->next, pElement += property.pElementType->size)
            if (!readWorldProperties(*property.pElementType, pItem, pElement))
                return false;
        return true;
    }
    default:
    {
        const uint32_t count = floatCount(property.kind);
        if (!count || !cJSON_IsArray(pJson) || cJSON_GetArraySize(pJson) != (int)count)
            return false;
        float    values[16] = {};
        uint32_t i = 0;
        for (const cJSON* pItem = pJson->child; pItem; pItem = pItem->next)
        {
            if (!cJSON_IsNumber(pItem) || !isfinite(pItem->valuedouble) || fabs(pItem->valuedouble) > FLT_MAX)
                return false;
            values[i++] = (float)pItem->valuedouble;
        }
        if (property.kind == PropertyKind::Matrix4)
        {
            Matrix4& matrix = *(Matrix4*)pValue;
            for (i = 0; i < 16; ++i)
                matrix.setElem(i / 4, i % 4, values[i]);
        }
        else
        {
            memset(pValue, 0, property.size);
            memcpy(pValue, values, count * sizeof(float));
        }
        return true;
    }
    }
}
bool readWorldProperties(const TypeDesc& type, const cJSON* pObject, void* pValue)
{
    if (!uniqueWorldFields(pObject))
        return false;
    for (const cJSON* pField = pObject->child; pField; pField = pField->next)
    {
        const PropertyDesc* pProperty = type.getProperty(pField->string);
        if (!pProperty || !serialized(*pProperty) || !readProperty(*pProperty, pField, (uint8_t*)pValue + pProperty->offset))
        {
            LOGF(eERROR, "Invalid serialized property '%s.%s'", type.pName, pField->string);
            return false;
        }
    }
    return true;
}
bool validateWorldReferences(const TypeDesc& type, const void* pValue, const World& world)
{
    for (const PropertyDesc& property : type.properties)
    {
        if (!serialized(property))
            continue;
        const void* pField = (const uint8_t*)pValue + property.offset;
        if (property.kind == PropertyKind::ObjectReference)
        {
            const ObjectID& id = *(const ObjectID*)pField;
            if (id.isValid() && !world.getEntity(id).isValid())
                return false;
        }
        if (property.kind == PropertyKind::Array)
        {
            const uint8_t* pElements = (const uint8_t*)property.pArrayOperations->pData(pField);
            for (uint32_t i = 0; i < property.pArrayOperations->pCount(pField); ++i)
                if (!validateWorldReferences(*property.pElementType, pElements + (size_t)i * property.pElementType->size, world))
                    return false;
        }
    }
    return true;
}
static int compareOverrides(const void* pA, const void* pB)
{
    const SubmeshID a = ((const MaterialOverride*)pA)->submesh;
    const SubmeshID b = ((const MaterialOverride*)pB)->submesh;
    return (a > b) - (a < b);
}
bool normalizeWorldComponent(TypeID type, void* pValue, bool sortOverrides)
{
    if (type == SceneTypes::meshRenderer)
    {
        Array<MaterialOverride>& overrides = ((MeshRenderer*)pValue)->overrides;
        if (sortOverrides && !overrides.empty())
            qsort(overrides.data(), overrides.size(), sizeof(MaterialOverride), compareOverrides);
        for (uint32_t i = 0; i < overrides.size(); ++i)
            if (!overrides[i].submesh || !overrides[i].material.isValid() || (i && overrides[i - 1].submesh >= overrides[i].submesh))
                return false;
    }
    if (type == SceneTypes::localTransform)
    {
        const Quat& rotation = ((const LocalTransform*)pValue)->rotation;
        const float norm = (float)dot(rotation, rotation);
        if (!isfinite(norm) || fabsf(norm - 1.0f) > 0.001f)
            return false;
    }
    if (type == SceneTypes::localMatrix)
    {
        const Matrix4& matrix = ((const LocalMatrix*)pValue)->value;
        for (uint32_t column = 0; column < 4; ++column)
            if ((float)matrix.getElem(column, 3) != (column == 3 ? 1.0f : 0.0f))
                return false;
    }
    return true;
}
} // namespace hz
