#include <gtest/gtest.h>

#include "Scene/SceneID.h"

#include <string.h>
#include <type_traits>

static_assert(sizeof(hz::AssetID) == 16);
static_assert(sizeof(hz::Guid) == 16);
static_assert(std::is_trivially_copyable_v<hz::AssetID>);
static_assert(std::is_trivially_copyable_v<hz::Guid>);
static_assert(!std::is_convertible_v<hz::AssetID, hz::Guid>);
static_assert(!std::is_convertible_v<hz::Guid, hz::AssetID>);

TEST(SceneIDTest, NilAndValueComparison)
{
    EXPECT_FALSE(hz::AssetID{}.IsValid());
    EXPECT_FALSE(hz::Guid{}.IsValid());
    const hz::AssetID first = { .high = 1, .low = 2 };
    const hz::AssetID same = { .high = 1, .low = 2 };
    const hz::AssetID nextLow = { .high = 1, .low = 3 };
    const hz::AssetID nextHigh = { .high = 2 };
    EXPECT_TRUE(first.IsValid());
    EXPECT_EQ(first, same);
    EXPECT_NE(first, nextLow);
    EXPECT_LT(first, nextLow);
    EXPECT_LT(nextLow, nextHigh);
    EXPECT_FALSE(first < same);
    EXPECT_LT((hz::Guid{ .low = UINT64_MAX }), (hz::Guid{ .high = 1 }));
}

TEST(SceneIDTest, ParsesCanonicalTextAndFormatsLowercase)
{
    hz::AssetID asset;
    hz::Guid    object;
    ASSERT_TRUE(asset.Parse("00112233-4455-4677-8899-aabbccddeeff"));
    ASSERT_TRUE(object.Parse("00112233-4455-4677-8899-aabbccddeeff"));
    EXPECT_EQ(asset.high, UINT64_C(0x0011223344554677));
    EXPECT_EQ(asset.low, UINT64_C(0x8899aabbccddeeff));
    EXPECT_EQ(object.high, asset.high);
    EXPECT_EQ(object.low, asset.low);
    char text[hz::kIDStringCapacity];
    asset.ToString(text);
    EXPECT_STREQ(text, "00112233-4455-4677-8899-aabbccddeeff");
    object.ToString(text);
    EXPECT_STREQ(text, "00112233-4455-4677-8899-aabbccddeeff");
}

TEST(SceneIDTest, PreservesNilAndAllBits)
{
    const char* values[] = { "00000000-0000-0000-0000-000000000000", "ffffffff-ffff-ffff-ffff-ffffffffffff" };
    for (const char* value : values)
    {
        hz::AssetID asset;
        hz::Guid    object;
        ASSERT_TRUE(asset.Parse(value));
        ASSERT_TRUE(object.Parse(value));
        char text[hz::kIDStringCapacity];
        asset.ToString(text);
        EXPECT_STREQ(text, value);
        object.ToString(text);
        EXPECT_STREQ(text, value);
    }
}

TEST(SceneIDTest, RejectsMalformedInputWithoutChangingOutput)
{
    const char* invalid[] = {
        nullptr,
        "",
        "00112233445546778899aabbccddeeff",
        "{00112233-4455-4677-8899-aabbccddeeff}",
        "00112233-4455-4677-8899-aabbccddeef",
        "00112233-4455-4677-8899-aabbccddeeff0",
        "00112233_4455-4677-8899-aabbccddeeff",
        "00112233-4455_4677-8899-aabbccddeeff",
        "00112233-4455-4677_8899-aabbccddeeff",
        "00112233-4455-4677-8899_aabbccddeeff",
        "00112233-4455-4677-8899-aabbccddeefg",
        " 0112233-4455-4677-8899-aabbccddeeff",
    };
    const hz::AssetID originalAsset = { .high = 12, .low = 34 };
    const hz::Guid    originalObject = { .high = 56, .low = 78 };
    for (const char* value : invalid)
    {
        hz::AssetID asset = originalAsset;
        hz::Guid    object = originalObject;
        EXPECT_FALSE(asset.Parse(value));
        EXPECT_FALSE(object.Parse(value));
        EXPECT_EQ(asset, originalAsset);
        EXPECT_EQ(object, originalObject);
    }
}

static bool generateSequence(void* UserData, uint8_t (&bytes)[16])
{
    uint8_t* pNext = (uint8_t*)UserData;
    for (uint32_t i = 0; i < 16; ++i)
        bytes[i] = (*pNext)++;
    return true;
}

static bool failGeneration(void*, uint8_t (&bytes)[16])
{
    memset(bytes, 0xff, sizeof(bytes));
    return false;
}

TEST(SceneIDTest, InjectedSequenceIsDeterministicAndSetsUuidBits)
{
    uint8_t               next = 0;
    const hz::IDGenerator generator = { .pGenerate = generateSequence, .UserData = &next };
    const hz::AssetID     asset = hz::AssetID::Create(generator);
    const hz::Guid        object = hz::Guid::Create(generator);
    char                  text[hz::kIDStringCapacity];
    asset.ToString(text);
    EXPECT_STREQ(text, "00010203-0405-4607-8809-0a0b0c0d0e0f");
    object.ToString(text);
    EXPECT_STREQ(text, "10111213-1415-4617-9819-1a1b1c1d1e1f");
    next = 0;
    EXPECT_EQ(hz::AssetID::Create(generator), asset);
    EXPECT_EQ(hz::Guid::Create(generator), object);
    EXPECT_FALSE(hz::AssetID::Create({ .pGenerate = failGeneration }).IsValid());
    EXPECT_FALSE(hz::Guid::Create({ .pGenerate = failGeneration }).IsValid());
}

TEST(SceneIDTest, SystemGeneratorProducesValidDistinctIds)
{
    hz::AssetID assets[32];
    for (uint32_t i = 0; i < 32; ++i)
    {
        assets[i] = hz::AssetID::Create();
        ASSERT_TRUE(assets[i].IsValid());
        EXPECT_EQ((assets[i].high >> 12) & 0xf, 4u);
        EXPECT_EQ(assets[i].low >> 62, 2u);
        for (uint32_t j = 0; j < i; ++j)
            EXPECT_NE(assets[i], assets[j]);
    }
    const hz::Guid object = hz::Guid::Create();
    ASSERT_TRUE(object.IsValid());
    EXPECT_EQ((object.high >> 12) & 0xf, 4u);
    EXPECT_EQ(object.low >> 62, 2u);
}

TEST(SceneIDTest, HashUsesCanonicalBytes)
{
    hz::AssetID lowercase;
    hz::AssetID uppercase;
    hz::Guid    object;
    ASSERT_TRUE(lowercase.Parse("00112233-4455-4677-8899-aabbccddeeff"));
    ASSERT_TRUE(uppercase.Parse("00112233-4455-4677-8899-aabbccddeeff"));
    ASSERT_TRUE(object.Parse("00112233-4455-4677-8899-aabbccddeeff"));
    EXPECT_EQ(lowercase.Hash(), uppercase.Hash());
    EXPECT_EQ(lowercase.Hash(), object.Hash());
    EXPECT_EQ(lowercase.Hash(), UINT64_C(0xe5401c29d52a2355));
    EXPECT_NE(lowercase.Hash(), hz::AssetID{}.Hash());
    EXPECT_NE(lowercase.Hash(), (hz::AssetID{ .high = lowercase.high, .low = lowercase.low ^ 1 }).Hash());
}
