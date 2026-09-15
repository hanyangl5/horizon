/* Copyright (c) 2026 Horizon */

#include "Scene/SceneID.h"

#include "Core/ILog.h"
#include "Core/IRandom.h"

#include <string.h>

namespace hz
{
static uint64_t readIDWord(const uint8_t* pBytes)
{
    uint64_t word = 0;
    for (uint32_t i = 0; i < 8; ++i)
        word = (word << 8) | pBytes[i];
    return word;
}

static bool generateID(const IDGenerator& generator, uint64_t& high, uint64_t& low)
{
    uint8_t    bytes[16] = {};
    const bool generated =
        generator.pGenerate ? generator.pGenerate(generator.pUserData, bytes) : getSystemRandomBytes(bytes, sizeof(bytes));
    if (!generated)
    {
        LOGF(eERROR, "Failed to generate a persistent scene ID");
        return false;
    }

    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    high = readIDWord(bytes);
    low = readIDWord(bytes + 8);
    return true;
}

static int hexDigit(char value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

static bool parseID(const char* pText, uint64_t& high, uint64_t& low)
{
    if (!pText || strlen(pText) != kIDStringCapacity - 1)
        return false;

    uint8_t  bytes[16] = {};
    uint32_t offset = 0;
    for (uint32_t i = 0; i < 16; ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
        {
            if (pText[offset++] != '-')
                return false;
        }
        const int upper = hexDigit(pText[offset++]);
        const int lower = hexDigit(pText[offset++]);
        if (upper < 0 || lower < 0)
            return false;
        bytes[i] = (uint8_t)((upper << 4) | lower);
    }
    high = readIDWord(bytes);
    low = readIDWord(bytes + 8);
    return true;
}

static void formatIDWords(uint64_t high, uint64_t low, char (&text)[kIDStringCapacity])
{
    const char digits[] = "0123456789abcdef";
    uint32_t   offset = 0;
    for (uint32_t i = 0; i < 16; ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            text[offset++] = '-';
        const uint8_t byte = (uint8_t)((i < 8 ? high : low) >> ((7 - i % 8) * 8));
        text[offset++] = digits[byte >> 4];
        text[offset++] = digits[byte & 0x0f];
    }
    text[offset] = '\0';
}

static uint64_t hashIDWords(uint64_t high, uint64_t low)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    for (uint32_t i = 0; i < 16; ++i)
    {
        hash ^= (uint8_t)((i < 8 ? high : low) >> ((7 - i % 8) * 8));
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

AssetID AssetID::create(const IDGenerator& generator)
{
    AssetID id;
    generateID(generator, id.high, id.low);
    return id;
}

ObjectID ObjectID::create(const IDGenerator& generator)
{
    ObjectID id;
    generateID(generator, id.high, id.low);
    return id;
}

bool     AssetID::parse(const char* pText) { return parseID(pText, high, low); }
bool     ObjectID::parse(const char* pText) { return parseID(pText, high, low); }
void     AssetID::toString(char (&text)[kIDStringCapacity]) const { formatIDWords(high, low, text); }
void     ObjectID::toString(char (&text)[kIDStringCapacity]) const { formatIDWords(high, low, text); }
uint64_t AssetID::hash() const { return hashIDWords(high, low); }
uint64_t ObjectID::hash() const { return hashIDWords(high, low); }
} // namespace hz
