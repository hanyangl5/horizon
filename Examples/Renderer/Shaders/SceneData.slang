#pragma once

// Matches GpuDrawData uploaded by the renderer.
struct GpuDrawData
{
    uint Instance;
    uint Material;
    uint MaterialId;
    uint AlphaMode;
    uint Positions;
    uint Normals;
    uint Texcoords;
    uint Indices;
    uint IndexStride;
    uint FirstIndex;
    int  VertexOffset;
    uint IndexCount;
};
