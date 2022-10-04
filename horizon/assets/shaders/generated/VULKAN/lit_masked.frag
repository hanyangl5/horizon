//--------------------------------------
// Generated from Horizon Shading Language
// 2022-10-05 00:52:44.096259
// "D:\codes\horizon\horizon\assets\shaders\lit_masked.frag.hsl"
//--------------------------------------

#version 450 core
#extension GL_GOOGLE_include_directive : require
precision highp float;
precision highp int;

#define STAGE_FRAG
#ifndef _VULKAN_H
#define _VULKAN_H

#define UINT_MAX 4294967295
#define FLT_MAX 3.402823466e+38F

#if VK_EXT_DESCRIPTOR_INDEXING_ENABLED
#extension GL_EXT_nonuniform_qualifier : enable
#endif

#if VR_MULTIVIEW_ENABLED && defined(STAGE_VERT)
#extension GL_OVR_multiview2 : require
#endif

#define f4(X) vec4(X)
#define f3(X) vec3(X)
#define f2(X) vec2(X)
#define u4(X) uvec4(X)
#define u3(X) uvec3(X)
#define u2(X) uvec2(X)
#define i4(X) ivec4(X)
#define i3(X) ivec3(X)
#define i2(X) ivec2(X)

#define h4 f4
#define half4 float4
#define half3 float3
#define half2 float2
#define half float

#define short4 int4
#define short3 int3
#define short2 int2
#define short int

#define ushort4 uint4
#define ushort3 uint3
#define ushort2 uint2
#define ushort uint

#define SV_VERTEXID gl_VertexIndex
#define SV_INSTANCEID gl_InstanceIndex
#define SV_ISFRONTFACE gl_FrontFacing
#define SV_GROUPID gl_WorkGroupID
#define SV_DISPATCHTHREADID gl_GlobalInvocationID
#define SV_GROUPTHREADID gl_LocalInvocationID
#define SV_GROUPINDEX gl_LocalInvocationIndex
#define SV_SAMPLEINDEX gl_SampleID
#define SV_PRIMITIVEID gl_PrimitiveID
#define SV_SHADINGRATE 0u // ShadingRateKHR

#define SV_OUTPUTCONTROLPOINTID gl_InvocationID
#define SV_DOMAINLOCATION gl_TessCoord

#define inout(T) inout T
#define out(T) out T
#define in(T) in T
#define inout_array(T, X) inout(T) X
#define out_array(T, X) out(T) X
#define in_array(T, X) in(T)X
#define groupshared inout

#define Get(X) _Get##X

#define _NONE
#define NUM_THREADS(X, Y, Z) layout(local_size_x = X, local_size_y = Y, local_size_z = Z) in(_NONE);

// for preprocessor to evaluate symbols before concatenation
#define CAT(a, b) a##b
#define XCAT(a, b) CAT(a, b)

#ifndef UPDATE_FREQ_NONE
#define UPDATE_FREQ_NONE set = 0
#endif
#ifndef UPDATE_FREQ_PER_FRAME
#define UPDATE_FREQ_PER_FRAME set = 1
#endif
#ifndef UPDATE_FREQ_PER_BATCH
#define UPDATE_FREQ_PER_BATCH set = 2
#endif
#ifndef UPDATE_FREQ_PER_DRAW
#define UPDATE_FREQ_PER_DRAW set = 3
#endif
#define UPDATE_FREQ_USER UPDATE_FREQ_NONE
#define space4 UPDATE_FREQ_NONE
#define space5 UPDATE_FREQ_NONE
#define space6 UPDATE_FREQ_NONE
#define space10 UPDATE_FREQ_NONE

#define STRUCT(NAME) struct NAME
#define DATA(TYPE, NAME, SEM) TYPE NAME
#define FLAT(TYPE) TYPE

#define AnyLessThan(X, Y) any(LessThan((X), (Y)))
#define AnyLessThanEqual(X, Y) any(LessThanEqual((X), (Y)))
#define AnyGreaterThan(X, Y) any(GreaterThan((X), (Y)))
#define AnyGreaterThanEqual(X, Y) any(GreaterThanEqual((X), (Y)))

#define AllLessThan(X, Y) all(LessThan((X), (Y)))
#define AllLessThanEqual(X, Y) all(LessThanEqual((X), (Y)))
#define AllGreaterThan(X, Y) all(GreaterThan((X), (Y)))
#define AllGreaterThanEqual(X, Y) all(GreaterThanEqual((X), (Y)))

#define select lerp

bvec3 Equal(vec3 X, float Y) { return equal(X, vec3(Y)); }

bvec2 LessThan(in(vec2)a, in(float)b) { return lessThan(a, vec2(b)); }
bvec2 LessThan(in(vec2)a, in(vec2)b) { return lessThan(a, b); }
bvec3 LessThan(in(vec3)X, in(float)Y) { return lessThan(X, f3(Y)); }
bvec3 LessThanEqual(in(vec3)X, in(float)Y) { return lessThanEqual(X, f3(Y)); }

bvec2 GreaterThan(in(vec2)a, in(float)b) { return greaterThan(a, vec2(b)); }
bvec3 GreaterThan(in(vec3)a, in(float)b) { return greaterThan(a, f3(b)); }
bvec2 GreaterThan(in(uvec2)a, in(uint)b) { return greaterThan(a, uvec2(b)); }
bvec2 GreaterThan(in(vec2)a, in(vec2)b) { return greaterThan(a, b); }
bvec4 GreaterThan(in(vec4)a, in(vec4)b) { return greaterThan(a, b); }
bvec3 GreaterThanEqual(in(vec3)a, in(float)b) { return greaterThanEqual(a, vec3(b)); }

bvec2 And(in(bvec2)a, in(bvec2)b) { return bvec2(a.x && b.x, a.y && b.y); }

bool allGreaterThan(const vec4 a, const vec4 b) { return all(greaterThan(a, b)); }

#define GroupMemoryBarrier()                                                                                           \
    {                                                                                                                  \
        groupMemoryBarrier();                                                                                          \
        barrier();                                                                                                     \
    }
// #define AllMemoryBarrier AllMemoryBarrierWithGroupSync()
#define AllMemoryBarrier()                                                                                             \
    {                                                                                                                  \
        groupMemoryBarrier();                                                                                          \
        memoryBarrier();                                                                                               \
        barrier();                                                                                                     \
    }

#define clip(COND)                                                                                                     \
    if ((COND) < 0) {                                                                                                  \
        discard;                                                                                                       \
        return;                                                                                                        \
    }

#extension GL_ARB_shader_ballot : enable

// uint NonUniformResourceIndex(uint textureIdx)
// {
// 	while (true)
// 	{
// 		uint currentIdx = readFirstInvocationARB(textureIdx);

// 		if (currentIdx == textureIdx)
// 			return currentIdx;
// 	}

// 	return 0; //Execution should never land here
// }

