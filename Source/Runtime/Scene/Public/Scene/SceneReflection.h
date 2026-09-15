/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/ISpan.h"

namespace hz
{
using TypeID = uint64_t;
using PropertyID = uint32_t;

enum class PropertyKind : uint8_t
{
    Boolean,
    SignedInteger,
    UnsignedInteger,
    Float,
    Vector2,
    Vector3,
    Vector4,
    Quaternion,
    Color,
    Matrix4,
    String,
    Enum,
    AssetReference,
    ObjectReference,
    Array,
};

enum PropertyFlags : uint32_t
{
    PROPERTY_SERIALIZED = 1 << 0,
    PROPERTY_TRANSIENT = 1 << 1,
    PROPERTY_READ_ONLY = 1 << 2,
    PROPERTY_EDITOR_ONLY = 1 << 3,
};

struct TypeDesc;

struct EnumValue
{
    const char* pName = nullptr;
    int32_t     value = 0;
};

struct ArrayOperations
{
    uint32_t size = 0;
    uint32_t alignment = 1;
    uint32_t (*pCount)(const void* pArray) = nullptr;
    const void* (*pData)(const void* pArray) = nullptr;
    void (*pResize)(void* pArray, uint32_t count) = nullptr;
    bool (*pInsert)(void* pArray, uint32_t index, const void* pValue) = nullptr;
    bool (*pRemove)(void* pArray, uint32_t index) = nullptr;
};

struct PropertyDesc
{
    PropertyID             id = 0;
    const char*            pName = nullptr;
    PropertyKind           kind = PropertyKind::Boolean;
    uint32_t               offset = 0;
    uint32_t               size = 0;
    uint32_t               flags = PROPERTY_SERIALIZED;
    const TypeDesc*        pElementType = nullptr;
    const ArrayOperations* pArrayOperations = nullptr;
    Span<EnumValue>        enumValues;
    double                 minimum = 0.0;
    double                 maximum = 0.0;
    double                 step = 0.0;
};

// Copy/move construct into uninitialized storage. A moved-from source remains destructible.
struct TypeLifecycle
{
    void (*pConstruct)(void* pDestination) = nullptr;
    void (*pDestroy)(void* pValue) = nullptr;
    void (*pCopyConstruct)(void* pDestination, const void* pSource) = nullptr;
    void (*pMoveConstruct)(void* pDestination, void* pSource) = nullptr;
};

struct TypeDesc
{
    TypeID             id = 0;
    const char*        pName = nullptr;
    uint32_t           version = 1;
    uint32_t           size = 0;
    uint32_t           alignment = 1;
    Span<PropertyDesc> properties;
    bool               triviallyCopyable = true;
    TypeLifecycle      lifecycle;

    const PropertyDesc* getProperty(PropertyID property) const;
    const PropertyDesc* getProperty(const char* pPropertyName) const;
    void                construct(void* pDestination) const;
    void                destroy(void* pValue) const;
    void                copyConstruct(void* pDestination, const void* pSource) const;
    void                moveConstruct(void* pDestination, void* pSource) const;
};

class TypeRegistry
{
public:
    TypeRegistry() = default;
    ~TypeRegistry();
    TypeRegistry(const TypeRegistry&) = delete;
    TypeRegistry& operator=(const TypeRegistry&) = delete;

    // Descriptors and their referenced data must outlive this registry. Registration is atomic per batch.
    bool            registerTypes(Span<TypeDesc> types);
    bool            registerBuiltins();
    const TypeDesc* getType(TypeID id) const;
    const TypeDesc* getType(const char* pName) const;
    const TypeDesc* getTypeAt(uint32_t index) const;
    uint32_t        size() const;

private:
    const TypeDesc** ppTypes = nullptr;
};
} // namespace hz
