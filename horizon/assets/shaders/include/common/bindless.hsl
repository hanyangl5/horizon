
#define MAX_DRAW_COUNT 2048
#define MAX_MATERIAL_COUNT 2048

#define VERETX_LAYOUT_STRIDE 13

float3 GetVertexPositionFromPackedVertexBuffer(float packed_buffer[VERETX_LAYOUT_STRIDE]) {
    return float3(packed_buffer[0], packed_buffer[1], packed_buffer[2]);
}

float3 GetVertexNormalFromPackedVertexBuffer(float packed_buffer[VERETX_LAYOUT_STRIDE]) {
    return float3(packed_buffer[3], packed_buffer[4], packed_buffer[5]);
}

float2 GetVertexUv0FromPackedVertexBuffer(float packed_buffer[VERETX_LAYOUT_STRIDE]) {
    return float2(packed_buffer[6], packed_buffer[7]);
}

float2 GetVertexUv1FromPackedVertexBuffer(float packed_buffer[VERETX_LAYOUT_STRIDE]) {
    return float2(packed_buffer[8], packed_buffer[9]);
}

float3 GetVertexTangentFromPackedVertexBuffer(float packed_buffer[VERETX_LAYOUT_STRIDE]) {
    return float3(packed_buffer[10], packed_buffer[11], packed_buffer[12]);
}
