#ifndef SKELETON_RESOURCES_H
#define SKELETON_RESOURCES_H
#define MAX_INSTANCES 804
struct SkeletonUniforms
{
#if FT_MULTIVIEW
    float4x4 mvp[VR_MULTIVIEW_COUNT];
#else
    float4x4 mvp;
#endif
    float4x4 viewMatrix;
    float4 color[MAX_INSTANCES];
    // Point Light Information
    float4 lightPosition;
    float4 lightColor;
    float4 jointColor;
    uint4 skeletonInfo;
    float4x4 toWorld[MAX_INSTANCES];
};
cbuffer RootConstant0 : register(b0) { uint uniformIndex; };
float4 getRow(float4x4 m, uint row) { return m[row]; }
float4 getCol(float4x4 m, uint col) { return float4(m[0][col], m[1][col], m[2][col], m[3][col]); }
float getElem(float4x4 m, uint col, uint row) { return m[row][col]; }
void setRow(inout float4x4 m, float4 value, uint row) { m[row] = value; }
#endif
