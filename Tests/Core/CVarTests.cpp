#include <gtest/gtest.h>

#include "Core/ICVar.h"
#include <limits>

TEST(CoreCVarTest, NamedAndDirectAccessShareTheRegisteredValue)
{
    {
        hz::CVar Exposure("test.Exposure", 1.0f, 0.0f, "Exposure");
        hz::CVar* Entry = hz::GetCVar("TEST.exposure");
        ASSERT_EQ(Entry, &Exposure);
        EXPECT_TRUE(Entry->Set(2.5f));
        EXPECT_EQ(Exposure.GetFloat(), 2.5f);
        EXPECT_FALSE(Entry->Set(true));
        EXPECT_FALSE(Exposure.Set(-1.0f));
        EXPECT_FALSE(Exposure.Set(std::numeric_limits<float>::infinity()));
        EXPECT_EQ(Entry->GetFloat(), 2.5f);
        Exposure.SetAvailable(false);
        EXPECT_EQ(hz::GetCVar("test.Exposure"), nullptr);
        EXPECT_FALSE(Entry->Set(3.0f));
        Exposure.SetAvailable(true);
        EXPECT_EQ(hz::GetCVar("test.Exposure"), &Exposure);
    }
    EXPECT_EQ(hz::GetCVar("test.Exposure"), nullptr);
}

TEST(CoreCVarTest, RejectedDeviceChangePreservesValue)
{
    hz::CVar Sync("test.Sync", true, "VSync");
    Sync.SetApplyBool([](void*, bool) { return false; }, nullptr);
    EXPECT_FALSE(Sync.Set(false));
    EXPECT_TRUE(Sync.GetBool());
    Sync.SetApplyBool(nullptr, nullptr);
    EXPECT_TRUE(Sync.Set(false));
    EXPECT_FALSE(Sync.GetBool());
}
