// set 0 , bind 0
[[vk::binding(0, 0)]] cbuffer ViewProjection : register(b0) { matrix vp; };

// set 0, bind 1
[[vk::binding(1, 0)]] cbuffer X : register(b0) { float x; };

struct vs_in {
  float3 position : POSITION;
  float3 normal : NORMAL;
  float2 uv0 : TEXCOORD0;
  float2 uv1 : TEXCOORD1;
};

struct vs_out {
  float4 position : SV_Position;
  float4 color : COLOR;
};

vs_out vs_main(vs_in input) {
  vs_out output;
  float4 pos = float4(input.position, 1.0f);
  output.position = mul(pos, vp);
  output.color = float4(1.0, 1.0, 1.0, 1.0);
  return output;
}

float4 ps_main(vs_out input) : SV_Target { return x * input.color; }