#include <gtest/gtest.h>

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"
#include "TextureContainers.h"

TEST(TextureContainersTest, DecodesSingleTexelBC5EndpointsAndInterpolation)
{
    uint8_t block[8] = { 127, 127, 0 };
    EXPECT_EQ(decodeBC5SingleTexelChannel(block), 127u);

    block[0] = 255;
    block[1] = 0;
    const uint8_t eightValues[] = { 255, 0, 218, 182, 145, 109, 72, 36 };
    for (uint8_t i = 0; i < 8; ++i)
    {
        block[2] = i;
        EXPECT_EQ(decodeBC5SingleTexelChannel(block), eightValues[i]);
    }
    block[0] = 0;
    block[1] = 255;
    const uint8_t sixValues[] = { 0, 255, 51, 102, 153, 204, 0, 255 };
    for (uint8_t i = 0; i < 8; ++i)
    {
        block[2] = (uint8_t)(i | 0xf8); // Other texels' selector bits must not affect the result.
        EXPECT_EQ(decodeBC5SingleTexelChannel(block), sixValues[i]);
    }
}