#define AtomicAdd(DEST, VALUE, ORIGINAL_VALUE)                                                                         \
    { ORIGINAL_VALUE = atomicAdd(DEST, VALUE); }
#define AtomicStore(DEST, VALUE) (DEST) = (VALUE)
#define AtomicLoad(SRC) SRC
#define AtomicExchange(DEST, VALUE, ORIGINAL_VALUE) ORIGINAL_VALUE = atomicExchange((DEST), (VALUE))

#define CBUFFER(NAME, FREQ, REG, BINDING) layout(std140, FREQ, BINDING) uniform NAME
#define PUSH_CONSTANT(NAME, REG) layout(push_constant) uniform NAME##Block

// #define mul(a, b) (a * b)
mat4 mul(mat4 a, mat4 b) { return a * b; }
vec4 mul(mat4 a, vec4 b) { return a * b; }
vec4 mul(vec4 a, mat4 b) { return a * b; }

mat3 mul(mat3 a, mat3 b) { return a * b; }
vec3 mul(mat3 a, vec3 b) { return a * b; }
vec3 mul(vec3 a, mat3 b) { return a * b; }

vec2 mul(mat2 a, vec2 b) { return a * b; }
vec3 mul(vec3 a, float b) { return a * b; }

#define row_major(X) layout(row_major) X

#define RES(TYPE, NAME, FREQ, REG, BINDING) layout(FREQ, BINDING) uniform TYPE NAME

vec4 SampleLvlTex2D(texture2D NAME, sampler SAMPLER, vec2 COORD, float LEVEL) {
    return textureLod(sampler2D(NAME, SAMPLER), COORD, LEVEL);
}
vec4 SampleLvlTex3D(texture3D NAME, sampler SAMPLER, vec3 COORD, float LEVEL) {
    return textureLod(sampler3D(NAME, SAMPLER), COORD, LEVEL);
}
vec4 SampleLvlTex3D(texture2DArray NAME, sampler SAMPLER, vec3 COORD, float LEVEL) {
    return textureLod(sampler2DArray(NAME, SAMPLER), COORD, LEVEL);
}
vec4 SampleLvlTexCube(textureCube NAME, sampler SAMPLER, vec3 COORD, float LEVEL) {
    return textureLod(samplerCube(NAME, SAMPLER), COORD, LEVEL);
}

// vec4 SampleLvlOffsetTex2D( texture2D NAME, sampler SAMPLER, vec2 COORD, float LEVEL, const in(ivec2) OFFSET )
// { return textureLodOffset(sampler2D(NAME, SAMPLER), COORD, LEVEL, OFFSET); }
#define SampleLvlOffsetTex2D(NAME, SAMPLER, COORD, LEVEL, OFFSET)                                                      \
    textureLodOffset(sampler2D(NAME, SAMPLER), COORD, LEVEL, OFFSET)
#define SampleLvlOffsetTex2DArray(NAME, SAMPLER, COORD, LEVEL, OFFSET)                                                 \
    textureLodOffset(sampler2DArray(NAME, SAMPLER), COORD, LEVEL, OFFSET)
#define SampleLvlOffsetTex3D(NAME, SAMPLER, COORD, LEVEL, OFFSET)                                                      \
    textureLodOffset(sampler3D(NAME, SAMPLER), COORD, LEVEL, OFFSET)

#define LoadByte(BYTE_BUFFER, ADDRESS) ((BYTE_BUFFER)[(ADDRESS) >> 2])
#define LoadByte4(BYTE_BUFFER, ADDRESS)                                                                                \
    uint4((BYTE_BUFFER)[((ADDRESS) >> 2) + 0], (BYTE_BUFFER)[((ADDRESS) >> 2) + 1],                                    \
          (BYTE_BUFFER)[((ADDRESS) >> 2) + 2], (BYTE_BUFFER)[((ADDRESS) >> 2) + 3])

#define StoreByte(BYTE_BUFFER, ADDRESS, VALUE) (BYTE_BUFFER)[((ADDRESS) >> 2) + 0] = VALUE;

// #define LoadLvlTex2D(TEX, SMP, P, L) _LoadLvlTex2D(TEX, SMP, ivec2((P).xy), L)
// vec4 _LoadLvlTex2D(texture2D TEX, sampler SMP, ivec2 P, int L) { return texelFetch(sampler2D(TEX, SMP), P, L); }
// #ifdef GL_EXT_samplerless_texture_functions
// #extension GL_EXT_samplerless_texture_functions : enable
// vec4 _LoadLvlTex2D(texture2D TEX, uint _NO_SAMPLER, ivec2 P, int L) { return texelFetch(TEX, P, L); }
// #endif

// #define LoadTex3D(TEX, SMP, P, LOD) _LoadTex3D()
//  vec4 _LoadTex2D( texture2D TEX, sampler SMP, ivec2 P, int lod) { return texelFetch( sampler2DArray(TEX, SMP), P,
//  lod); }
// uvec4 _LoadTex2D(utexture2D TEX, sampler SMP, ivec2 P, int lod) { return texelFetch(usampler2DArray(TEX, SMP), P,
// lod); } ivec4 _LoadTex2D(itexture2D TEX, sampler SMP, ivec2 P, int lod) { return texelFetch(isampler2DArray(TEX,
// SMP), P, lod); }

#define LoadTex2D(TEX, SMP, P, LOD) _LoadTex2D((TEX), (SMP), ivec2((P).xy), int(LOD))
vec4 _LoadTex2D(texture2D TEX, sampler SMP, ivec2 P, int lod) { return texelFetch(sampler2D(TEX, SMP), P, lod); }
uvec4 _LoadTex2D(utexture2D TEX, sampler SMP, ivec2 P, int lod) { return texelFetch(usampler2D(TEX, SMP), P, lod); }
ivec4 _LoadTex2D(itexture2D TEX, sampler SMP, ivec2 P, int lod) { return texelFetch(isampler2D(TEX, SMP), P, lod); }

#define LoadRWTex2D(TEX, P) imageLoad(TEX, ivec2(P))
#define LoadRWTex3D(TEX, P) imageLoad(TEX, ivec3(P))

// vec4 _LoadTex2D(writeonly image2D img, sampler SMP, ivec2 P, int lod) { return imageLoad(img, P); }

// #define Load2D( NAME, COORD ) imageLoad( (NAME), ivec2((COORD).xy) )
// #define Load3D( NAME, COORD ) imageLoad( (NAME), ivec3((COORD).xyz) )

