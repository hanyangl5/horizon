/* Copyright (c) 2026 Horizon */

#include "WorldInternal.h"
#include "WorldHierarchy.h"

#include <string.h>

namespace hz
{
static bool validatePropertyValue(const PropertyDesc& property, const PropertyValue& value)
{
    if (!value.pData || value.kind != property.kind || value.kind == PropertyKind::Array ||
        (value.kind == PropertyKind::String ? !value.size || value.size > property.size : value.size != property.size))
    {
        LOGF(eERROR, "Property '%s': value kind or size does not match", property.pName);
        return false;
    }
    if (value.kind == PropertyKind::String && !memchr(value.pData, 0, value.size))
    {
        LOGF(eERROR, "Property '%s': string is not null terminated", property.pName);
        return false;
    }
    if (value.kind == PropertyKind::Boolean && *(const uint8_t*)value.pData > 1)
    {
        LOGF(eERROR, "Property '%s': invalid boolean", property.pName);
        return false;
    }
    if (value.kind == PropertyKind::Enum)
    {
        int32_t enumValue;
        memcpy(&enumValue, value.pData, sizeof(enumValue));
        for (const EnumValue& entry : property.enumValues)
            if (entry.value == enumValue)
                return true;
        LOGF(eERROR, "Property '%s': unknown enum value", property.pName);
        return false;
    }
    return true;
}

static void writeProperty(void* pDestination, const PropertyDesc& property, const PropertyValue& value)
{
    const size_t size = value.kind == PropertyKind::String ? strlen((const char*)value.pData) + 1 : value.size;
    memmove(pDestination, value.pData, size);
    if (size < property.size)
        memset((uint8_t*)pDestination + size, 0, property.size - size);
}

void* WorldImpl::findEditableProperty(ecs_entity_t entity, TypeID type, PropertyID property, const PropertyDesc*& pProperty)
{
    checkThread();
    const WorldType* pType = findType(type);
    if (!entity || !ecs_is_alive(pWorld, entity) || !pType)
    {
        LOGF(eERROR, "Property edit requires a live entity and registered component type");
        return nullptr;
    }
    pProperty = pType->pType->getProperty(property);
    if (!pProperty || (pProperty->flags & PROPERTY_READ_ONLY) || type == SceneTypes::objectIdentity)
    {
        LOGF(eERROR, "Property edit references an unknown or read-only property");
        return nullptr;
    }
    void* pComponent = const_cast<void*>(ecs_get_id(pWorld, entity, pType->component));
    if (!pComponent)
    {
        LOGF(eERROR, "Property edit requires an existing '%s' component", pType->pType->pName);
        return nullptr;
    }
    return (uint8_t*)pComponent + pProperty->offset;
}

void WorldImpl::notifyPropertyChange(ecs_entity_t entity, TypeID type) { ecs_modified_id(pWorld, entity, findType(type)->component); }

bool World::setProperty(Entity entity, TypeID type, PropertyID property, const PropertyValue& value)
{
    const PropertyDesc* pProperty = nullptr;
    void*               pDestination = pImpl->findEditableProperty(entity.value, type, property, pProperty);
    if (!pDestination || !validatePropertyValue(*pProperty, value))
        return false;
    if (!pImpl->pHierarchy->prepareLocalChange(entity.value, type, true))
        return false;
    writeProperty(pDestination, *pProperty, value);
    pImpl->notifyPropertyChange(entity.value, type);
    return true;
}

static bool validateArray(const PropertyDesc& property)
{
    if (property.kind == PropertyKind::Array)
        return true;
    LOGF(eERROR, "Property '%s' is not an array", property.pName);
    return false;
}

bool World::resizeArray(Entity entity, TypeID type, PropertyID property, uint32_t count)
{
    const PropertyDesc* pProperty = nullptr;
    void*               pArray = pImpl->findEditableProperty(entity.value, type, property, pProperty);
    if (!pArray || !validateArray(*pProperty))
        return false;
    if (count == pProperty->pArrayOperations->pCount(pArray))
        return true;
    if (!pImpl->pHierarchy->prepareLocalChange(entity.value, type, true))
        return false;
    pProperty->pArrayOperations->pResize(pArray, count);
    pImpl->notifyPropertyChange(entity.value, type);
    return true;
}

bool World::insertArrayElement(Entity entity, TypeID type, PropertyID property, uint32_t index, const void* pValue)
{
    ASSERT(pValue);
    const PropertyDesc* pProperty = nullptr;
    void*               pArray = pImpl->findEditableProperty(entity.value, type, property, pProperty);
    if (!pArray || !validateArray(*pProperty))
        return false;
    if (index > pProperty->pArrayOperations->pCount(pArray))
    {
        LOGF(eERROR, "Property '%s': insertion index is out of range", pProperty->pName);
        return false;
    }
    for (const PropertyDesc& field : pProperty->pElementType->properties)
        if (!validatePropertyValue(field, { .kind = field.kind, .pData = (const uint8_t*)pValue + field.offset, .size = field.size }))
            return false;
    if (!pImpl->pHierarchy->prepareLocalChange(entity.value, type, true))
        return false;
    if (!pProperty->pArrayOperations->pInsert(pArray, index, pValue))
        return false;
    pImpl->notifyPropertyChange(entity.value, type);
    return true;
}

bool World::removeArrayElement(Entity entity, TypeID type, PropertyID property, uint32_t index)
{
    const PropertyDesc* pProperty = nullptr;
    void*               pArray = pImpl->findEditableProperty(entity.value, type, property, pProperty);
    if (!pArray || !validateArray(*pProperty))
        return false;
    if (index >= pProperty->pArrayOperations->pCount(pArray))
    {
        LOGF(eERROR, "Property '%s': removal index is out of range", pProperty->pName);
        return false;
    }
    if (!pImpl->pHierarchy->prepareLocalChange(entity.value, type, true))
        return false;
    if (!pProperty->pArrayOperations->pRemove(pArray, index))
        return false;
    pImpl->notifyPropertyChange(entity.value, type);
    return true;
}

bool World::setArrayElementProperty(Entity entity, TypeID type, PropertyID property, uint32_t index, PropertyID elementProperty,
                                    const PropertyValue& value)
{
    const PropertyDesc* pProperty = nullptr;
    void*               pArray = pImpl->findEditableProperty(entity.value, type, property, pProperty);
    if (!pArray || !validateArray(*pProperty))
        return false;
    if (index >= pProperty->pArrayOperations->pCount(pArray))
    {
        LOGF(eERROR, "Property '%s': element index is out of range", pProperty->pName);
        return false;
    }
    const PropertyDesc* pField = pProperty->pElementType->getProperty(elementProperty);
    if (!pField || (pField->flags & PROPERTY_READ_ONLY))
    {
        LOGF(eERROR, "Array edit references an unknown or read-only element property");
        return false;
    }
    if (!validatePropertyValue(*pField, value))
        return false;
    if (!pImpl->pHierarchy->prepareLocalChange(entity.value, type, true))
        return false;
    void* pData = const_cast<void*>(pProperty->pArrayOperations->pData(pArray));
    writeProperty((uint8_t*)pData + (size_t)index * pProperty->pElementType->size + pField->offset, *pField, value);
    pImpl->notifyPropertyChange(entity.value, type);
    return true;
}
} // namespace hz
