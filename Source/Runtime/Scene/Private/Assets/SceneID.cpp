/* Copyright (c) 2026 Horizon */

#include "Scene/SceneID.h"

#include "Core/ILog.h"
#include "Core/IRandom.h"

#include <string.h>

namespace hz
{
static uint64_t ReadIDWord(const uint8_t* BytesPointer)
{
    uint64_t Word = 0;
    for (uint32_t I = 0; I < 8; ++I)
    {
        Word = (Word << 8) | BytesPointer[I];
    }
    return Word;
}

static bool GenerateID(const IDGenerator& Generator, uint64_t& High, uint64_t& Low)
{
    uint8_t    BytesValue[16] = {};
    const bool Generated =
        Generator.pGenerate ? Generator.pGenerate(Generator.UserData, BytesValue) : GetSystemRandomBytes(BytesValue, sizeof(BytesValue));
    if (!Generated)
    {
        LOGF(Error, "Failed to generate a persistent scene ID");
        return false;
    }

    BytesValue[6] = (BytesValue[6] & 0x0f) | 0x40;
    BytesValue[8] = (BytesValue[8] & 0x3f) | 0x80;
    High = ReadIDWord(BytesValue);
    Low = ReadIDWord(BytesValue + 8);
    return true;
}

static int HexDigit(char Value)
{
    if (Value >= '0' && Value <= '9')
    {
        return Value - '0';
    }
    if (Value >= 'a' && Value <= 'f')
    {
        return Value - 'a' + 10;
    }
    if (Value >= 'A' && Value <= 'F')
    {
        return Value - 'A' + 10;
    }
    return -1;
}

static bool ParseID(const char* TextPointer, uint64_t& High, uint64_t& Low)
{
    if (!TextPointer || strlen(TextPointer) != kIDStringCapacity - 1)
    {
        return false;
    }

    uint8_t  BytesValue[16] = {};
    uint32_t Offset = 0;
    for (uint32_t I = 0; I < 16; ++I)
    {
        if (I == 4 || I == 6 || I == 8 || I == 10)
        {
            if (TextPointer[Offset++] != '-')
            {
                return false;
            }
        }
        const int Upper = HexDigit(TextPointer[Offset++]);
        const int Lower = HexDigit(TextPointer[Offset++]);
        if (Upper < 0 || Lower < 0)
        {
            return false;
        }
        BytesValue[I] = (uint8_t)((Upper << 4) | Lower);
    }
    High = ReadIDWord(BytesValue);
    Low = ReadIDWord(BytesValue + 8);
    return true;
}

static void FormatIDWords(uint64_t High, uint64_t Low, char (&TextValue)[kIDStringCapacity])
{
    const char Digits[] = "0123456789abcdef";
    uint32_t   Offset = 0;
    for (uint32_t I = 0; I < 16; ++I)
    {
        if (I == 4 || I == 6 || I == 8 || I == 10)
        {
            TextValue[Offset++] = '-';
        }
        const uint8_t Byte = (uint8_t)((I < 8 ? High : Low) >> ((7 - I % 8) * 8));
        TextValue[Offset++] = Digits[Byte >> 4];
        TextValue[Offset++] = Digits[Byte & 0x0f];
    }
    TextValue[Offset] = '\0';
}

static uint64_t HashIDWords(uint64_t High, uint64_t Low)
{
    uint64_t Hash = UINT64_C(14695981039346656037);
    for (uint32_t I = 0; I < 16; ++I)
    {
        Hash ^= (uint8_t)((I < 8 ? High : Low) >> ((7 - I % 8) * 8));
        Hash *= UINT64_C(1099511628211);
    }
    return Hash;
}

AssetID AssetID::Create(const IDGenerator& Generator)
{
    AssetID Id;
    GenerateID(Generator, Id.high, Id.low);
    return Id;
}

Guid Guid::Create(const IDGenerator& Generator)
{
    Guid Id;
    GenerateID(Generator, Id.high, Id.low);
    return Id;
}

bool     AssetID::Parse(const char* TextPointer) { return ParseID(TextPointer, high, low); }
bool     Guid::Parse(const char* TextPointer) { return ParseID(TextPointer, high, low); }
void     AssetID::ToString(char (&TextValue)[kIDStringCapacity]) const { FormatIDWords(high, low, TextValue); }
void     Guid::ToString(char (&TextValue)[kIDStringCapacity]) const { FormatIDWords(high, low, TextValue); }
uint64_t AssetID::Hash() const { return HashIDWords(high, low); }
uint64_t Guid::Hash() const { return HashIDWords(high, low); }
} // namespace hz
