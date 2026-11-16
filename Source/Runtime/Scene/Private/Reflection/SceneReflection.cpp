/* Copyright (c) 2026 Horizon */

#include "Scene/SceneReflection.h"

#include "Core/ILog.h"

#include <ThirdParty/stb/stb_ds.h>
#include <string.h>

namespace hz
{
static bool validName(const char* Name) { return Name && *Name; }
static bool validAlignment(uint32_t alignment) { return alignment && !(alignment & (alignment - 1)); }

static bool validProperties(const TypeDesc& Type, bool arrayElement)
{
    if (!Type.id || !validName(Type.Name) || !Type.version || !Type.Size || !validAlignment(Type.alignment) ||
        Type.Size % Type.alignment || (Type.properties.Count && !Type.properties.Data))
        return false;
    const TypeLifecycle& ops = Type.lifecycle;
    if (!Type.triviallyCopyable && (!ops.pConstruct || !ops.pDestroy || !ops.pCopyConstruct || !ops.pMoveConstruct))
        return false;
    if (ops.pDestroy && (!ops.pConstruct || !ops.pCopyConstruct || !ops.pMoveConstruct))
        return false;
    if (arrayElement && (!Type.triviallyCopyable || ops.pDestroy || !Type.properties.Count))
        return false;

    for (uint32_t i = 0; i < Type.properties.Count; ++i)
    {
        const PropertyDesc& property = Type.properties.Data[i];
        if (!property.id || !validName(property.Name) || !property.Size || property.offset > Type.Size ||
            property.Size > Type.Size - property.offset ||
            (property.Flags & ~(PROPERTY_SERIALIZED | PROPERTY_TRANSIENT | PROPERTY_READ_ONLY | PROPERTY_EDITOR_ONLY)) ||
            ((property.Flags & PROPERTY_SERIALIZED) && (property.Flags & PROPERTY_TRANSIENT)))
            return false;
        for (uint32_t j = 0; j < i; ++j)
        {
            if (property.id == Type.properties.Data[j].id || !strcmp(property.Name, Type.properties.Data[j].Name))
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
                if (!validName(value.Name))
                    return false;
                for (uint32_t k = 0; k < j; ++k)
                {
                    if (value.value == property.enumValues.Data[k].value || !strcmp(value.Name, property.enumValues.Data[k].Name))
                        return false;
                }
            }
            break;
        case PropertyKind::Array:
        {
            const ArrayOperations* pArray = property.pArrayOperations;
            if (arrayElement || Type.triviallyCopyable || !property.pElementType || !pArray ||
                !validProperties(*property.pElementType, true) || !validAlignment(pArray->alignment) ||
                Type.alignment < pArray->alignment || property.offset % pArray->alignment || property.Size != pArray->Size ||
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
        if (!strcmp(desc.Name, pPropertyName))
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

const TypeDesc* TypeRegistry::getType(const char* Name) const
{
    ASSERT(Name);
    for (uint32_t i = 0; i < Size(); ++i)
        if (!strcmp(ppTypes[i]->Name, Name))
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
        const TypeDesc& Type = types.Data[i];
        if (!validProperties(Type, false))
        {
            LOGF(Error, "Invalid reflection metadata for type '%s'", Type.Name ? Type.Name : "<unnamed>");
            return false;
        }
        bool duplicate = getType(Type.id) || getType(Type.Name);
        for (uint32_t j = 0; j < i; ++j)
            duplicate |= Type.id == types.Data[j].id || !strcmp(Type.Name, types.Data[j].Name);
        if (duplicate)
        {
            LOGF(Error, "Duplicate reflection type '%s'", Type.Name);
            return false;
        }
    }
    arrsetcap(ppTypes, Size() + types.Count);
    for (const TypeDesc& Type : types)
        arrpush(ppTypes, &Type);
    return true;
}
} // namespace hz
