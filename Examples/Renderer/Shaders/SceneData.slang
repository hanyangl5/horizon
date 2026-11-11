#pragma once

// Matches GpuDrawData uploaded by the renderer.
struct GpuDrawData
{
    uint instance;
    uint material;
    uint materialId;
    uint alphaMode;
    uint positions;
    uint normals;
    uint texcoords;
    uint indices;
    uint indexStride;
    uint firstIndex;
    int  vertexOffset;
    uint indexCount;
};
