//--------------------------------------
// Generated from Horizon Shading Language
// 2022-09-26 22:05:17.721748
// "C:\FILES\horizon\horizon\assets\shaders\lit_opaque.frag.hsl"
//--------------------------------------

#define DIRECT3D12
#define STAGE_FRAG
#ifndef _D3D_H
#define _D3D_H

#define UINT_MAX 4294967295
#define FLT_MAX 3.402823466e+38F

inline float2 f2(float x) { return float2(x, x); }
#if !defined(DIRECT3D12)
inline float2 f2(bool x) { return float2(x, x); }
inline float2 f2(int x) { return float2(x, x); }
inline float2 f2(uint x) { return float2(x, x); }
#endif

#define f4(X) float4(X, X, X, X)
#define f3(X) float3(X, X, X)
// #define f2(X) float2(X,X)
#define u4(X) uint4(X, X, X, X)
#define u3(X) uint3(X, X, X)
#define u2(X) uint2(X, X)
#define i4(X) int4(X, X, X, X)
#define i3(X) int3(X, X, X)
#define i2(X) int2(X, X)

#define h4(X) half4(X, X, X, X)

#define short4 int4
#define short3 int3
#define short2 int2
#define short int

#define ushort4 uint4
#define ushort3 uint3
#define ushort2 uint2
#define ushort uint

#if !defined(DIRECT3D12) && !defined(DIRECT3D11)
#define min16float half
#define min16float2 half2
#define min16float3 half3
#define min16float4 half4
#endif

#if defined(DIRECT3D12) || defined(DIRECT3D11)
#define Get(X) X
#else
#define Get(X) srt_##X
#endif

/* Matrix */

// float3x3 f3x3(float4x4 X) { return (float3x3)X; }

#define to_f3x3(M) ((float3x3)M)

// #define f2x3 float3x2
// #define f3x2 float2x3

#define f2x2 float2x2
#define f2x3 float3x2
#define f2x4 float4x2
#define f3x2 float2x3
#define f3x3 float3x3
#define f3x4 float4x3
#define f4x2 float2x4
#define f4x3 float3x4
#define f4x4 float4x4

#define make_f2x2_cols(C0, C1) transpose(f2x2(C0, C1))
#define make_f2x2_rows(R0, R1) f2x2(R0, R1)
#define make_f2x2_col_elems(E00, E01, E10, E11) f2x2(E00, E10, E01, E11)
#define make_f2x3_cols(C0, C1) transpose(f3x2(C0, C1))
#define make_f2x3_rows(R0, R1, R2) f2x3(R0, R1, R2)
#define make_f2x3_col_elems(E00, E01, E10, E11, E20, E21) f2x3(E00, E10, E20, E01, E11, E21)

#define make_f3x3_row_elems f3x3

inline f3x2 make_f3x2_cols(float2 c0, float2 c1, float2 c2) { return transpose(f2x3(c0, c1, c2)); }
// TODO: add all the others

#define make_f3x3_cols(C0, C1, C2) transpose(float3x3(C0, C1, C2))
inline f3x3 make_f3x3_rows(float3 r0, float3 r1, float3 r2) { return f3x3(r0, r1, r2); }

#define make_f4x4_col_elems(E00, E01, E02, E03, E10, E11, E12, E13, E20, E21, E22, E23, E30, E31, E32, E33)            \
    f4x4(E00, E10, E20, E30, E01, E11, E21, E31, E02, E12, E22, E32, E03, E13, E23, E33)
#define make_f4x4_row_elems f4x4

inline f4x4 Identity() {
    return make_f4x4_row_elems(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                               1.0f);
}

inline void setElem(inout f4x4 M, int i, int j, float val) { M[j][i] = val; }
inline float getElem(f4x4 M, int i, int j) { return M[j][i]; }

inline float4 getCol(in f4x4 M, const uint i) { return float4(M[0][i], M[1][i], M[2][i], M[3][i]); }
inline float3 getCol(in f4x3 M, const uint i) { return float3(M[0][i], M[1][i], M[2][i]); }
inline float2 getCol(in f4x2 M, const uint i) { return float2(M[0][i], M[1][i]); }

inline float4 getCol(in f3x4 M, const uint i) { return float4(M[0][i], M[1][i], M[2][i], M[3][i]); }
inline float3 getCol(in f3x3 M, const uint i) { return float3(M[0][i], M[1][i], M[2][i]); }
inline float2 getCol(in f3x2 M, const uint i) { return float2(M[0][i], M[1][i]); }

inline float4 getCol(in f2x4 M, const uint i) { return float4(M[0][i], M[1][i], M[2][i], M[3][i]); }
inline float3 getCol(in f2x3 M, const uint i) { return float3(M[0][i], M[1][i], M[2][i]); }
inline float2 getCol(in f2x2 M, const uint i) { return float2(M[0][i], M[1][i]); }

#define getCol0(M) getCol(M, 0)
#define getCol1(M) getCol(M, 1)
#define getCol2(M) getCol(M, 2)
#define getCol3(M) getCol(M, 3)

#define getRow(M, I) (M)[I]
#define getRow0(M) getRow(M, 0)
#define getRow1(M) getRow(M, 1)
#define getRow2(M) getRow(M, 2)
#define getRow3(M) getRow(M, 3)

inline f4x4 setCol(inout f4x4 M, in float4 col, const uint i) {
    M[0][i] = col[0];
    M[1][i] = col[1];
    M[2][i] = col[2];
    M[3][i] = col[3];
    return M;
}
inline f4x3 setCol(inout f4x3 M, in float3 col, const uint i) {
    M[0][i] = col[0];
    M[1][i] = col[1];
    M[2][i] = col[2];
    ;
    return M;
}
inline f4x2 setCol(inout f4x2 M, in float2 col, const uint i) {
    M[0][i] = col[0];
    M[1][i] = col[1];
    return M;
}

inline f3x4 setCol(inout f3x4 M, in float4 col, const uint i) {
    M[0][i] = col[0];
    M[1][i] = col[1];
    M[2][i] = col[2];
    M[3][i] = col[3];
    return M;
}
inline f3x3 setCol(inout f3x3 M, in float3 col, const uint i) {
    M[0][i] = col[0];
    M[1][i] = col[1];
    M[2][i] = col[2];
    ;
    return M;
}
inline f3x2 setCol(inout f3x2 M, in float2 col, const uint i) {
    M[0][i] = col[0];
    M[1][i] = col[1];
    return M;
}

inline f2x4 setCol(inout f2x4 M, in float4 col, const uint i) {
    M[0][i] = col[0];
    M[1][i] = col[1];
    M[2][i] = col[2];
    M[3][i] = col[3];
    return M;
}
inline f2x3 setCol(inout f2x3 M, in float3 col, const uint i) {
    M[0][i] = col[0];
    M[1][i] = col[1];
    M[2][i] = col[2];
    ;
    return M;
}
inline f2x2 setCol(inout f2x2 M, in float2 col, const uint i) {
    M[0][i] = col[0];
    M[1][i] = col[1];
    return M;
}

#define setCol0(M, C) setCol(M, C, 0)
#define setCol1(M, C) setCol(M, C, 1)
#define setCol2(M, C) setCol(M, C, 2)
#define setCol3(M, C) setCol(M, C, 3)

f4x4 setRow(inout f4x4 M, in float4 row, const uint i) {
    M[i] = row;
    return M;
}
f4x3 setRow(inout f4x3 M, in float4 row, const uint i) {
    M[i] = row;
    return M;
}
f4x2 setRow(inout f4x2 M, in float4 row, const uint i) {
    M[i] = row;
    return M;
}

f3x4 setRow(inout f3x4 M, in float3 row, const uint i) {
    M[i] = row;
    return M;
}
f3x3 setRow(inout f3x3 M, in float3 row, const uint i) {
    M[i] = row;
    return M;
}
f3x2 setRow(inout f3x2 M, in float3 row, const uint i) {
    M[i] = row;
    return M;
}

f2x4 setRow(inout f2x4 M, in float2 row, const uint i) {
    M[i] = row;
    return M;
}
f2x3 setRow(inout f2x3 M, in float2 row, const uint i) {
    M[i] = row;
    return M;
}
f2x2 setRow(inout f2x2 M, in float2 row, const uint i) {
    M[i] = row;
    return M;
}

#define setRow0(M, R) setRow(M, R, 0)
#define setRow1(M, R) setRow(M, R, 1)
#define setRow2(M, R) setRow(M, R, 2)
#define setRow3(M, R) setRow(M, R, 3)

// mapping of glsl format qualifiers
#define rgba8 float4

#define VS_MAIN main
#define PS_MAIN main
#define CS_MAIN main
#define TC_MAIN main
#define TE_MAIN main

#ifdef DIRECT3D12
#define HSL_REG(REG_0, REG_1) REG_1
#else
#define HSL_REG(REG_0, REG_1) REG_0
#endif

#ifdef RETURN_TYPE
#define INIT_MAIN RETURN_TYPE Out
#define RETURN return Out
#else
#define INIT_MAIN
#define RETURN return
#endif

// #if defined(DIRECT3D12) || defined(DIRECT3D11)
//     #define HSL_VertexID(NAME)         uint  NAME : SV_VertexID
//     #define SV_InstanceID(NAME)        uint  NAME : SV_InstanceID
//     #define HSL_GroupID(NAME)          uint3 NAME : SV_GroupID
//     #define HSL_DispatchThreadID(NAME) uint3 NAME : SV_DispatchThreadID
//     #define HSL_GroupThreadID(NAME)    uint3 NAME : SV_GroupThreadID
//     #define HSL_GroupIndex(NAME)       uint  NAME : SV_GroupIndex
//     #define HSL_SampleIndex(NAME)      uint  NAME : SV_SampleIndex
//     #define HSL_PrimitiveID(NAME)      uint  NAME : SV_PrimitiveID
// #endif