#define LoadTex3D(TEX, SMP, P, LOD) _LoadTex3D((TEX), (SMP), ivec3((P).xyz), int(LOD))
vec4 _LoadTex3D(texture2DArray TEX, sampler SMP, ivec3 P, int lod) {
    return texelFetch(sampler2DArray(TEX, SMP), P, lod);
}
uvec4 _LoadTex3D(utexture2DArray TEX, sampler SMP, ivec3 P, int lod) {
    return texelFetch(usampler2DArray(TEX, SMP), P, lod);
}
ivec4 _LoadTex3D(itexture2DArray TEX, sampler SMP, ivec3 P, int lod) {
    return texelFetch(isampler2DArray(TEX, SMP), P, lod);
}
vec4 _LoadTex3D(texture3D TEX, sampler SMP, ivec3 P, int lod) { return texelFetch(sampler3D(TEX, SMP), P, lod); }
uvec4 _LoadTex3D(utexture3D TEX, sampler SMP, ivec3 P, int lod) { return texelFetch(usampler3D(TEX, SMP), P, lod); }
ivec4 _LoadTex3D(itexture3D TEX, sampler SMP, ivec3 P, int lod) { return texelFetch(isampler3D(TEX, SMP), P, lod); }

#ifdef GL_EXT_samplerless_texture_functions
#extension GL_EXT_samplerless_texture_functions : enable
vec4 _LoadTex2D(texture2D TEX, uint _NO_SAMPLER, ivec2 P, int lod) { return texelFetch(TEX, P, lod); }
uvec4 _LoadTex2D(utexture2D TEX, uint _NO_SAMPLER, ivec2 P, int lod) { return texelFetch(TEX, P, lod); }
ivec4 _LoadTex2D(itexture2D TEX, uint _NO_SAMPLER, ivec2 P, int lod) { return texelFetch(TEX, P, lod); }
vec4 _LoadTex2DMS(texture2DMS TEX, uint _NO_SAMPLER, ivec2 P, int lod) { return texelFetch(TEX, P, lod); }
uvec4 _LoadTex2DMS(utexture2DMS TEX, uint _NO_SAMPLER, ivec2 P, int lod) { return texelFetch(TEX, P, lod); }
ivec4 _LoadTex2DMS(itexture2DMS TEX, uint _NO_SAMPLER, ivec2 P, int lod) { return texelFetch(TEX, P, lod); }
vec4 _LoadTex3D(texture2DArray TEX, uint _NO_SAMPLER, ivec3 P, int lod) { return texelFetch(TEX, P, lod); }
uvec4 _LoadTex3D(utexture2DArray TEX, uint _NO_SAMPLER, ivec3 P, int lod) { return texelFetch(TEX, P, lod); }
ivec4 _LoadTex3D(itexture2DArray TEX, uint _NO_SAMPLER, ivec3 P, int lod) { return texelFetch(TEX, P, lod); }
vec4 _LoadTex3D(texture3D TEX, uint _NO_SAMPLER, ivec3 P, int lod) { return texelFetch(TEX, P, lod); }
uvec4 _LoadTex3D(utexture3D TEX, uint _NO_SAMPLER, ivec3 P, int lod) { return texelFetch(TEX, P, lod); }
ivec4 _LoadTex3D(itexture3D TEX, uint _NO_SAMPLER, ivec3 P, int lod) { return texelFetch(TEX, P, lod); }
#endif

#define LoadLvlOffsetTex2D(TEX, SMP, P, L, O) _LoadLvlOffsetTex2D((TEX), (SMP), (P), (L), (O))
vec4 _LoadLvlOffsetTex2D(texture2D TEX, sampler SMP, ivec2 P, int L, ivec2 O) {
    return texelFetch(sampler2D(TEX, SMP), P + O, L);
}

#define LoadLvlOffsetTex3D(TEX, SMP, P, L, O) _LoadLvlOffsetTex3D((TEX), (SMP), (P), (L), (O))
vec4 _LoadLvlOffsetTex3D(texture2DArray TEX, sampler SMP, ivec3 P, int L, ivec3 O) {
    return texelFetch(sampler2DArray(TEX, SMP), P + O, L);
}

#define LoadTex2DMS(NAME, SMP, P, S) _LoadTex2DMS((NAME), (SMP), ivec2(P.xy), int(S))
vec4 _LoadTex2DMS(texture2DMS TEX, sampler SMP, ivec2 P, int S) { return texelFetch(sampler2DMS(TEX, SMP), P, S); }
#define LoadTex2DArrayMS(NAME, SMP, P, S) _LoadTex2DArrayMS((NAME), (SMP), ivec3(P.xyz), int(S))
vec4 _LoadTex2DArrayMS(texture2DMSArray TEX, sampler SMP, ivec3 P, int S) {
    return texelFetch(sampler2DMSArray(TEX, SMP), P, S);
}
#ifdef GL_EXT_samplerless_texture_functions
vec4 _LoadTex2DArrayMS(texture2DMSArray TEX, uint _NO_SAMPLER, ivec3 P, int S) { return texelFetch(TEX, P, S); }
#endif

#define SampleGradTex2D(TEX, SMP, P, DX, DY) _SampleGradTex2D((TEX), (SMP), vec2((P).xy), vec2((DX).xy), vec2((DY).xy))
vec4 _SampleGradTex2D(texture2D TEX, sampler SMP, vec2 P, vec2 DX, vec2 DY) {
    return textureGrad(sampler2D(TEX, SMP), P, DX, DY);
}

#define GatherRedTex2D(TEX, SMP, P) _GatherRedTex2D((TEX), (SMP), vec2(P.xy))
vec4 _GatherRedTex2D(texture2D TEX, sampler SMP, vec2 P) { return textureGather(sampler2D(TEX, SMP), P, 0); }

#define SampleTexCube(TEX, SMP, P) _SampleTexCube((TEX), (SMP), vec3((P).xyz))
vec4 _SampleTexCube(textureCube TEX, sampler SMP, vec3 P) { return texture(samplerCube(TEX, SMP), P); }
uvec4 _SampleTexCube(utextureCube TEX, sampler SMP, vec3 P) { return texture(usamplerCube(TEX, SMP), P); }
ivec4 _SampleTexCube(itextureCube TEX, sampler SMP, vec3 P) { return texture(isamplerCube(TEX, SMP), P); }

#define SampleTex1D(TEX, SMP, P) _SampleTex1D((TEX), (SMP), float((P).x))
vec4 _SampleTex1D(texture1D TEX, sampler SMP, float P) { return texture(sampler1D(TEX, SMP), P); }

#define SampleTex2D(TEX, SMP, P) _SampleTex2D((TEX), (SMP), vec2((P).xy))
vec4 _SampleTex2D(texture2D TEX, sampler SMP, vec2 P) { return texture(sampler2D(TEX, SMP), P); }

#define SampleTex2DArray(TEX, SMP, P) _SampleTex2DArray((TEX), (SMP), vec3((P).xyz))
vec4 _SampleTex2DArray(texture2DArray TEX, sampler SMP, vec3 P) { return texture(sampler2DArray(TEX, SMP), P); }

#define SampleTex2DProj(TEX, SMP, P) _SampleTex2DProj((TEX), (SMP), vec4(P.xyzw))
vec4 _SampleTex2DProj(texture2D TEX, sampler SMP, vec4 P) { return textureProj(sampler2D(TEX, SMP), P); }

