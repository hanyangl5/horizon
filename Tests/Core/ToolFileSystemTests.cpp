#include <gtest/gtest.h>

#include <string.h>

#include "Core/IToolFileSystem.h"

namespace
{
void expectNormalizedPath(const char* input, const char* expected, char separator = '/')
{
    char buffer[FS_MAX_PATH] = {};
    const size_t length = NormalizePath(input, separator, buffer);

    EXPECT_EQ(length, strlen(expected));
    EXPECT_STREQ(buffer, expected);
    EXPECT_TRUE(IsNormalizedPath(buffer, separator));
}
} // namespace

// Verifies that path normalization resolves dot segments and mixed separators into a canonical path.
TEST(CoreToolFileSystemTest, NormalizePathResolvesCurrentParentAndMixedSeparators)
{
    EXPECT_FALSE(IsNormalizedPath("Assets\\Meshes\\.\\Characters\\..\\Hero//body.mesh", '/'));
    expectNormalizedPath("Assets\\Meshes\\.\\Characters\\..\\Hero//body.mesh", "Assets/Meshes/Hero/body.mesh");
}

// Verifies that normalization preserves leading parent traversals that cannot be collapsed further.
TEST(CoreToolFileSystemTest, NormalizePathPreservesLeadingUnresolvableParents)
{
    expectNormalizedPath("../../Shaders/../Common/lighting.hlsl", "../../Common/lighting.hlsl");
    expectNormalizedPath("a/..", ".");
}

// Verifies that normalization keeps Windows drive prefixes while cleaning the remaining path segments.
TEST(CoreToolFileSystemTest, NormalizePathHandlesWindowsDriveLetters)
{
    expectNormalizedPath("C:\\Project\\Assets\\..\\Shaders\\.\\main.hlsl", "C:/Project/Shaders/main.hlsl");
}

// Verifies that merging a directory and file name also normalizes relative path segments in the result.
TEST(CoreToolFileSystemTest, MergeDirAndFileNameNormalizesCombinedPath)
{
    char output[FS_MAX_PATH] = {};

    ASSERT_TRUE(MergeDirAndFileName("Assets/Models", "../Textures/./hero_albedo.dds", '/', FS_MAX_PATH, output));
    EXPECT_STREQ(output, "Assets/Textures/hero_albedo.dds");
}

// Verifies that path component and extension helpers split, append, and replace path parts consistently.
TEST(CoreToolFileSystemTest, PathExtensionAndComponentHelpersSplitAndAppendConsistently)
{
    char appended[FS_MAX_PATH] = {};
    char merged[FS_MAX_PATH] = {};
    char replaced[FS_MAX_PATH] = {};
    char parent[FS_MAX_PATH] = {};
    char fileName[FS_MAX_PATH] = {};
    char extension[FS_MAX_PATH] = {};
    char noExtension[FS_MAX_PATH] = {};

    ASSERT_TRUE(AppendPathComponent("foo/bar", "baz.txt", merged));
    AppendPathExtension("foo/bar.txt", "bak", appended);
    ReplacePathExtension("foo/bar.txt", ".bin", replaced);
    GetParentPath("foo/bar/baz.txt", parent);
    GetPathFileName("foo/bar/baz.txt", fileName);
    GetPathExtension("foo/bar/baz.txt", extension);
    GetPathExtension("foo/bar/baz", noExtension);

    EXPECT_STREQ(merged, "foo/bar/baz.txt");
    EXPECT_STREQ(appended, "foo/bar.txt.bak");
    EXPECT_STREQ(replaced, "foo/bar.bin");
    EXPECT_STREQ(parent, "foo/bar");
    EXPECT_STREQ(fileName, "baz");
    EXPECT_STREQ(extension, "txt");
    EXPECT_STREQ(noExtension, "");
}