#define SV_PointSize NONE

#define packed_float3 float3

// #define DDX ddx
// #define DDY ddy

// bool greaterThanEqual(float2 a, float b)
// {
//     return all(a >= float2(b, b));
// }
// bool greaterThan(float2 a, float b)
// {
//     return all(a > float2(b, b));
// }

bool2 And(const bool2 a, const bool2 b) { return a && b; }

#define _GREATER_THAN(TYPE)                                                                                            \
    // bool2 GreaterThan(const TYPE##2 a, const TYPE b) { return a > b; } \
// bool3 GreaterThan(const TYPE##3 a, const TYPE b) { return a > b; } \
// bool4 GreaterThan(const TYPE##4 a, const TYPE b) { return a > b; } \
// bool2 GreaterThan(const TYPE a, const TYPE##2 b) { return a > b; } \
// bool3 GreaterThan(const TYPE a, const TYPE##3 b) { return a > b; } \
// bool4 GreaterThan(const TYPE a, const TYPE##4 b) { return a > b; }                                            \
    // _GREATER_THAN(float)
    // _GREATER_THAN(int)
    // bool4 GreaterThan(const float4 a, const float4 b) { return a > b; }

#define GreaterThan(A, B) ((A) > (B))
#define GreaterThanEqual(A, B) ((A) >= (B))
#define LessThan(A, B) ((A) < (B))
#define LessThanEqual(A, B) ((A) <= (B))

#define AllGreaterThan(X, Y) all(GreaterThan(X, Y))
#define AllGreaterThanEqual(X, Y) all(GreaterThanEqual(X, Y))
#define AllLessThan(X, Y) all(LessThan(X, Y))
#define AllLessThanEqual(X, Y) all(LessThanEqual((X), (Y)))

#define AnyGreaterThan(X, Y) any(GreaterThan(X, Y))
#define AnyGreaterThanEqual(X, Y) any(GreaterThanEqual(X, Y))
#define AnyLessThan(X, Y) any(LessThan(X, Y))
#define AnyLessThanEqual(X, Y) any(LessThanEqual((X), (Y)))

#define select lerp

#define fast_min min
#define fast_max max
#define isordered(X, Y) (((X) == (X)) && ((Y) == (Y)))
#define isunordered(X, Y) (isnan(X) || isnan(Y))

uint extract_bits(uint src, uint off, uint bits) {
    uint mask = (1u << bits) - 1;
    return (src >> off) & mask;
} // ABfe
// https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/bfi---sm5---asm-
uint insert_bits(uint src, uint ins, uint off, uint bits) {
    uint bitmask = (((1u << bits) - 1) << off) & 0xffffffff;
    return ((ins << off) & bitmask) | (src & ~bitmask);
} // ABfiM

#define Equal(X, Y) ((X) == (Y))

#define row_major(X) X

#if defined(ORBIS) || defined(PROSPERO)
#define NonUniformResourceIndex(X) (X)
#endif

// #if defined(DIRECT3D12) || defined(DIRECT3D11)
//     #define GroupMemoryBarrier GroupMemoryBarrierWithGroupSync
//     #define AllMemoryBarrier AllMemoryBarrierWithGroupSync
// #else
#undef GroupMemoryBarrier
#define GroupMemoryBarrier GroupMemoryBarrierWithGroupSync
#undef AllMemoryBarrier
#define AllMemoryBarrier AllMemoryBarrierWithGroupSync
// #endif

#define _DECL_FLOAT_TYPES(EXPR)                                                                                        \
    EXPR(half)                                                                                                         \
    EXPR(half2)                                                                                                        \
    EXPR(half3)                                                                                                        \
    EXPR(half4)                                                                                                        \
    EXPR(float)                                                                                                        \
    EXPR(float2)                                                                                                       \
    EXPR(float3)                                                                                                       \
    EXPR(float4)

#define _DECL_TYPES(EXPR)                                                                                              \
    EXPR(int)                                                                                                          \
    EXPR(int2)                                                                                                         \
    EXPR(int3)                                                                                                         \
    EXPR(int4)                                                                                                         \
    EXPR(uint)                                                                                                         \
    EXPR(uint2)                                                                                                        \
    EXPR(uint3)                                                                                                        \
    EXPR(uint4)                                                                                                        \
    EXPR(half)                                                                                                         \
    EXPR(half2)                                                                                                        \
    EXPR(half3)                                                                                                        \
    EXPR(half4)                                                                                                        \
    EXPR(float)                                                                                                        \
    EXPR(float2)                                                                                                       \
    EXPR(float3)                                                                                                       \
    EXPR(float4)

#define _DECL_SCALAR_TYPES(EXPR)                                                                                       \
    EXPR(int)                                                                                                          \
    EXPR(uint)                                                                                                         \
    EXPR(half)                                                                                                         \
    EXPR(float)

#define atomic_uint uint

#if defined(DIRECT3D12) || defined(DIRECT3D11)
#define GetRes(X) X
#else
#define GetRes(X) srt_##X
#endif

#if defined(DIRECT3D12) || defined(DIRECT3D11)
// #define _DECL_AtomicAdd(TYPE) \
// inline void AtomicAdd(inout TYPE dst, TYPE value, out TYPE original_val) \
// { InterlockedAdd(dst, value, original_val); }
// _DECL_AtomicAdd(int)
// _DECL_AtomicAdd(uint)
#define AtomicAdd(DEST, VALUE, ORIGINAL_VALUE) InterlockedAdd(DEST, VALUE, ORIGINAL_VALUE)
#endif

// #define AtomicStore(DEST, VALUE) \
//     (DEST) = (VALUE)

#define _DECL_AtomicStore(TYPE)                                                                                        \
    inline void AtomicStore(inout TYPE dst, TYPE value) { dst = value; }
_DECL_TYPES(_DECL_AtomicStore)

#define AtomicLoad(SRC) SRC

#define AtomicExchange(DEST, VALUE, ORIGINAL_VALUE) InterlockedExchange((DEST), (VALUE), (ORIGINAL_VALUE))

#if defined(DIRECT3D12) || defined(ORBIS) || defined(PROSPERO)
#define inout(T) inout T
#define out(T) out T
#define in(T) in T
#define inout_array(T, X) inout T X
#define out_array(T, X) out T X
#define in_array(T, X) in T X
#undef groupshared
#define groupshared(T) inout T
#else
// fxc macro expansion workaround
#define inout_float inout float
#define inout_float2 inout float2
#define inout_float3 inout float3
#define inout_float4 inout float4
#define inout_uint inout uint
#define inout_uint2 inout uint2
#define inout_uint3 inout uint3
#define inout_uint4 inout uint4
#define inout_int inout int
#define inout_int2 inout int2
#define inout_int3 inout int3
#define inout_int4 inout int4
#define inout(T) inout_##T

#define out_float out float
#define out_float2 out float2
#define out_float3 out float3
#define out_float4 out float4
#define out_uint out uint
#define out_uint2 out uint2
#define out_uint3 out uint3
#define out_uint4 out uint4
#define out_int out int
#define out_int2 out int2
#define out_int3 out int3
#define out_int4 out int4
#define out(T) out_##T

#define in_float in float
#define in_float2 in float2
#define in_float3 in float3
#define in_float4 in float4
#define in_uint in uint
#define in_uint2 in uint2
#define in_uint3 in uint3
#define in_uint4 in uint4
#define in_int in int
#define in_int2 in int2
#define in_int3 in int3
#define in_int4 in int4
#define in(T) in_##T

#define groupshared(T) inout_##T

#endif

#define NUM_THREADS(X, Y, Z) [numthreads(X, Y, Z)]

#define UPDATE_FREQ_NONE space0
#define UPDATE_FREQ_PER_FRAME space1
#define UPDATE_FREQ_PER_BATCH space2
#define UPDATE_FREQ_PER_DRAW space3
#define UPDATE_FREQ_USER UPDATE_FREQ_NONE

#define FLAT(X) nointerpolation X

#define STRUCT(NAME) struct NAME

#define DATA(TYPE, NAME, SEM) TYPE NAME : SEM

#define ByteBuffer ByteAddressBuffer
#define RWByteBuffer RWByteAddressBuffer
#define WByteBuffer RWByteAddressBuffer

inline int LoadByte(ByteBuffer buff, int address) { return buff.Load(address); }
inline int4 LoadByte4(ByteBuffer buff, int address) { return buff.Load4(address); }
inline int LoadByte(RWByteBuffer buff, int address) { return buff.Load(address); }
inline int4 LoadByte4(RWByteBuffer buff, int address) { return buff.Load4(address); }

inline void StoreByte(RWByteBuffer buff, int address, int val) { buff.Store(address, val); }

#define _DECL_SampleLvlTexCube(TYPE)                                                                                   \
    inline TYPE SampleLvlTexCube(TextureCube<TYPE> tex, SamplerState smp, float3 p, float l) {                         \
        return tex.SampleLevel(smp, p, l);                                                                             \
    }
