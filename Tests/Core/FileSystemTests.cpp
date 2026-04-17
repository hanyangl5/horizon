#include <gtest/gtest.h>

#include <string.h>

#include "Core/IFileSystem.h"

#define IMEMORY_FROM_HEADER
#include "Core/IMemory.h"

// Verifies that memory streams support sequential reads, seeking, substring search, mapping, and closing.
TEST(CoreFileSystemTest, MemoryStreamSupportsReadSeekFindAndClose)
{
    constexpr char buffer[] = "abcXYZabc";

    FileStream stream = {};
    ASSERT_TRUE(fsOpenStreamFromMemory(buffer, sizeof(buffer) - 1, FM_READ, false, &stream));
    ASSERT_TRUE(fsIsMemoryStream(&stream));

    char prefix[4] = {};
    EXPECT_EQ(fsReadFromStream(&stream, prefix, 3), 3u);
    EXPECT_STREQ(prefix, "abc");

    EXPECT_TRUE(fsSeekStream(&stream, SBO_START_OF_FILE, 0));

    ssize_t position = -1;
    ASSERT_TRUE(fsFindStream(&stream, "XYZ", 3, fsGetStreamFileSize(&stream), &position));
    EXPECT_EQ(position, 3);
    EXPECT_EQ(fsGetStreamSeekPosition(&stream), 3);

    EXPECT_TRUE(fsSeekStream(&stream, SBO_END_OF_FILE, 0));
    ASSERT_TRUE(fsFindReverseStream(&stream, "abc", 3, fsGetStreamFileSize(&stream), &position));
    EXPECT_EQ(position, 6);

    size_t mappedSize = 0;
    const void* mappedData = nullptr;
    ASSERT_TRUE(fsStreamMemoryMap(&stream, &mappedSize, &mappedData));
    EXPECT_EQ(mappedSize, sizeof(buffer) - 1);
    ASSERT_NE(mappedData, nullptr);

    EXPECT_TRUE(fsCloseStream(&stream));
    EXPECT_EQ(stream.pIO, nullptr);
}

// Verifies that writable memory streams can grow independently without mutating the original source buffer.
TEST(CoreFileSystemTest, WritableMemoryStreamGrowsWithoutTouchingOriginalBuffer)
{
    char original[] = "abc";
    constexpr char suffix[] = "def";

    FileStream stream = {};
    ASSERT_TRUE(fsOpenStreamFromMemory(original, 3, FM_READ_WRITE_APPEND, false, &stream));

    EXPECT_EQ(fsWriteToStream(&stream, suffix, sizeof(suffix) - 1), sizeof(suffix) - 1);
    EXPECT_EQ(fsGetStreamFileSize(&stream), 6);
    size_t mappedSize = 0;
    const void* mappedData = nullptr;
    EXPECT_FALSE(fsStreamMemoryMap(&stream, &mappedSize, &mappedData));

    EXPECT_TRUE(fsSeekStream(&stream, SBO_START_OF_FILE, 0));

    char combined[7] = {};
    EXPECT_EQ(fsReadFromStream(&stream, combined, 6), 6u);
    EXPECT_STREQ(combined, "abcdef");
    EXPECT_STREQ(original, "abc");

    EXPECT_TRUE(fsCloseStream(&stream));
}

// Verifies that Buny archive block pointers preserve compression, size, and offset metadata through encode/decode.
TEST(CoreFileSystemTest, BunyArchiveBlockPointerRoundTripsMetadata)
{
    const BunyArBlockInfo info = { true, 345u, 432u };
    BunyArBlockPointer ptr = 0;

    ASSERT_TRUE(bunyArEncodeBlockPointer(info, &ptr));

    const BunyArBlockInfo decoded = bunyArDecodeBlockPointer(ptr);
    EXPECT_EQ(decoded.isCompressed, info.isCompressed);
    EXPECT_EQ(decoded.size, info.size);
    EXPECT_EQ(decoded.offset, info.offset);
}

// Verifies that the Buny archive hash table resolves existing entries and reports misses outside the valid range.
TEST(CoreFileSystemTest, BunyArchiveHashTableResolvesExistingAndMissingEntries)
{
    constexpr char nodeNames[] = "foo\0bar\0baz.txt\0";
    BunyArNode nodes[] = {
        { BUNYAR_FILE_FORMAT_RAW, 10, { 0, 3 }, { 0, 10 } },
        { BUNYAR_FILE_FORMAT_RAW, 20, { 4, 3 }, { 10, 20 } },
        { BUNYAR_FILE_FORMAT_RAW, 30, { 8, 7 }, { 30, 30 } },
    };

    BunyArHashTable* hashTable = bunyArHashTableConstruct(TF_ARRAY_COUNT(nodes), nodes, nodeNames);
    ASSERT_NE(hashTable, nullptr);

    EXPECT_EQ(bunyArHashTableLookup(hashTable, "foo", TF_ARRAY_COUNT(nodes), nodes, nodeNames), 0u);
    EXPECT_EQ(bunyArHashTableLookup(hashTable, "bar", TF_ARRAY_COUNT(nodes), nodes, nodeNames), 1u);
    EXPECT_EQ(bunyArHashTableLookup(hashTable, "baz.txt", TF_ARRAY_COUNT(nodes), nodes, nodeNames), 2u);
    EXPECT_GE(bunyArHashTableLookup(hashTable, "missing", TF_ARRAY_COUNT(nodes), nodes, nodeNames), TF_ARRAY_COUNT(nodes));

    tf_free(hashTable);
}
