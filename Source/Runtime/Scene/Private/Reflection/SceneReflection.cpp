/* Copyright (c) 2026 Horizon */

#include "Scene/SceneReflection.h"

#include "Core/ILog.h"

#include <ThirdParty/stb/stb_ds.h>
#include <string.h>

namespace hz
{
static bool ValidName(const char* Name) { return Name && *Name; }
static bool ValidAlignment(uint32_t Alignment) { return Alignment && !(Alignment & (Alignment - 1)); }

static bool ValidProperties(const TypeDesc& Type, bool ArrayElement)
{
    if (!Type.id || !ValidName(Type.Name) || !Type.version || !Type.Size || !ValidAlignment(Type.alignment) || Type.Size % Type.alignment ||
        (Type.properties.Count && !Type.properties.Data))
    {
        return false;
    }
    const TypeLifecycle& Ops = Type.lifecycle;
    if (!Type.triviallyCopyable && (!Ops.pConstruct || !Ops.pDestroy || !Ops.pCopyConstruct || !Ops.pMoveConstruct))
    {
        return false;
    }
    if (Ops.pDestroy && (!Ops.pConstruct || !Ops.pCopyConstruct || !Ops.pMoveConstruct))
        return false;
    if (ArrayElement && (!Type.triviallyCopyable || Ops.pDestroy || !Type.properties.Count))
    {
        return false;
    }

    for (uint32_t I = 0; I < Type.properties.Count; ++I)
    {
        const PropertyDesc& Property = Type.properties.Data[I];
        if (!Property.id || !ValidName(Property.Name) || !Property.Size || Property.offset > Type.Size ||
            Property.Size > Type.Size - Property.offset ||
            (Property.Flags & ~(PROPERTY_SERIALIZED | PROPERTY_TRANSIENT | PROPERTY_READ_ONLY | PROPERTY_EDITOR_ONLY)) ||
            ((Property.Flags & PROPERTY_SERIALIZED) && (Property.Flags & PROPERTY_TRANSIENT)))
        {
            return false;
        }
        for (uint32_t J = 0; J < I; ++J)
        {
            if (Property.id == Type.properties.Data[J].id || !strcmp(Property.Name, Type.properties.Data[J].Name))
            {
                return false;
            }
        }
        if (Property.kind != PropertyKind::Array && (Property.pElementType || Property.pArrayOperations))
            return false;
        if (Property.kind != PropertyKind::Enum && Property.enumValues.Count)
        {
            return false;
        }

        switch (Property.kind)
        {
        case PropertyKind::Boolean:
            if (Property.Size != sizeof(bool))
            {
                return false;
            }
            break;
        case PropertyKind::SignedInteger:
        case PropertyKind::UnsignedInteger:
            if (Property.Size != 1 && Property.Size != 2 && Property.Size != 4 && Property.Size != 8)
            {
                return false;
            }
            break;
        case PropertyKind::Float:
            if (Property.Size != 4 && Property.Size != 8)
            {
                return false;
            }
            break;
        case PropertyKind::Vector2:
            if (Property.Size != 8)
            {
                return false;
            }
            break;
        case PropertyKind::Vector3:
        case PropertyKind::Color:
            if (Property.Size != 12 && Property.Size != 16)
            {
                return false;
            }
            break;
        case PropertyKind::Vector4:
        case PropertyKind::Quaternion:
        case PropertyKind::AssetReference:
        case PropertyKind::EntityReference:
            if (Property.Size != 16)
            {
                return false;
            }
            break;
        case PropertyKind::Matrix4:
            if (Property.Size != 64)
            {
                return false;
            }
            break;
        case PropertyKind::String:
            break;
        case PropertyKind::Enum:
            if (Property.Size != sizeof(int32_t) || !Property.enumValues.Count || !Property.enumValues.Data)
            {
                return false;
            }
            for (uint32_t J = 0; J < Property.enumValues.Count; ++J)
            {
                const EnumValue& ValueValue = Property.enumValues.Data[J];
                if (!ValidName(ValueValue.Name))
                {
                    return false;
                }
                for (uint32_t K = 0; K < J; ++K)
                {
                    if (ValueValue.value == Property.enumValues.Data[K].value || !strcmp(ValueValue.Name, Property.enumValues.Data[K].Name))
                    {
                        return false;
                    }
                }
            }
            break;
        case PropertyKind::Array:
        {
            const ArrayOperations* ArrayPointer = Property.pArrayOperations;
            if (ArrayElement || Type.triviallyCopyable || !Property.pElementType || !ArrayPointer ||
                !ValidProperties(*Property.pElementType, true) || !ValidAlignment(ArrayPointer->alignment) ||
                Type.alignment < ArrayPointer->alignment || Property.offset % ArrayPointer->alignment || Property.Size != ArrayPointer->Size ||
                !ArrayPointer->pCount || !ArrayPointer->Data || !ArrayPointer->pResize || !ArrayPointer->pInsert || !ArrayPointer->pRemove)
            {
                return false;
            }
            break;
        }
        default:
            return false;
        }
    }
    return true;
}

const PropertyDesc* TypeDesc::GetProperty(PropertyID Property) const
{
    for (const PropertyDesc& Desc : properties)
        if (Desc.id == Property)
            return &Desc;
    return nullptr;
}

const PropertyDesc* TypeDesc::GetProperty(const char* PropertyName) const
{
    ASSERT(PropertyName);
    for (const PropertyDesc& Desc : properties)
        if (!strcmp(Desc.Name, PropertyName))
        {
            return &Desc;
        }
    return nullptr;
}

void TypeDesc::Construct(void* Destination) const
{
    ASSERT(Destination);
    if (lifecycle.pConstruct)
        lifecycle.pConstruct(Destination);
    else
        memset(Destination, 0, Size);
}

void TypeDesc::Destroy(void* ValueData) const
{
    ASSERT(ValueData);
    if (lifecycle.pDestroy)
        lifecycle.pDestroy(ValueData);
}

void TypeDesc::CopyConstruct(void* Destination, const void* Source) const
{
    ASSERT(Destination && Source && Destination != Source);
    if (lifecycle.pCopyConstruct)
        lifecycle.pCopyConstruct(Destination, Source);
    else
        memcpy(Destination, Source, Size);
}

void TypeDesc::MoveConstruct(void* Destination, void* Source) const
{
    ASSERT(Destination && Source && Destination != Source);
    if (lifecycle.pMoveConstruct)
        lifecycle.pMoveConstruct(Destination, Source);
    else
        memcpy(Destination, Source, Size);
}

TypeRegistry::~TypeRegistry() { arrfree(ppTypes); }
uint32_t TypeRegistry::Size() const { return (uint32_t)arrlenu(ppTypes); }

const TypeDesc* TypeRegistry::GetType(TypeID Id) const
{
    for (uint32_t I = 0; I < Size(); ++I)
    {
        if (ppTypes[I]->id == Id)
            return ppTypes[I];
    }
    return nullptr;
}

const TypeDesc* TypeRegistry::GetType(const char* Name) const
{
    ASSERT(Name);
    for (uint32_t I = 0; I < Size(); ++I)
    {
        if (!strcmp(ppTypes[I]->Name, Name))
        {
            return ppTypes[I];
        }
    }
    return nullptr;
}

const TypeDesc* TypeRegistry::GetTypeAt(uint32_t IndexValue) const
{
    ASSERT(IndexValue < Size());
    return ppTypes[IndexValue];
}

bool TypeRegistry::RegisterTypes(Span<TypeDesc> Types)
{
    ASSERT(Types.Data || !Types.Count);
    for (uint32_t I = 0; I < Types.Count; ++I)
    {
        const TypeDesc& Type = Types.Data[I];
        if (!ValidProperties(Type, false))
        {
            LOGF(Error, "Invalid reflection metadata for type '%s'", Type.Name ? Type.Name : "<unnamed>");
            return false;
        }
        bool Duplicate = GetType(Type.id) || GetType(Type.Name);
        for (uint32_t J = 0; J < I; ++J)
            Duplicate |= Type.id == Types.Data[J].id || !strcmp(Type.Name, Types.Data[J].Name);
        if (Duplicate)
        {
            LOGF(Error, "Duplicate reflection type '%s'", Type.Name);
            return false;
        }
    }
    arrsetcap(ppTypes, Size() + Types.Count);
    for (const TypeDesc& Type : Types)
    {
        arrpush(ppTypes, &Type);
    }
    return true;
}
} // namespace hz
