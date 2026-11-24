cbuffer RootConstant0
{
    uint MaskIndex;
    uint SamplerIndex;
    float2 Jitter;
    uint OutputMode;
    uint EncodeSRGB;
};

struct OutlineVertex
{
    float4 Position : SV_Position;
    float2 UV : TEXCOORD0;
};

OutlineVertex VSMain(uint Id : SV_VertexID)
{
    OutlineVertex Output;
    Output.UV = float2((Id << 1) & 2, Id & 2);
    Output.Position = float4(Output.UV * float2(2, -2) + float2(-1, 1), 0, 1);
    return Output;
}

float4 PSMain(OutlineVertex Input) : SV_Target0
{
    Texture2D<float> Mask = ResourceDescriptorHeap[MaskIndex];
    SamplerState ClampSampler = SamplerDescriptorHeap[SamplerIndex];
    const float2 UV = Input.UV + Jitter;
    const float Center = Mask.SampleLevel(ClampSampler, UV, 0);
    if (Center == 0.0)
    {
        discard;
    }
    uint Width, Height;
    Mask.GetDimensions(Width, Height);
    const float2 Radius = 2.0 / float2(Width, Height);
    float Interior = min(Mask.SampleLevel(ClampSampler, UV - float2(Radius.x, 0), 0),
                         Mask.SampleLevel(ClampSampler, UV + float2(Radius.x, 0), 0));
    Interior = min(Interior, Mask.SampleLevel(ClampSampler, UV - float2(0, Radius.y), 0));
    Interior = min(Interior, Mask.SampleLevel(ClampSampler, UV + float2(0, Radius.y), 0));
    const float Coverage = saturate(Center - Interior);
    if (Coverage == 0.0)
    {
        discard;
    }
    // Linear Rec.709 orange, encoded for the presentation surface at a 203 nit HDR reference white.
    float3 Color = float3(1.0, 0.32, 0.02);
    if (OutputMode == 1)
    {
        Color = float3(0.548565, 0.477920, 0.318745); // Rec.2020 / PQ.
    }
    else if (OutputMode == 2)
    {
        Color *= 203.0 / 80.0;
    }
    else if (EncodeSRGB != 0)
    {
        Color = float3(1.0, 0.601243, 0.151704);
    }
    return float4(Color, Coverage);
}