_DECL_FLOAT_TYPES(_DECL_SampleLvlTexCube)
// _DECL_SampleLvlTexCube(float)
// _DECL_SampleLvlTexCube(float2)
// _DECL_SampleLvlTexCube(float3)
// _DECL_SampleLvlTexCube(float4)

// #define SampleLvlTexCube(NAME, SAMPLER, COORD, LEVEL) NAME.SampleLevel(SAMPLER, COORD, LEVEL)
// #define SampleLvlTex2D(NAME, SAMPLER, COORD, LEVEL) NAME.SampleLevel(SAMPLER, COORD, LEVEL)
#define _DECL_SampleLvlTex2D(TYPE)                                                                                     \
    inline TYPE SampleLvlTex2D(Texture2D<TYPE> tex, SamplerState smp, float2 p, float l) {                             \
        return tex.SampleLevel(smp, p, l);                                                                             \
    }
_DECL_FLOAT_TYPES(_DECL_SampleLvlTex2D)

// #define SampleLvlTex3D(NAME, SAMPLER, COORD, LEVEL) NAME.SampleLevel(SAMPLER, COORD, LEVEL)
#define _DECL_SampleLvlTex3D(TYPE)                                                                                     \
    inline TYPE SampleLvlTex3D(Texture3D<TYPE> tex, SamplerState smp, float3 p, float l) {                             \
        return tex.SampleLevel(smp, p, l);                                                                             \
    }
_DECL_FLOAT_TYPES(_DECL_SampleLvlTex3D)

// #define SampleLvlOffsetTex2D(NAME, SAMPLER, COORD, LEVEL, OFFSET) NAME.SampleLevel(SAMPLER, COORD, LEVEL, OFFSET)
#define SampleLvlOffsetTex3D(NAME, SAMPLER, COORD, LEVEL, OFFSET) NAME.SampleLevel(SAMPLER, COORD, LEVEL, OFFSET)

#define _DECL_SampleLvlOffsetTex2D(TYPE)                                                                               \
    inline TYPE SampleLvlOffsetTex2D(Texture2D<TYPE> tex, SamplerState smp, float2 p, float l, int2 o) {               \
        return tex.SampleLevel(smp, p, l, o);                                                                          \
    }
_DECL_FLOAT_TYPES(_DECL_SampleLvlOffsetTex2D)
// _DECL_SampleLvlOffsetTex2D(float)
// _DECL_SampleLvlOffsetTex2D(float2)
// _DECL_SampleLvlOffsetTex2D(float3)
// _DECL_SampleLvlOffsetTex2D(float4)

float4 _to4(in float4 x) { return x; }
float4 _to4(in float3 x) { return float4(x, 0); }
float4 _to4(in float2 x) { return float4(x, 0, 0); }
float4 _to4(in float x) { return float4(x, 0, 0, 0); }

// #ifdef ORBIS
// #define LoadTex2D(TEX, SMP, P) ((TEX)[P])
// #else
// inline TYPE LoadTex2D(RWTexture2D<TYPE> tex, SamplerState smp, int2 p) { return tex.Load(p); }
// #if 0 && (defined(DIRECT3D12) || defined(DIRECT3D11))
// #define _DECL_LoadTex2D(TYPE) \
// inline TYPE LoadTex2D(Texture2D<TYPE>   tex, SamplerState smp, int2 p) { return tex.Load(int3(p, 0)); } \
// inline TYPE LoadTex2D(RWTexture2D<TYPE> tex, SamplerState smp, int2 p) { return tex[p]; } \
// inline TYPE LoadTex2D(Texture2D<TYPE>   tex, int _, int2 p) { return tex.Load(int3(p, 0)); } \
// inline TYPE LoadTex2D(RWTexture2D<TYPE> tex, int _, int2 p) { return tex[p]; }
// #else
// #define _DECL_LoadTex2D(TYPE) \
// inline TYPE LoadTex2D(Texture2D<TYPE>   tex, SamplerState smp, int2 p) { return tex[p]; } \
// inline TYPE LoadTex2D(RWTexture2D<TYPE> tex, SamplerState smp, int2 p) { return tex[p]; } \
// inline TYPE LoadTex2D(Texture2D<TYPE>   tex, int _, int2 p) { return tex[p]; } \
// inline TYPE LoadTex2D(RWTexture2D<TYPE> tex, int _, int2 p) { return tex[p]; }
// #endif

#define _DECL_LoadTex2D(TYPE)                                                                                          \
    inline TYPE LoadTex2D(Texture2D<TYPE> tex, SamplerState smp, int2 p, int lod) { return tex.Load(int3(p, lod)); }   \
    inline TYPE LoadTex2D(Texture2D<TYPE> tex, int _, int2 p, int lod) { return tex.Load(int3(p, lod)); }              \
    inline TYPE LoadRWTex2D(RWTexture2D<TYPE> tex, int2 p) { return tex[p]; }
_DECL_TYPES(_DECL_LoadTex2D)

#if defined(DIRECT3D12)
#define _DECL_LoadRasterizerOrderedTexture2D(TYPE)                                                                     \
    inline TYPE LoadRWTex2D(RasterizerOrderedTexture2D<TYPE> tex, int2 p) { return tex[p]; }
_DECL_TYPES(_DECL_LoadRasterizerOrderedTexture2D)
#endif

#define _DECL_LoadTex3D(TYPE)                                                                                          \
    inline TYPE LoadTex3D(Texture2DArray<TYPE> tex, SamplerState smp, int3 p, int lod) {                               \
        return tex.Load(int4(p, lod));                                                                                 \
    }                                                                                                                  \
    inline TYPE LoadTex3D(Texture3D<TYPE> tex, SamplerState smp, int3 p, int lod) { return tex.Load(int4(p, lod)); }   \
    inline TYPE LoadTex3D(Texture2DArray<TYPE> tex, int _, int3 p, int lod) { return tex.Load(int4(p, lod)); }         \
    inline TYPE LoadTex3D(Texture3D<TYPE> tex, int _, int3 p, int lod) { return tex.Load(int4(p, lod)); }              \
    inline TYPE LoadRWTex3D(RWTexture3D<TYPE> tex, int3 p) { return tex[p]; }                                          \
    inline TYPE LoadRWTex3D(RWTexture2DArray<TYPE> tex, int3 p) { return tex[p]; }
_DECL_TYPES(_DECL_LoadTex3D)

#define LoadTex2DMS(NAME, SAMPLER, COORD, SMP) NAME.Load(COORD, SMP)
#define LoadTex2DArrayMS(NAME, SAMPLER, COORD, SMP) NAME.Load(COORD, SMP)

#define LoadLvlOffsetTex2D(TEX, SMP, P, L, O) (TEX).Load(int3((P).xy, L), O)

// #define SampleGradTex2D(NAME, SAMPLER, COORD, DX, DY) (NAME).SampleGrad( (SAMPLER), (COORD), (DX), (DY) )
#define _DECL_SampleGradTex2D(TYPE)                                                                                    \
    inline TYPE SampleGradTex2D(Texture2D<TYPE> tex, SamplerState smp, float2 p, float2 dx, float2 dy) {               \
        return tex.SampleGrad(smp, p, dx, dy);                                                                         \
    }
_DECL_FLOAT_TYPES(_DECL_SampleGradTex2D)

#define GatherRedTex2D(NAME, SAMPLER, COORD) (NAME).GatherRed((SAMPLER), (COORD))

// #define SampleTexCube(NAME, SAMPLER, COORD) (NAME).Sample( (SAMPLER), (COORD) )

#define _DECL_SampleTexCube(TYPE)                                                                                      \
    inline TYPE SampleTexCube(TextureCube<TYPE> tex, SamplerState smp, float3 p) { return tex.Sample(smp, p); }
_DECL_FLOAT_TYPES(_DECL_SampleTexCube)

#define _DECL_SampleTex2D(TYPE)                                                                                        \
    inline TYPE SampleTex2D(Texture2D<TYPE> tex, SamplerState smp, float2 p) { return tex.Sample(smp, p); }
_DECL_FLOAT_TYPES(_DECL_SampleTex2D)

#define _DECL_SampleTex1D(TYPE)                                                                                        \
    inline TYPE SampleTex1D(Texture1D<TYPE> tex, SamplerState smp, float p) { return tex.Sample(smp, p); }
_DECL_FLOAT_TYPES(_DECL_SampleTex1D)

#define _DECL_SampleTex2DProj(TYPE)                                                                                    \
    inline TYPE SampleTex2DProj(Texture2D<TYPE> tex, SamplerState smp, float4 p) { return tex.Sample(smp, p.xy / p.w); }
_DECL_FLOAT_TYPES(_DECL_SampleTex2DProj)

#define _DECL_SampleTex2DArray(TYPE)                                                                                   \
    inline TYPE SampleTex2DArray(Texture2DArray<TYPE> tex, SamplerState smp, float3 p) { return tex.Sample(smp, p); }
_DECL_FLOAT_TYPES(_DECL_SampleTex2DArray)

// inline float4 SampleTex2D(Texture2D<float4> tex, SamplerState smp, float2 p)
// { return tex.Sample(smp, p); }

// #define SampleTex1D(NAME, SAMPLER, COORD) NAME.Sample(SAMPLER, COORD)
// #define SampleTex2D SampleTex1D
// #define SampleTex3D SampleTex1D
// #define SampleTex2DArray SampleTex1D

// #define CmpLvl0Tex2D(TEX, SMP, PC) (TEX).SampleCmpLevelZero(SMP, PC.xy, PC.z)

