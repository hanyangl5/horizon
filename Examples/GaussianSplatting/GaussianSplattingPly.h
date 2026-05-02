#pragma once

#include <ThirdParty/stb/stb_ds.h>
#include "Core/IFileSystem.h"
#include "GaussianSplattingCore.h"

namespace GaussianSplattingPly
{
using namespace GaussianSplattingCore;
using namespace GaussianSplattingHelpers;

enum class PlyFormat
{
    Ascii,
    BinaryLittleEndian,
};

enum class PlyScalarType
{
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Float32,
    Float64,
};

struct PlyProperty
{
    char          name[64];
    PlyScalarType type;
};

struct PlyHeader
{
    PlyFormat    format = PlyFormat::Ascii;
    uint64_t     vertexCount = 0;
    PlyProperty* vertexProperties = nullptr;
};

struct MemoryCursor
{
    const uint8_t* current = nullptr;
    const uint8_t* end = nullptr;
};

inline bool isPathSeparator(char value) { return value == '/' || value == '\\'; }

inline bool isDriveAbsolutePath(const char* path)
{
    return path && isalpha((unsigned char)path[0]) && path[1] == ':' && isPathSeparator(path[2]);
}

inline bool pathCharEquals(char lhs, char rhs)
{
    if (isPathSeparator(lhs) && isPathSeparator(rhs))
    {
        return true;
    }

    return tolower((unsigned char)lhs) == tolower((unsigned char)rhs);
}

inline const char* skipLaunchRelativePrefix(const char* path)
{
    const char* cursor = path;
    for (;;)
    {
        if (cursor[0] == '.' && isPathSeparator(cursor[1]))
        {
            cursor += 2;
            continue;
        }

        if (cursor[0] == '.' && cursor[1] == '.' && isPathSeparator(cursor[2]))
        {
            cursor += 3;
            continue;
        }

        break;
    }

    return cursor;
}

inline bool makeProjectRelativePath(const char* path, char* output, size_t outputCapacity)
{
    if (!path || !path[0] || outputCapacity == 0)
    {
        return false;
    }

    const char* projectRoot = fsGetResourceDirectory(RD_OTHER_FILES);
    if (!projectRoot || !projectRoot[0])
    {
        return false;
    }

    const char* pathCursor = path;
    const char* rootCursor = projectRoot;
    while (*pathCursor && *rootCursor && pathCharEquals(*pathCursor, *rootCursor))
    {
        ++pathCursor;
        ++rootCursor;
    }

    if (*rootCursor != '\0')
    {
        return false;
    }

    while (isPathSeparator(*pathCursor))
    {
        ++pathCursor;
    }

    if (!pathCursor[0])
    {
        return false;
    }

    strncpy(output, pathCursor, outputCapacity - 1);
    output[outputCapacity - 1] = '\0';
    return true;
}

inline bool makeLaunchRelativeProjectPath(const char* path, char* output, size_t outputCapacity)
{
    if (!path || !path[0] || outputCapacity == 0 || isDriveAbsolutePath(path))
    {
        return false;
    }

    const char* trimmedPath = skipLaunchRelativePrefix(path);
    if (trimmedPath == path || !trimmedPath[0])
    {
        return false;
    }

    strncpy(output, trimmedPath, outputCapacity - 1);
    output[outputCapacity - 1] = '\0';
    return true;
}

inline bool openSplatInputStream(const char* path, FileStream* input)
{
    char projectRelativePath[FS_MAX_PATH] = {};
    if (makeProjectRelativePath(path, projectRelativePath, sizeof(projectRelativePath)) ||
        makeLaunchRelativeProjectPath(path, projectRelativePath, sizeof(projectRelativePath)))
    {
        return fsOpenStreamFromPath(RD_OTHER_FILES, projectRelativePath, FileMode(FM_READ | FM_ALLOW_READ), input);
    }

    if (isDriveAbsolutePath(path))
    {
        return false;
    }

    return fsOpenStreamFromPath(RD_OTHER_FILES, path, FileMode(FM_READ | FM_ALLOW_READ), input);
}

inline bool readFileToStbArray(const char* path, uint8_t** data, const char** error)
{
    *data = nullptr;

    FileStream input = {};
    if (!openSplatInputStream(path, &input))
    {
        *error = "failed to open file";
        return false;
    }

    const ssize_t fileSize = fsGetStreamFileSize(&input);
    if (fileSize <= 0)
    {
        fsCloseStream(&input);
        *error = "empty file";
        return false;
    }

    arrsetlen(*data, (size_t)fileSize + 1);
    const size_t bytesRead = fsReadFromStream(&input, *data, (size_t)fileSize);
    fsCloseStream(&input);

    if (bytesRead != (size_t)fileSize)
    {
        arrfree(*data);
        *data = nullptr;
        *error = "failed to read file";
        return false;
    }

    (*data)[fileSize] = 0;
    return true;
}

inline bool readLine(MemoryCursor* cursor, char* line, size_t lineCapacity)
{
    if (cursor->current >= cursor->end || lineCapacity == 0)
    {
        return false;
    }

    size_t length = 0;
    while (cursor->current < cursor->end && *cursor->current != '\n')
    {
        if (length + 1 < lineCapacity)
        {
            line[length++] = (char)*cursor->current;
        }
        ++cursor->current;
    }

    if (cursor->current < cursor->end && *cursor->current == '\n')
    {
        ++cursor->current;
    }
    if (length > 0 && line[length - 1] == '\r')
    {
        --length;
    }

    line[length] = '\0';
    return true;
}

inline bool nextToken(char** cursor, char* token, size_t tokenCapacity)
{
    char* text = *cursor;
    while (*text && isspace((unsigned char)*text))
    {
        ++text;
    }

    if (!*text || tokenCapacity == 0)
    {
        *cursor = text;
        return false;
    }

    size_t length = 0;
    while (*text && !isspace((unsigned char)*text))
    {
        if (length + 1 < tokenCapacity)
        {
            token[length++] = *text;
        }
        ++text;
    }

    token[length] = '\0';
    *cursor = text;
    return length > 0;
}

inline bool parseU64Token(const char* token, uint64_t* value)
{
    if (!token || !*token || !value)
    {
        return false;
    }

    errno = 0;
    char*                    end = NULL;
    const unsigned long long parsed = strtoull(token, &end, 10);
    if (errno != 0 || !end || *end != '\0')
    {
        return false;
    }

    *value = (uint64_t)parsed;
    return true;
}

inline bool parseScalarType(const char* typeName, PlyScalarType* type)
{
    if (stringEquals(typeName, "char") || stringEquals(typeName, "int8"))
    {
        *type = PlyScalarType::Int8;
        return true;
    }
    if (stringEquals(typeName, "uchar") || stringEquals(typeName, "uint8") || stringEquals(typeName, "uint8_t"))
    {
        *type = PlyScalarType::UInt8;
        return true;
    }
    if (stringEquals(typeName, "short") || stringEquals(typeName, "int16"))
    {
        *type = PlyScalarType::Int16;
        return true;
    }
    if (stringEquals(typeName, "ushort") || stringEquals(typeName, "uint16"))
    {
        *type = PlyScalarType::UInt16;
        return true;
    }
    if (stringEquals(typeName, "int") || stringEquals(typeName, "int32"))
    {
        *type = PlyScalarType::Int32;
        return true;
    }
    if (stringEquals(typeName, "uint") || stringEquals(typeName, "uint32"))
    {
        *type = PlyScalarType::UInt32;
        return true;
    }
    if (stringEquals(typeName, "float") || stringEquals(typeName, "float32"))
    {
        *type = PlyScalarType::Float32;
        return true;
    }
    if (stringEquals(typeName, "double") || stringEquals(typeName, "float64"))
    {
        *type = PlyScalarType::Float64;
        return true;
    }

    return false;
}

inline bool hasProperty(const PlyProperty* properties, const char* name)
{
    for (size_t propertyIndex = 0; propertyIndex < arrlenu(properties); ++propertyIndex)
    {
        if (stringEquals(properties[propertyIndex].name, name))
        {
            return true;
        }
    }
    return false;
}

inline bool parseHeader(MemoryCursor* cursor, PlyHeader* header, const char** error)
{
    char line[512] = {};
    if (!readLine(cursor, line, sizeof(line)))
    {
        *error = "empty file";
        return false;
    }
    if (!stringEquals(line, "ply"))
    {
        *error = "missing ply magic header";
        return false;
    }

    char currentElement[64] = {};
    bool foundFormat = false;
    bool foundEndHeader = false;

    while (readLine(cursor, line, sizeof(line)))
    {
        char* lineCursor = line;
        char  keyword[64] = {};
        if (!nextToken(&lineCursor, keyword, sizeof(keyword)))
        {
            continue;
        }

        if (stringEquals(keyword, "format"))
        {
            char formatName[64] = {};
            if (!nextToken(&lineCursor, formatName, sizeof(formatName)))
            {
                *error = "missing PLY format name";
                return false;
            }

            if (stringEquals(formatName, "ascii"))
            {
                header->format = PlyFormat::Ascii;
            }
            else if (stringEquals(formatName, "binary_little_endian"))
            {
                header->format = PlyFormat::BinaryLittleEndian;
            }
            else
            {
                *error = "only ascii and binary_little_endian PLY are supported";
                return false;
            }
            foundFormat = true;
        }
        else if (stringEquals(keyword, "element"))
        {
            char elementName[64] = {};
            char countToken[64] = {};
            if (!nextToken(&lineCursor, elementName, sizeof(elementName)) || !nextToken(&lineCursor, countToken, sizeof(countToken)))
            {
                *error = "malformed PLY element line";
                return false;
            }

            strncpy(currentElement, elementName, sizeof(currentElement) - 1);
            currentElement[sizeof(currentElement) - 1] = '\0';

            uint64_t count = 0;
            if (!parseU64Token(countToken, &count))
            {
                *error = "malformed PLY element count";
                return false;
            }

            if (stringEquals(currentElement, "vertex"))
            {
                header->vertexCount = count;
            }
        }
        else if (stringEquals(keyword, "property") && stringEquals(currentElement, "vertex"))
        {
            char typeName[64] = {};
            char propertyName[64] = {};
            if (!nextToken(&lineCursor, typeName, sizeof(typeName)))
            {
                *error = "malformed PLY property line";
                return false;
            }
            if (stringEquals(typeName, "list"))
            {
                *error = "list properties inside vertex elements are not supported";
                return false;
            }
            if (!nextToken(&lineCursor, propertyName, sizeof(propertyName)))
            {
                *error = "missing PLY property name";
                return false;
            }

            PlyScalarType propertyType = PlyScalarType::Float32;
            if (!parseScalarType(typeName, &propertyType))
            {
                *error = "unsupported PLY vertex property type";
                return false;
            }

            PlyProperty property = {};
            strncpy(property.name, propertyName, sizeof(property.name) - 1);
            property.name[sizeof(property.name) - 1] = '\0';
            property.type = propertyType;
            arrpush(header->vertexProperties, property);
        }
        else if (stringEquals(keyword, "end_header"))
        {
            foundEndHeader = true;
            break;
        }
    }

    if (!foundFormat)
    {
        *error = "missing PLY format line";
        return false;
    }
    if (!foundEndHeader)
    {
        *error = "missing PLY end_header";
        return false;
    }
    if (header->vertexCount == 0)
    {
        *error = "PLY has no vertices";
        return false;
    }
    if (arrlenu(header->vertexProperties) == 0)
    {
        *error = "PLY vertex element has no scalar properties";
        return false;
    }

    return true;
}

inline bool readMemoryBytes(MemoryCursor* cursor, void* value, size_t size)
{
    if ((size_t)(cursor->end - cursor->current) < size)
    {
        return false;
    }

    memcpy(value, cursor->current, size);
    cursor->current += size;
    return true;
}

inline bool readScalar(MemoryCursor* cursor, PlyScalarType type, double* value)
{
    switch (type)
    {
    case PlyScalarType::Int8:
    {
        int8_t rawValue = 0;
        if (!readMemoryBytes(cursor, &rawValue, sizeof(rawValue)))
        {
            return false;
        }
        *value = rawValue;
        return true;
    }
    case PlyScalarType::UInt8:
    {
        uint8_t rawValue = 0;
        if (!readMemoryBytes(cursor, &rawValue, sizeof(rawValue)))
        {
            return false;
        }
        *value = rawValue;
        return true;
    }
    case PlyScalarType::Int16:
    {
        int16_t rawValue = 0;
        if (!readMemoryBytes(cursor, &rawValue, sizeof(rawValue)))
        {
            return false;
        }
        *value = rawValue;
        return true;
    }
    case PlyScalarType::UInt16:
    {
        uint16_t rawValue = 0;
        if (!readMemoryBytes(cursor, &rawValue, sizeof(rawValue)))
        {
            return false;
        }
        *value = rawValue;
        return true;
    }
    case PlyScalarType::Int32:
    {
        int32_t rawValue = 0;
        if (!readMemoryBytes(cursor, &rawValue, sizeof(rawValue)))
        {
            return false;
        }
        *value = rawValue;
        return true;
    }
    case PlyScalarType::UInt32:
    {
        uint32_t rawValue = 0;
        if (!readMemoryBytes(cursor, &rawValue, sizeof(rawValue)))
        {
            return false;
        }
        *value = rawValue;
        return true;
    }
    case PlyScalarType::Float32:
    {
        float rawValue = 0.0f;
        if (!readMemoryBytes(cursor, &rawValue, sizeof(rawValue)))
        {
            return false;
        }
        *value = rawValue;
        return true;
    }
    case PlyScalarType::Float64:
    {
        double rawValue = 0.0;
        if (!readMemoryBytes(cursor, &rawValue, sizeof(rawValue)))
        {
            return false;
        }
        *value = rawValue;
        return true;
    }
    }

    return false;
}

inline bool parseAsciiDouble(MemoryCursor* cursor, double* value)
{
    while (cursor->current < cursor->end && isspace((unsigned char)*cursor->current))
    {
        ++cursor->current;
    }

    if (cursor->current >= cursor->end)
    {
        return false;
    }

    errno = 0;
    char*        parsedEnd = NULL;
    const double parsedValue = strtod((const char*)cursor->current, &parsedEnd);
    if (errno != 0 || parsedEnd == (const char*)cursor->current)
    {
        return false;
    }

    cursor->current = (const uint8_t*)parsedEnd;
    *value = parsedValue;
    return true;
}

inline bool getValue(const double* values, const PlyProperty* properties, const char* name, double* value)
{
    for (size_t propertyIndex = 0; propertyIndex < arrlenu(properties); ++propertyIndex)
    {
        if (stringEquals(properties[propertyIndex].name, name))
        {
            *value = values[propertyIndex];
            return true;
        }
    }

    return false;
}

inline bool makeSplatFromPlyValues(const double* values, const PlyProperty* properties, Splat* splat)
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    if (!getValue(values, properties, "x", &x) || !getValue(values, properties, "y", &y) || !getValue(values, properties, "z", &z))
    {
        return false;
    }

