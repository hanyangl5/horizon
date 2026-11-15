/* Copyright (c) 2026 Horizon */

#include "Scene/SceneReflection.h"

#include "Core/ILog.h"

#include <ThirdParty/stb/stb_ds.h>
#include <string.h>

namespace hz
{
static bool validName(const char* pName) { return pName && *pName; }
static bool validAlignment(uint32_t alignment) { return alignment && !(alignment & (alignment - 1)); }

static bool validProperties(const TypeDesc& type, bool arrayElement)
{
    if (!type.id || !validName(type.pName) || !type.version || !type.Size || !validAlignment(type.alignment) ||
        type.Size % type.alignment || (type.properties.Count && !type.properties.Data))
        return false;
    const TypeLifecycle& ops = type.lifecycle;
    if (!type.triviallyCopyable && (!ops.pConstruct || !ops.pDestroy || !ops.pCopyConstruct || !ops.pMoveConstruct))
        return false;
    if (ops.pDestroy && (!ops.pConstruct || !ops.pCopyConstruct || !ops.pMoveConstruct))
        return false;
    if (arrayElement && (!type.triviallyCopyable || ops.pDestroy || !type.properties.Count))
        return false;

    for (uint32_t i = 0; i < type.properties.Count; ++i)
    {
        const PropertyDesc& property = type.properties.Data[i];
        if (!property.id || !validName(property.pName) || !property.Size || property.offset > type.Size ||
            property.Size > type.Size - property.offset ||
            (property.flags & ~(PROPERTY_SERIALIZED | PROPERTY_TRANSIENT | PROPERTY_READ_ONLY | PROPERTY_EDITOR_ONLY)) ||
            ((property.flags & PROPERTY_SERIALIZED) && (property.flags & PROPERTY_TRANSIENT)))
            return false;
        for (uint32_t j = 0; j < i; ++j)
        {
            if (property.id == type.properties.Data[j].id || !strcmp(property.pName, type.properties.Data[j].pName))
                return false;
        }
        if (property.kind != PropertyKind::Array && (property.pElementType || property.pArrayOperations))
            return false;
        if (property.kind != PropertyKind::Enum && property.enumValues.Count)
            return false;

        switch (property.kind)
        {
        case PropertyKind::Boolean:
            if (property.Size != sizeof(bool))
                return false;
            break;
        case PropertyKind::SignedInteger:
        case PropertyKind::UnsignedInteger:
            if (property.Size != 1 && property.Size != 2 && property.Size != 4 && property.Size != 8)
                return false;
            break;
        case PropertyKind::Float:
            if (property.Size != 4 && property.Size != 8)
                return false;
            break;
        case PropertyKind::Vector2:
            if (property.Size != 8)
                return false;
            break;
        case PropertyKind::Vector3:
        case PropertyKind::Color:
            if (property.Size != 12 && property.Size != 16)
                return false;
            break;
        case PropertyKind::Vector4:
        case PropertyKind::Quaternion:
        case PropertyKind::AssetReference:
        case PropertyKind::EntityReference:
            if (property.Size != 16)
                return false;
            break;
        case PropertyKind::Matrix4:
            if (property.Size != 64)
                return false;
            break;
        case PropertyKind::String:
            break;
        case PropertyKind::Enum:
            if (property.Size != sizeof(int32_t) || !property.enumValues.Count || !property.enumValues.Data)
                return false;
            for (uint32_t j = 0; j < property.enumValues.Count; ++j)
            {
                const EnumValue& value = property.enumValues.Data[j];
                if (!validName(value.pName))
                    return false;
                for (uint32_t k = 0; k < j; ++k)
                {
                    if (value.value == property.enumValues.Data[k].value || !strcmp(value.pName, property.enumValues.Data[k].pName))
                        return false;
                }
            }
            break;
        case PropertyKind::Array:
        {
            const ArrayOperations* pArray = property.pArrayOperations;
            if (arrayElement || type.triviallyCopyable || !property.pElementType || !pArray ||
                !validProperties(*property.pElementType, true) || !validAlignment(pArray->alignment) ||
                type.alignment < pArray->alignment || property.offset % pArray->alignment || property.Size != pArray->Size ||
                !pArray->pCount || !pArray->Data || !pArray->pResize || !pArray->pInsert || !pArray->pRemove)
                return false;
            break;
        }
        default:
            return false;
        }
    }
    return true;
}

const PropertyDesc* TypeDesc::getProperty(PropertyID property) const
{
    for (const PropertyDesc& desc : properties)
        if (desc.id == property)
            return &desc;
    return nullptr;
}

const PropertyDesc* TypeDesc::getProperty(const char* pPropertyName) const
{
    ASSERT(pPropertyName);
    for (const PropertyDesc& desc : properties)
        if (!strcmp(desc.pName, pPropertyName))
            return &desc;
    return nullptr;
}

void TypeDesc::construct(void* pDestination) const
{
    ASSERT(pDestination);
    if (lifecycle.pConstruct)
        lifecycle.pConstruct(pDestination);
    else
        memset(pDestination, 0, Size);
}

void TypeDesc::destroy(void* pValue) const
{
    ASSERT(pValue);
    if (lifecycle.pDestroy)
        lifecycle.pDestroy(pValue);
}

void TypeDesc::copyConstruct(void* pDestination, const void* pSource) const
{
    ASSERT(pDestination && pSource && pDestination != pSource);
    if (lifecycle.pCopyConstruct)
        lifecycle.pCopyConstruct(pDestination, pSource);
    else
        memcpy(pDestination, pSource, Size);
}

void TypeDesc::moveConstruct(void* pDestination, void* pSource) const
{
    ASSERT(pDestination && pSource && pDestination != pSource);
    if (lifecycle.pMoveConstruct)
        lifecycle.pMoveConstruct(pDestination, pSource);
    else
        memcpy(pDestination, pSource, Size);
}

TypeRegistry::~TypeRegistry() { arrfree(ppTypes); }
uint32_t TypeRegistry::Size() const { return (uint32_t)arrlenu(ppTypes); }

const TypeDesc* TypeRegistry::getType(TypeID id) const
{
    for (uint32_t i = 0; i < Size(); ++i)
        if (ppTypes[i]->id == id)
            return ppTypes[i];
    return nullptr;
}

const TypeDesc* TypeRegistry::getType(const char* pName) const
{
    ASSERT(pName);
    for (uint32_t i = 0; i < Size(); ++i)
        if (!strcmp(ppTypes[i]->pName, pName))
            return ppTypes[i];
    return nullptr;
}

const TypeDesc* TypeRegistry::getTypeAt(uint32_t index) const
{
    ASSERT(index < Size());
    return ppTypes[index];
}

bool TypeRegistry::registerTypes(Span<TypeDesc> types)
{
    ASSERT(types.Data || !types.Count);
    for (uint32_t i = 0; i < types.Count; ++i)
    {
        const TypeDesc& type = types.Data[i];
        if (!validProperties(type, false))
        {
            LOGF(Error, "Invalid reflection metadata for type '%s'", type.pName ? type.pName : "<unnamed>");
            return false;
        }
        bool duplicate = getType(type.id) || getType(type.pName);
        for (uint32_t j = 0; j < i; ++j)
            duplicate |= type.id == types.Data[j].id || !strcmp(type.pName, types.Data[j].pName);
        if (duplicate)
        {
            LOGF(Error, "Duplicate reflection type '%s'", type.pName);
            return false;
        }
    }
    arrsetcap(ppTypes, Size() + types.Count);
    for (const TypeDesc& type : types)
        arrpush(ppTypes, &type);
    return true;
}
} // namespace hz
