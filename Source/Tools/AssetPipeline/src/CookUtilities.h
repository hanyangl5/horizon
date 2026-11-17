/* Copyright (c) 2026 Horizon */
#pragma once

#include "Core/IFileSystem.h"
#include "Core/ILog.h"
#include "Core/IToolFileSystem.h"
#include <stdint.h>
#include <stdio.h>

static const uint64_t kCookHashSeed = UINT64_C(14695981039346656037);

// FNV-1a. Cook signatures and the published content hashes depend on this exact function.
inline uint64_t cookHash(const void* pData, size_t size, uint64_t hash = kCookHashSeed)
{
    const uint8_t* pBytes = (const uint8_t*)pData;
    for (size_t i = 0; i < size; ++i)
        hash = (hash ^ pBytes[i]) * UINT64_C(1099511628211);
    return hash;
}

// Writes a cook output to "<path>.tmp" and publishes it by renaming over the previous file, which is kept as
// "<path>.bak" until the rename succeeds. A backup left by an interrupted cook is restored before writing again.
class CookedFile
{
public:
    CookedFile(ResourceDirectory directory, const char* pPath): directory(directory), pPath(pPath)
    {
        if (snprintf(temporary, sizeof(temporary), "%s.tmp", pPath) >= (int)sizeof(temporary) ||
            snprintf(backup, sizeof(backup), "%s.bak", pPath) >= (int)sizeof(backup))
            return;
        if (!FileExists(directory, pPath) && FileExists(directory, backup) && !RenameFile(directory, backup, pPath))
            return;
        opened = OpenStreamFromPath(directory, temporary, FileModeWriteAllowRead, &stream);
        written = opened;
    }
    ~CookedFile()
    {
        if (opened)
            CloseStream(&stream);
    }
    CookedFile(const CookedFile&) = delete;
    CookedFile& operator=(const CookedFile&) = delete;

    void Write(const void* pData, size_t size)
    {
        if (written)
            written = WriteToStream(&stream, pData, size) == size;
    }
    bool Publish()
    {
        if (!opened)
            return false;
        const bool closed = CloseStream(&stream);
        opened = false;
        if (!written || !closed)
            return false;
        const bool exists = FileExists(directory, pPath);
        if ((FileExists(directory, backup) && !RemoveFile(directory, backup)) || (exists && !RenameFile(directory, pPath, backup)))
        {
            LOGF(Error, "Cannot back up '%s'; check for an interrupted cook", pPath);
            return false;
        }
        if (!RenameFile(directory, temporary, pPath))
        {
            if (exists && !RenameFile(directory, backup, pPath))
                LOGF(Error, "Restore '%s' from '%s' before retrying", pPath, backup);
            return false;
        }
        if (exists && !RemoveFile(directory, backup))
            LOGF(Warning, "Cannot remove completed cook backup '%s'", backup);
        return true;
    }

private:
    ResourceDirectory directory;
    const char*       pPath;
    FileStream        stream = {};
    char              temporary[FS_MAX_PATH] = {};
    char              backup[FS_MAX_PATH] = {};
    bool              opened = false;
    bool              written = false;
};
