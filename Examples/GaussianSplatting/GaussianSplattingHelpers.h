#pragma once

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "Core/IMath.h"

namespace GaussianSplattingHelpers
{
inline float componentX(const vec2& value) { return value.getX(); }
inline float componentY(const vec2& value) { return value.getY(); }
inline float componentX(const vec3& value) { return static_cast<float>(value.getX()); }
inline float componentY(const vec3& value) { return static_cast<float>(value.getY()); }
inline float componentZ(const vec3& value) { return static_cast<float>(value.getZ()); }

inline vec2 makeVec2(float x, float y) { return vec2(x, y); }
inline vec3 makeVec3(float x, float y, float z) { return vec3(x, y, z); }

inline void setVec3(vec3& value, float x, float y, float z)
{
    value.setX(x);
    value.setY(y);
    value.setZ(z);
}

inline float minFloat(float a, float b) { return a < b ? a : b; }

inline float maxFloat(float a, float b) { return a > b ? a : b; }

inline float clamp01(float value) { return maxFloat(0.0f, minFloat(1.0f, value)); }

inline float clampFloat(float value, float minValue, float maxValue) { return maxFloat(minValue, minFloat(maxValue, value)); }

inline float mix(float a, float b, float t) { return a + (b - a) * t; }

inline float sigmoid(float value) { return 1.0f / (1.0f + expf(-value)); }

inline float safeExp(float value) { return expf(clampFloat(value, -20.0f, 20.0f)); }

inline bool stringEquals(const char* lhs, const char* rhs) { return lhs && rhs && strcmp(lhs, rhs) == 0; }

inline bool stringEqualsIgnoreCase(const char* lhs, const char* rhs)
{
    if (!lhs || !rhs)
    {
        return false;
    }

    while (*lhs && *rhs)
    {
        const unsigned char lhsChar = (unsigned char)*lhs++;
        const unsigned char rhsChar = (unsigned char)*rhs++;
        if (tolower(lhsChar) != tolower(rhsChar))
        {
            return false;
        }
    }

    return *lhs == '\0' && *rhs == '\0';
}

inline bool hasSupportedPlyExtension(const char* path)
{
    const char* extension = path ? strrchr(path, '.') : NULL;
    return stringEqualsIgnoreCase(extension, ".ply");
}

inline bool parseSize(const char* text, size_t* value)
{
    if (!text || !*text || !value)
    {
        return false;
    }

    errno = 0;
    char*                    end = NULL;
    const unsigned long long parsedValue = strtoull(text, &end, 10);
    if (errno != 0 || !end || *end != '\0')
    {
        return false;
    }

    *value = (size_t)parsedValue;
    return true;
}

inline bool shouldKeepSample(uint64_t index, uint64_t totalCount, size_t maxCount)
{
    if (maxCount == 0 || totalCount <= maxCount)
    {
        return true;
    }

    return ((index + 1) * maxCount / totalCount) != (index * maxCount / totalCount);
}

inline mat3 identityMatrix() { return mat3::identity(); }

inline mat3 makeTutorialViewRotation()
{
    // The public food.ply tutorial looks from +X toward the origin with Z as up.
    // This matrix maps model axes into the sample's view-space convention.
    return mat3(makeVec3(0.0f, 0.0f, -1.0f), makeVec3(1.0f, 0.0f, 0.0f), makeVec3(0.0f, -1.0f, 0.0f));
}

inline mat3 makeRotationX(float radians) { return mat3::rotationX(radians); }

inline mat3 makeRotationY(float radians) { return mat3::rotationY(radians); }

inline mat3 quaternionToRotation(float w, float x, float y, float z)
{
    const float lengthSq = w * w + x * x + y * y + z * z;
    if (lengthSq <= 0.000001f)
    {
        return identityMatrix();
    }

    const float invLength = 1.0f / sqrtf(lengthSq);
    w *= invLength;
    x *= invLength;
    y *= invLength;
    z *= invLength;

    return mat3(makeVec3(1.0f - 2.0f * (y * y + z * z), 2.0f * (x * y + w * z), 2.0f * (x * z - w * y)),
                makeVec3(2.0f * (x * y - w * z), 1.0f - 2.0f * (x * x + z * z), 2.0f * (y * z + w * x)),
                makeVec3(2.0f * (x * z + w * y), 2.0f * (y * z - w * x), 1.0f - 2.0f * (x * x + y * y)));
}

inline float quadraticForm2D(const vec3& a, const mat3& matrix, const vec3& b)
{
    const vec3 mb = matrix * b;
    return componentX(a) * componentX(mb) + componentY(a) * componentY(mb) + componentZ(a) * componentZ(mb);
}
} // namespace GaussianSplattingHelpers