#define _DECL_CompareTex2D(TYPE)                                                                                       \
    inline TYPE CompareTex2D(Texture2D<TYPE> tex, SamplerComparisonState smp, float3 p) {                              \
        return tex.SampleCmpLevelZero(smp, p.xy, p.z);                                                                 \
    }
_DECL_CompareTex2D(float)

#define _DECL_CompareTex2DProj(TYPE)                                                                                   \
    inline float CompareTex2DProj(Texture2D<TYPE> tex, SamplerComparisonState smp, float4 p) {                         \
        return tex.SampleCmpLevelZero(smp, p.xy / p.w, p.z / p.w);                                                     \
    }
    _DECL_FLOAT_TYPES(_DECL_CompareTex2DProj)

// inline void Write2D(RWTexture2D<float4> tex, int2 p, float4 val)
// { tex[p] = val; }

#define _DECL_Write2D(TYPE)                                                                                            \
    inline void Write2D(RWTexture2D<TYPE> tex, int2 p, TYPE val) { tex[p] = val; }
        _DECL_TYPES(_DECL_Write2D)

#if defined(DIRECT3D12)
#define _DECL_WriteRasterizerOrderedTexture2D(TYPE)                                                                    \
    inline void Write2D(RasterizerOrderedTexture2D<TYPE> tex, int2 p, TYPE val) { tex[p] = val; }
            _DECL_TYPES(_DECL_WriteRasterizerOrderedTexture2D)
#endif

#define _DECL_Write3D(TYPE)                                                                                            \
    inline void Write3D(RWTexture3D<TYPE> tex, int3 p, TYPE val) { tex[p] = val; }                                     \
    inline void Write3D(RWTexture2DArray<TYPE> tex, int3 p, TYPE val) { tex[p] = val; }
                _DECL_TYPES(_DECL_Write3D)

#define Load2D(TEX, P) (TEX[(P)])
#define Load3D(TEX, P) (TEX[(P)])

// #define _DECL_Load2D(TYPE) \
// inline TYPE Load2D(Texture2D<TYPE> tex,   SamplerState smp, int2 p) { return tex[p]; }
// _DECL_TYPES(_DECL_Load2D)

#define residency_status uint

// the 0 in Sample() is an optional offset
#define SampleClampedSparseTex2D(TEX, SMP, P, L, RC) ((TEX).Sample((SMP), (P), 0, L, RC))
#define CalculateLvlTex2D(TEX, SMP, P) ((TEX).CalculateLevelOfDetailUnclamped(SMP, P))
#define IsFullyMapped(RC) CheckAccessFullyMapped(RC)

// void Write2D(RWTexture2D<float>  dst, int2 coord, float4 val) { dst[coord] = val.x;    }
// void Write2D(RWTexture2D<float2> dst, int2 coord, float4 val) { dst[coord] = val.xy;   }
// void Write2D(RWTexture2D<float3> dst, int2 coord, float4 val) { dst[coord] = val.xyz;  }
// void Write2D(RWTexture2D<float4> dst, int2 coord, float4 val) { dst[coord] = val.xyzw; }

// void Write3D(RWTexture3D<float>  dst, int3 coord, float4 val) { dst[coord] = val.x;    }
// void Write3D(RWTexture3D<float2> dst, int3 coord, float4 val) { dst[coord] = val.xy;   }
// void Write3D(RWTexture3D<float3> dst, int3 coord, float4 val) { dst[coord] = val.xyz;  }
// void Write3D(RWTexture3D<float4> dst, int3 coord, float4 val) { dst[coord] = val.xyzw; }

// void Write3D(RWTexture2DArray<float>  dst, int3 coord, float4 val) { dst[coord] = val.x;    }
// void Write3D(RWTexture2DArray<float2> dst, int3 coord, float4 val) { dst[coord] = val.xy;   }
// void Write3D(RWTexture2DArray<float3> dst, int3 coord, float4 val) { dst[coord] = val.xyz;  }
// void Write3D(RWTexture2DArray<float4> dst, int3 coord, float4 val) { dst[coord] = val.xyzw; }
// void Write3D(RWTexture2DArray<uint>   dst, int3 coord, uint4  val) { dst[coord] = val.x;    }

// #define AtomicMin3D( DST, COORD, VALUE, ORIGINAL_VALUE ) (InterlockedMin((DST)[uint3((COORD).xyz)], (VALUE),
// (ORIGINAL_VALUE)))
#define _DECL_AtomicMin3D(TYPE)                                                                                        \
    inline void AtomicMin3D(RWTexture3D<TYPE> tex, int3 p, TYPE val, out TYPE original_val) {                          \
        InterlockedMin(tex[p], val, original_val);                                                                     \
    }                                                                                                                  \
    inline void AtomicMin3D(RWTexture2DArray<TYPE> tex, int3 p, TYPE val, out TYPE original_val) {                     \
        InterlockedMin(tex[p], val, original_val);                                                                     \
    }
                    _DECL_AtomicMin3D(uint)

// #define _DECL_AtomicMin2DArray(TYPE) \
// inline void AtomicMin2DArray(RWTexture2DArray<TYPE> tex, int2 p, int layer, TYPE val, out TYPE original_val) \
// { InterlockedMin(tex[int3(p, layer)], val, original_val); }
// _DECL_AtomicMin2DArray(uint)

#if defined(DIRECT3D12)
#define AtomicMin InterlockedMin
#define AtomicMax InterlockedMax
#endif

// #ifndef ORBIS
// #if !defined(ORBIS) && !defined(PROSPERO)
// #endif

// #define WRITE2D(NAME, COORD, VAL) NAME[int2(COORD.xy)] = VAL
#define WRITE2D(NAME, COORD, VAL) write2D(NAME, COORD.xy, VAL)
#define WRITE3D(NAME, COORD, VAL) write3D(NAME, COORD.xyz, VAL)

#define UNROLL_N(X) [unroll(X)]
#define UNROLL [unroll]
#define LOOP [loop]
#define FLATTEN [flatten]

#if defined(ORBIS) || defined(PROSPERO)
#define PUSH_CONSTANT(NAME, REG) struct NAME
#else
#define PUSH_CONSTANT(NAME, REG) cbuffer NAME : register(REG)
#endif

#if defined(ORBIS) || defined(PROSPERO)
#undef Buffer
#undef RWBuffer
#endif

#define Buffer(TYPE) StructuredBuffer<TYPE>
#define RWBuffer(TYPE) RWStructuredBuffer<TYPE>
#define WBuffer(TYPE) RWStructuredBuffer<TYPE>

#if defined(DIRECT3D12)
#define RWCoherentBuffer(TYPE) globallycoherent RWBuffer(TYPE)
#else
#define RWCoherentBuffer(TYPE) RWBuffer(TYPE)
#endif

#define NO_SAMPLER 0u
#if defined(DIRECT3D12)
                        inline int2 GetDimensions(RasterizerOrderedTexture2D<uint> t, int _) {
    uint2 d;
    t.GetDimensions(d.x, d.y);
    return d;
}
#endif
inline int2 GetDimensions(Texture2D t, int _) {
    uint2 d;
    t.GetDimensions(d.x, d.y);
    return d;
}
inline int2 GetDimensions(Texture2D t, SamplerState smp) { return GetDimensions(t, NO_SAMPLER); }
inline int2 GetDimensions(Texture2D<uint> t, int _) {
    uint2 d;
    t.GetDimensions(d.x, d.y);
    return d;
}
inline int2 GetDimensions(Texture2D<uint> t, SamplerState smp) { return GetDimensions(t, NO_SAMPLER); }
inline int2 GetDimensions(Texture2D<float> t, int _) {
    uint2 d;
    t.GetDimensions(d.x, d.y);
    return d;
}
inline int2 GetDimensions(Texture2D<float> t, SamplerState smp) { return GetDimensions(t, NO_SAMPLER); }
inline int2 GetDimensions(RWTexture2D<uint> t, int _) {
    uint2 d;
    t.GetDimensions(d.x, d.y);
    return d;
}
inline int2 GetDimensions(RWTexture2D<uint> t, SamplerState smp) { return GetDimensions(t, NO_SAMPLER); }
inline int2 GetDimensions(RWTexture2D<float> t, int _) {
    uint2 d;
    t.GetDimensions(d.x, d.y);
    return d;
}
inline int2 GetDimensions(RWTexture2D<float> t, SamplerState smp) { return GetDimensions(t, NO_SAMPLER); }
inline int2 GetDimensions(RWTexture2D<float4> t, int _) {
    uint2 d;
    t.GetDimensions(d.x, d.y);
    return d;
}
inline int2 GetDimensions(RWTexture2D<float4> t, SamplerState smp) { return GetDimensions(t, NO_SAMPLER); }
inline int2 GetDimensions(TextureCube t, int _) {
    uint2 d;
    t.GetDimensions(d.x, d.y);
    return d;
}
inline int2 GetDimensions(TextureCube t, SamplerState smp) { return GetDimensions(t, NO_SAMPLER); }
// #define GetDimensions(TEX, SAMPLER) _GetDimensions(TEX)

// int2 GetDimensions(Texture2D t, SamplerState s)
// { uint2 d; t.GetDimensions(d[0], d[1]); return d; }
// int2 GetDimensions(TextureCube t, SamplerState s)
// { uint2 d; t.GetDimensions(d[0], d[1]); return d; }

#define TexCube(ELEM_TYPE) TextureCube<ELEM_TYPE>

