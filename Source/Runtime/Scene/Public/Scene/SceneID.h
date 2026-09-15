/* Copyright (c) 2026 Horizon */
#pragma once

#include <stdint.h>

namespace hz
{
constexpr uint32_t kIDStringCapacity = 37;

struct IDGenerator
{
    // Optional source for deterministic tools/tests. Bytes use canonical UUID order.
    bool (*pGenerate)(void* pUserData, uint8_t (&bytes)[16]) = nullptr;
    void* pUserData = nullptr;
};

// high/low follow the canonical UUID text order, independently of host byte order.
struct AssetID
{
    uint64_t high = 0;
    uint64_t low = 0;

    // Uses the system random source by default. Failure logs an error and returns an invalid ID.
    static AssetID create(const IDGenerator& generator = {});

    // Accepts exactly 36 characters in UUID form, including nil; failure leaves this ID unchanged.
    bool     parse(const char* pText);
    void     toString(char (&text)[kIDStringCapacity]) const;
    // FNV-1a over the 16 canonical bytes; compare the full ID when hashes match.
    uint64_t hash() const;

    constexpr bool isValid() const { return high != 0 || low != 0; }
    constexpr bool operator==(const AssetID&) const = default;
    constexpr bool operator<(const AssetID& other) const { return high < other.high || (high == other.high && low < other.low); }
};

struct ObjectID
{
    uint64_t high = 0;
    uint64_t low = 0;

    // Generation and text conversion follow the same contract as AssetID.
    static ObjectID create(const IDGenerator& generator = {});
    bool            parse(const char* pText);
    void            toString(char (&text)[kIDStringCapacity]) const;
    uint64_t        hash() const;

    constexpr bool isValid() const { return high != 0 || low != 0; }
    constexpr bool operator==(const ObjectID&) const = default;
    constexpr bool operator<(const ObjectID& other) const { return high < other.high || (high == other.high && low < other.low); }
};

} // namespace hz
