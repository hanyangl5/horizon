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
// Read-only ozz stream over cooked Bytes, so archives load without another copy of the file.
class SpanStream final: public ozz::io::Stream
{
public:
    explicit SpanStream(Span<char> Bytes): Bytes(Bytes) {}
    bool   opened() const override { return true; }
    size_t Read(void* Buffer, size_t Size) override
    {
        const size_t Count = Size < Bytes.Count - Position ? Size : Bytes.Count - Position;
        memcpy(Buffer, Bytes.Data + Position, Count);
        Position += (uint32_t)Count;
        return Count;
    }
    size_t Write(const void*, size_t) override { return 0; }
    int    Seek(int Offset, Origin Origin) override
    {
        const int64_t Base = Origin == kSet ? 0 : (Origin == kCurrent ? (int64_t)Position : (int64_t)Bytes.Count);
        if (Base + Offset < 0 || Base + Offset > (int64_t)Bytes.Count)
            return -1;
        Position = (uint32_t)(Base + Offset);
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
    explicit SpanReader(Span<char> Bytes): Bytes(Bytes) {}
    bool Read(void* Output, uint32_t Size)
    {
        if (Size > Bytes.Count - Position)
            return false;
        memcpy(Output, Bytes.Data + Position, Size);
        Position += Size;
        return true;
    }
    Span<char> GetRemaining() const { return { Bytes.Data + Position, Bytes.Count - Position }; }
    bool       Holds(uint64_t Size) const { return Size <= Bytes.Count - Position; }

private:
    Span<char> Bytes;
    uint32_t   Position = 0;
};

SkeletonAsset::~SkeletonAsset() { MemDelete(Data); }

bool SkeletonAsset::Parse(Span<char> Bytes)
{
    SpanReader         Reader(Bytes);
    SkeletonFileHeader Header;
    const uint32_t     Magic = Header.magic, Version = Header.version;
    if (!Reader.Read(&Header, sizeof(Header)) || Header.magic != Magic || Header.version != Version || !Header.skinCount ||
        !Reader.Holds((uint64_t)Header.skinCount * sizeof(uint32_t)))
    {
        LOGF(Error, "Invalid skeleton file header");
        return false;
    }
    SkeletonAssetData* Candidate = MemNew(SkeletonAssetData);
    Candidate->skins.Resize(Header.skinCount);
    bool Valid = true;
    for (SkinData& Skin : Candidate->skins)
    {
        uint32_t JointCount = 0;
        // Per joint: index, inverse bind matrix and bounds.
        Valid = Reader.Read(&JointCount, sizeof(JointCount)) && JointCount && JointCount <= kMaxSkinJoints &&
                Reader.Holds((uint64_t)JointCount * (sizeof(uint32_t) + 22 * sizeof(float)));
        if (!Valid)
            break;
        Skin.joints.Resize(JointCount);
        Skin.inverseBind.Resize(JointCount);
        Skin.bounds.Resize(JointCount * 6);
        Valid = Reader.Read(Skin.joints.GetData(), JointCount * sizeof(uint32_t));
        for (uint32_t Joint = 0; Valid && Joint < JointCount; ++Joint)
        {
            float Values[16];
            Valid = Reader.Read(Values, sizeof(Values));
            Skin.inverseBind[Joint] = Matrix4(Vector4(Values[0], Values[1], Values[2], Values[3]), Vector4(Values[4], Values[5], Values[6], Values[7]),
                                              Vector4(Values[8], Values[9], Values[10], Values[11]),
                                              Vector4(Values[12], Values[13], Values[14], Values[15]));
        }
        Valid = Valid && Reader.Read(Skin.bounds.GetData(), JointCount * 6 * sizeof(float));
        if (!Valid)
            break;
    }
    const Span<char> ArchiveBytes = Reader.GetRemaining();
    if (Valid && ArchiveBytes.Count == Header.archiveSize)
    {
        SpanStream         Stream(ArchiveBytes);
        ozz::io::IArchive Archive(&Stream);
        Valid = Archive.TestTag<ozz::animation::Skeleton>();
        if (Valid)
            Archive >> Candidate->skeleton;
        Valid = Valid && Candidate->skeleton.num_joints() > 0;
    }
    else
        Valid = false;
    for (uint32_t S = 0; Valid && S < Candidate->skins.Size(); ++S)
        for (uint32_t Joint : Candidate->skins[S].joints)
            Valid &= Joint < (uint32_t)Candidate->skeleton.num_joints();
    if (!Valid)
    {
        LOGF(Error, "Invalid skeleton file contents");
        MemDelete(Candidate);
        return false;
    }
    MemDelete(Data);
    Data = Candidate;
    return true;
}

uint32_t SkeletonAsset::GetJointCount() const { return Data ? (uint32_t)Data->skeleton.num_joints() : 0; }
uint32_t SkeletonAsset::GetSkinCount() const { return Data ? Data->skins.Size() : 0; }
uint32_t SkeletonAsset::GetSkinJointCount(uint32_t Skin) const
{
    ASSERT(Data && Skin < Data->skins.Size());
    return Data->skins[Skin].joints.Size();
}

AnimationClipAsset::~AnimationClipAsset() { MemDelete(Data); }

bool AnimationClipAsset::Parse(Span<char> Bytes)
{
    SpanReader          Reader(Bytes);
    AnimationFileHeader Header;
    const uint32_t      Magic = Header.magic, Version = Header.version;
    if (!Reader.Read(&Header, sizeof(Header)) || Header.magic != Magic || Header.version != Version ||
        Reader.GetRemaining().Count != Header.archiveSize)
    {
        LOGF(Error, "Invalid animation file header");
        return false;
    }
    AnimationClipAssetData* Candidate = MemNew(AnimationClipAssetData);
    SpanStream              Stream(Reader.GetRemaining());
    ozz::io::IArchive       Archive(&Stream);
    bool                    Valid = Archive.TestTag<ozz::animation::Animation>();
    if (Valid)
        Archive >> Candidate->animation;
    if (!Valid || Candidate->animation.num_tracks() <= 0 || !(Candidate->animation.duration() > 0.0f))
    {
        LOGF(Error, "Invalid animation file contents");
        MemDelete(Candidate);
        return false;
    }
    MemDelete(Data);
    Data = Candidate;
    return true;
}

float    AnimationClipAsset::GetDuration() const { return Data ? Data->animation.duration() : 0.0f; }
uint32_t AnimationClipAsset::GetTrackCount() const { return Data ? (uint32_t)Data->animation.num_tracks() : 0; }
} // namespace hz