#define Tex1D(ELEM_TYPE) Texture1D<ELEM_TYPE>
#define Tex2D(ELEM_TYPE) Texture2D<ELEM_TYPE>
#define Tex3D(ELEM_TYPE) Texture3D<ELEM_TYPE>

#if defined(DIRECT3D11)
#define Tex2DMS(ELEM_TYPE, SMP_CNT) Texture2DMS<ELEM_TYPE>
#else
#define Tex2DMS(ELEM_TYPE, SMP_CNT) Texture2DMS<ELEM_TYPE, SMP_CNT>
#endif

#define Tex1DArray(ELEM_TYPE) Texture1DArray<ELEM_TYPE>
#define Tex2DArray(ELEM_TYPE) Texture2DArray<ELEM_TYPE>

#define RWTex1D(ELEM_TYPE) RWTexture1D<ELEM_TYPE>
#define RWTex2D(ELEM_TYPE) RWTexture2D<ELEM_TYPE>
#define RWTex3D(ELEM_TYPE) RWTexture3D<ELEM_TYPE>

#define RWTex1DArray(ELEM_TYPE) RWTexture1DArray<ELEM_TYPE>
#define RWTex2DArray(ELEM_TYPE) RWTexture2DArray<ELEM_TYPE>

#define WTex1D RWTex1D
#define WTex2D RWTex2D
#define WTex3D RWTex3D
#define WTex1DArray RWTex1DArray
#define WTex2DArray RWTex2DArray

#define RTex1D RWTex1D
#define RTex2D RWTex2D
#define RTex3D RWTex3D
#define RTex1DArray RWTex1DArray
#define RTex2DArray RWTex2DArray

#ifdef DIRECT3D12
#define RasterizerOrderedTex2D(ELEM_TYPE) RasterizerOrderedTexture2D<ELEM_TYPE>
#else
#define RasterizerOrderedTex2D RWTex2D
#endif

#define Depth2D Tex2D
#define Depth2DMS Tex2DMS

#define SHADER_CONSTANT(INDEX, TYPE, NAME, VALUE) const TYPE NAME = VALUE

#define HSL_CONST(TYPE, NAME) static const TYPE NAME
#define STATIC static
#define INLINE inline

#define ToFloat3x3(NAME) ((float3x3)NAME)

#if defined(DIRECT3D12)
#define CBUFFER(NAME, FREQ, REG, BINDING) cbuffer NAME : register(REG, FREQ)
#define RES(TYPE, NAME, FREQ, REG, BINDING) TYPE NAME : register(REG, FREQ)
#elif defined(ORBIS) || defined(PROSPERO)
#define CBUFFER(NAME, FREQ, REG, BINDING) struct NAME
#define RES(TYPE, NAME, FREQ, REG, BINDING)
#else
#define CBUFFER(NAME, FREQ, REG, BINDING) cbuffer NAME : register(REG)
#define RES(TYPE, NAME, FREQ, REG, BINDING) TYPE NAME : register(REG)
#endif

#define EARLY_FRAGMENT_TESTS [earlydepthstencil]

// tesselation
#define TESS_VS_SHADER(X)

#define PCF_INIT
#define INPUT_PATCH(T, NC) InputPatch<T, NC>
#define OUTPUT_PATCH(T, NC) OutputPatch<T, NC>
#define HSL_OutputControlPointID(N) uint N : SV_OutputControlPointID
#define PATCH_CONSTANT_FUNC(F) [patchconstantfunc(F)]
#define OUTPUT_CONTROL_POINTS(P) [outputcontrolpoints(P)]
#define MAX_TESS_FACTOR(F) [maxtessfactor(F)]
#define HSL_DomainPartitioning(X, Y) [domain(X)][partitioning(Y)]
#define HSL_OutputTopology(T) [outputtopology(T)]

#ifdef STAGE_TESE
#define TESS_LAYOUT(D, P, T) [domain(D)]
#endif

#ifdef STAGE_TESC
#define TESS_LAYOUT(D, P, T) [domain(D)][partitioning(P)][outputtopology(T)]
#endif

#if defined(DIRECT3D12) || defined(DIRECT3D11)
#define HSL_DomainLocation(N) float3 N : SV_DomainLocation
#else
#define HSL_DomainLocation(N) float2 N : SV_DomainLocation
#endif

#ifdef ENABLE_WAVEOPS

#define WaveGetMaxActiveIndex() WaveActiveMax(WaveGetLaneIndex())

#if !defined(ballot_t)
#define ballot_t uint4
#endif

#if !defined(CountBallot)
#define CountBallot(B) (countbits((B).x) + countbits((B).y) + countbits((B).z) + countbits((B).w))
#endif

#endif

#ifdef DIRECT3D11
#define EnablePSInterlock
#define BeginPSInterlock
#define EndPSInterlock
#else
#define EnablePSInterlock()
#define BeginPSInterlock()
#define EndPSInterlock()
#endif

#ifndef STAGE_VERT
#define VR_VIEW_ID(VID) (0)
#else
#define VR_VIEW_ID 0
#endif
#define VR_MULTIVIEW_COUNT 1

#endif // _D3D_H

#line 1 "C:/FILES/horizon/horizon/assets/shaders/lit_opaque.frag.hsl"
#line 1 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/material_params_defination.hsl"
#define HAS_BASE_COLOR 0x01
#define HAS_NORMAL 0x10
#define HAS_METALLIC_ROUGHNESS  0x100
#define HAS_EMISSIVE 0x1000
#define HAS_ALPHA 0x10000

struct MaterialProperties{
    float3 albedo;
    float3 normal;
    float3 f0;
    float metallic;
    float roughness;
    float roughness2;
    float3 emissive;
};
#line 2 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/lit_opaque.frag.hsl"
#line 1 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/light_defination.h"
struct LightParams{
    float4 color_intensity; // r, g, b, intensity
    float4 position_type; // x, y, z, type
    float4 direction;
    float4 radius_inner_outer; // radius, innerradius, outerradius
};

#define MAX_DYNAMIC_LIGHT_COUNT 256

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2
#line 3 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/lit_opaque.frag.hsl"
#line 1 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/lighting.h"
#line 1 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/shading_models.h"
#line 1 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/brdf.h"
#line 1 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/../common/common_math.h"
#define PI 3.1415926535897932384626433f
#define HALF_PI 1.57079632679489661923f

#define POW_CLAMP 0.000001f

float Pow2(float x) {return x * x;}

float Pow3(float x) {return x * x * x;}

float Pow4(float x) {return x * x * x * x;}

float Pow5(float x) {return x * x * x * x * x;}

#line 2 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/brdf.h"
#line 1 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/../common/fastmath.hsl"

/*
    horizon fast math lib
*/

/******************************************************************************
    Shader Fast Math Lib (v0.41)
    A shader math library for optimized approximate transcendental functions.
    Optimized and tested on AMD GCN architecture.
********************************************************************************/

/******************************************************************************
    The MIT License (MIT)

    Copyright (c) <2014> <Michal Drobot>

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
********************************************************************************/

#line 1 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/../common/common_math.h"

#line 37 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/../common/fastmath.hsl"

// utils

float sqrtIEEEIntApproximation(float x, const int inSqrtConst)
{
	int _x = asint(x);
	_x = inSqrtConst + (_x >> 1);
	return asfloat(_x);
}

// Approximate guess using integer float arithmetics based on IEEE floating point standard
float rcpSqrtIEEEIntApproximation(float x, const int inRcpSqrtConst)
{
	int _x = asint(x);
	_x = inRcpSqrtConst - (_x >> 1);
	return asfloat(_x);
}

float rcpSqrtNewtonRaphson(float xHalf, float inRcpX)
{
	return inRcpX * (-xHalf * (inRcpX * inRcpX) + 1.5f);
}

float rcpIEEEIntApproximation(float x, const int inRcpConst)
{
	int _x = asint(x);
	_x = inRcpConst - _x;
	return asfloat(_x);
}

float rcpNewtonRaphson(float x, float inRcpX)
{
	return inRcpX * (-inRcpX * x + 2.0f);
}

// ------------------------------------------------------------
// rsqrt

//
// Using 0 Newton Raphson iterations
// Relative error : ~3.4% over full
// Precise format : ~small float
// 2 ALU
//
float RSqrtFast0(float x) {
    return rcpSqrtIEEEIntApproximation(x, 0x5f3759df);
}

//
// Using 1 Newton Raphson iterations
// Relative error : ~0.2% over full
// Precise format : ~half float
// 6 ALU
//
float RSqrtFast1(float x) {
	float  xhalf = 0.5f * x;
	float  xRcpSqrt = rcpSqrtIEEEIntApproximation(x, 0x5F375A86);
	xRcpSqrt = rcpSqrtNewtonRaphson(xhalf, xRcpSqrt);
	return xRcpSqrt;
}

//
// Using 2 Newton Raphson iterations
// Relative error : ~4.6e-004%  over full
// Precise format : ~full float
// 9 ALU
//
float RSqrtFast2(float x) {
	float  xhalf = 0.5f * x;
	float  xRcpSqrt = rcpSqrtIEEEIntApproximation(x, 0x5F375A86);
	xRcpSqrt = rcpSqrtNewtonRaphson(xhalf, xRcpSqrt);
	xRcpSqrt = rcpSqrtNewtonRaphson(xhalf, xRcpSqrt);
	return xRcpSqrt;
}

// ----------------------------------------------

// Using 0 Newton Raphson iterations
// Relative error : < 0.7% over full
// Precise format : ~small float
// 1 ALU
//
float SqrtFast0(float x) {
    return sqrtIEEEIntApproximation(x, 0x1FBD1DF5);
}

