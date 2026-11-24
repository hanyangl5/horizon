/* Copyright (c) 2026 Horizon */
#pragma once

#include <stdint.h>

namespace hz
{
constexpr uint32_t kIDStringCapacity = 37;

struct IDGenerator
{
    // Optional source for deterministic tools/tests. Bytes use canonical UUID order.
    bool (*pGenerate)(void* UserData, uint8_t (&bytes)[16]) = nullptr;
    void* UserData = nullptr;
};

// high/low follow the canonical UUID text order, independently of host byte order.
struct AssetID
{
    uint64_t high = 0;
    uint64_t low = 0;

    // Uses the system random source by default. Failure logs an error and returns an invalid ID.
    static AssetID Create(const IDGenerator& generator = {});

    // Accepts exactly 36 characters in UUID form, including nil; failure leaves this ID unchanged.
    bool     Parse(const char* pText);
    void     ToString(char (&text)[kIDStringCapacity]) const;
    // FNV-1a over the 16 canonical bytes; compare the full ID when hashes match.
    uint64_t Hash() const;

    constexpr bool IsValid() const { return high != 0 || low != 0; }
    constexpr bool operator==(const AssetID&) const = default;
    constexpr bool operator<(const AssetID& other) const { return high < other.high || (high == other.high && low < other.low); }
};

struct Guid
{
    uint64_t high = 0;
    uint64_t low = 0;

    // Generation and text conversion follow the same contract as AssetID.
    static Guid Create(const IDGenerator& generator = {});
    bool        Parse(const char* pText);
    void        ToString(char (&text)[kIDStringCapacity]) const;
    uint64_t    Hash() const;

    constexpr bool IsValid() const { return high != 0 || low != 0; }
    constexpr bool operator==(const Guid&) const = default;
    constexpr bool operator<(const Guid& other) const { return high < other.high || (high == other.high && low < other.low); }
};

} // namespace hz