// #define SampleTex3D(NAME, SAMPLER, COORD)            texture(_getSampler(NAME, SAMPLER), COORD)
#define SampleTex3D(TEX, SMP, P) _SampleTex3D((TEX), (SMP), vec3((P).xyz))
vec4 _SampleTex3D(texture3D TEX, sampler SMP, vec3 P) { return texture(sampler3D(TEX, SMP), P); }

#define SampleCmp2D(TEX, SMP, PC) _SampleCmp2D((TEX), (SMP), vec3((PC).xyz))
float _SampleCmp2D(texture2D TEX, sampler SMP, vec3 PC) { return texture(sampler2DShadow(TEX, SMP), PC); }

#define CompareTex2D(TEX, SMP, PC) _CompareTex2D((TEX), (SMP), vec3((PC).xyz))
float _CompareTex2D(texture2D TEX, sampler SMP, vec3 PC) { return textureLod(sampler2DShadow(TEX, SMP), PC, 0.0f); }

#define CompareTex2DProj(TEX, SMP, PC) _CompareTex2DProj((TEX), (SMP), vec4((PC).xyzw))
float _CompareTex2DProj(texture2D TEX, sampler SMP, vec4 PC) { return textureProj(sampler2DShadow(TEX, SMP), PC); }

#ifdef STAGE_FRAG // only available in ps
#define CalculateLvlTex2D(TEX, SMP, P) _CalculateLvlTex2D((TEX), (SMP), vec2((P).xy))
float _CalculateLvlTex2D(texture2D TEX, sampler SMP, vec2 P) { return textureQueryLod(sampler2D(TEX, SMP), P).y; }

#define residency_status int

#extension GL_ARB_sparse_texture2 : enable
#extension GL_ARB_sparse_texture_clamp : enable
#define SampleClampedSparseTex2D(TEX, SMP, P, L, RC) _SampleClampedSparseTex2D((TEX), (SMP), vec2((P).xy), (L), (RC))
vec4 _SampleClampedSparseTex2D(texture2D TEX, sampler SMP, vec2 P, float L, out(residency_status) RC) {
    vec4 texel;
    RC = sparseTextureClampARB(sampler2D(TEX, SMP), P, L, texel);
    return texel;
}

#define IsFullyMapped(RC) sparseTexelsResidentARB(RC)
#endif

#define SHADER_CONSTANT(INDEX, TYPE, NAME, VALUE) layout(constant_id = INDEX) const TYPE NAME = VALUE

#define HSL_CONST(TYPE, NAME) const TYPE NAME
#define STATIC
#define INLINE

// #define WRITE2D(NAME, COORD, VAL) imageStore(NAME, int2(COORD.xy), VAL)
// #define WRITE3D(NAME, COORD, VAL) imageStore(NAME, int3(COORD.xyz), VAL)

// void write3D()

// void write3D( layout(rgba32f) image2DArray dst, ivec3 coord, vec4 value) { imageStore(dst, coord, value); }

// #define Write2D(TEX, P, V) _Write2D((TEX), ivec2((P).xy), (V))
// void _Write2D(layout(rg32f) image2D TEX, ivec2 P, vec2 V) { imageStore(TEX, P, vec4(V, 0, 0)); }

vec4 _to4(in(vec4)x) { return x; }
vec4 _to4(in(vec3)x) { return vec4(x, 0); }
vec4 _to4(in(vec2)x) { return vec4(x, 0, 0); }
vec4 _to4(in(float)x) { return vec4(x, 0, 0, 0); }
uvec4 _to4(in(uvec4)x) { return x; }
uvec4 _to4(in(uvec3)x) { return uvec4(x, 0); }
uvec4 _to4(in(uvec2)x) { return uvec4(x, 0, 0); }
uvec4 _to4(in(uint)x) { return uvec4(x, 0, 0, 0); }
ivec4 _to4(in(ivec4)x) { return x; }
ivec4 _to4(in(ivec3)x) { return ivec4(x, 0); }
ivec4 _to4(in(ivec2)x) { return ivec4(x, 0, 0); }
ivec4 _to4(in(int)x) { return ivec4(x, 0, 0, 0); }

#define Write2D(TEX, P, V) imageStore(TEX, ivec2((P).xy), _to4(V))
#define Write3D(TEX, P, V) imageStore(TEX, ivec3((P.xyz)), _to4(V))
#define Write2DArray(TEX, P, I, V) imageStore(TEX, ivec3(P, I), _to4(V))

// void _Write2D(layout(rg32f) image2D TEX, ivec2 P, vec2 V) { imageStore(TEX, P, vec4(V, 0, 0)); }
// void _Write2D(image2D TEX, ivec2 P, vec2 V) { imageStore(TEX, P, vec4(V, 0, 0)); }

// #define Write2D( NAME, COORD, VALUE ) imageStore( (NAME), ivec2((COORD).xy), vec4(VALUE, 0, 0) )
// void Write2D(image2D img, )
// #define Write3D( NAME, COORD, VALUE ) imageStore( (NAME), ivec3((COORD).xyz), (VALUE) )

#define Load2D(NAME, COORD) imageLoad((NAME), ivec2((COORD).xy))
#define Load3D(NAME, COORD) imageLoad((NAME), ivec3((COORD).xyz))

#define AtomicMin3D(DST, COORD, VALUE, ORIGINAL_VALUE)                                                                 \
    ((ORIGINAL_VALUE) = imageAtomicMin((DST), ivec3((COORD).xyz), (VALUE)))
#define AtomicMin(DST, VALUE) atomicMin(DST, VALUE)
#define AtomicMax(DST, VALUE) atomicMax(DST, VALUE)

#define UNROLL_N(X)
#define UNROLL
#define LOOP
#define FLATTEN

#define sincos(angle, s, c)                                                                                            \
    {                                                                                                                  \
        (s) = sin(angle);                                                                                              \
        (c) = cos(angle);                                                                                              \
    }

/* Matrix */

#define to_f3x3(M) mat3(M)

#define f2x2 mat2x2
#define f2x3 mat2x3
#define f2x4 mat2x4
#define f3x2 mat3x2
#define f3x3 mat3x3
#define f3x4 mat3x4
#define f4x2 mat4x2
#define f4x3 mat4x3
#define f4x4 mat4x4

#define make_f2x2_cols(C0, C1) f2x2(C0, C1)
#define make_f2x2_rows(R0, R1) transpose(f2x2(R0, R1))
#define make_f2x2_col_elems f2x2
#define make_f2x3_cols(C0, C1) f2x3(C0, C1)
#define make_f2x3_rows(R0, R1, R2) transpose(f3x2(R0, R1, R2))
#define make_f2x3_col_elems f2x3
#define make_f2x4_cols(C0, C1) f2x4(C0, C1)
#define make_f2x4_rows(R0, R1, R2, R3) transpose(f4x2(R0, R1, R2, R3))
#define make_f2x4_col_elems f2x4

