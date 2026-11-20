/* Copyright (c) 2026 Horizon */

#include "WorldInternal.h"
#include "WorldHierarchy.h"

#include <string.h>

namespace hz
{
static bool ValidatePropertyValue(const PropertyDesc& property, const PropertyValue& ValueValue)
{
    if (!ValueValue.Data || ValueValue.kind != property.kind || ValueValue.kind == PropertyKind::Array ||
        (ValueValue.kind == PropertyKind::String ? !ValueValue.Size || ValueValue.Size > property.Size : ValueValue.Size != property.Size))
    {
        LOGF(Error, "Property '%s': value kind or size does not match", property.Name);
        return false;
    }
    if (ValueValue.kind == PropertyKind::String && !memchr(ValueValue.Data, 0, ValueValue.Size))
    {
        LOGF(Error, "Property '%s': string is not null terminated", property.Name);
        return false;
    }
    if (ValueValue.kind == PropertyKind::Boolean && *(const uint8_t*)ValueValue.Data > 1)
    {
        LOGF(Error, "Property '%s': invalid boolean", property.Name);
        return false;
    }
    if (ValueValue.kind == PropertyKind::Enum)
    {
        int32_t EnumValueValue;
        memcpy(&EnumValueValue, ValueValue.Data, sizeof(EnumValueValue));
        for (const EnumValue& Entry : property.enumValues)
            if (Entry.value == EnumValueValue)
                return true;
        LOGF(Error, "Property '%s': unknown enum value", property.Name);
        return false;
    }
    return true;
}

static void WriteProperty(void* Destination, const PropertyDesc& property, const PropertyValue& ValueValue)
{
    const size_t Size = ValueValue.kind == PropertyKind::String ? strlen((const char*)ValueValue.Data) + 1 : ValueValue.Size;
    memmove(Destination, ValueValue.Data, Size);
    if (Size < property.Size)
    {
        memset((uint8_t*)Destination + Size, 0, property.Size - Size);
    }
}

void* WorldImpl::findEditableProperty(ecs_entity_t EntityHandle, TypeID Type, PropertyID property, const PropertyDesc*& PropertyPointer)
{
    checkThread();
    const WorldType* TypeInfo = findType(Type);
    if (!EntityHandle || !ecs_is_alive(pWorld, EntityHandle) || !TypeInfo)
    {
        LOGF(Error, "Property edit requires a live entity and registered component type");
        return nullptr;
    }
    PropertyPointer = TypeInfo->pType->GetProperty(property);
    if (!PropertyPointer || (PropertyPointer->Flags & PROPERTY_READ_ONLY) || Type == SceneTypes::entityIdentity)
    {
        LOGF(Error, "Property edit references an unknown or read-only property");
        return nullptr;
    }
    void* Component = const_cast<void*>(ecs_get_id(pWorld, EntityHandle, TypeInfo->component));
    if (!Component)
    {
        LOGF(Error, "Property edit requires an existing '%s' component", TypeInfo->pType->Name);
        return nullptr;
    }
    return (uint8_t*)Component + PropertyPointer->offset;
}

void WorldImpl::notifyPropertyChange(ecs_entity_t EntityHandle, TypeID Type) { ecs_modified_id(pWorld, EntityHandle, findType(Type)->component); }

bool World::SetProperty(Entity EntityHandle, TypeID Type, PropertyID property, const PropertyValue& ValueValue)
{
    if (!IsAlive(EntityHandle))
    {
        return false;
    }
    const PropertyDesc* PropertyPointer = nullptr;
    void*               Destination = pImpl->findEditableProperty(EntityHandle.value, Type, property, PropertyPointer);
    if (!Destination || !ValidatePropertyValue(*PropertyPointer, ValueValue))
        return false;
    if (!pImpl->pHierarchy->prepareLocalChange(EntityHandle.value, Type, true))
    {
        return false;
    }
    WriteProperty(Destination, *PropertyPointer, ValueValue);
    pImpl->notifyPropertyChange(EntityHandle.value, Type);
    return true;
}

static bool ValidateArray(const PropertyDesc& property)
{
    if (property.kind == PropertyKind::Array)
        return true;
    LOGF(Error, "Property '%s' is not an array", property.Name);
    return false;
}

bool World::ResizeArray(Entity EntityHandle, TypeID Type, PropertyID property, uint32_t Count)
{
    if (!IsAlive(EntityHandle))
    {
        return false;
    }
    const PropertyDesc* PropertyPointer = nullptr;
    void*               ArrayPointer = pImpl->findEditableProperty(EntityHandle.value, Type, property, PropertyPointer);
    if (!ArrayPointer || !ValidateArray(*PropertyPointer))
        return false;
    if (Count == PropertyPointer->pArrayOperations->pCount(ArrayPointer))
    {
        return true;
    }
    if (!pImpl->pHierarchy->prepareLocalChange(EntityHandle.value, Type, true))
    {
        return false;
    }
    PropertyPointer->pArrayOperations->pResize(ArrayPointer, Count);
    pImpl->notifyPropertyChange(EntityHandle.value, Type);
    return true;
}

bool World::InsertArrayElement(Entity EntityHandle, TypeID Type, PropertyID property, uint32_t IndexValue, const void* ValueData)
{
    if (!IsAlive(EntityHandle))
    {
        return false;
    }
    ASSERT(ValueData);
    const PropertyDesc* PropertyPointer = nullptr;
    void*               ArrayPointer = pImpl->findEditableProperty(EntityHandle.value, Type, property, PropertyPointer);
    if (!ArrayPointer || !ValidateArray(*PropertyPointer))
        return false;
    if (IndexValue > PropertyPointer->pArrayOperations->pCount(ArrayPointer))
    {
        LOGF(Error, "Property '%s': insertion index is out of range", PropertyPointer->Name);
        return false;
    }
    for (const PropertyDesc& FieldValue : PropertyPointer->pElementType->properties)
        if (!ValidatePropertyValue(FieldValue, { .kind = FieldValue.kind, .Data = (const uint8_t*)ValueData + FieldValue.offset, .Size = FieldValue.Size }))
        {
            return false;
        }
    if (!pImpl->pHierarchy->prepareLocalChange(EntityHandle.value, Type, true))
    {
        return false;
    }
    if (!PropertyPointer->pArrayOperations->pInsert(ArrayPointer, IndexValue, ValueData))
        return false;
    pImpl->notifyPropertyChange(EntityHandle.value, Type);
    return true;
}

bool World::RemoveArrayElement(Entity EntityHandle, TypeID Type, PropertyID property, uint32_t IndexValue)
{
    if (!IsAlive(EntityHandle))
    {
        return false;
    }
    const PropertyDesc* PropertyPointer = nullptr;
    void*               ArrayPointer = pImpl->findEditableProperty(EntityHandle.value, Type, property, PropertyPointer);
    if (!ArrayPointer || !ValidateArray(*PropertyPointer))
        return false;
    if (IndexValue >= PropertyPointer->pArrayOperations->pCount(ArrayPointer))
    {
        LOGF(Error, "Property '%s': removal index is out of range", PropertyPointer->Name);
        return false;
    }
    if (!pImpl->pHierarchy->prepareLocalChange(EntityHandle.value, Type, true))
    {
        return false;
    }
    if (!PropertyPointer->pArrayOperations->pRemove(ArrayPointer, IndexValue))
        return false;
    pImpl->notifyPropertyChange(EntityHandle.value, Type);
    return true;
}

bool World::SetArrayElementProperty(Entity EntityHandle, TypeID Type, PropertyID property, uint32_t IndexValue, PropertyID ElementProperty,
                                    const PropertyValue& ValueValue)
{
    if (!IsAlive(EntityHandle))
    {
        return false;
    }
    const PropertyDesc* PropertyPointer = nullptr;
    void*               ArrayPointer = pImpl->findEditableProperty(EntityHandle.value, Type, property, PropertyPointer);
    if (!ArrayPointer || !ValidateArray(*PropertyPointer))
        return false;
    if (IndexValue >= PropertyPointer->pArrayOperations->pCount(ArrayPointer))
    {
        LOGF(Error, "Property '%s': element index is out of range", PropertyPointer->Name);
        return false;
    }
    const PropertyDesc* FieldPointer = PropertyPointer->pElementType->GetProperty(ElementProperty);
    if (!FieldPointer || (FieldPointer->Flags & PROPERTY_READ_ONLY))
    {
        LOGF(Error, "Array edit references an unknown or read-only element property");
        return false;
    }
    if (!ValidatePropertyValue(*FieldPointer, ValueValue))
        return false;
    if (!pImpl->pHierarchy->prepareLocalChange(EntityHandle.value, Type, true))
    {
        return false;
    }
    void* Data = const_cast<void*>(PropertyPointer->pArrayOperations->Data(ArrayPointer));
    WriteProperty((uint8_t*)Data + (size_t)IndexValue * PropertyPointer->pElementType->Size + FieldPointer->offset, *FieldPointer, ValueValue);
    pImpl->notifyPropertyChange(EntityHandle.value, Type);
    return true;
}
} // namespace hz
