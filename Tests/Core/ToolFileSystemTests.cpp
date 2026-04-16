#include <gtest/gtest.h>

#include <string.h>

#include "Core/IToolFileSystem.h"

namespace
{
void expectNormalizedPath(const char* input, const char* expected, char separator = '/')
{
    char buffer[FS_MAX_PATH] = {};
    const size_t length = fsNormalizePath(input, separator, buffer);

    EXPECT_EQ(length, strlen(expected));
    EXPECT_STREQ(buffer, expected);
    EXPECT_TRUE(fsIsNormalizedPath(buffer, separator));
}
} // namespace

TEST(CoreToolFileSystemTest, NormalizePathResolvesCurrentParentAndMixedSeparators)
{
    EXPECT_FALSE(fsIsNormalizedPath("Assets\\Meshes\\.\\Characters\\..\\Hero//body.mesh", '/'));
    expectNormalizedPath("Assets\\Meshes\\.\\Characters\\..\\Hero//body.mesh", "Assets/Meshes/Hero/body.mesh");
}

TEST(CoreToolFileSystemTest, NormalizePathPreservesLeadingUnresolvableParents)
{
    expectNormalizedPath("../../Shaders/../Common/lighting.hlsl", "../../Common/lighting.hlsl");
    expectNormalizedPath("a/..", ".");
}

TEST(CoreToolFileSystemTest, NormalizePathHandlesWindowsDriveLetters)
{
    expectNormalizedPath("C:\\Project\\Assets\\..\\Shaders\\.\\main.hlsl", "C:/Project/Shaders/main.hlsl");
}

TEST(CoreToolFileSystemTest, MergeDirAndFileNameNormalizesCombinedPath)
{
    char output[FS_MAX_PATH] = {};

    ASSERT_TRUE(fsMergeDirAndFileName("Assets/Models", "../Textures/./hero_albedo.dds", '/', FS_MAX_PATH, output));
    EXPECT_STREQ(output, "Assets/Textures/hero_albedo.dds");
}

TEST(CoreToolFileSystemTest, PathExtensionAndComponentHelpersSplitAndAppendConsistently)
{
    char appended[FS_MAX_PATH] = {};
    char merged[FS_MAX_PATH] = {};
    char replaced[FS_MAX_PATH] = {};
    char parent[FS_MAX_PATH] = {};
    char fileName[FS_MAX_PATH] = {};
    char extension[FS_MAX_PATH] = {};
    char noExtension[FS_MAX_PATH] = {};

    ASSERT_TRUE(fsAppendPathComponent("foo/bar", "baz.txt", merged));
    fsAppendPathExtension("foo/bar.txt", "bak", appended);
    fsReplacePathExtension("foo/bar.txt", ".bin", replaced);
    fsGetParentPath("foo/bar/baz.txt", parent);
    fsGetPathFileName("foo/bar/baz.txt", fileName);
    fsGetPathExtension("foo/bar/baz.txt", extension);
    fsGetPathExtension("foo/bar/baz", noExtension);

    EXPECT_STREQ(merged, "foo/bar/baz.txt");
    EXPECT_STREQ(appended, "foo/bar.txt.bak");
    EXPECT_STREQ(replaced, "foo/bar.bin");
    EXPECT_STREQ(parent, "foo/bar");
    EXPECT_STREQ(fileName, "baz");
    EXPECT_STREQ(extension, "txt");
    EXPECT_STREQ(noExtension, "");
}