#define make_f3x2_cols(C0, C1, C2) f3x2(C0, C1, C2)
#define make_f3x2_rows(R0, R1) transpose(f2x3(R0, R1))
#define make_f3x2_col_elems f3x2
#define make_f3x3_cols(C0, C1, C2) f3x3(C0, C1, C2)
#define make_f3x3_rows(R0, R1, R2) transpose(f3x3(R0, R1, R2))
#define make_f3x3_col_elems f3x3
#define make_f3x3_row_elems(E00, E01, E02, E10, E11, E12, E20, E21, E22)                                               \
    f3x3(E00, E10, E20, E01, E11, E21, E02, E12, E22)
#define f3x4_cols(C0, C1, C2) f3x4(C0, C1, C2)
#define f3x4_rows(R0, R1, R2, R3) transpose(f4x3(R0, R1, R2, R3))
#define f3x4_col_elems f3x4

#define make_f4x2_cols(C0, C1, C2, C3) f4x2(C0, C1, C2, C3)
#define make_f4x2_rows(R0, R1) transpose(f2x4(R0, R1))
#define make_f4x2_col_elems f4x2
#define make_f4x3_cols(C0, C1, C2, C3) f4x3(C0, C1, C2, C3)
#define make_f4x3_rows(R0, R1, R2) transpose(f3x4(R0, R1, R2))
#define make_f4x3_col_elems f4x3
#define make_f4x4_cols(C0, C1, C2, C3) f4x4(C0, C1, C2, C3)
#define make_f4x4_rows(R0, R1, R2, R3) transpose(f4x4(R0, R1, R2, R3))
#define make_f4x4_col_elems f4x4
#define make_f4x4_row_elems(E00, E01, E02, E03, E10, E11, E12, E13, E20, E21, E22, E23, E30, E31, E32, E33)            \
    f4x4(E00, E10, E20, E30, E01, E11, E21, E31, E02, E12, E22, E32, E03, E13, E23, E33)

f4x4 Identity() { return f4x4(1.0f); }

#define setElem(M, I, J, V)                                                                                            \
    { M[I][J] = V; }
#define getElem(M, I, J) (M[I][J])

#define getCol(M, I) M[I]
#define getCol0(M) getCol(M, 0)
#define getCol1(M) getCol(M, 1)
#define getCol2(M) getCol(M, 2)
#define getCol3(M) getCol(M, 3)

vec4 getRow(in(mat4)M, uint i) { return vec4(M[0][i], M[1][i], M[2][i], M[3][i]); }
vec4 getRow(in(mat4x3)M, uint i) { return vec4(M[0][i], M[1][i], M[2][i], M[3][i]); }
vec4 getRow(in(mat4x2)M, uint i) { return vec4(M[0][i], M[1][i], M[2][i], M[3][i]); }

vec3 getRow(in(mat3x4)M, uint i) { return vec3(M[0][i], M[1][i], M[2][i]); }
vec3 getRow(in(mat3)M, uint i) { return vec3(M[0][i], M[1][i], M[2][i]); }
vec3 getRow(in(mat3x2)M, uint i) { return vec3(M[0][i], M[1][i], M[2][i]); }

vec2 getRow(in(mat2x4)M, uint i) { return vec2(M[0][i], M[1][i]); }
vec2 getRow(in(mat2x3)M, uint i) { return vec2(M[0][i], M[1][i]); }
vec2 getRow(in(mat2)M, uint i) { return vec2(M[0][i], M[1][i]); }

#define getRow0(M) getRow(M, 0)
#define getRow1(M) getRow(M, 1)
#define getRow2(M) getRow(M, 2)
#define getRow3(M) getRow(M, 3)

f4x4 setCol(inout(f4x4) M, in(vec4)col, const uint i) {
    M[i] = col;
    return M;
}
f4x3 setCol(inout(f4x3) M, in(vec3)col, const uint i) {
    M[i] = col;
    return M;
}
f4x2 setCol(inout(f4x2) M, in(vec2)col, const uint i) {
    M[i] = col;
    return M;
}

f3x4 setCol(inout(f3x4) M, in(vec4)col, const uint i) {
    M[i] = col;
    return M;
}
f3x3 setCol(inout(f3x3) M, in(vec3)col, const uint i) {
    M[i] = col;
    return M;
}
f3x2 setCol(inout(f3x2) M, in(vec2)col, const uint i) {
    M[i] = col;
    return M;
}

f2x4 setCol(inout(f2x4) M, in(vec4)col, const uint i) {
    M[i] = col;
    return M;
}
f2x3 setCol(inout(f2x3) M, in(vec3)col, const uint i) {
    M[i] = col;
    return M;
}
f2x2 setCol(inout(f2x2) M, in(vec2)col, const uint i) {
    M[i] = col;
    return M;
}

#define setCol0(M, C) setCol(M, C, 0)
#define setCol1(M, C) setCol(M, C, 1)
#define setCol2(M, C) setCol(M, C, 2)
#define setCol3(M, C) setCol(M, C, 3)

f4x4 setRow(inout(f4x4) M, in(vec4)row, const uint i) {
    M[0][i] = row[0];
    M[1][i] = row[1];
    M[2][i] = row[2];
    M[3][i] = row[3];
    return M;
}
f4x3 setRow(inout(f4x3) M, in(vec4)row, const uint i) {
    M[0][i] = row[0];
    M[1][i] = row[1];
    M[2][i] = row[2];
    M[3][i] = row[3];
    return M;
}
f4x2 setRow(inout(f4x2) M, in(vec4)row, const uint i) {
    M[0][i] = row[0];
    M[1][i] = row[1];
    M[2][i] = row[2];
    M[3][i] = row[3];
    return M;
}

f3x4 setRow(inout(f3x4) M, in(vec3)row, const uint i) {
    M[0][i] = row[0];
    M[1][i] = row[1];
    M[2][i] = row[2];
    return M;
}
f3x3 setRow(inout(f3x3) M, in(vec3)row, const uint i) {
    M[0][i] = row[0];
    M[1][i] = row[1];
    M[2][i] = row[2];
    return M;
}
f3x2 setRow(inout(f3x2) M, in(vec3)row, const uint i) {
    M[0][i] = row[0];
    M[1][i] = row[1];
    M[2][i] = row[2];
    return M;
}

f2x4 setRow(inout(f2x4) M, in(vec2)row, const uint i) {
    M[0][i] = row[0];
    M[1][i] = row[1];
    return M;
}
f2x3 setRow(inout(f2x3) M, in(vec2)row, const uint i) {
    M[0][i] = row[0];
    M[1][i] = row[1];
    return M;
}
f2x2 setRow(inout(f2x2) M, in(vec2)row, const uint i) {
    M[0][i] = row[0];
    M[1][i] = row[1];
    return M;
}