//
// Use inverse Rcp Sqrt
// Using 1 Newton Raphson iterations
// Relative error : ~0.2% over full
// Precise format : ~half float
// 6 ALU
//
float SqrtFast1(float x) {
    return x * RSqrtFast1(x);
}

//
// Use inverse Rcp Sqrt
// Using 2 Newton Raphson iterations
// Relative error : ~4.6e-004%  over full
// Precise format : ~full float
// 9 ALU
//
float SqrtFast2(float x) {
    return x * RSqrtFast2(x);
}


// ------------------------------------------------------------
// rcp

//
// Using 0 Newton Raphson iterations
// Relative error : < 0.4% over full
// Precise format : ~small float
// 1 ALU
//
float RcpFast0(float x) {
    return rcpIEEEIntApproximation(x, 0x7EF311C2);
}

//
// Using 1 Newton Raphson iterations
// Relative error : < 0.02% over full
// Precise format : ~half float
// 3 ALU
//
float RcpFast1(float x) {
	float  xRcp = rcpIEEEIntApproximation(x, 0x7EF311C3);
	xRcp = rcpNewtonRaphson(x, xRcp);
	return xRcp;
}

//
// Using 2 Newton Raphson iterations
// Relative error : < 5.0e-005%  over full
// Precise format : ~full float
// 5 ALU
//
float RcpFast2(float x) {
	float  xRcp = rcpIEEEIntApproximation(x, 0x7EF312AC);
	xRcp = rcpNewtonRaphson(x, xRcp);
	xRcp = rcpNewtonRaphson(x, xRcp);
	return xRcp;
}


// utils function

float LengthFast0(float3 v) {
	float l2 = dot(v,v);
	return SqrtFast0( l2 );
}

float3 NormalizeFast0(float3 v) {
	float l2 = dot(v, v);
	return v * RSqrtFast0(1.0);
}

//
// Trigonometric functions
//

// max absolute error 9.0x10^-3
// Eberly's polynomial degree 1 - respect bounds
// 4 VGPR, 12 FR (8 FR, 1 QR), 1 scalar
// input [-1, 1] and output [0, PI]
float AcosFast(float x) 
{
    float _x = abs(x);
    float res = -0.156583f * _x + (0.5 * PI);
    res *= sqrt(1.0f - _x);
    return (x >= 0) ? res : PI - res;
}

float2 AcosFast( float2 x )
{
	return float2( AcosFast(x.x), AcosFast(x.y) );
}

float3 AcosFast( float3 x )
{
	return float3( AcosFast(x.x), AcosFast(x.y), AcosFast(x.z) );
}

float4 AcosFast( float4 x )
{
	return float4( AcosFast(x.x), AcosFast(x.y), AcosFast(x.z), AcosFast(x.w) );
}


// Same cost as AcosFast + 1 FR
// Same error
// input [-1, 1] and output [-PI/2, PI/2]
float AsinFast( float x )
{
    return (0.5 * PI) - AcosFast(x);
}

float2 AsinFast( float2 x)
{
	return float2( AsinFast(x.x), AsinFast(x.y) );
}

float3 AsinFast( float3 x)
{
	return float3( AsinFast(x.x), AsinFast(x.y), AsinFast(x.z) );
}

float4 AsinFast( float4 x )
{
	return float4( AsinFast(x.x), AsinFast(x.y), AsinFast(x.z), AsinFast(x.w) );
}

// max absolute error 1.3x10^-3
// Eberly's odd polynomial degree 5 - respect bounds
// 4 VGPR, 14 FR (10 FR, 1 QR), 2 scalar
// input [0, infinity] and output [0, PI/2]
float AtanFastPos( float x ) 
{ 
    float t0 = (x < 1.0f) ? x : 1.0f / x;
    float t1 = t0 * t0;
    float poly = 0.0872929f;
    poly = -0.301895f + poly * t1;
    poly = 1.0f + poly * t1;
    poly = poly * t0;
    return (x < 1.0f) ? poly : (0.5 * PI) - poly;
}

// 4 VGPR, 16 FR (12 FR, 1 QR), 2 scalar
// input [-infinity, infinity] and output [-PI/2, PI/2]
float AtanFast( float x )
{
    float t0 = AtanFastPos( abs(x) );
    return (x < 0) ? -t0: t0;
}

float2 AtanFast( float2 x )
{
	return float2( AtanFast(x.x), AtanFast(x.y) );
}

float3 AtanFast( float3 x )
{
	return float3( AtanFast(x.x), AtanFast(x.y), AtanFast(x.z) );
}

float4 AtanFast( float4 x )
{
	return float4( AtanFast(x.x), AtanFast(x.y), AtanFast(x.z), AtanFast(x.w) );
}

float Atan2Fast( float y, float x )
{
	float t0 = max( abs(x), abs(y) );
	float t1 = min( abs(x), abs(y) );
	float t3 = t1 / t0;
	float t4 = t3 * t3;

	// Same polynomial as AtanFastPos
	t0 =         + 0.0872929;
	t0 = t0 * t4 - 0.301895;
	t0 = t0 * t4 + 1.0;
	t3 = t0 * t3;

	t3 = abs(y) > abs(x) ? (0.5 * PI) - t3 : t3;
	t3 = x < 0 ? PI - t3 : t3;
	t3 = y < 0 ? -t3 : t3;

	return t3;
}

float2 Atan2Fast( float2 y, float2 x )
{
	return float2( Atan2Fast(y.x, x.x), Atan2Fast(y.y, x.y) );
}

float3 Atan2Fast( float3 y, float3 x )
{
	return float3( Atan2Fast(y.x, x.x), Atan2Fast(y.y, x.y), Atan2Fast(y.z, x.z) );
}

float4 Atan2Fast( float4 y, float4 x )
{
	return float4( Atan2Fast(y.x, x.x), Atan2Fast(y.y, x.y), Atan2Fast(y.z, x.z), Atan2Fast(y.w, x.w) );
}

// 4th order polynomial approximation
// 4 VGRP, 16 ALU Full Rate
// 7 * 10^-5 radians precision
// Reference : Handbook of Mathematical Functions (chapter : Elementary Transcendental Functions), M. Abramowitz and I.A. Stegun, Ed.
float AcosFast4(float x)
{
	float x1 = abs(x);
	float x2 = x1 * x1;
	float x3 = x2 * x1;
	float s;

	s = -0.2121144f * x1 + 1.5707288f;
	s = 0.0742610f * x2 + s;
	s = -0.0187293f * x3 + s;
	s = sqrt(1.0f - x1) * s;

	// Acos function mirroring
	// check per platform if compiles to a selector - no branch neeeded
	return x >= 0.0f ? s : PI - s;
}

// 4th order polynomial approximation
// 4 VGRP, 16 ALU Full Rate
// 7 * 10^-5 radians precision 
float AsinFast4( float x )
{
	return (0.5 * PI) - AcosFast4(x);
}

// some functions may already optimized on modern platform

// // Spherical Gaussian Power Function 
// float PowFast(float x, float n)
// {
//     n = n * 1.4427f + 1.4427f; // 1.4427f --> 1/ln(2)
//     return exp2(x * n - n);
// }
// 
// // from idtech6
// float PowFastIDTech (float fBase, float fPower)
// {
// 	uint param_1 = uint((fPower * float (asuint (fBase))) - (((fPower - 1.0) * 127.0) * 8388608.0));
// 	return asfloat (param_1);
// }
// 
// // from idtech6
// float Log2FastIdTech (float f)
// {
// 	float param = f;
// 	return (float (asuint (param)) / 8388608.0) - 127.0;
// }
// 
// // from idtech6
// float Exp2FastIdTech (float f)
// {
// 	uint param = uint((f + 127.0) * 8388608.0);
// 	return asfloat (param);
// }
#line 3 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/brdf.h"

float3 Diffuse_Lambert( float3 DiffuseColor )
{
	return DiffuseColor * (1 / PI);
}

// [Burley 2012, "Physically-Based Shading at Disney"]
float3 Diffuse_Burley( float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH )
{
	float FD90 = 0.5 + 2 * VoH * VoH * Roughness;
	float FdV = 1 + (FD90 - 1) * Pow5( 1 - NoV );
	float FdL = 1 + (FD90 - 1) * Pow5( 1 - NoL );
	return DiffuseColor * ( (1 / PI) * FdV * FdL );
}

// [Gotanda 2012, "Beyond a Simple Physically Based Blinn-Phong Model in Real-Time"]
float3 Diffuse_OrenNayar( float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH )
{
	float a = Roughness * Roughness;
	float s = a;// / ( 1.29 + 0.5 * a );
	float s2 = s * s;
	float VoL = 2 * VoH * VoH - 1;		// double angle identity
	float Cosri = VoL - NoV * NoL;
	float C1 = 1 - 0.5 * s2 / (s2 + 0.33);
	float C2 = 0.45 * s2 / (s2 + 0.09) * Cosri * ( Cosri >= 0 ? rcp( max( NoL, NoV ) ) : 1 );
	return DiffuseColor / PI * ( C1 + C2 ) * ( 1 + Roughness * 0.5 );
}

