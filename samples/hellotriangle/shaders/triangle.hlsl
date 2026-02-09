// Simple triangle shader for DX12 backend testing

struct VSInput
{
    float3 position : POSITION;
    //float3 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    //float3 color : COLOR;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = float4(input.position, 1.0);
    //output.color = float3(1.0, 0.0, 0.0);
    return output;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return float4(1.0, 0.0, 0.0, 1.0);
}
