#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

#include <core/definations.h>
#include <core/math.h>

namespace Horizon::MeshImportUtils
{

constexpr u32 INVALID_NODE_INDEX = std::numeric_limits<u32>::max();

inline Math::float4x4 ExternalToInternalMatrix(const float *m)
{
    return Math::float4x4{
        m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12], m[13], m[14], m[15],
    };
}

inline Math::float4x4 ComposeMatrix(const Math::float3 &translation, const Math::float4 &rotation,
                                    const Math::float3 &scale)
{
    Math::float4 q = rotation;
    q.Normalize();

    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float xw = q.x * q.w;
    const float yw = q.y * q.w;
    const float zw = q.z * q.w;

    const float r00 = 1.0f - 2.0f * (yy + zz);
    const float r01 = 2.0f * (xy - zw);
    const float r02 = 2.0f * (xz + yw);
    const float r10 = 2.0f * (xy + zw);
    const float r11 = 1.0f - 2.0f * (xx + zz);
    const float r12 = 2.0f * (yz - xw);
    const float r20 = 2.0f * (xz - yw);
    const float r21 = 2.0f * (yz + xw);
    const float r22 = 1.0f - 2.0f * (xx + yy);

    return Math::float4x4{
        r00 * scale.x, r10 * scale.x, r20 * scale.x, 0.0f, r01 * scale.y, r11 * scale.y, r21 * scale.y, 0.0f,
        r02 * scale.z, r12 * scale.z, r22 * scale.z, 0.0f, translation.x, translation.y, translation.z, 1.0f,
    };
}

inline Math::float4 QuaternionFromRotationRows(float r00, float r01, float r02, float r10, float r11, float r12,
                                               float r20, float r21, float r22)
{
    Math::float4 out{};
    const float trace = r00 + r11 + r22;
    if (trace > 0.0f)
    {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        out.w = 0.25f * s;
        out.x = (r21 - r12) / s;
        out.y = (r02 - r20) / s;
        out.z = (r10 - r01) / s;
    }
    else if (r00 > r11 && r00 > r22)
    {
        const float s = std::sqrt(1.0f + r00 - r11 - r22) * 2.0f;
        out.w = (r21 - r12) / s;
        out.x = 0.25f * s;
        out.y = (r01 + r10) / s;
        out.z = (r02 + r20) / s;
    }
    else if (r11 > r22)
    {
        const float s = std::sqrt(1.0f + r11 - r00 - r22) * 2.0f;
        out.w = (r02 - r20) / s;
        out.x = (r01 + r10) / s;
        out.y = 0.25f * s;
        out.z = (r12 + r21) / s;
    }
    else
    {
        const float s = std::sqrt(1.0f + r22 - r00 - r11) * 2.0f;
        out.w = (r10 - r01) / s;
        out.x = (r02 + r20) / s;
        out.y = (r12 + r21) / s;
        out.z = 0.25f * s;
    }
    out.Normalize();
    return out;
}

inline void DecomposeExternalMatrix(const float *m, Math::float3 &translation, Math::float4 &rotation,
                                    Math::float3 &scale)
{
    translation = Math::float3(m[12], m[13], m[14]);

    Math::float3 column0(m[0], m[1], m[2]);
    Math::float3 column1(m[4], m[5], m[6]);
    Math::float3 column2(m[8], m[9], m[10]);

    scale.x = column0.Length();
    scale.y = column1.Length();
    scale.z = column2.Length();

    if (scale.x > 0.0f)
    {
        column0 /= scale.x;
    }
    if (scale.y > 0.0f)
    {
        column1 /= scale.y;
    }
    if (scale.z > 0.0f)
    {
        column2 /= scale.z;
    }

    const float determinant = column0.Cross(column1).Dot(column2);
    if (determinant < 0.0f)
    {
        scale.x = -scale.x;
        column0 = -column0;
    }

    rotation = QuaternionFromRotationRows(column0.x, column1.x, column2.x, column0.y, column1.y, column2.y, column0.z,
                                          column1.z, column2.z);
}

inline Math::float4 SlerpQuaternion(const Math::float4 &lhs, const Math::float4 &rhs, float t)
{
    Math::float4 a = lhs;
    Math::float4 b = rhs;
    a.Normalize();
    b.Normalize();

    float dot = a.Dot(b);
    if (dot < 0.0f)
    {
        b = -b;
        dot = -dot;
    }

    if (dot > 0.9995f)
    {
        Math::float4 result = a + (b - a) * t;
        result.Normalize();
        return result;
    }

    dot = std::clamp(dot, -1.0f, 1.0f);
    const float theta = std::acos(dot);
    const float sin_theta = std::sin(theta);
    if (sin_theta <= std::numeric_limits<float>::epsilon())
    {
        return a;
    }

    const float weight0 = std::sin((1.0f - t) * theta) / sin_theta;
    const float weight1 = std::sin(t * theta) / sin_theta;
    Math::float4 result = a * weight0 + b * weight1;
    result.Normalize();
    return result;
}

inline void NormalizeJointWeights(Math::float4 &weights)
{
    const float weight_sum = weights.x + weights.y + weights.z + weights.w;
    if (weight_sum > 0.0f)
    {
        const float inv = 1.0f / weight_sum;
        weights.x *= inv;
        weights.y *= inv;
        weights.z *= inv;
        weights.w *= inv;
    }
}

} // namespace Horizon::MeshImportUtils