// [Gotanda 2014, "Designing Reflectance Models for New Consoles"]
float3 Diffuse_Gotanda( float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH )
{
	float a = Roughness * Roughness;
	float a2 = a * a;
	float F0 = 0.04;
	float VoL = 2 * VoH * VoH - 1;		// double angle identity
	float Cosri = VoL - NoV * NoL;
#if 1
	float a2_13 = a2 + 1.36053;
	float Fr = ( 1 - ( 0.542026*a2 + 0.303573*a ) / a2_13 ) * ( 1 - pow( 1 - NoV, 5 - 4*a2 ) / a2_13 ) * ( ( -0.733996*a2*a + 1.50912*a2 - 1.16402*a ) * pow( 1 - NoV, 1 + rcp(39*a2*a2+1) ) + 1 );
	//float Fr = ( 1 - 0.36 * a ) * ( 1 - pow( 1 - NoV, 5 - 4*a2 ) / a2_13 ) * ( -2.5 * Roughness * ( 1 - NoV ) + 1 );
	float Lm = ( max( 1 - 2*a, 0 ) * ( 1 - Pow5( 1 - NoL ) ) + min( 2*a, 1 ) ) * ( 1 - 0.5*a * (NoL - 1) ) * NoL;
	float Vd = ( a2 / ( (a2 + 0.09) * (1.31072 + 0.995584 * NoV) ) ) * ( 1 - pow( 1 - NoL, ( 1 - 0.3726732 * NoV * NoV ) / ( 0.188566 + 0.38841 * NoV ) ) );
	float Bp = Cosri < 0 ? 1.4 * NoV * NoL * Cosri : Cosri;
	float Lr = (21.0 / 20.0) * (1 - F0) * ( Fr * Lm + Vd + Bp );
	return DiffuseColor / PI * Lr;
#else

#line 49 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/brdf.h"
	float a2_13 = a2 + 1.36053;
	float Fr = ( 1 - ( 0.542026*a2 + 0.303573*a ) / a2_13 ) * ( 1 - pow( 1 - NoV, 5 - 4*a2 ) / a2_13 ) * ( ( -0.733996*a2*a + 1.50912*a2 - 1.16402*a ) * pow( 1 - NoV, 1 + rcp(39*a2*a2+1) ) + 1 );
	float Lm = ( max( 1 - 2*a, 0 ) * ( 1 - Pow5( 1 - NoL ) ) + min( 2*a, 1 ) ) * ( 1 - 0.5*a + 0.5*a * NoL );
	float Vd = ( a2 / ( (a2 + 0.09) * (1.31072 + 0.995584 * NoV) ) ) * ( 1 - pow( 1 - NoL, ( 1 - 0.3726732 * NoV * NoV ) / ( 0.188566 + 0.38841 * NoV ) ) );
	float Bp = Cosri < 0 ? 1.4 * NoV * Cosri : Cosri / max( NoL, 1e-8 );
	float Lr = (21.0 / 20.0) * (1 - F0) * ( Fr * Lm + Vd + Bp );
	return DiffuseColor / PI * Lr;
#endif

#line 57 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/brdf.h"
}

// [ Chan 2018, "Material Advances in Call of Duty: WWII" ]
// It has been extended here to fade out retro reflectivity contribution from area light in order to avoid visual artefacts.
float3 Diffuse_Chan( float3 DiffuseColor, float a2, float NoV, float NoL, float VoH, float NoH, float RetroReflectivityWeight)
{
	// We saturate each input to avoid out of range negative values which would result in weird darkening at the edge of meshes (resulting from tangent space interpolation).
	NoV = saturate(NoV);
	NoL = saturate(NoL);
	VoH = saturate(VoH);
	NoH = saturate(NoH);

	// a2 = 2 / ( 1 + exp2( 18 * g )
	float g = saturate( (1.0 / 18.0) * log2( 2 / a2 - 1 ) );

	float F0 = VoH + Pow5( 1 - VoH );
	float FdV = 1 - 0.75 * Pow5( 1 - NoV );
	float FdL = 1 - 0.75 * Pow5( 1 - NoL );

	// Rough (F0) to smooth (FdV * FdL) response interpolation
	float Fd = lerp( F0, FdV * FdL, saturate( 2.2 * g - 0.5 ) );

	// Retro reflectivity contribution.
	float Fb = ( (34.5 * g - 59 ) * g + 24.5 ) * VoH * exp2( -max( 73.2 * g - 21.2, 8.9 ) * sqrt( NoH ) );
	// It fades out when lights become area lights in order to avoid visual artefacts.
	Fb *= RetroReflectivityWeight;
	
	return DiffuseColor * ( (1 / PI) * ( Fd + Fb ) );
}

// [Blinn 1977, "Models of light reflection for computer synthesized pictures"]
float D_Blinn( float a2, float NoH )
{
	float n = 2 / a2 - 2;
	return (n+2) / (2*PI) * pow(max(abs(NoH),POW_CLAMP),n);		// 1 mad, 1 exp, 1 mul, 1 log
}

// [Beckmann 1963, "The scattering of electromagnetic waves from rough surfaces"]
float D_Beckmann( float a2, float NoH )
{
	float NoH2 = NoH * NoH;
	return exp( (NoH2 - 1) / (a2 * NoH2) ) / ( PI * a2 * NoH2 * NoH2 );
}

// GGX / Trowbridge-Reitz
// [Walter et al. 2007, "Microfacet models for refraction through rough surfaces"]
float D_GGX( float a2, float NoH )
{
	float d = ( NoH * a2 - NoH ) * NoH + 1;	// 2 mad
	return a2 / ( PI*d*d );					// 4 mul, 1 rcp
}

// Anisotropic GGX
// [Burley 2012, "Physically-Based Shading at Disney"]
float D_GGXaniso( float ax, float ay, float NoH, float XoH, float YoH )
{
// The two formulations are mathematically equivalent
#if 1
	float a2 = ax * ay;
	float3 V = float3(ay * XoH, ax * YoH, a2 * NoH);
	float S = dot(V, V);

	return (1.0f / PI) * a2 * Pow2(a2 / S);
#else

#line 121 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/brdf.h"
	float d = XoH*XoH / (ax*ax) + YoH*YoH / (ay*ay) + NoH*NoH;
	return 1.0f / ( PI * ax*ay * d*d );
#endif

#line 124 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/brdf.h"
}

float Vis_Implicit()
{
	return 0.25;
}

// [Neumann et al. 1999, "Compact metallic reflectance models"]
float Vis_Neumann( float NoV, float NoL )
{
	return 1 / ( 4 * max( NoL, NoV ) );
}

// [Kelemen 2001, "A microfacet based coupled specular-matte brdf model with importance sampling"]
float Vis_Kelemen( float VoH )
{
	// constant to prevent NaN
	return rcp( 4 * VoH * VoH + 1e-5);
}

// Tuned to match behavior of Vis_Smith
// [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
float Vis_Schlick( float a2, float NoV, float NoL )
{
	float k = sqrt(a2) * 0.5;
	float Vis_SchlickV = NoV * (1 - k) + k;
	float Vis_SchlickL = NoL * (1 - k) + k;
	return 0.25 / ( Vis_SchlickV * Vis_SchlickL );
}

// Smith term for GGX
// [Smith 1967, "Geometrical shadowing of a random rough surface"]
float Vis_Smith( float a2, float NoV, float NoL )
{
	float Vis_SmithV = NoV + sqrt( NoV * (NoV - NoV * a2) + a2 );
	float Vis_SmithL = NoL + sqrt( NoL * (NoL - NoL * a2) + a2 );
	return rcp( Vis_SmithV * Vis_SmithL );
}

// Appoximation of joint Smith term for GGX
// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
float Vis_SmithJointApprox( float a2, float NoV, float NoL )
{
	float a = sqrt(a2);
	float Vis_SmithV = NoL * ( NoV * ( 1 - a ) + a );
	float Vis_SmithL = NoV * ( NoL * ( 1 - a ) + a );
	return 0.5 * rcp( Vis_SmithV + Vis_SmithL );
}

// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
float Vis_SmithJoint(float a2, float NoV, float NoL) 
{
	float Vis_SmithV = NoL * sqrt(NoV * (NoV - NoV * a2) + a2);
	float Vis_SmithL = NoV * sqrt(NoL * (NoL - NoL * a2) + a2);
	return 0.5 * rcp(Vis_SmithV + Vis_SmithL);
}

// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
float Vis_SmithJointAniso(float ax, float ay, float NoV, float NoL, float XoV, float XoL, float YoV, float YoL)
{
	float Vis_SmithV = NoL * length(float3(ax * XoV, ay * YoV, NoV));
	float Vis_SmithL = NoV * length(float3(ax * XoL, ay * YoL, NoL));
	return 0.5 * rcp(Vis_SmithV + Vis_SmithL);
}

// [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
float3 F_Schlick( float3 SpecularColor, float VoH )
{
	float Fc = Pow5( 1 - VoH );					// 1 sub, 3 mul
	//return Fc + (1 - Fc) * SpecularColor;		// 1 add, 3 mad
	
	// Anything less than 2% is physically impossible and is instead considered to be shadowing
	return saturate( 50.0 * SpecularColor.g ) * Fc + (1 - Fc) * SpecularColor;
}

float3 F_Schlick(float3 F0, float3 F90, float VoH)
{
	float Fc = Pow5(1 - VoH);
	return F90 * Fc + (1 - Fc) * F0;
}


struct BXDF
{
    float NoV;
    float NoL;
    float VoL;
    float NoH;
    float VoH;
    float XoV;
    float XoL;
    float XoH;
    float YoV;
    float YoL;
    float YoH;
};