    *splat = {};
    splat->position = makeVec3((float)x, (float)y, (float)z);

    double r = 1.0;
    double g = 1.0;
    double b = 1.0;
    if (getValue(values, properties, "f_dc_0", &r) && getValue(values, properties, "f_dc_1", &g) &&
        getValue(values, properties, "f_dc_2", &b))
    {
        // 3DGS f_dc_* values are 0th-order spherical harmonic coefficients, not direct RGB.
        // 0.5 + C0 * coeff is the common decode path.
        setColor(*splat, clamp01(0.5f + kShC0 * (float)r), clamp01(0.5f + kShC0 * (float)g), clamp01(0.5f + kShC0 * (float)b));
    }
    else if (getValue(values, properties, "red", &r) && getValue(values, properties, "green", &g) &&
             getValue(values, properties, "blue", &b))
    {
        const float colorScale = (r > 1.0 || g > 1.0 || b > 1.0) ? (1.0f / 255.0f) : 1.0f;
        setColor(*splat, clamp01((float)r * colorScale), clamp01((float)g * colorScale), clamp01((float)b * colorScale));
    }
    else
    {
        setColor(*splat, 0.9f, 0.9f, 0.9f);
    }

    double opacity = 0.0;
    if (getValue(values, properties, "opacity", &opacity))
    {
        // Opacity in standard 3DGS PLY files is usually a logit value, so apply sigmoid to recover [0, 1].
        splat->opacity = clamp01(sigmoid((float)opacity));
    }
    else if (getValue(values, properties, "alpha", &opacity))
    {
        splat->opacity = clamp01((float)opacity * (opacity > 1.0 ? (1.0f / 255.0f) : 1.0f));
    }
    else
    {
        splat->opacity = 0.55f;
    }

