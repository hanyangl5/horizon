#include <metal_stdlib>

using namespace metal;

struct VSInput
{
    packed_float3 position;
    packed_float3 color;
};

struct VSOutput
{
    float4 position [[position]];
    float3 color;
};

vertex VSOutput VSMain(const device VSInput *vertices [[buffer(0)]], uint vertex_id [[vertex_id]])
{
    const VSInput input = vertices[vertex_id];

    VSOutput output;
    output.position = float4(float3(input.position), 1.0);
    output.color = float3(input.color);
    return output;
}

fragment float4 PSMain(VSOutput input [[stage_in]])
{
    return float4(input.color, 1.0);
}
