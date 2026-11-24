cbuffer RootConstant0
{
    float2 Origin;
    float2 ViewSize;
    float2 U;
    float2 V;
    uint ToolIndex;
    uint AxisIndex;
    float Thickness;
    uint OutputMode;
    uint EncodeSRGB;
};

static const uint Quad[6] = { 0, 1, 2, 2, 1, 3 };

float2 RingPoint(float Angle)
{
    return U * cos(Angle) + V * sin(Angle);
}

float4 VSMain(uint Id : SV_VertexID) : SV_Position
{
    float2 Pixel;
    if (ToolIndex == 1 || Id < 6)
    {
        float2 A = 0.12 * U;
        float2 B = U;
        if (ToolIndex == 1)
        {
            const float Step = 6.283185307 / 64.0;
            A = RingPoint((Id / 6) * Step);
            B = RingPoint((Id / 6 + 1) * Step);
        }
        const float2 Edge = B - A;
        const float2 Side = float2(-Edge.y, Edge.x) * (Thickness / max(length(Edge), 0.001));
        const uint Corner = Quad[Id % 6];
        Pixel = ((Corner & 2) ? B : A) + ((Corner & 1) ? Side : -Side);
    }
    else
    {
        const float2 Along = normalize(U);
        const float2 Side = float2(-Along.y, Along.x);
        if (ToolIndex == 0)
        {
            Pixel = Id == 6 ? U : U - 9 * Thickness * Along + (Id == 7 ? 4 : -4) * Thickness * Side;
        }
        else
        {
            const uint Corner = Quad[Id - 6];
            Pixel = U + ((Corner & 2) ? Along : -Along) * 3 * Thickness + ((Corner & 1) ? Side : -Side) * 3 * Thickness;
        }
    }
    return float4((Origin + Pixel) / ViewSize * float2(2, -2) + float2(-1, 1), 0, 1);
}

float4 PSMain() : SV_Target0
{
    static const float3 Colors[4] = { float3(1, 0.03, 0.03), float3(0.03, 1, 0.03), float3(0.04, 0.22, 1), float3(1, 0.8, 0.02) };
    float3 Color = Colors[AxisIndex];
    if (OutputMode == 1)
    {
        const float3 Rec2020 = mul(float3x3(0.6274, 0.3293, 0.0433, 0.0691, 0.9195, 0.0114, 0.0164, 0.0880, 0.8956), Color);
        const float3 L = pow(Rec2020 * (203.0 / 10000.0), 0.1593017578);
        Color = pow((0.8359375 + 18.8515625 * L) / (1.0 + 18.6875 * L), 78.84375);
    }
    else if (OutputMode == 2)
    {
        Color *= 203.0 / 80.0;
    }
    else if (EncodeSRGB != 0)
    {
        // All four gizmo colors are above the linear segment of the sRGB curve.
        Color = 1.055 * pow(Color, 1.0 / 2.4) - 0.055;
    }
    return float4(Color, 1);
}
