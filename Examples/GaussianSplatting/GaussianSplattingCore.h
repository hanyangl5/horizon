#pragma once

#include <ThirdParty/stb/stb_ds.h>
#include "GaussianSplattingHelpers.h"

namespace GaussianSplattingCore
{
using namespace GaussianSplattingHelpers;

// Core GS constants. The app/render-pipeline file should not need to know how
// these values are derived; it only uses them to size buffers and drive loading.
constexpr uint32_t kRibbonSplatCount = 192;
constexpr uint32_t kCoreSplatCount = 160;
constexpr uint32_t kAccentSplatCount = 32;
constexpr uint32_t kIntroSplatCount = kRibbonSplatCount + kCoreSplatCount + kAccentSplatCount;
constexpr uint32_t kVerticesPerSplat = 6;
constexpr size_t   kDefaultLoadedSplatLimit = 200000;
constexpr size_t   kHardLoadedSplatLimit = 1000000;
constexpr float    kPi = 3.14159265358979323846f;
constexpr float    kGoldenAngle = 2.39996322972865332f;
constexpr float    kTanHalfFov = 0.8422883804630794f; // tan(1.4 radians / 2).
constexpr float    kShC0 = 0.28209479177387814f;
constexpr float    kSplatSigmaExtent = 4.0f;
constexpr float    kMinSplatAxisPixels = 0.35f;
constexpr float    kMaxSplatAxisPixels = 1024.0f;
constexpr float    kSceneDistance = 2.35f;

struct Splat
{
    // A splat is "not quite a point": its center is a point, but rendering expands
    // it into a small transparent ellipse.
    vec3  position;
    // A real 3DGS splat stores a 3D Gaussian ellipsoid; scale is the standard
    // deviation along its three local axes.
    float scale[3];
    // Unit quaternion in w, x, y, z order. It orients the three scale axes in 3D.
    float rotation[4];
    // Precomputed model-space 3D covariance. This avoids rebuilding the same
    // matrix from quaternion/scale every frame.
    mat3  covariance;
    // Intro version uses direct RGB. Real GS data often uses spherical harmonic
    // coefficients for view-dependent color.
    float color[3];
    // Alpha blending strength, usually called opacity in GS data.
    float opacity;
};

struct ProjectedSplat
{
    // CPU-projected screen-space splat. Coordinates are already near NDC:
    // x/y roughly live in [-1, 1] and are sent directly to the vertex shader.
    vec2  center;
    // Screen-space ellipse axes. axis0 is usually the major axis and axis1 the minor axis.
    vec2  axis0;
    vec2  axis1;
    // View-space depth used for back-to-front transparent sorting.
    float depth;
    // RGBA, where A is the splat opacity.
    float color[4];
};

struct SplatVertex
{
    // Already in clip space, so the vertex shader does not apply a matrix transform.
    float position[4];
    // Local quad coordinates in [-1, 1]. The pixel shader uses them for falloff.
    float uv[2];
    float color[4];
};

// Each splat is expanded into two triangles, or six vertices.
// Every vertex stores a local quad corner so the pixel shader can measure distance from the center.
constexpr float kQuadCorners[kVerticesPerSplat][2] = {
    { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f },
};

inline int compareProjectedSplatsByDepth(const void* lhs, const void* rhs)
{
    const ProjectedSplat* a = (const ProjectedSplat*)lhs;
    const ProjectedSplat* b = (const ProjectedSplat*)rhs;
    if (a->depth > b->depth)
    {
        return -1;
    }
    if (a->depth < b->depth)
    {
        return 1;
    }
    return 0;
}

inline mat3 makeCovarianceMatrix(const Splat& splat)
{
    const mat3 rotation = quaternionToRotation(splat.rotation[0], splat.rotation[1], splat.rotation[2], splat.rotation[3]);
    const mat3 varianceScale =
        mat3::scale(makeVec3(splat.scale[0] * splat.scale[0], splat.scale[1] * splat.scale[1], splat.scale[2] * splat.scale[2]));
    return rotation * varianceScale * transpose(rotation);
}

inline void precomputeSplatCovariances(Splat* splats)
{
    for (size_t splatIndex = 0; splatIndex < arrlenu(splats); ++splatIndex)
    {
        splats[splatIndex].covariance = makeCovarianceMatrix(splats[splatIndex]);
    }
}

inline bool covarianceToAxes(const Splat& splat, const vec3& viewPosition, const mat3& viewRotation, float width, float height, vec2& axis0,
                             vec2& axis1)
{
    const mat3 viewCovariance = viewRotation * splat.covariance * transpose(viewRotation);

    const float aspect = width / height;
    const float focalLengthX = width * 0.5f / (kTanHalfFov * aspect);
    const float focalLengthY = height * 0.5f / kTanHalfFov;
    const float viewX = componentX(viewPosition);
    const float viewY = componentY(viewPosition);
    const float viewZ = componentZ(viewPosition);
    const float invZ = 1.0f / viewZ;
    const float invZ2 = invZ * invZ;
    const vec3  jacobianX = makeVec3(focalLengthX * invZ, 0.0f, -focalLengthX * viewX * invZ2);
    const vec3  jacobianY = makeVec3(0.0f, -focalLengthY * invZ, focalLengthY * viewY * invZ2);

    // Core 3DGS projection step: screenCov = J * viewCov * J^T.
    // screenCov is a 2x2 symmetric matrix; its eigenvectors/eigenvalues give the
    // screen-space ellipse directions and radii.
    float cov00 = quadraticForm2D(jacobianX, viewCovariance, jacobianX);
    float cov01 = quadraticForm2D(jacobianX, viewCovariance, jacobianY);
    float cov11 = quadraticForm2D(jacobianY, viewCovariance, jacobianY);

    if (!isfinite(cov00) || !isfinite(cov01) || !isfinite(cov11))
    {
        return false;
    }

    cov00 = maxFloat(cov00, 0.0f);
    cov11 = maxFloat(cov11, 0.0f);

    const float mid = 0.5f * (cov00 + cov11);
    const float radius = sqrtf(maxFloat(0.0f, 0.25f * (cov00 - cov11) * (cov00 - cov11) + cov01 * cov01));
    const float lambda0 = maxFloat(mid + radius, 0.0f);
    const float lambda1 = maxFloat(mid - radius, 0.0f);

    if (lambda0 <= 0.0f)
    {
        return false;
    }

    vec2 direction0 = makeVec2(cov01, lambda0 - cov00);

    float       direction0X = componentX(direction0);
    float       direction0Y = componentY(direction0);
    const float directionLength = sqrtf(direction0X * direction0X + direction0Y * direction0Y);
    if (directionLength <= 0.0000001f)
    {
        direction0 = cov00 >= cov11 ? makeVec2(1.0f, 0.0f) : makeVec2(0.0f, 1.0f);
        direction0X = componentX(direction0);
        direction0Y = componentY(direction0);
    }
    else
    {
        direction0X /= directionLength;
        direction0Y /= directionLength;
        direction0 = makeVec2(direction0X, direction0Y);
    }

    const vec2  direction1 = makeVec2(direction0Y, -direction0X);
    const float axisLength0 = clampFloat(sqrtf(lambda0) * kSplatSigmaExtent, kMinSplatAxisPixels, kMaxSplatAxisPixels);
    const float axisLength1 = clampFloat(sqrtf(maxFloat(lambda1, 0.0f)) * kSplatSigmaExtent, kMinSplatAxisPixels, kMaxSplatAxisPixels);

    axis0 = makeVec2(direction0X * axisLength0 * 2.0f / width, direction0Y * axisLength0 * 2.0f / height);
    axis1 = makeVec2(componentX(direction1) * axisLength1 * 2.0f / width, componentY(direction1) * axisLength1 * 2.0f / height);
    return true;
}

inline void setColor(Splat& splat, float r, float g, float b)
{
    splat.color[0] = r;
    splat.color[1] = g;
    splat.color[2] = b;
}

inline void setScale(Splat& splat, float sx, float sy, float sz)
{
    splat.scale[0] = maxFloat(0.000001f, sx);
    splat.scale[1] = maxFloat(0.000001f, sy);
    splat.scale[2] = maxFloat(0.000001f, sz);
}

inline void setRotation(Splat& splat, float w, float x, float y, float z)
{
    splat.rotation[0] = w;
    splat.rotation[1] = x;
    splat.rotation[2] = y;
    splat.rotation[3] = z;
}

inline void setRotationAroundZ(Splat& splat, float radians)
{
    const float halfAngle = radians * 0.5f;
    setRotation(splat, cosf(halfAngle), 0.0f, 0.0f, sinf(halfAngle));
}

inline void makeIntroSplatCloud(Splat** splats)
{
    arrsetlen(*splats, kIntroSplatCount);
    uint32_t index = 0;

    // Group 1: a colorful spiral ribbon. It makes the layered transparent splats
    // read as a continuous surface.
    for (uint32_t i = 0; i < kRibbonSplatCount; ++i)
    {
        const float t = (float)i / (float)(kRibbonSplatCount - 1);
        const float angle = t * kPi * 7.5f;
        const float radius = mix(0.18f, 0.82f, t);
        const float vertical = mix(-0.78f, 0.78f, t);
        const float pulse = sinf(t * kPi * 10.0f) * 0.08f;

        Splat& splat = (*splats)[index++];
        splat.position = makeVec3(cosf(angle) * (radius + pulse), vertical, sinf(angle) * radius * 0.82f);
        const float splatRadius = mix(0.070f, 0.038f, t);
        setScale(splat, splatRadius * 1.65f, splatRadius / 1.65f, splatRadius * 0.45f);
        setRotationAroundZ(splat, angle + kPi * 0.25f);
        splat.opacity = 0.46f;
        setColor(splat, mix(1.0f, 0.18f, t), mix(0.38f, 0.78f, t), mix(0.12f, 1.0f, t));
    }

    // Group 2: a central point cloud. Real GS input is often a point cloud plus
    // attributes; here the golden angle distributes points on a sphere without
    // looking too regular.
    for (uint32_t i = 0; i < kCoreSplatCount; ++i)
    {
        const float u = ((float)i + 0.5f) / (float)kCoreSplatCount;
        const float y = 1.0f - 2.0f * u;
        const float shellRadius = sqrtf(maxFloat(0.0f, 1.0f - y * y));
        const float angle = (float)i * kGoldenAngle;
        const float scale = 0.42f + 0.18f * sinf(angle * 2.1f);

        Splat& splat = (*splats)[index++];
        splat.position = makeVec3(shellRadius * cosf(angle) * scale, y * 0.52f, shellRadius * sinf(angle) * scale);
        const float splatRadius = 0.062f;
        const float anisotropy = 1.0f + 0.28f * sinf(angle);
        setScale(splat, splatRadius * anisotropy, splatRadius / anisotropy, splatRadius * 0.75f);
        setRotationAroundZ(splat, angle);
        splat.opacity = 0.34f;
        setColor(splat, 0.16f + 0.26f * clamp01(y + 0.6f), 0.78f, 0.95f - 0.20f * clamp01(y + 0.2f));
    }

    // Group 3: small accent points near the bottom to make transparency stacking
    // and depth relationships easier to see.
    for (uint32_t i = 0; i < kAccentSplatCount; ++i)
    {
        const float t = (float)i / (float)kAccentSplatCount;
        const float angle = t * kPi * 2.0f;

        Splat& splat = (*splats)[index++];
        splat.position = makeVec3(cosf(angle) * 0.96f, -0.88f + 0.07f * sinf(angle * 3.0f), sinf(angle) * 0.32f);
        setScale(splat, 0.072f * 1.35f, 0.072f / 1.35f, 0.032f);
        setRotationAroundZ(splat, -angle);
        splat.opacity = 0.30f;
        setColor(splat, 1.0f, 0.78f, 0.25f);
    }

    precomputeSplatCovariances(*splats);
}

inline void normalizeLoadedSplats(Splat* splats)
{
    const size_t splatCount = arrlenu(splats);
    if (splatCount == 0)
    {
        return;
    }

    float minX = componentX(splats[0].position);
    float minY = componentY(splats[0].position);
    float minZ = componentZ(splats[0].position);
    float maxX = minX;
    float maxY = minY;
    float maxZ = minZ;
    for (size_t splatIndex = 0; splatIndex < splatCount; ++splatIndex)
    {
        const Splat& splat = splats[splatIndex];
        const float  x = componentX(splat.position);
        const float  y = componentY(splat.position);
        const float  z = componentZ(splat.position);
        minX = minFloat(minX, x);
        minY = minFloat(minY, y);
        minZ = minFloat(minZ, z);
        maxX = maxFloat(maxX, x);
        maxY = maxFloat(maxY, y);
        maxZ = maxFloat(maxZ, z);
    }

    const vec3  center = makeVec3((minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f);
    const float centerX = componentX(center);
    const float centerY = componentY(center);
    const float centerZ = componentZ(center);

    float maxDistance = 0.0f;
    for (size_t splatIndex = 0; splatIndex < splatCount; ++splatIndex)
    {
        const Splat& splat = splats[splatIndex];
        const float  dx = componentX(splat.position) - centerX;
        const float  dy = componentY(splat.position) - centerY;
        const float  dz = componentZ(splat.position) - centerZ;
        maxDistance = maxFloat(maxDistance, sqrtf(dx * dx + dy * dy + dz * dz));
    }

    const float modelScale = maxDistance > 0.000001f ? 1.15f / maxDistance : 1.0f;
    const float minScale = 0.000001f;

    for (size_t splatIndex = 0; splatIndex < splatCount; ++splatIndex)
    {
        Splat& splat = splats[splatIndex];
        setVec3(splat.position, (componentX(splat.position) - centerX) * modelScale, (componentY(splat.position) - centerY) * modelScale,
                (componentZ(splat.position) - centerZ) * modelScale);
        for (float& scale : splat.scale)
        {
            // Preserve the trained splat size relationship. The old density-based
            // clamp made large Gaussians collapse and changed the final look.
            scale = maxFloat(scale * modelScale, minScale);
        }
        splat.opacity = clamp01(splat.opacity);
    }

    precomputeSplatCovariances(splats);
}

inline uint32_t buildSplatVertexBuffer(SplatVertex* vertices, uint32_t maxVertexCount, const Splat* splats,
                                       ProjectedSplat** projectedSplats, float width, float height, const char* inputPath, bool orbitCamera,
                                       float elapsedTime)
{
    if (!vertices || maxVertexCount == 0)
    {
        return 0;
    }

    // This is the core sample step. A real GS renderer projects 3D covariance
    // into screen-space 2D covariance, then extracts ellipse axes from its
    // eigenvalues/eigenvectors.
    width = maxFloat(1.0f, width);
    height = maxFloat(1.0f, height);
    const float aspect = width / height;

    // External PLY files default to the public food.ply tutorial view:
    // looking from +X with Z up. The built-in intro cloud keeps a simple
    // front view. Pass --orbit to rotate around the scene.
    const float orbitY = orbitCamera ? elapsedTime * 0.38f : 0.0f;
    const float orbitX = orbitCamera ? sinf(elapsedTime * 0.31f) * 0.22f : 0.0f;
    const mat3  baseViewRotation = !(inputPath && inputPath[0]) ? identityMatrix() : makeTutorialViewRotation();
    const mat3  orbitRotation = orbitCamera ? makeRotationX(orbitX) * makeRotationY(orbitY) : identityMatrix();
    const mat3  viewRotation = baseViewRotation * orbitRotation;

    arrsetlen(*projectedSplats, 0);

    for (size_t splatIndex = 0; splatIndex < arrlenu(splats); ++splatIndex)
    {
        const Splat& splat = splats[splatIndex];
        // Transform the splat from model space to view space.
        vec3         viewPosition = viewRotation * splat.position;

        // Place the whole cloud in front of the camera. Larger z means farther away.
        const float viewX = componentX(viewPosition);
        const float viewY = componentY(viewPosition);
        float       viewZ = componentZ(viewPosition) + kSceneDistance;
        viewPosition.setZ(viewZ);

        if (viewZ <= 0.25f)
        {
            continue;
        }

        const float projectionScale = 1.0f / (kTanHalfFov * viewZ);
        const vec2  center = makeVec2(viewX * projectionScale / aspect, -viewY * projectionScale);

        vec2 axis0(0.0f);
        vec2 axis1(0.0f);
        if (!covarianceToAxes(splat, viewPosition, viewRotation, width, height, axis0, axis1))
        {
            continue;
        }

        ProjectedSplat projected = {
            .center = center,
            // axis0/axis1 are screen-space ellipse axes projected from 3D covariance.
            // Later, center + axis0 * u + axis1 * v builds the enclosing quad.
            .axis0 = axis0,
            .axis1 = axis1,
            .depth = viewZ,
            .color = {
                splat.color[0],
                splat.color[1],
                splat.color[2],
                splat.opacity,
            },
        };

        const float axis0X = componentX(projected.axis0);
        const float axis0Y = componentY(projected.axis0);
        const float axis1X = componentX(projected.axis1);
        const float axis1Y = componentY(projected.axis1);
        const float centerX = componentX(center);
        const float centerY = componentY(center);
        const float bound = maxFloat(sqrtf(axis0X * axis0X + axis0Y * axis0Y), sqrtf(axis1X * axis1X + axis1Y * axis1Y));
        // Coarse off-screen culling: skip vertex generation when the ellipse is fully outside the view.
        if (centerX + bound < -1.2f || centerX - bound > 1.2f || centerY + bound < -1.2f || centerY - bound > 1.2f)
        {
            continue;
        }

        arrpush(*projectedSplats, projected);
    }

    // Transparent objects are usually drawn back-to-front. Larger depth means
    // farther away, so sort descending. This is not the fastest option, but
    // it is a clear first implementation.
    if (arrlenu(*projectedSplats) > 1)
    {
        qsort(*projectedSplats, arrlenu(*projectedSplats), sizeof(ProjectedSplat), compareProjectedSplatsByDepth);
    }

    uint32_t vertexCount = 0;

    // Write each screen-space ellipse as six vertices. The pixel shader uses
    // uv to compute Gaussian opacity, so the quad corners are not solid.
    for (size_t splatIndex = 0; splatIndex < arrlenu(*projectedSplats); ++splatIndex)
    {
        if (vertexCount + kVerticesPerSplat > maxVertexCount)
        {
            break;
        }

        const ProjectedSplat& splat = (*projectedSplats)[splatIndex];
        for (uint32_t cornerIndex = 0; cornerIndex < kVerticesPerSplat; ++cornerIndex)
        {
            const float u = kQuadCorners[cornerIndex][0];
            const float v = kQuadCorners[cornerIndex][1];

            SplatVertex& vertex = vertices[vertexCount++];
            // Map the unit square corner (u, v) to the enclosing ellipse quad in screen space.
            vertex.position[0] = componentX(splat.center) + componentX(splat.axis0) * u + componentX(splat.axis1) * v;
            vertex.position[1] = componentY(splat.center) + componentY(splat.axis0) * u + componentY(splat.axis1) * v;
            vertex.position[2] = 0.5f;
            vertex.position[3] = 1.0f;

            // uv is not a texture coordinate; it is the local distance from the splat center.
            vertex.uv[0] = u;
            vertex.uv[1] = v;
            memcpy(vertex.color, splat.color, sizeof(vertex.color));
        }
    }

    return vertexCount;
}
} // namespace GaussianSplattingCore
