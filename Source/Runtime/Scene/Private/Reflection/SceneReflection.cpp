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
    if (!type.id || !validName(type.pName) || !type.version || !type.size || !validAlignment(type.alignment) ||
        type.size % type.alignment || (type.properties.count && !type.properties.pData))
        return false;
    const TypeLifecycle& ops = type.lifecycle;
    if (!type.triviallyCopyable && (!ops.pConstruct || !ops.pDestroy || !ops.pCopyConstruct || !ops.pMoveConstruct))
        return false;
    if (ops.pDestroy && (!ops.pConstruct || !ops.pCopyConstruct || !ops.pMoveConstruct))
        return false;
    if (arrayElement && (!type.triviallyCopyable || ops.pDestroy || !type.properties.count))
        return false;

    for (uint32_t i = 0; i < type.properties.count; ++i)
    {
        const PropertyDesc& property = type.properties.pData[i];
        if (!property.id || !validName(property.pName) || !property.size || property.offset > type.size ||
            property.size > type.size - property.offset ||
            (property.flags & ~(PROPERTY_SERIALIZED | PROPERTY_TRANSIENT | PROPERTY_READ_ONLY | PROPERTY_EDITOR_ONLY)) ||
            ((property.flags & PROPERTY_SERIALIZED) && (property.flags & PROPERTY_TRANSIENT)))
            return false;
        for (uint32_t j = 0; j < i; ++j)
        {
            if (property.id == type.properties.pData[j].id || !strcmp(property.pName, type.properties.pData[j].pName))
                return false;
        }
        if (property.kind != PropertyKind::Array && (property.pElementType || property.pArrayOperations))
            return false;
        if (property.kind != PropertyKind::Enum && property.enumValues.count)
            return false;

        switch (property.kind)
        {
        case PropertyKind::Boolean:
            if (property.size != sizeof(bool))
                return false;
            break;
        case PropertyKind::SignedInteger:
        case PropertyKind::UnsignedInteger:
            if (property.size != 1 && property.size != 2 && property.size != 4 && property.size != 8)
                return false;
            break;
        case PropertyKind::Float:
            if (property.size != 4 && property.size != 8)
                return false;
            break;
        case PropertyKind::Vector2:
            if (property.size != 8)
                return false;
            break;
        case PropertyKind::Vector3:
        case PropertyKind::Color:
            if (property.size != 12 && property.size != 16)
                return false;
            break;
        case PropertyKind::Vector4:
        case PropertyKind::Quaternion:
        case PropertyKind::AssetReference:
        case PropertyKind::ObjectReference:
            if (property.size != 16)
                return false;
            break;
        case PropertyKind::Matrix4:
            if (property.size != 64)
                return false;
            break;
        case PropertyKind::String:
            break;
        case PropertyKind::Enum:
            if (property.size != sizeof(int32_t) || !property.enumValues.count || !property.enumValues.pData)
                return false;
            for (uint32_t j = 0; j < property.enumValues.count; ++j)
            {
                const EnumValue& value = property.enumValues.pData[j];
                if (!validName(value.pName))
                    return false;
                for (uint32_t k = 0; k < j; ++k)
                {
                    if (value.value == property.enumValues.pData[k].value || !strcmp(value.pName, property.enumValues.pData[k].pName))
                        return false;
                }
            }
            break;
        case PropertyKind::Array:
        {
            const ArrayOperations* pArray = property.pArrayOperations;
            if (arrayElement || type.triviallyCopyable || !property.pElementType || !pArray ||
                !validProperties(*property.pElementType, true) || !validAlignment(pArray->alignment) ||
                type.alignment < pArray->alignment || property.offset % pArray->alignment || property.size != pArray->size ||
                !pArray->pCount || !pArray->pData || !pArray->pResize || !pArray->pInsert || !pArray->pRemove)
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
        memset(pDestination, 0, size);
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
        memcpy(pDestination, pSource, size);
}

void TypeDesc::moveConstruct(void* pDestination, void* pSource) const
{
    ASSERT(pDestination && pSource && pDestination != pSource);
    if (lifecycle.pMoveConstruct)
        lifecycle.pMoveConstruct(pDestination, pSource);
    else
        memcpy(pDestination, pSource, size);
}

TypeRegistry::~TypeRegistry() { arrfree(ppTypes); }
uint32_t TypeRegistry::size() const { return (uint32_t)arrlenu(ppTypes); }

const TypeDesc* TypeRegistry::getType(TypeID id) const
{
    for (uint32_t i = 0; i < size(); ++i)
        if (ppTypes[i]->id == id)
            return ppTypes[i];
    return nullptr;
}

const TypeDesc* TypeRegistry::getType(const char* pName) const
{
    ASSERT(pName);
    for (uint32_t i = 0; i < size(); ++i)
        if (!strcmp(ppTypes[i]->pName, pName))
            return ppTypes[i];
    return nullptr;
}

const TypeDesc* TypeRegistry::getTypeAt(uint32_t index) const
{
    ASSERT(index < size());
    return ppTypes[index];
}

bool TypeRegistry::registerTypes(Span<TypeDesc> types)
{
    ASSERT(types.pData || !types.count);
    for (uint32_t i = 0; i < types.count; ++i)
    {
        const TypeDesc& type = types.pData[i];
        if (!validProperties(type, false))
        {
            LOGF(eERROR, "Invalid reflection metadata for type '%s'", type.pName ? type.pName : "<unnamed>");
            return false;
        }
        bool duplicate = getType(type.id) || getType(type.pName);
        for (uint32_t j = 0; j < i; ++j)
            duplicate |= type.id == types.pData[j].id || !strcmp(type.pName, types.pData[j].pName);
        if (duplicate)
        {
            LOGF(eERROR, "Duplicate reflection type '%s'", type.pName);
            return false;
        }
    }
    arrsetcap(ppTypes, size() + types.count);
    for (const TypeDesc& type : types)
        arrpush(ppTypes, &type);
    return true;
}
} // namespace hz
