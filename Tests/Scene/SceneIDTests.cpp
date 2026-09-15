#include <gtest/gtest.h>

#include "Scene/SceneID.h"

#include <string.h>
#include <type_traits>

static_assert(sizeof(hz::AssetID) == 16);
static_assert(sizeof(hz::ObjectID) == 16);
static_assert(std::is_trivially_copyable_v<hz::AssetID>);
static_assert(std::is_trivially_copyable_v<hz::ObjectID>);
static_assert(!std::is_convertible_v<hz::AssetID, hz::ObjectID>);
static_assert(!std::is_convertible_v<hz::ObjectID, hz::AssetID>);

TEST(SceneIDTest, NilAndValueComparison)
{
    EXPECT_FALSE(hz::AssetID{}.isValid());
    EXPECT_FALSE(hz::ObjectID{}.isValid());
    const hz::AssetID first = { .high = 1, .low = 2 };
    const hz::AssetID same = { .high = 1, .low = 2 };
    const hz::AssetID nextLow = { .high = 1, .low = 3 };
    const hz::AssetID nextHigh = { .high = 2 };
    EXPECT_TRUE(first.isValid());
    EXPECT_EQ(first, same);
    EXPECT_NE(first, nextLow);
    EXPECT_LT(first, nextLow);
    EXPECT_LT(nextLow, nextHigh);
    EXPECT_FALSE(first < same);
    EXPECT_LT((hz::ObjectID{ .low = UINT64_MAX }), (hz::ObjectID{ .high = 1 }));
}

TEST(SceneIDTest, ParsesCanonicalTextAndFormatsLowercase)
{
    hz::AssetID  asset;
    hz::ObjectID object;
    ASSERT_TRUE(asset.parse("00112233-4455-4677-8899-AABBCCDDEEFF"));
    ASSERT_TRUE(object.parse("00112233-4455-4677-8899-aabbccddeeff"));
    EXPECT_EQ(asset.high, UINT64_C(0x0011223344554677));
    EXPECT_EQ(asset.low, UINT64_C(0x8899aabbccddeeff));
    EXPECT_EQ(object.high, asset.high);
    EXPECT_EQ(object.low, asset.low);
    char text[hz::kIDStringCapacity];
    asset.toString(text);
    EXPECT_STREQ(text, "00112233-4455-4677-8899-aabbccddeeff");
    object.toString(text);
    EXPECT_STREQ(text, "00112233-4455-4677-8899-aabbccddeeff");
}

TEST(SceneIDTest, PreservesNilAndAllBits)
{
    const char* values[] = { "00000000-0000-0000-0000-000000000000", "ffffffff-ffff-ffff-ffff-ffffffffffff" };
    for (const char* value : values)
    {
        hz::AssetID  asset;
        hz::ObjectID object;
        ASSERT_TRUE(asset.parse(value));
        ASSERT_TRUE(object.parse(value));
        char text[hz::kIDStringCapacity];
        asset.toString(text);
        EXPECT_STREQ(text, value);
        object.toString(text);
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
    const hz::AssetID  originalAsset = { .high = 12, .low = 34 };
    const hz::ObjectID originalObject = { .high = 56, .low = 78 };
    for (const char* value : invalid)
    {
        hz::AssetID  asset = originalAsset;
        hz::ObjectID object = originalObject;
        EXPECT_FALSE(asset.parse(value));
        EXPECT_FALSE(object.parse(value));
        EXPECT_EQ(asset, originalAsset);
        EXPECT_EQ(object, originalObject);
    }
}

static bool generateSequence(void* pUserData, uint8_t (&bytes)[16])
{
    uint8_t* pNext = (uint8_t*)pUserData;
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
    const hz::IDGenerator generator = { .pGenerate = generateSequence, .pUserData = &next };
    const hz::AssetID     asset = hz::AssetID::create(generator);
    const hz::ObjectID    object = hz::ObjectID::create(generator);
    char                  text[hz::kIDStringCapacity];
    asset.toString(text);
    EXPECT_STREQ(text, "00010203-0405-4607-8809-0a0b0c0d0e0f");
    object.toString(text);
    EXPECT_STREQ(text, "10111213-1415-4617-9819-1a1b1c1d1e1f");
    next = 0;
    EXPECT_EQ(hz::AssetID::create(generator), asset);
    EXPECT_EQ(hz::ObjectID::create(generator), object);
    EXPECT_FALSE(hz::AssetID::create({ .pGenerate = failGeneration }).isValid());
    EXPECT_FALSE(hz::ObjectID::create({ .pGenerate = failGeneration }).isValid());
}

TEST(SceneIDTest, SystemGeneratorProducesValidDistinctIds)
{
    hz::AssetID assets[32];
    for (uint32_t i = 0; i < 32; ++i)
    {
        assets[i] = hz::AssetID::create();
        ASSERT_TRUE(assets[i].isValid());
        EXPECT_EQ((assets[i].high >> 12) & 0xf, 4u);
        EXPECT_EQ(assets[i].low >> 62, 2u);
        for (uint32_t j = 0; j < i; ++j)
            EXPECT_NE(assets[i], assets[j]);
    }
    const hz::ObjectID object = hz::ObjectID::create();
    ASSERT_TRUE(object.isValid());
    EXPECT_EQ((object.high >> 12) & 0xf, 4u);
    EXPECT_EQ(object.low >> 62, 2u);
}

TEST(SceneIDTest, HashUsesCanonicalBytes)
{
    hz::AssetID  lowercase;
    hz::AssetID  uppercase;
    hz::ObjectID object;
    ASSERT_TRUE(lowercase.parse("00112233-4455-4677-8899-aabbccddeeff"));
    ASSERT_TRUE(uppercase.parse("00112233-4455-4677-8899-AABBCCDDEEFF"));
    ASSERT_TRUE(object.parse("00112233-4455-4677-8899-aabbccddeeff"));
    EXPECT_EQ(lowercase.hash(), uppercase.hash());
    EXPECT_EQ(lowercase.hash(), object.hash());
    EXPECT_EQ(lowercase.hash(), UINT64_C(0xe5401c29d52a2355));
    EXPECT_NE(lowercase.hash(), hz::AssetID{}.hash());
    EXPECT_NE(lowercase.hash(), (hz::AssetID{ .high = lowercase.high, .low = lowercase.low ^ 1 }).hash());
}