#define setRow0(M, R) setRow(M, R, 0)
#define setRow1(M, R) setRow(M, R, 1)
#define setRow2(M, R) setRow(M, R, 2)
#define setRow3(M, R) setRow(M, R, 3)

#define atomic_uint uint
#define packed_float3 vec3

#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4
#define float float
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define float2x2 mat2
#define float3x3 mat3
#define float3x2 mat3x2
#define float2x3 mat2x3
#define float4x4 mat4

#define double2 dvec2
#define double3 dvec3
#define double4 dvec4
#define double2x2 dmat2
#define double3x3 dmat3
#define double4x4 dmat4

#define min16float mediump float
#define min16float2 mediump float2
#define min16float3 mediump float3
#define min16float4 mediump float4

#define SamplerState sampler
#define SamplerComparisonState sampler
#define NO_SAMPLER 0u

// #define GetDimensions(TEXTURE, SAMPLER) textureSize(TEXTURE, 0)
// int2 GetDimensions(texture2D t, sampler s)
//     { return textureSize(sampler2D(t, s), 0); }
// int2 GetDimensions(textureCube t, sampler s)
//     { return textureSize(samplerCube(t, s), 0); }

#ifdef GL_EXT_samplerless_texture_functions
#extension GL_EXT_samplerless_texture_functions : enable
// int2 GetDimensions( texture2D t, uint _NO_SAMPLER) { return textureSize(t, 0); }
// int2 GetDimensions(utexture2D t, uint _NO_SAMPLER) { return textureSize(t, 0); }
// int2 GetDimensions(itexture2D t, uint _NO_SAMPLER) { return textureSize(t, 0); }
#endif

// #ifdef GL_EXT_samplerless_texture_functions
// #extension GL_EXT_samplerless_texture_functions : enable
//  vec4 _LoadTex2D( texture2D TEX, uint _NO_SAMPLER, ivec2 P) { return texelFetch(TEX, P, 0); }
// uvec4 _LoadTex2D(utexture2D TEX, uint _NO_SAMPLER, ivec2 P) { return texelFetch(TEX, P, 0); }
// ivec4 _LoadTex2D(itexture2D TEX, uint _NO_SAMPLER, ivec2 P) { return texelFetch(TEX, P, 0); }
// #endif

int2 imageSize(utexture2D TEX) { return textureSize(TEX, 0); }
int2 imageSize(texture2D TEX) { return textureSize(TEX, 0); }
int2 imageSize(textureCube TEX) { return textureSize(TEX, 0); }
int3 imageSize(texture2DArray TEX) { return textureSize(TEX, 0); }
int3 imageSize(utexture2DArray TEX) { return textureSize(TEX, 0); }

#define GetDimensions(TEX, SMP) imageSize(TEX)

// int2 GetDimensions(writeonly iimage2D t, uint _NO_SAMPLER) { return imageSize(t); }
// int2 GetDimensions(writeonly uimage2D t, uint _NO_SAMPLER) { return imageSize(t); }
// int2 GetDimensions(writeonly image2D t, uint _NO_SAMPLER) { return imageSize(t); }

#define VK_T_uint(T) u##T
#define VK_T_uint2(T) u##T
#define VK_T_uint4(T) u##T
#define VK_T_int(T) i##T
#define VK_T_int2(T) i##T
#define VK_T_int4(T) i##T
#define VK_T_float(T) T
#define VK_T_float2(T) T
#define VK_T_float4(T) T
#define VK_T_half(T) T
#define VK_T_half2(T) T
#define VK_T_half4(T) T

// TODO: get rid of these
#define VK_T_uint3(T) u##T
#define VK_T_int3(T) i##T
#define VK_T_float3(T) T
#define VK_T_half3(T) T

#define TexCube(T) VK_T_##T(textureCube)
#define TexCubeArray(T) VK_T_##T(textureCubeArray)

#define Tex1D(T) VK_T_##T(texture1D)
#define Tex2D(T) VK_T_##T(texture2D)
#define Tex3D(T) VK_T_##T(texture3D)

#define Tex2DMS(T, S) VK_T_##T(texture2DMS)

#define Tex1DArray(T) VK_T_##T(texture1DArray)
#define Tex2DArray(T) VK_T_##T(texture2DArray)
#define Tex2DArrayMS(T, S) VK_T_##T(texture2DMSArray)

#define RWTexCube(T) VK_T_##T(imageCube)
#define RWTexCubeArray(T) VK_T_##T(imageCubeArray)

#define RWTex1D(T) VK_T_##T(image1D)
#define RWTex2D(T) VK_T_##T(image2D)
#define RWTex3D(T) VK_T_##T(image3D)

#define RWTex2DMS(T, S) VK_T_##T(image2DMS)

#define RWTex1DArray(T) VK_T_##T(image1DArray)
#define RWTex2DArray(T) VK_T_##T(image2DArray)

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

#define RWTex2DArrayMS(T, S) VK_T_##T(image2DMSArray)

#define RasterizerOrderedTex2D(T) VK_T_##T(image2D)
#define RasterizerOrderedTex2DArray(T) VK_T_##T(image2DArray)

#define Depth2D(T) VK_T_##T(texture2D)
#define Depth2DMS(T, S) VK_T_##T(texture2DMS)
#define Depth2DArray(T) VK_T_##T(texture2DArray)
#define Depth2DArrayMS(T, S) VK_T_##T(texture2DMSArray)

// matching hlsl semantics, glsl mod preserves sign(Y)
#define fmod(X, Y) (abs(mod(X, Y)) * sign(X))

#define fast_min min
#define fast_max max
#define isordered(X, Y) (((X) == (X)) && ((Y) == (Y)))
#define isunordered(X, Y) (isnan(X) || isnan(Y))

#define extract_bits bitfieldExtract
#define insert_bits bitfieldInsert

#define frac(VALUE) fract(VALUE)
#define lerp mix
// #define lerp(X, Y, VALUE)	mix(X, Y, VALUE)
// vec4 lerp(vec4 X, vec4 Y, float VALUE) { return mix(X, Y, VALUE); }
// vec3 lerp(vec3 X, vec3 Y, float VALUE) { return mix(X, Y, VALUE); }
// vec2 lerp(vec2 X, vec2 Y, float VALUE) { return mix(X, Y, VALUE); }
// float lerp(float X, float Y, float VALUE) { return mix(X, Y, VALUE); }
#define rsqrt(VALUE) inversesqrt(VALUE)
float saturate(float VALUE) { return clamp(VALUE, 0.0f, 1.0f); }
vec2 saturate(vec2 VALUE) { return clamp(VALUE, 0.0f, 1.0f); }
vec3 saturate(vec3 VALUE) { return clamp(VALUE, 0.0f, 1.0f); }
vec4 saturate(vec4 VALUE) { return clamp(VALUE, 0.0f, 1.0f); }
// #define saturate(VALUE)		clamp(VALUE, 0.0f, 1.0f)
#define ddx(VALUE) dFdx(VALUE)
#define ddy(VALUE) dFdy(VALUE)
#define ddx_fine(VALUE) dFdx(VALUE) // fallback
#define ddy_fine(VALUE) dFdy(VALUE) // dFdxFine/dFdyFine available in opengl 4.5
#define rcp(VALUE) (1.0f / (VALUE))
#define atan2(X, Y) atan(X, Y)
#define reversebits(X) bitfieldReverse(X)
#define asuint(X) floatBitsToUint(X)
#define asint(X) floatBitsToInt(X)
#define asfloat(X) uintBitsToFloat(X)
#define mad(a, b, c) (a) * (b) + (c)