void InitBXDF( inout(BXDF) context, float3 N, float3 V, float3 L )
{
	context.NoL = saturate(dot(N, L));
	context.NoV = saturate(dot(N, V));
	context.VoL = saturate(dot(V, L));
	float InvLenH = rsqrt( 2 + 2 * context.VoL );
	context.NoH = saturate( ( context.NoL + context.NoV ) * InvLenH );
	context.VoH = saturate( InvLenH + InvLenH * context.VoL );
	//NoL = saturate( NoL );
	//NoV = saturate( abs( NoV ) + 1e-5 );
	context.XoV = 0.0f;
	context.XoL = 0.0f;
	context.XoH = 0.0f;
	context.YoV = 0.0f;
	context.YoL = 0.0f;
	context.YoH = 0.0f;
}

void InitBXDF( inout(BXDF) context, float3 N, float3 X, float3 Y, float3 V, float3 L )
{
	context.NoL = dot(N, L);
	context.NoV = dot(N, V);
	context.VoL = dot(V, L);
	float InvLenH = rsqrt( 2 + 2 * context.VoL );
	context.NoH = saturate( ( context.NoL + context.NoV ) * InvLenH );
	context.VoH = saturate( InvLenH + InvLenH * context.VoL );
	//NoL = saturate( NoL );
	//NoV = saturate( abs( NoV ) + 1e-5 );

	context.XoV = dot(X, V);
	context.XoL = dot(X, L);
	context.XoH = (context.XoL + context.XoV) * InvLenH;
	context.YoV = dot(Y, V);
	context.YoL = dot(Y, L);
	context.YoH = (context.YoL + context.YoV) * InvLenH;
}
#line 2 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/shading_models.h"
#line 1 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/material_params_defination.hsl"

#line 3 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/shading_models.h"

// UnLit

float3 OpaqueBrdf(MaterialProperties mat, BXDF bxdf) {
    float lighting;    

    float D = D_GGX(mat.roughness2, bxdf.NoH);
    float G = Vis_SmithJoint(mat.roughness2, bxdf.NoV, bxdf.NoL);
    float f90 = saturate(dot(mat.f0, float3(50.0 * 0.33, 50.0 * 0.33, 50.0 * 0.33)));
    float3 F = F_Schlick(mat.f0, float3(f90, f90, f90), bxdf.NoH);

    float3 FDG = D * G * F;

    float3 kd = (float3(1.0, 1.0, 1.0) - F) * (1.0 - mat.metallic);
    float3 ks = float3(1.0, 1.0, 1.0) - kd;

    float3 diffuse = kd * Diffuse_Lambert(mat.albedo);

    float3 specular = ks * FDG;

    return diffuse + specular;
}

//Masked()

// SubSurface

// ClearCoat

// Hair

// Skin
// Eye

#line 2 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/shading/lighting.h"

float DistanceFalloff(float dist, float r, float3 light_dir) {
    // Brian Karis, 2013. Real Shading in Unreal Engine 4.
    float d2 = dist * dist;
    float r2 = r * r;
    float a = saturate(1.0f - (d2 * d2) / (r2 * r2));
    return a * a / max(d2, 1e-4);
}

float AngleFalloff(float innerRadius, float outerRadius, float3 direction, float3 light_dir) {
    float cosOuter = cos(outerRadius);
    float spotScale = 1.0 / max(cos(innerRadius) - cosOuter, 1e-4);
    float spotOffset = -cosOuter * spotScale;

    float cd = dot(normalize(-direction), light_dir);
    float attenuation = clamp(cd * spotScale + spotOffset, 0.0, 1.0);
    return attenuation * attenuation;
}

float4 Radiance(MaterialProperties mat, LightParams light, float3 n, float3 v, float3 world_pos) {
    float3 attenuation;
    float3 light_dir;

    if(asuint(light.position_type).w == DIRECTIONAL_LIGHT) {
        light_dir = -normalize(light.direction.xyz);
        attenuation = light.color_intensity.xyz * light.color_intensity.w;
    } else if(asuint(light.position_type).w == POINT_LIGHT) {
        light_dir = light.position_type.xyz - world_pos;
        float dist = length(light_dir);
        light_dir = normalize(light_dir);

        float lightAttenuation = DistanceFalloff(dist, light.radius_inner_outer.x, light_dir);

        attenuation = lightAttenuation * light.color_intensity.xyz * light.color_intensity.w;
    } else if (asuint(light.position_type).w == SPOT_LIGHT) {

        light_dir = light.position_type.xyz - world_pos;
        float dist = length(light_dir);
        light_dir = normalize(light_dir);

        float lightAttenuation = DistanceFalloff(dist, light.radius_inner_outer.x, light_dir) * AngleFalloff(light.radius_inner_outer.y, light.radius_inner_outer.z, light.direction.xyz, light_dir);

        attenuation = lightAttenuation * light.color_intensity.xyz * light.color_intensity.w;

    }

    if(dot(n, light_dir) < 0.0){
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    BXDF bxdf;
    InitBXDF(bxdf, n, v, light_dir);
    
    float3 radiance = OpaqueBrdf(mat, bxdf) * attenuation * bxdf.NoL;

    return float4(radiance, 1.0);
}

#line 4 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/lit_opaque.frag.hsl"
#line 1 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/include/postprocess/postprocess.h"
float3 TonemapACES(float3 x)
{
	const float A = 2.51f;
	const float B = 0.03f;
	const float C = 2.43f;
	const float D = 0.59f;
	const float E = 0.14f;
	return (x * (A * x + B)) / (x * (C * x + D) + E);
}

// float3 GammaCorrection(float3 x){
// 	return pow( x, float3( 1.0 / 2.2 ));
// }
#line 5 "C:/FILES/horizon/horizon/assets/C:/FILES/horizon/horizon/assets/shaders/lit_opaque.frag.hsl"

RES(Tex2D(float4), base_color_texture, UPDATE_FREQ_PER_BATCH, t0, binding = 0);
RES(Tex2D(float4), normal_texture, UPDATE_FREQ_PER_BATCH, t1, binding = 1);
RES(Tex2D(float4), metallic_roughness_texture, UPDATE_FREQ_PER_BATCH, t2, binding = 2);
RES(Tex2D(float4), emissive_texture, UPDATE_FREQ_PER_BATCH, t3, binding = 3);

RES(SamplerState, default_sampler, UPDATE_FREQ_PER_BATCH, s0, binding = 4);

CBUFFER(MaterialParamsUb, UPDATE_FREQ_PER_BATCH, b2, binding = 5)
{
    float4 base_color_roughness;
    float4 emmissive_factor_metallic;
    uint param_bitmask;
#line 18
};

CBUFFER(CameraParamsUb, UPDATE_FREQ_PER_FRAME, b0, binding = 0)
{
    float4x4 vp;
    float4 camera_position_exposure;
#line 24
};

CBUFFER(LightCountUb, UPDATE_FREQ_PER_FRAME, b4, binding = 1)
{
    uint light_count;
#line 29
};

CBUFFER(LightDataUb, UPDATE_FREQ_PER_FRAME, b5, binding = 2)
{
    LightParams light_data[MAX_DYNAMIC_LIGHT_COUNT];
#line 34
};


STRUCT(VSOutput)
{
	DATA(float4, position, SV_Position);
    DATA(float3, world_pos, POSITION);
	DATA(float3, normal, NORMAL);
	DATA(float2, uv, TEXCOORD0);
#line 43
};

STRUCT(PSOutput)
{
    DATA(float4, color, SV_Target0);
#line 48
};

PSOutput PS_MAIN(VSOutput vsout)
{
    //INIT_MAIN;
    PSOutput psout;

    uint has_metallic_roughness = Get(param_bitmask) & HAS_METALLIC_ROUGHNESS;
    uint has_normal = Get(param_bitmask) & HAS_NORMAL;
    uint has_base_color = Get(param_bitmask) & HAS_BASE_COLOR;
    uint has_emissive = Get(param_bitmask) & HAS_EMISSIVE;

    MaterialProperties mat;
    mat.albedo = has_base_color != 0 ? SampleTex2D(Get(base_color_texture), default_sampler, vsout.uv).xyz : float3(1.0, 1.0, 1.0);
    mat.normal = has_normal != 0 ? SampleTex2D(Get(normal_texture), default_sampler, vsout.uv).xyz : float3(0.0, 0.0, 0.0); // normal map
    float2 mr = has_metallic_roughness != 0 ? SampleTex2D(Get(metallic_roughness_texture), default_sampler, vsout.uv).xy : float2(0.0, 1.0);
    mat.emissive = has_emissive != 0 ? SampleTex2D(Get(emissive_texture), default_sampler, vsout.uv).xyz : float3(0.0, 0.0, 0.0);
    mat.metallic = mr.x;
    mat.roughness = mr.y;
    mat.roughness2 = Pow2(mr.y);
    mat.f0 = lerp(float3(0.04, 0.04, 0.04), mat.albedo, mat.metallic);
    
    float3 n = normalize(vsout.normal);
    float3 v = - normalize(vsout.world_pos - Get(camera_position_exposure).xyz);

    float4 radiance = float4(0.0, 0.0, 0.0, 0.0);

    for(uint i = 0; i < Get(light_count); i++) {
        radiance += Radiance(mat, Get(light_data)[i], n, v, vsout.world_pos);
    }

    // radiance.xyz += mat.emissive; // EMISSIVE?

    radiance.xyz *= Get(camera_position_exposure).w;

    radiance.xyz = TonemapACES(radiance.xyz);
    
    psout.color = radiance;
    return (psout);
    
}
