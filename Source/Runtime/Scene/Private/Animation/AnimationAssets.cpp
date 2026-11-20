/* Copyright (c) 2026 Horizon */

#include "Scene/AnimationAssets.h"
#include "AnimationAssetData.h"
#include "AnimationFiles.h"
#include "Core/ILog.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include <string.h>

#include "Core/IMemory.h"

namespace hz
{
// Read-only ozz stream over cooked bytes, so archives load without another copy of the file.
class SpanStream final: public ozz::io::Stream
{
public:
    explicit SpanStream(Span<char> bytes): Bytes(bytes) {}
    bool   opened() const override { return true; }
    size_t Read(void* pBuffer, size_t size) override
    {
        const size_t count = size < Bytes.Count - Position ? size : Bytes.Count - Position;
        memcpy(pBuffer, Bytes.Data + Position, count);
        Position += (uint32_t)count;
        return count;
    }
    size_t Write(const void*, size_t) override { return 0; }
    int    Seek(int offset, Origin origin) override
    {
        const int64_t base = origin == kSet ? 0 : (origin == kCurrent ? (int64_t)Position : (int64_t)Bytes.Count);
        if (base + offset < 0 || base + offset > (int64_t)Bytes.Count)
            return -1;
        Position = (uint32_t)(base + offset);
        return 0;
    }
    int    Tell() const override { return (int)Position; }
    size_t Size() const override { return Bytes.Count; }

private:
    Span<char> Bytes;
    uint32_t   Position = 0;
};

class SpanReader
{
public:
    explicit SpanReader(Span<char> bytes): Bytes(bytes) {}
    bool Read(void* pOutput, uint32_t size)
    {
        if (size > Bytes.Count - Position)
            return false;
        memcpy(pOutput, Bytes.Data + Position, size);
        Position += size;
        return true;
    }
    Span<char> GetRemaining() const { return { Bytes.Data + Position, Bytes.Count - Position }; }

private:
    Span<char> Bytes;
    uint32_t   Position = 0;
};

SkeletonAsset::~SkeletonAsset() { MemDelete(Data); }

bool SkeletonAsset::Parse(Span<char> bytes)
{
    SpanReader         reader(bytes);
    SkeletonFileHeader header;
    const uint32_t     magic = header.magic, version = header.version;
    if (!reader.Read(&header, sizeof(header)) || header.magic != magic || header.version != version || !header.skinCount)
    {
        LOGF(Error, "Invalid skeleton file header");
        return false;
    }
    SkeletonAssetData* pCandidate = MemNew(SkeletonAssetData);
    pCandidate->skins.Resize(header.skinCount);
    bool valid = true;
    for (SkinData& skin : pCandidate->skins)
    {
        uint32_t jointCount = 0;
        valid = reader.Read(&jointCount, sizeof(jointCount)) && jointCount && jointCount <= kMaxSkinJoints;
        if (!valid)
            break;
        skin.joints.Resize(jointCount);
        skin.inverseBind.Resize(jointCount);
        skin.bounds.Resize(jointCount * 6);
        valid = reader.Read(skin.joints.GetData(), jointCount * sizeof(uint32_t));
        for (uint32_t joint = 0; valid && joint < jointCount; ++joint)
        {
            float values[16];
            valid = reader.Read(values, sizeof(values));
            skin.inverseBind[joint] = Matrix4(Vector4(values[0], values[1], values[2], values[3]), Vector4(values[4], values[5], values[6], values[7]),
                                              Vector4(values[8], values[9], values[10], values[11]),
                                              Vector4(values[12], values[13], values[14], values[15]));
        }
        valid = valid && reader.Read(skin.bounds.GetData(), jointCount * 6 * sizeof(float));
        if (!valid)
            break;
    }
    const Span<char> archiveBytes = reader.GetRemaining();
    if (valid && archiveBytes.Count == header.archiveSize)
    {
        SpanStream         stream(archiveBytes);
        ozz::io::IArchive archive(&stream);
        valid = archive.TestTag<ozz::animation::Skeleton>();
        if (valid)
            archive >> pCandidate->skeleton;
        valid = valid && pCandidate->skeleton.num_joints() > 0;
    }
    else
        valid = false;
    for (uint32_t s = 0; valid && s < pCandidate->skins.Size(); ++s)
        for (uint32_t joint : pCandidate->skins[s].joints)
            valid &= joint < (uint32_t)pCandidate->skeleton.num_joints();
    if (!valid)
    {
        LOGF(Error, "Invalid skeleton file contents");
        MemDelete(pCandidate);
        return false;
    }
    MemDelete(Data);
    Data = pCandidate;
    return true;
}

uint32_t SkeletonAsset::GetJointCount() const { return Data ? (uint32_t)Data->skeleton.num_joints() : 0; }
uint32_t SkeletonAsset::GetSkinCount() const { return Data ? Data->skins.Size() : 0; }
uint32_t SkeletonAsset::GetSkinJointCount(uint32_t skin) const
{
    ASSERT(Data && skin < Data->skins.Size());
    return Data->skins[skin].joints.Size();
}

AnimationClipAsset::~AnimationClipAsset() { MemDelete(Data); }

bool AnimationClipAsset::Parse(Span<char> bytes)
{
    SpanReader          reader(bytes);
    AnimationFileHeader header;
    const uint32_t      magic = header.magic, version = header.version;
    if (!reader.Read(&header, sizeof(header)) || header.magic != magic || header.version != version ||
        reader.GetRemaining().Count != header.archiveSize)
    {
        LOGF(Error, "Invalid animation file header");
        return false;
    }
    AnimationClipAssetData* pCandidate = MemNew(AnimationClipAssetData);
    SpanStream              stream(reader.GetRemaining());
    ozz::io::IArchive       archive(&stream);
    bool                    valid = archive.TestTag<ozz::animation::Animation>();
    if (valid)
        archive >> pCandidate->animation;
    if (!valid || pCandidate->animation.num_tracks() <= 0 || !(pCandidate->animation.duration() > 0.0f))
    {
        LOGF(Error, "Invalid animation file contents");
        MemDelete(pCandidate);
        return false;
    }
    MemDelete(Data);
    Data = pCandidate;
    return true;
}

float    AnimationClipAsset::GetDuration() const { return Data ? Data->animation.duration() : 0.0f; }
uint32_t AnimationClipAsset::GetTrackCount() const { return Data ? (uint32_t)Data->animation.num_tracks() : 0; }
} // namespace hz