// case list
#define REPEAT_TEN(base)                                                                                               \
    CASE(base)                                                                                                         \
    CASE(base + 1) CASE(base + 2) CASE(base + 3) CASE(base + 4) CASE(base + 5) CASE(base + 6) CASE(base + 7)           \
        CASE(base + 8) CASE(base + 9)
#define REPEAT_HUNDRED(base)                                                                                           \
    REPEAT_TEN(base)                                                                                                   \
    REPEAT_TEN(base + 10) REPEAT_TEN(base + 20) REPEAT_TEN(base + 30) REPEAT_TEN(base + 40) REPEAT_TEN(base + 50)      \
        REPEAT_TEN(base + 60) REPEAT_TEN(base + 70) REPEAT_TEN(base + 80) REPEAT_TEN(base + 90)

#define CASE_LIST_256                                                                                                  \
    CASE(0)                                                                                                            \
    REPEAT_HUNDRED(1) REPEAT_HUNDRED(101) REPEAT_TEN(201) REPEAT_TEN(211) REPEAT_TEN(221) REPEAT_TEN(231)              \
        REPEAT_TEN(241) CASE(251) CASE(252) CASE(253) CASE(254) CASE(255)

#define EARLY_FRAGMENT_TESTS layout(early_fragment_tests) in;

bool any(vec2 x) { return any(notEqual(x, vec2(0))); }
bool any(vec3 x) { return any(notEqual(x, vec3(0))); }

//  tessellation
#define INPUT_PATCH(T, NC) T[NC]
#define OUTPUT_PATCH(T, NC) T[NC]
#define TESS_VS_SHADER(X)

#define PATCH_CONSTANT_FUNC(F)       //[patchconstantfunc(F)]
#define OUTPUT_CONTROL_POINTS(P)     //[outputcontrolpoints(P)]
#define MAX_TESS_FACTOR(F)           //[maxtessfactor(F)]
#define HSL_DomainPartitioning(X, Y) //[domain(X)] [partitioning(Y)]
#define HSL_OutputTopology(T)        //[outputtopology(T)]

#define VK_TESS_quad quads
#define VK_TESS_integer equal_spacing
#define VK_TESS_triangle_ccw ccw

#ifdef STAGE_TESE
#define TESS_LAYOUT(D, P, T) layout(VK_TESS_##D, VK_TESS_##P, VK_TESS_##T) in(_NONE);
#else
#define TESS_LAYOUT(D, P, T)
#endif

#ifdef ENABLE_WAVEOPS

#define ballot_t uvec4

#define CountBallot(X) (bitCount(X.x) + bitCount(X.y) + bitCount(X.z) + bitCount(X.w))

#extension GL_KHR_shader_subgroup_basic : enable
#ifdef TARGET_SWITCH
#extension GL_ARB_shader_ballot : enable
#extension GL_ARB_gpu_shader_int64 : enable
bool WaveIsFirstLane() { return gl_SubGroupInvocationARB == 0; }
#define WaveGetLaneIndex() gl_SubGroupInvocationARB
#define WaveGetMaxActiveIndex() (gl_SubGroupSizeARB - 1)
#define WaveReadLaneFirst readFirstInvocationARB
#else
#extension GL_KHR_shader_subgroup_arithmetic : enable
#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_quad : enable
#extension GL_KHR_shader_subgroup_shuffle : enable
#extension GL_KHR_shader_subgroup_vote : enable
#define WaveIsFirstLane subgroupElect
#define WaveGetLaneIndex() gl_SubgroupInvocationID
#define WaveGetMaxActiveIndex() subgroupMax(gl_SubgroupInvocationID)
#define WaveReadLaneFirst subgroupBroadcastFirst
#endif

#define WaveReadLaneAt(X, Y) subgroupShuffle(X, Y)
#define WaveActiveAnyTrue subgroupAny
#define WavePrefixCountBits(X) subgroupBallotInclusiveBitCount(subgroupBallot(X))
#define WaveActiveCountBits(X) subgroupBallotBitCount(subgroupBallot(X))

#define WaveActiveMax subgroupMax
#ifdef TARGET_ANDROID
#define WaveActiveBallot(X) subgroupBallot(false)
#else
#define WaveActiveBallot subgroupBallot
#endif

#define countbits bitCount
#define WaveActiveSum subgroupAdd
#define WavePrefixSum subgroupExclusiveAdd

#define QuadReadAcrossX subgroupQuadSwapHorizontal
#define QuadReadAcrossY subgroupQuadSwapVertical

#endif

#ifdef GL_ARB_fragment_shader_interlock
#extension GL_ARB_fragment_shader_interlock : enable
#define BeginPSInterlock() beginInvocationInterlockARB()
#define EndPSInterlock() endInvocationInterlockARB()
#else
#define EnablePSInterlock()
#define BeginPSInterlock()
#define EndPSInterlock()
#endif

#define DECLARE_RESOURCES()
#define INDIRECT_DRAW()
#define SET_OUTPUT_FORMAT(FMT)
#define PS_ZORDER_EARLYZ()

#if VR_MULTIVIEW_ENABLED
#ifndef STAGE_VERT
#define VR_VIEW_ID(VID) VID
#else
#define VR_VIEW_ID (gl_ViewID_OVR)
#endif
#define VR_MULTIVIEW_COUNT 2
#else
#ifndef STAGE_VERT
#define VR_VIEW_ID(VID) (0)
#else
#define VR_VIEW_ID 0
#endif
#define VR_MULTIVIEW_COUNT 1
#endif

#endif // _VULKAN_H

#line 1 "D:/codes/horizon/horizon/assets/shaders/lit_masked.frag.hsl"
#line 1 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/material_params_defination.hsl"
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
#line 2 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/lit_masked.frag.hsl"
#line 1 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/light_defination.h"
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
#line 3 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/lit_masked.frag.hsl"
#line 1 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/lighting.h"
#line 1 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/shading_models.h"
#line 1 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/brdf.h"
#line 1 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/../common/common_math.h"
#define PI 3.1415926535897932384626433f
#define HALF_PI 1.57079632679489661923f

