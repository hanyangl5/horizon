/* Copyright (c) 2026 Horizon */
#include "WorldJson.h"
#include "Scene/SceneComponents.h"
#include "../SceneMaterialOverrides.h"
#include <charconv>
#include <math.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

namespace hz
{
bool ValidWorldJsonStrings(Span<char> json)
{
    // cJSON loses decoded string lengths, so check for embedded nulls before consuming its strings.
    bool InString = false;
    for (uint32_t I = 0; I < json.Count; ++I)
    {
        const char ValueValue = json.Data[I];
        if (ValueValue == '"')
        {
            InString = !InString;
        }
        else if (InString)
        {
            if (!ValueValue || (ValueValue == '\\' && json.Count - I >= 6 && !memcmp(json.Data + I + 1, "u0000", 5)))
            {
                LOGF(Error, "World JSON strings cannot contain embedded nulls");
                return false;
            }
            if (ValueValue == '\\')
            {
                ++I;
            }
        }
    }
    return true;
}
cJSON* ParseWorldJson(Span<char> json)
{
    if (!json.Data || !json.Count || json.Count > 64 * 1024 * 1024)
    {
        return nullptr;
    }
    const char* End = nullptr;
    cJSON*      ValueData = cJSON_ParseWithLengthOpts(json.Data, json.Count, &End, false);
    if (!ValueData)
    {
        return nullptr;
    }
    while (End < json.Data + json.Count && isspace((unsigned char)*End))
    {
        ++End;
    }
    if (End < json.Data + json.Count && End + 1 == json.Data + json.Count && !*End)
    {
        ++End;
    }
    if (End != json.Data + json.Count)
    {
        cJSON_Delete(ValueData);
        return nullptr;
    }
    return ValueData;
}
bool UniqueWorldFields(const cJSON* ObjectPointer)
{
    if (!cJSON_IsObject(ObjectPointer))
    {
        return false;
    }
    for (const cJSON* LeftValue = ObjectPointer->child; LeftValue; LeftValue = LeftValue->next)
    {
        for (const cJSON* RightValue = LeftValue->next; RightValue; RightValue = RightValue->next)
        {
            if (!strcmp(LeftValue->string, RightValue->string))
            {
                return false;
            }
        }
    }
    return true;
}
bool WorldVersion(const cJSON* ValueData, uint32_t& Version)
{
    if (!cJSON_IsNumber(ValueData) || !(ValueData->valuedouble >= 1 && ValueData->valuedouble <= UINT32_MAX) ||
        floor(ValueData->valuedouble) != ValueData->valuedouble)
    {
        return false;
    }
    Version = (uint32_t)ValueData->valuedouble;
    return true;
}
static bool IsSerializedProperty(const PropertyDesc& property) { return (property.Flags & PROPERTY_SERIALIZED) != 0; }
static int  CompareProperties(const void* LeftValue, const void* RightValue)
{
    const PropertyID AValue = (*(const PropertyDesc* const*)LeftValue)->id;
    const PropertyID BValue = (*(const PropertyDesc* const*)RightValue)->id;
    return (AValue > BValue) - (AValue < BValue);
}
static cJSON* CreateJsonNumber(double ValueValue, int Precision)
{
    if (!isfinite(ValueValue))
    {
        return nullptr;
    }
    char                       Text[64];
    const std::to_chars_result ResultValue =
        std::to_chars(Text, Text + sizeof(Text) - 1, ValueValue == 0 ? 0.0 : ValueValue, std::chars_format::general, Precision);
    if (ResultValue.ec != std::errc{})
    {
        return nullptr;
    }
    *ResultValue.ptr = 0;
    return cJSON_CreateRaw(Text);
}
static uint64_t UnsignedValue(const void* ValueData, uint32_t Size)
{
    uint64_t ValueValue = 0;
    memcpy(&ValueValue, ValueData, Size);
    return ValueValue;
}
static int64_t SignedValue(const void* ValueData, uint32_t Size)
{
    const uint64_t ValueValue = UnsignedValue(ValueData, Size);
    return (int64_t)(ValueValue << (64 - Size * 8)) >> (64 - Size * 8);
}
static uint32_t FloatCount(PropertyKind Kind)
{
    switch (Kind)
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
static cJSON* WriteProperty(const PropertyDesc& property, const void* ValueData)
{
    switch (property.kind)
    {
    case PropertyKind::Boolean:
        return cJSON_CreateBool(*(const bool*)ValueData);
    case PropertyKind::SignedInteger:
    case PropertyKind::UnsignedInteger:
    {
        const bool Sign = property.kind == PropertyKind::SignedInteger;
        if (property.Size < 8)
        {
            return cJSON_CreateNumber(Sign ? (double)SignedValue(ValueData, property.Size)
                                           : (double)UnsignedValue(ValueData, property.Size));
        }
        char                       Text[32];
        const std::to_chars_result ResultValue = Sign ? std::to_chars(Text, Text + sizeof(Text) - 1, SignedValue(ValueData, 8))
                                                      : std::to_chars(Text, Text + sizeof(Text) - 1, UnsignedValue(ValueData, 8));
        *ResultValue.ptr = 0;
        return cJSON_CreateString(Text);
    }
    case PropertyKind::Float:
    {
        if (property.Size == sizeof(float))
        {
            float ValueValue;
            memcpy(&ValueValue, ValueData, sizeof(ValueValue));
            return CreateJsonNumber(ValueValue, 9);
        }
        double ValueValue;
        memcpy(&ValueValue, ValueData, sizeof(ValueValue));
        return CreateJsonNumber(ValueValue, 17);
    }
    case PropertyKind::String:
        return memchr(ValueData, 0, property.Size) ? cJSON_CreateString((const char*)ValueData) : nullptr;
    case PropertyKind::Enum:
        for (const EnumValue& Entry : property.enumValues)
        {
            if (Entry.value == SignedValue(ValueData, property.Size))
            {
                return cJSON_CreateString(Entry.Name);
            }
        }
        return nullptr;
    case PropertyKind::AssetReference:
    {
        const AssetID& Id = *(const AssetID*)ValueData;
        char           Text[kIDStringCapacity];
        Id.ToString(Text);
        return Id.IsValid() ? cJSON_CreateString(Text) : cJSON_CreateNull();
    }
    case PropertyKind::EntityReference:
    {
        const Guid& Id = *(const Guid*)ValueData;
        char        Text[kIDStringCapacity];
        Id.ToString(Text);
        return Id.IsValid() ? cJSON_CreateString(Text) : cJSON_CreateNull();
    }
    case PropertyKind::Array:
    {
        JsonValue      ArrayValue(cJSON_CreateArray());
        const uint8_t* Elements = (const uint8_t*)property.pArrayOperations->Data(ValueData);
        for (uint32_t I = 0; I < property.pArrayOperations->pCount(ValueData); ++I)
        {
            cJSON* Element = WriteWorldProperties(*property.pElementType, Elements + (size_t)I * property.pElementType->Size);
            if (!Element)
            {
                return nullptr;
            }
            cJSON_AddItemToArray(ArrayValue.pValue, Element);
        }
        cJSON* ResultPointer = ArrayValue.pValue;
        ArrayValue.pValue = nullptr;
        return ResultPointer;
    }
    default:
    {
        const uint32_t Count = FloatCount(property.kind);
        float          Values[16] = {};
        if (!Count)
        {
            return nullptr;
        }
        if (property.kind == PropertyKind::Matrix4)
        {
            for (uint32_t I = 0; I < 16; ++I)
            {
                Values[I] = (float)((const Matrix4*)ValueData)->getElem(I / 4, I % 4);
            }
        }
        else
        {
            memcpy(Values, ValueData, Count * sizeof(float));
        }
        JsonValue ArrayValue(cJSON_CreateArray());
        for (uint32_t I = 0; I < Count; ++I)
        {
            cJSON* Number = CreateJsonNumber(Values[I], 9);
            if (!Number)
            {
                return nullptr;
            }
            cJSON_AddItemToArray(ArrayValue.pValue, Number);
        }
        cJSON* ResultPointer = ArrayValue.pValue;
        ArrayValue.pValue = nullptr;
        return ResultPointer;
    }
    }
}
cJSON* WriteWorldProperties(const TypeDesc& Type, const void* ValueData)
{
    Array<const PropertyDesc*> Properties;
    for (const PropertyDesc& property : Type.properties)
    {
        if (IsSerializedProperty(property))
        {
            Properties.PushBack(&property);
        }
    }
    if (!Properties.Empty())
    {
        qsort(Properties.GetData(), Properties.Size(), sizeof(Properties[0]), CompareProperties);
    }
    JsonValue ObjectValue(cJSON_CreateObject());
    for (const PropertyDesc* PropertyPointer : Properties)
    {
        cJSON* Field = WriteProperty(*PropertyPointer, (const uint8_t*)ValueData + PropertyPointer->offset);
        if (!Field)
        {
            LOGF(Error, "Cannot Serialize property '%s.%s'", Type.Name, PropertyPointer->Name);
            return nullptr;
        }
        cJSON_AddItemToObject(ObjectValue.pValue, PropertyPointer->Name, Field);
    }
    cJSON* ResultPointer = ObjectValue.pValue;
    ObjectValue.pValue = nullptr;
    return ResultPointer;
}
static bool ReadProperty(const PropertyDesc& property, const cJSON* JsonPointer, void* ValueData)
{
    switch (property.kind)
    {
    case PropertyKind::Boolean:
        if (!cJSON_IsBool(JsonPointer))
        {
            return false;
        }
        *(bool*)ValueData = cJSON_IsTrue(JsonPointer);
        return true;
    case PropertyKind::SignedInteger:
    case PropertyKind::UnsignedInteger:
    {
        const bool Sign = property.kind == PropertyKind::SignedInteger;
        if (property.Size == 8)
        {
            if (!cJSON_IsString(JsonPointer))
            {
                return false;
            }
            const char*                  End = JsonPointer->valuestring + strlen(JsonPointer->valuestring);
            uint64_t                     UnsignedNumber = 0;
            int64_t                      SignedNumber = 0;
            const std::from_chars_result ResultValue = Sign ? std::from_chars(JsonPointer->valuestring, End, SignedNumber)
                                                            : std::from_chars(JsonPointer->valuestring, End, UnsignedNumber);
            if (ResultValue.ec != std::errc{} || ResultValue.ptr != End)
            {
                return false;
            }
            memcpy(ValueData, Sign ? (const void*)&SignedNumber : (const void*)&UnsignedNumber, 8);
            return true;
        }
        const double Minimum = Sign ? -(double)(UINT64_C(1) << (property.Size * 8 - 1)) : 0;
        const double Maximum = (double)((UINT64_C(1) << (property.Size * 8 - (Sign ? 1 : 0))) - 1);
        if (!cJSON_IsNumber(JsonPointer) || !(JsonPointer->valuedouble >= Minimum && JsonPointer->valuedouble <= Maximum) ||
            floor(JsonPointer->valuedouble) != JsonPointer->valuedouble)
        {
            return false;
        }
        const uint64_t ValueValue = Sign ? (uint64_t)(int64_t)JsonPointer->valuedouble : (uint64_t)JsonPointer->valuedouble;
        memcpy(ValueData, &ValueValue, property.Size);
        return true;
    }
    case PropertyKind::Float:
        if (!cJSON_IsNumber(JsonPointer) || !isfinite(JsonPointer->valuedouble))
        {
            return false;
        }
        if (property.Size == sizeof(float))
        {
            if (fabs(JsonPointer->valuedouble) > FLT_MAX)
            {
                return false;
            }
            const float ValueValue = (float)JsonPointer->valuedouble;
            memcpy(ValueData, &ValueValue, sizeof(ValueValue));
        }
        else
        {
            memcpy(ValueData, &JsonPointer->valuedouble, sizeof(double));
        }
        return true;
    case PropertyKind::String:
        if (!cJSON_IsString(JsonPointer) || strlen(JsonPointer->valuestring) >= property.Size)
        {
            return false;
        }
        memset(ValueData, 0, property.Size);
        memcpy(ValueData, JsonPointer->valuestring, strlen(JsonPointer->valuestring));
        return true;
    case PropertyKind::Enum:
        if (!cJSON_IsString(JsonPointer))
        {
            return false;
        }
        for (const EnumValue& Entry : property.enumValues)
        {
            if (!strcmp(Entry.Name, JsonPointer->valuestring))
            {
                memcpy(ValueData, &Entry.value, sizeof(Entry.value));
                return true;
            }
        }
        return false;
    case PropertyKind::AssetReference:
        if (cJSON_IsNull(JsonPointer))
        {
            *(AssetID*)ValueData = {};
            return true;
        }
        return cJSON_IsString(JsonPointer) && ((AssetID*)ValueData)->Parse(JsonPointer->valuestring);
    case PropertyKind::EntityReference:
        if (cJSON_IsNull(JsonPointer))
        {
            *(Guid*)ValueData = {};
            return true;
        }
        return cJSON_IsString(JsonPointer) && ((Guid*)ValueData)->Parse(JsonPointer->valuestring);
    case PropertyKind::Array:
    {
        if (!cJSON_IsArray(JsonPointer))
        {
            return false;
        }
        property.pArrayOperations->pResize(ValueData, (uint32_t)cJSON_GetArraySize(JsonPointer));
        uint8_t* Element = (uint8_t*)property.pArrayOperations->Data(ValueData);
        for (const cJSON* Item = JsonPointer->child; Item; Item = Item->next, Element += property.pElementType->Size)
        {
            if (!ReadWorldProperties(*property.pElementType, Item, Element))
            {
                return false;
            }
        }
        return true;
    }
    default:
    {
        const uint32_t Count = FloatCount(property.kind);
        if (!Count || !cJSON_IsArray(JsonPointer) || cJSON_GetArraySize(JsonPointer) != (int)Count)
        {
            return false;
        }
        float    Values[16] = {};
        uint32_t I = 0;
        for (const cJSON* Item = JsonPointer->child; Item; Item = Item->next)
        {
            if (!cJSON_IsNumber(Item) || !isfinite(Item->valuedouble) || fabs(Item->valuedouble) > FLT_MAX)
            {
                return false;
            }
            Values[I++] = (float)Item->valuedouble;
        }
        if (property.kind == PropertyKind::Matrix4)
        {
            Matrix4& Matrix = *(Matrix4*)ValueData;
            for (I = 0; I < 16; ++I)
            {
                Matrix.setElem(I / 4, I % 4, Values[I]);
            }
        }
        else
        {
            memset(ValueData, 0, property.Size);
            memcpy(ValueData, Values, Count * sizeof(float));
        }
        return true;
    }
    }
}
bool ReadWorldProperties(const TypeDesc& Type, const cJSON* ObjectPointer, void* ValueData)
{
    if (!UniqueWorldFields(ObjectPointer))
    {
        return false;
    }
    for (const cJSON* Field = ObjectPointer->child; Field; Field = Field->next)
    {
        const PropertyDesc* PropertyPointer = Type.GetProperty(Field->string);
        if (!PropertyPointer || !IsSerializedProperty(*PropertyPointer) ||
            !ReadProperty(*PropertyPointer, Field, (uint8_t*)ValueData + PropertyPointer->offset))
        {
            LOGF(Error, "Invalid serialized property '%s.%s'", Type.Name, Field->string);
            return false;
        }
    }
    return true;
}
bool ValidateWorldReferences(const TypeDesc& Type, const void* ValueData, const World& WorldData)
{
    for (const PropertyDesc& property : Type.properties)
    {
        if (!IsSerializedProperty(property))
        {
            continue;
        }
        const void* Field = (const uint8_t*)ValueData + property.offset;
        if (property.kind == PropertyKind::EntityReference)
        {
            const Guid& Id = *(const Guid*)Field;
            if (Id.IsValid() && !WorldData.GetEntity(Id).IsValid())
            {
                return false;
            }
        }
        if (property.kind == PropertyKind::Array)
        {
            const uint8_t* Elements = (const uint8_t*)property.pArrayOperations->Data(Field);
            for (uint32_t I = 0; I < property.pArrayOperations->pCount(Field); ++I)
            {
                if (!ValidateWorldReferences(*property.pElementType, Elements + (size_t)I * property.pElementType->Size, WorldData))
                {
                    return false;
                }
            }
        }
    }
    return true;
}
bool NormalizeWorldComponent(TypeID Type, void* ValueData, bool SortOverrides)
{
    if (Type == SceneTypes::camera)
    {
        const Camera& CameraData = *(const Camera*)ValueData;
        if (!isfinite(CameraData.verticalFov) || CameraData.verticalFov <= 0.0f || CameraData.verticalFov >= 3.141592654f ||
            !isfinite(CameraData.nearPlane) || !isfinite(CameraData.farPlane) || CameraData.nearPlane <= 0.0f ||
            CameraData.farPlane <= CameraData.nearPlane)
        {
            return false;
        }
    }
    if (Type == SceneTypes::meshRenderer)
    {
        Array<MaterialOverride>& Overrides = ((MeshRenderer*)ValueData)->overrides;
        if (SortOverrides && !Overrides.Empty())
        {
            qsort(Overrides.GetData(), Overrides.Size(), sizeof(MaterialOverride), CompareMaterialOverrides);
        }
        for (uint32_t I = 0; I < Overrides.Size(); ++I)
        {
            if (!Overrides[I].submesh || !Overrides[I].material.IsValid() || (I && Overrides[I - 1].submesh >= Overrides[I].submesh))
            {
                return false;
            }
        }
    }
    if (Type == SceneTypes::localTransform)
    {
        const Quat& Rotation = ((const LocalTransform*)ValueData)->rotation;
        const float Norm = (float)dot(Rotation, Rotation);
        if (!isfinite(Norm) || fabsf(Norm - 1.0f) > 0.001f)
        {
            return false;
        }
    }
    if (Type == SceneTypes::localMatrix)
    {
        Matrix4* Matrix = (Matrix4*)ValueData;
        for (uint32_t Column = 0; Column < 4; ++Column)
        {
            if ((float)Matrix->getElem(Column, 3) != (Column == 3 ? 1.0f : 0.0f))
            {
                return false;
            }
        }
    }
    return true;
}
} // namespace hz