    double scale0 = 0.0;
    double scale1 = 0.0;
    double scale2 = 0.0;
    if (getValue(values, properties, "scale_0", &scale0) && getValue(values, properties, "scale_1", &scale1))
    {
        const bool  hasScale2 = getValue(values, properties, "scale_2", &scale2);
        const float sx = safeExp((float)scale0);
        const float sy = safeExp((float)scale1);
        const float sz = hasScale2 ? safeExp((float)scale2) : sy;
        setScale(*splat, sx, sy, sz);
    }
    else
    {
        setScale(*splat, 0.01f, 0.01f, 0.01f);
    }

    double rot0 = 1.0;
    double rot1 = 0.0;
    double rot2 = 0.0;
    double rot3 = 0.0;
    if (getValue(values, properties, "rot_0", &rot0) && getValue(values, properties, "rot_1", &rot1) &&
        getValue(values, properties, "rot_2", &rot2) && getValue(values, properties, "rot_3", &rot3))
    {
        setRotation(*splat, (float)rot0, (float)rot1, (float)rot2, (float)rot3);
    }
    else
    {
        setRotation(*splat, 1.0f, 0.0f, 0.0f, 0.0f);
    }

    return true;
}

inline bool loadPlySplatFile(const char* path, size_t maxSplats, Splat** splats, const char** error)
{
    uint8_t* fileData = nullptr;
    if (!readFileToStbArray(path, &fileData, error))
    {
        return false;
    }

    MemoryCursor cursor = {
        .current = fileData,
        .end = fileData + arrlenu(fileData) - 1,
    };

    PlyHeader header = {};
    if (!parseHeader(&cursor, &header, error))
    {
        arrfree(header.vertexProperties);
        arrfree(fileData);
        return false;
    }

    if (!hasProperty(header.vertexProperties, "x") || !hasProperty(header.vertexProperties, "y") ||
        !hasProperty(header.vertexProperties, "z"))
    {
        *error = "PLY vertex properties must contain x, y, z";
        arrfree(header.vertexProperties);
        arrfree(fileData);
        return false;
    }

    const uint64_t limit = maxSplats == 0 ? kHardLoadedSplatLimit : (uint64_t)maxSplats;
    const size_t   targetCount = (size_t)(header.vertexCount < limit ? header.vertexCount : limit);
    arrsetlen(*splats, 0);
    arrsetcap(*splats, targetCount);

    double* values = nullptr;
    arrsetlen(values, arrlenu(header.vertexProperties));

    for (uint64_t vertexIndex = 0; vertexIndex < header.vertexCount; ++vertexIndex)
    {
        for (size_t propertyIndex = 0; propertyIndex < arrlenu(header.vertexProperties); ++propertyIndex)
        {
            bool parsed = false;
            if (header.format == PlyFormat::Ascii)
            {
                parsed = parseAsciiDouble(&cursor, &values[propertyIndex]);
            }
            else
            {
                parsed = readScalar(&cursor, header.vertexProperties[propertyIndex].type, &values[propertyIndex]);
            }

            if (!parsed)
            {
                *error =
                    header.format == PlyFormat::Ascii ? "failed to parse ASCII PLY vertex row" : "unexpected end of binary PLY vertex data";
                arrfree(values);
                arrfree(header.vertexProperties);
                arrfree(fileData);
                return false;
            }
        }

        if (!shouldKeepSample(vertexIndex, header.vertexCount, targetCount))
        {
            continue;
        }

        Splat splat = {};
        if (makeSplatFromPlyValues(values, header.vertexProperties, &splat))
        {
            arrpush(*splats, splat);
        }
    }

    arrfree(values);
    arrfree(header.vertexProperties);
    arrfree(fileData);

    if (arrlenu(*splats) == 0)
    {
        *error = "no usable splats were decoded from PLY";
        return false;
    }

    normalizeLoadedSplats(*splats);
    return true;
}

inline bool loadSplatFile(const char* path, size_t maxSplats, Splat** splats, const char** error)
{
    if (!hasSupportedPlyExtension(path))
    {
        *error = "only .ply input is supported";
        return false;
    }

    return loadPlySplatFile(path, maxSplats, splats, error);
}
} // namespace GaussianSplattingPly