#define POW_CLAMP 0.000001f

float Pow2(float x) {return x * x;}

float Pow3(float x) {return x * x * x;}

float Pow4(float x) {return x * x * x * x;}

float Pow5(float x) {return x * x * x * x * x;}

#line 2 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/brdf.h"
#line 1 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/../common/fastmath.hsl"

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

#line 1 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/../common/common_math.h"

#line 37 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/../common/fastmath.hsl"

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
#line 3 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/brdf.h"

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

#line 49 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/brdf.h"
	float a2_13 = a2 + 1.36053;
	float Fr = ( 1 - ( 0.542026*a2 + 0.303573*a ) / a2_13 ) * ( 1 - pow( 1 - NoV, 5 - 4*a2 ) / a2_13 ) * ( ( -0.733996*a2*a + 1.50912*a2 - 1.16402*a ) * pow( 1 - NoV, 1 + rcp(39*a2*a2+1) ) + 1 );
	float Lm = ( max( 1 - 2*a, 0 ) * ( 1 - Pow5( 1 - NoL ) ) + min( 2*a, 1 ) ) * ( 1 - 0.5*a + 0.5*a * NoL );
	float Vd = ( a2 / ( (a2 + 0.09) * (1.31072 + 0.995584 * NoV) ) ) * ( 1 - pow( 1 - NoL, ( 1 - 0.3726732 * NoV * NoV ) / ( 0.188566 + 0.38841 * NoV ) ) );
	float Bp = Cosri < 0 ? 1.4 * NoV * Cosri : Cosri / max( NoL, 1e-8 );
	float Lr = (21.0 / 20.0) * (1 - F0) * ( Fr * Lm + Vd + Bp );
	return DiffuseColor / PI * Lr;
#endif

#line 57 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/brdf.h"
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

#line 121 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/brdf.h"
	float d = XoH*XoH / (ax*ax) + YoH*YoH / (ay*ay) + NoH*NoH;
	return 1.0f / ( PI * ax*ay * d*d );
#endif

#line 124 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/brdf.h"
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
#line 2 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/shading_models.h"
#line 1 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/material_params_defination.hsl"

#line 3 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/shading_models.h"

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

#line 2 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/shading/lighting.h"

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

#line 4 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/lit_masked.frag.hsl"

#line 1 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/include/postprocess/postprocess.h"
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
#line 6 "D:/codes/horizon/horizon/assets/D:/codes/horizon/horizon/assets/shaders/lit_masked.frag.hsl"

#define _Getbase_color_texture base_color_texture
#line 7
RES(Tex2D(float4), base_color_texture, UPDATE_FREQ_PER_BATCH, t0, binding = 0);
#define _Getnormal_texture normal_texture
#line 8
RES(Tex2D(float4), normal_texture, UPDATE_FREQ_PER_BATCH, t1, binding = 1);
#define _Getmetallic_roughness_texture metallic_roughness_texture
#line 9
RES(Tex2D(float4), metallic_roughness_texture, UPDATE_FREQ_PER_BATCH, t2, binding = 2);
#define _Getemissive_texture emissive_texture
#line 10
RES(Tex2D(float4), emissive_texture, UPDATE_FREQ_PER_BATCH, t3, binding = 3);
#define _Getalpha_texture alpha_texture
#line 11
RES(Tex2D(float4), alpha_texture, UPDATE_FREQ_PER_BATCH, t4, binding = 4);
// RES(Tex2D(float4), irradiance_map, UPDATE_FREQ_PER_BATCH, t5, binding = 5);

#define _Getdefault_sampler default_sampler
#line 14
RES(SamplerState, default_sampler, UPDATE_FREQ_PER_BATCH, s0, binding = 5);
// RES(SamplerState, cubemap_sampler, UPDATE_FREQ_PER_BATCH, s1, binding = 6);

CBUFFER(MaterialParamsUb, UPDATE_FREQ_PER_BATCH, b2, binding = 6)
{
#define _Getbase_color_roughness base_color_roughness
#line 19
    DATA(float4, base_color_roughness, None);
#define _Getemmissive_factor_metallic emmissive_factor_metallic
#line 20
    DATA(float4, emmissive_factor_metallic, None);
#define _Getparam_bitmask param_bitmask
#line 21
    DATA(uint, param_bitmask, None);
};

CBUFFER(CameraParamsUb, UPDATE_FREQ_PER_FRAME, b0, binding = 0)
{
#define _Getvp vp
#line 26
    DATA(float4x4, vp, None);
#define _Getcamera_position_exposure camera_position_exposure
#line 27
    DATA(float4, camera_position_exposure, None);
};

CBUFFER(LightCountUb, UPDATE_FREQ_PER_FRAME, b4, binding = 1)
{
#define _Getlight_count light_count
#line 32
    DATA(uint, light_count, None);
};

CBUFFER(LightDataUb, UPDATE_FREQ_PER_FRAME, b5, binding = 2)
{
#define _Getlight_data light_data
#line 37
    DATA(LightParams, light_data[MAX_DYNAMIC_LIGHT_COUNT], None);
};


STRUCT(VSOutput)
{
#define _position_1794
#line 43
	DATA(float4, position, SV_Position);
#define _world_pos_1799
#line 44
    DATA(float3, world_pos, POSITION);
#define _normal_1804
#line 45
	DATA(float3, normal, NORMAL);
#define _uv_1809
#line 46
	DATA(float2, uv, TEXCOORD0);
};
#ifdef _world_pos_1799
layout(location = 0) in(float3) vsout_world_pos;
#endif
#ifdef _normal_1804
layout(location = 1) in(float3) vsout_normal;
#endif
#ifdef _uv_1809
layout(location = 2) in(float2) vsout_uv;
#endif
#line 48

STRUCT(PSOutput)
{
#define _color_1828
#line 51
    DATA(float4, color, SV_Target0);
};
#ifdef _color_1828
layout(location = 0) out(float4) out_PSOutput_color;
#endif
#line 53

void main()
#line 54
//PSOutput PS_MAIN(VSOutput vsout)
{
	VSOutput vsout;
#ifdef _position_1794
	vsout.position = float4(float4(gl_FragCoord.xyz, 1.0f / gl_FragCoord.w));
#endif
#ifdef _world_pos_1799
	vsout.world_pos = vsout_world_pos;
#endif
#ifdef _normal_1804
	vsout.normal = vsout_normal;
#endif
#ifdef _uv_1809
	vsout.uv = vsout_uv;
#endif
#line 56
//    INIT_MAIN;
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

    radiance.xyz *= Get(camera_position_exposure).w;

    radiance.xyz = TonemapACES(radiance.xyz);
    
    psout.color = radiance;
    
    {
    	PSOutput out_PSOutput = psout;
#ifdef _color_1828
    	out_PSOutput_color = out_PSOutput.color;
#endif
    	return;
    }
#line 89
//    RETURN(psout);
    
}
