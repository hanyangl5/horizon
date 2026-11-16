/* Copyright (c) 2026 Horizon */

#include "WorldInternal.h"
#include "WorldHierarchy.h"

#include <string.h>

namespace hz
{
static bool validatePropertyValue(const PropertyDesc& property, const PropertyValue& value)
{
    if (!value.Data || value.kind != property.kind || value.kind == PropertyKind::Array ||
        (value.kind == PropertyKind::String ? !value.Size || value.Size > property.Size : value.Size != property.Size))
    {
        LOGF(Error, "Property '%s': value kind or size does not match", property.Name);
        return false;
    }
    if (value.kind == PropertyKind::String && !memchr(value.Data, 0, value.Size))
    {
        LOGF(Error, "Property '%s': string is not null terminated", property.Name);
        return false;
    }
    if (value.kind == PropertyKind::Boolean && *(const uint8_t*)value.Data > 1)
    {
        LOGF(Error, "Property '%s': invalid boolean", property.Name);
        return false;
    }
    if (value.kind == PropertyKind::Enum)
    {
        int32_t enumValue;
        memcpy(&enumValue, value.Data, sizeof(enumValue));
        for (const EnumValue& entry : property.enumValues)
            if (entry.value == enumValue)
                return true;
        LOGF(Error, "Property '%s': unknown enum value", property.Name);
        return false;
    }
    return true;
}

static void writeProperty(void* pDestination, const PropertyDesc& property, const PropertyValue& value)
{
    const size_t Size = value.kind == PropertyKind::String ? strlen((const char*)value.Data) + 1 : value.Size;
    memmove(pDestination, value.Data, Size);
    if (Size < property.Size)
        memset((uint8_t*)pDestination + Size, 0, property.Size - Size);
}

void* WorldImpl::findEditableProperty(ecs_entity_t entity, TypeID Type, PropertyID property, const PropertyDesc*& pProperty)
{
    checkThread();
    const WorldType* pType = findType(Type);
    if (!entity || !ecs_is_alive(pWorld, entity) || !pType)
    {
        LOGF(Error, "Property edit requires a live entity and registered component type");
        return nullptr;
    }
    pProperty = pType->pType->getProperty(property);
    if (!pProperty || (pProperty->Flags & PROPERTY_READ_ONLY) || Type == SceneTypes::entityIdentity)
    {
        LOGF(Error, "Property edit references an unknown or read-only property");
        return nullptr;
    }
    void* pComponent = const_cast<void*>(ecs_get_id(pWorld, entity, pType->component));
    if (!pComponent)
    {
        LOGF(Error, "Property edit requires an existing '%s' component", pType->pType->Name);
        return nullptr;
    }
    return (uint8_t*)pComponent + pProperty->offset;
}

void WorldImpl::notifyPropertyChange(ecs_entity_t entity, TypeID Type) { ecs_modified_id(pWorld, entity, findType(Type)->component); }

bool World::setProperty(Entity entity, TypeID Type, PropertyID property, const PropertyValue& value)
{
    if (!isAlive(entity))
        return false;
    const PropertyDesc* pProperty = nullptr;
    void*               pDestination = pImpl->findEditableProperty(entity.value, Type, property, pProperty);
    if (!pDestination || !validatePropertyValue(*pProperty, value))
        return false;
    if (!pImpl->pHierarchy->prepareLocalChange(entity.value, Type, true))
        return false;
    writeProperty(pDestination, *pProperty, value);
    pImpl->notifyPropertyChange(entity.value, Type);
    return true;
}

static bool validateArray(const PropertyDesc& property)
{
    if (property.kind == PropertyKind::Array)
        return true;
    LOGF(Error, "Property '%s' is not an array", property.Name);
    return false;
}

bool World::resizeArray(Entity entity, TypeID Type, PropertyID property, uint32_t Count)
{
    if (!isAlive(entity))
        return false;
    const PropertyDesc* pProperty = nullptr;
    void*               pArray = pImpl->findEditableProperty(entity.value, Type, property, pProperty);
    if (!pArray || !validateArray(*pProperty))
        return false;
    if (Count == pProperty->pArrayOperations->pCount(pArray))
        return true;
    if (!pImpl->pHierarchy->prepareLocalChange(entity.value, Type, true))
        return false;
    pProperty->pArrayOperations->pResize(pArray, Count);
    pImpl->notifyPropertyChange(entity.value, Type);
    return true;
}

bool World::insertArrayElement(Entity entity, TypeID Type, PropertyID property, uint32_t index, const void* pValue)
{
    if (!isAlive(entity))
        return false;
    ASSERT(pValue);
    const PropertyDesc* pProperty = nullptr;
    void*               pArray = pImpl->findEditableProperty(entity.value, Type, property, pProperty);
    if (!pArray || !validateArray(*pProperty))
        return false;
    if (index > pProperty->pArrayOperations->pCount(pArray))
    {
        LOGF(Error, "Property '%s': insertion index is out of range", pProperty->Name);
        return false;
    }
    for (const PropertyDesc& field : pProperty->pElementType->properties)
        if (!validatePropertyValue(field, { .kind = field.kind, .Data = (const uint8_t*)pValue + field.offset, .Size = field.Size }))
            return false;
    if (!pImpl->pHierarchy->prepareLocalChange(entity.value, Type, true))
        return false;
    if (!pProperty->pArrayOperations->pInsert(pArray, index, pValue))
        return false;
    pImpl->notifyPropertyChange(entity.value, Type);
    return true;
}

bool World::removeArrayElement(Entity entity, TypeID Type, PropertyID property, uint32_t index)
{
    if (!isAlive(entity))
        return false;
    const PropertyDesc* pProperty = nullptr;
    void*               pArray = pImpl->findEditableProperty(entity.value, Type, property, pProperty);
    if (!pArray || !validateArray(*pProperty))
        return false;
    if (index >= pProperty->pArrayOperations->pCount(pArray))
    {
        LOGF(Error, "Property '%s': removal index is out of range", pProperty->Name);
        return false;
    }
    if (!pImpl->pHierarchy->prepareLocalChange(entity.value, Type, true))
        return false;
    if (!pProperty->pArrayOperations->pRemove(pArray, index))
        return false;
    pImpl->notifyPropertyChange(entity.value, Type);
    return true;
}

bool World::setArrayElementProperty(Entity entity, TypeID Type, PropertyID property, uint32_t index, PropertyID elementProperty,
                                    const PropertyValue& value)
{
    if (!isAlive(entity))
        return false;
    const PropertyDesc* pProperty = nullptr;
    void*               pArray = pImpl->findEditableProperty(entity.value, Type, property, pProperty);
    if (!pArray || !validateArray(*pProperty))
        return false;
    if (index >= pProperty->pArrayOperations->pCount(pArray))
    {
        LOGF(Error, "Property '%s': element index is out of range", pProperty->Name);
        return false;
    }
    const PropertyDesc* pField = pProperty->pElementType->getProperty(elementProperty);
    if (!pField || (pField->Flags & PROPERTY_READ_ONLY))
    {
        LOGF(Error, "Array edit references an unknown or read-only element property");
        return false;
    }
    if (!validatePropertyValue(*pField, value))
        return false;
    if (!pImpl->pHierarchy->prepareLocalChange(entity.value, Type, true))
        return false;
    void* Data = const_cast<void*>(pProperty->pArrayOperations->Data(pArray));
    writeProperty((uint8_t*)Data + (size_t)index * pProperty->pElementType->Size + pField->offset, *pField, value);
    pImpl->notifyPropertyChange(entity.value, Type);
    return true;
}
} // namespace hz
