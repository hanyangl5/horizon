//--------------------------------------
// Generated from Horizon Shading Language
// 2023-08-11 10:44:49.175409
// "C:\hanyanglu\horizon\assets\shaders\decal.frag.hsl"
//--------------------------------------

#version 460 core
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
#ifndef UPDATE_FREQ_BINDLESS
#define UPDATE_FREQ_BINDLESS set = 4
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

#line 1 "C:/hanyanglu/horizon/assets/shaders/decal.frag.hsl"
#ifdef VULKAN
#extension GL_EXT_nonuniform_qualifier : enable // how to hide this?
#endif

#line 4 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/decal.frag.hsl"

#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/common/bindless.h"

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

#line 6 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/decal.frag.hsl"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/postprocess/postprocess.h"
float3 TonemapACES(float3 x)
{
	const float A = 2.51f;
	const float B = 0.03f;
	const float C = 2.43f;
	const float D = 0.59f;
	const float E = 0.14f;
	return (x * (A * x + B)) / (x * (C * x + D) + E);
}

float3 GammaCorrection(float3 x){
	return pow( x, float3( 1.0 / 2.2 ));
}
#line 7 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/decal.frag.hsl"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/ibl.h"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/../common/common_math.h"
#define _PI  3.141592654f
#define _2PI  6.283185307f
#define _1DIVPI  0.318309886f
#define _1DIV2PI  0.159154943f
#define _PIDIV2  1.570796327f
#define _PIDIV4  0.785398163f

#define POW_CLAMP 0.000001f

float Pow2(float x) {return x * x;}

float Pow3(float x) {return x * x * x;}

float Pow4(float x) {return x * x * x * x;}

float Pow5(float x) {return x * x * x * x * x;}

float SmoothStep(float e0, float e1, float x) { 
    float t = saturate((x - e0) / e1 - e0);
    return t * t * (3.0 - 2.0 * t);
}
#line 2 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/ibl.h"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/brdf_horizon.h"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/../common/common_math.h"

#line 2 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/brdf_horizon.h"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/../common/fastmath.hsl"

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

#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/../common/common_math.h"

#line 37 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/../common/fastmath.hsl"

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
// input [-1, 1] and output [0, _PI]
float AcosFast(float x) 
{
    float _x = abs(x);
    float res = -0.156583f * _x + (0.5 * _PI);
    res *= sqrt(1.0f - _x);
    return (x >= 0) ? res : _PI - res;
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
// input [-1, 1] and output [-_PI/2, _PI/2]
float AsinFast( float x )
{
    return (0.5 * _PI) - AcosFast(x);
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
// input [0, infinity] and output [0, _PI/2]
float AtanFastPos( float x ) 
{ 
    float t0 = (x < 1.0f) ? x : 1.0f / x;
    float t1 = t0 * t0;
    float poly = 0.0872929f;
    poly = -0.301895f + poly * t1;
    poly = 1.0f + poly * t1;
    poly = poly * t0;
    return (x < 1.0f) ? poly : (0.5 * _PI) - poly;
}

// 4 VGPR, 16 FR (12 FR, 1 QR), 2 scalar
// input [-infinity, infinity] and output [-_PI/2, _PI/2]
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

	t3 = abs(y) > abs(x) ? (0.5 * _PI) - t3 : t3;
	t3 = x < 0 ? _PI - t3 : t3;
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
	return x >= 0.0f ? s : _PI - s;
}

// 4th order polynomial approximation
// 4 VGRP, 16 ALU Full Rate
// 7 * 10^-5 radians precision 
float AsinFast4( float x )
{
	return (0.5 * _PI) - AcosFast4(x);
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
#line 3 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/brdf_horizon.h"

struct BXDF {
    float NoV;
    float NoL;
    float VoL;
    float NoM;
    float VoM;
    float XoV;
    float XoL;
    float XoM;
    float YoV;
    float YoL;
    float YoM;
};

void InitBXDF(inout(BXDF) bxdf, float3 N, float3 V, float3 L) {
    bxdf.NoL = saturate(dot(N, L));
    bxdf.NoV = saturate(dot(N, V));
    bxdf.VoL = saturate(dot(V, L));
    float3 M = (V + L) / 2.0;
    bxdf.NoM = saturate(dot(N, M));
    bxdf.VoM = saturate(dot(V, M));
    // bxdf.XoV = 0.0f;
    // bxdf.XoL = 0.0f;
    // bxdf.XoM = 0.0f;
    // bxdf.YoV = 0.0f;
    // bxdf.YoL = 0.0f;
    // bxdf.YoM = 0.0f;
}

float3 Diffuse_Lambert(float3 albedo) { return albedo * _1DIVPI; }

float3 Fresnel_Schlick(float3 F0, float LoM) { return F0 + (1.0 - F0) * Pow5(1 - LoM); }

// for isotropic ndf, we provide ggx and gtr

float NDF_GGX(float roughness2, float NoM) {
    float d = (NoM * roughness2 - NoM) * NoM + 1.0;
    return roughness2 * _1DIVPI / (d * d);
}

// float NDF_Aniso_GGX()

float Vis_SmithGGXCombined(float roughness2, float NoV, float NoL) {
    float Vis_SmithV = NoL * sqrt(NoV * (NoV - NoV * roughness2) + roughness2);
    float Vis_SmithL = NoV * sqrt(NoL * (NoL - NoL * roughness2) + roughness2);
    return 0.5 / (Vis_SmithV + Vis_SmithL);
}

float Vis_Aniso_SmithGGXCombined(float ax, float ay, float NoV, float NoL, float XoV, float XoL, float YoV, float YoL) {
    float Vis_SmithV = NoL * length(float3(ax * XoV, ay * YoV, NoV));
    float Vis_SmithL = NoV * length(float3(ax * XoL, ay * YoL, NoL));
    return 0.5 / (Vis_SmithV + Vis_SmithL);
}

#line 3 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/ibl.h"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/material_params_defination.hsl"
#define HAS_BASE_COLOR 0x01
#define HAS_NORMAL 0x10
#define HAS_METALLIC_ROUGHNESS 0x100
#define HAS_EMISSIVE 0x1000
#define HAS_ALPHA 0x10000

struct MaterialProperties {
    float3 albedo;
    float3 normal;
    float3 f0;
    float metallic;  // 0
    float roughness; // 0.5
    float roughness2;
    float3 emissive;

    // disney

    float anisotropic;     // 0
    float sheen;           // 0
    float sheen_tint;      // 0.5
    float subsurface;      // 0
    float clearcoat;       // 0
    float clearcoat_gloss; // 0.5
};

#define BLEND_STATE_OPAQUE 0
#define BLEND_STATE_MASKED 1
#line 4 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/ibl.h"
// float3 ComputeIBL(MaterialProperties mat,
// 	float NoV, float3 N, float3 V, TexCube(float4) iemCubemap, TexCube(float4) pmremCubemap, Tex2D(float4)
// environmentBRDF, SamplerState ibl_sampler)
// {
//     float3 albedo = mat.albedo;
//     float metalness = mat.metallic;
//     float roughness = mat.roughness;

// 	float3 F0 = float3(0.04f, 0.04f, 0.04f);
//     float3 diffuse = (1.0 - metalness) * albedo;
// 	float3 specular = lerp(F0, albedo, metalness);

// 	float3 R = normalize(reflect(-V, N));

// 	float3 irradiance = SampleTexCube(iemCubemap, ibl_sampler, N).rgb;
// 	float3 radiance = SampleLvlTexCube(pmremCubemap, ibl_sampler, R, roughness * 10.0).rgb;
// 	float2 brdf = SampleTex2D(environmentBRDF, ibl_sampler, float2(NoV, roughness)).rg;

// 	return irradiance * diffuse + radiance * (specular * brdf.x + brdf.y);
// }

float3 Irradiance_SphericalHarmonics(float3 sh[9], float3 normal) {
    return max(sh[0] + sh[1] * (normal.y) + sh[2] * (normal.z) + sh[3] * (normal.x) + sh[4] * (normal.y * normal.x) +
                   sh[5] * (normal.y * normal.z) + sh[6] * (3.0 * normal.z * normal.z - 1.0) +
                   sh[7] * (normal.z * normal.x) + sh[8] * (normal.x * normal.x - normal.y * normal.y),
               0.0);
}

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * Pow5(clamp(1.0 - cosTheta, 0.0, 1.0));
}

float3 IBL(float3 sh[9], float3 specular, float2 env, float3 normal, float NoV, MaterialProperties mat) {
    float3 specular_color = (mat.f0 * env.x + env.y) * specular;
    float3 diffuse_color = Irradiance_SphericalHarmonics(sh, normal) * Diffuse_Lambert(mat.albedo);

    float3 f = fresnelSchlickRoughness(NoV, mat.f0, mat.roughness);
    float3 kd = (float3(1.0) - f) * (1.0 - mat.metallic);
    diffuse_color *= kd;
    float3 ibl_color = diffuse_color + specular_color;
    return ibl_color;
}

#line 8 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/decal.frag.hsl"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/light_defination.h"
struct LightParams {
    float4 color_intensity; // r, g, b, intensity
    float4 position_type;   // x, y, z, type
    float4 direction;
    float4 radius_inner_outer; // radius, innerradius, outerradius
};

#define MAX_DYNAMIC_LIGHT_COUNT 256

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2
#line 9 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/decal.frag.hsl"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/lighting.h"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/shading_models.h"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/brdf_horizon.h"

#line 2 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/shading_models.h"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/brdfs.h"
/*
# Copyright Disney Enterprises, Inc.  All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License
# and the following modification to it: Section 6 Trademarks.
# deleted and replaced with:
#
# 6. Trademarks. This License does not grant permission to use the
# trade names, trademarks, service marks, or product names of the
# Licensor and its affiliates, except as required for reproducing
# the content of the NOTICE file.
#
# You may obtain a copy of the License at
# http://www.apache.org/licenses/LICENSE-2.0

https://github.com/wdas/brdf/blob/425f6ef183f3e57c2a351622790ca8ea472ddfe1/src/brdfs/disney.brdf

*/

#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/../common/common_math.h"

#line 22 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/brdfs.h"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/../common/fastmath.hsl"

#line 23 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/brdfs.h"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/../common/luminance.h"
float Luminance(float3 color) { return 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b; }
#line 24 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/brdfs.h"

// struct MaterialProperties_Disney12{
//     float3 baseColor;
//     float metallic;
//     float subsurface;
//     float specular;
//     float roughness;
//     float specularTint;
//     float anisotropic;
//     float sheen;
//     float sheenTint;
//     float clearcoat;
//     float clearcoatGloss;
// };

// Physically-Based Shading at Disney

float3 DiffuseBurley12(float3 albedo, float roughness, float LoH, float NoL, float NoV) {
    float fd90 = 0.5f + 2.0 * roughness * LoH * LoH;
    float f_theta_l = 1.0 + (fd90 - 1.0) * Pow5(1.0 - NoL); // incident light refractiion
    float f_theta_v = 1.0 + (fd90 - 1.0) * Pow5(1.0 - NoV); // outgoing light refraction
    return albedo * _1DIVPI * f_theta_v * f_theta_l;
}

// Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
// 1.25 scale is used to (roughly) preserve albedo
// Fss90 used to "flatten" retroreflection based on roughness
float3 SubsurfaceBurley12(float3 albedo, float roughness, float NoL, float NoV, float LoH) {
    float f_ss_90 = LoH * LoH * roughness;
    float f_ss = (1.0 + (f_ss_90 - 1.0) * Pow5(1.0 - NoL)) * (1.0 + (f_ss_90 - 1.0) * Pow5(1.0 - NoV));
    float3 ss = albedo * _1DIVPI * 1.25 * (f_ss * (1 / (NoL + NoV) - 0.5) + 0.5);
    return ss;
}

float NDF_GTR_GAMMA(float NoH, float a, float gamma) {
    float k;
    float a2 = a * a;
    if (a == 1.0) {
        k = 1.0;
    } else if (gamma == 1.0 && a != 1.0) {
        k = (a2 - 1.0) / log(a2);
    } else if (gamma != 1.0 && a != 1.0) {
        k = (gamma - 1.0) * (a2 - 1.0) / (1.0 - pow(a2, 1.0 - gamma));
    }
    float denom = pow(1.0 + Pow2(NoH) * (a2 - 1.0), gamma);
    return k / denom;
}

float NDF_GTR1(float NdotH, float a) {
    if (a >= 1)
        return 1 / _PI;
    float a2 = a * a;
    float t = 1 + (a2 - 1) * NdotH * NdotH;
    return (a2 - 1) / (_PI * log(a2) * t);
}

float NDF_GTR2(float NdotH, float a2) {
    float t = 1 + (a2 - 1) * NdotH * NdotH;
    return a2 / (_PI * t * t);
}

float NDF_GTR2_Aniso(float NoH, float XoH, float YoH, float ax, float ay) {
    return 1 / (_PI * ax * ay * Pow2(Pow2(XoH / ax) + Pow2(YoH / ay) + NoH * NoH));
}

float Vis_Smith_GGX(float NdotV, float alphaG) {
    float a = alphaG * alphaG;
    float b = NdotV * NdotV;
    return 1 / (NdotV + SqrtFast2(a + b - a * b));
}

float Vis_Smith_GGX_Aniso(float NdotV, float VdotX, float VdotY, float ax, float ay) {
    return 1 / (NdotV + SqrtFast2(Pow2(VdotX * ax) + Pow2(VdotY * ay) + Pow2(NdotV)));
}

float3 Sheen_Burley12(float3 albedo, float sheen_tint, float sheen, float f) {
    float l = Luminance(albedo);                                  // luminance approx.
    float3 c_tint = l > 0.0 ? albedo / l : float3(1.0, 1.0, 1.0); // normalize lum. to isolate hue+sat
    float3 c_sheen = mix(float3(1), c_tint, sheen_tint);
    return f * sheen * c_sheen;
}

float3 ClearCoat_Burley12(float NoL, float NoV, float NoH, float clearcoat, float clearcoat_gloss) {
    float Dr = NDF_GTR1(NoH, lerp(0.1, 0.001, clearcoat_gloss));
    float Fr = lerp(0.04, 1.0, Pow5(1.0 - NoH));
    float Gr = Vis_Smith_GGX(NoL, 0.25) * Vis_Smith_GGX(NoV, 0.25);
    return float3(0.25 * clearcoat * Dr * Fr * Gr);
}

// Moving Frostbite to Physically Based Rendering

float3 DiffuseFrostbite14(float3 albedo, float roughness, float LoH, float NoL, float NoV) {
    float linear_roughness = Pow2(roughness);
    float energy_normalization = lerp(1.0, 1.0 / 1.51, linear_roughness);
    return energy_normalization * DiffuseBurley12(albedo, roughness, LoH, NoL, NoV);
}

// Extending the Disney BRDF to a BSDF with Integrated Subsurface Scattering

// decoupled smooth and rough mat.subsurface scattering
float3 SubsurfaceBurley15(float3 albedo, float roughness, float NoL, float NoV, float LoM) {
    //
    float f_l = Pow5(1.0 - NoL); // incident light refractiion
    float f_v = Pow5(1.0 - NoV); // outgoing light refraction
    float r_r = 2.0 * roughness * LoM * LoM;
    // lambert
    float3 f_lambert = albedo * _1DIVPI;

    // retro-reflection
    float3 f_retroreflection = f_lambert * r_r * (f_l + f_v + f_l * f_v * (r_r - 1.0));

    float3 f_d = f_lambert * (1.0 - 0.5 * f_l) * (1.0 - 0.5 * f_v) + f_retroreflection;

    return f_d;
}

// unreal 5.1

// #include "../common/common_math.h"
// #include "../common/fastmath.hsl"

// float3 Diffuse_Lambert( float3 DiffuseColor )
// {
// 	return DiffuseColor * (1 / PI);
// }

// // [Burley 2012, "Physically-Based Shading at Disney"]
// float3 Diffuse_Burley( float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH )
// {
// 	float FD90 = 0.5 + 2 * VoH * VoH * Roughness;
// 	float FdV = 1 + (FD90 - 1) * Pow5( 1 - NoV );
// 	float FdL = 1 + (FD90 - 1) * Pow5( 1 - NoL );
// 	return DiffuseColor * ( (1 / PI) * FdV * FdL );
// }

// // [Gotanda 2012, "Beyond a Simple Physically Based Blinn-Phong Model in Real-Time"]
// float3 Diffuse_OrenNayar( float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH )
// {
// 	float a = Roughness * Roughness;
// 	float s = a;// / ( 1.29 + 0.5 * a );
// 	float s2 = s * s;
// 	float VoL = 2 * VoH * VoH - 1;		// double angle identity
// 	float Cosri = VoL - NoV * NoL;
// 	float C1 = 1 - 0.5 * s2 / (s2 + 0.33);
// 	float C2 = 0.45 * s2 / (s2 + 0.09) * Cosri * ( Cosri >= 0 ? rcp( max( NoL, NoV ) ) : 1 );
// 	return DiffuseColor / PI * ( C1 + C2 ) * ( 1 + Roughness * 0.5 );
// }

// // [Gotanda 2014, "Designing Reflectance Models for New Consoles"]
// float3 Diffuse_Gotanda( float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH )
// {
// 	float a = Roughness * Roughness;
// 	float a2 = a * a;
// 	float F0 = 0.04;
// 	float VoL = 2 * VoH * VoH - 1;		// double angle identity
// 	float Cosri = VoL - NoV * NoL;
// #if 1
// 	float a2_13 = a2 + 1.36053;
// 	float Fr = ( 1 - ( 0.542026*a2 + 0.303573*a ) / a2_13 ) * ( 1 - pow( 1 - NoV, 5 - 4*a2 ) / a2_13 ) * ( (
// -0.733996*a2*a + 1.50912*a2 - 1.16402*a ) * pow( 1 - NoV, 1 + rcp(39*a2*a2+1) ) + 1 );
// 	//float Fr = ( 1 - 0.36 * a ) * ( 1 - pow( 1 - NoV, 5 - 4*a2 ) / a2_13 ) * ( -2.5 * Roughness * ( 1 - NoV ) + 1
// ); 	float Lm = ( max( 1 - 2*a, 0 ) * ( 1 - Pow5( 1 - NoL ) ) + min( 2*a, 1 ) ) * ( 1 - 0.5*a * (NoL - 1) ) * NoL;
// 	float Vd = ( a2 / ( (a2 + 0.09) * (1.31072 + 0.995584 * NoV) ) ) * ( 1 - pow( 1 - NoL, ( 1 - 0.3726732 * NoV *
// NoV ) / ( 0.188566 + 0.38841 * NoV ) ) ); 	float Bp = Cosri < 0 ? 1.4 * NoV * NoL * Cosri : Cosri; 	float Lr = (21.0
// / 20.0) * (1 - F0) * ( Fr * Lm + Vd + Bp ); 	return DiffuseColor / PI * Lr; #else 	float a2_13 = a2 + 1.36053; 	float Fr
// = ( 1 - ( 0.542026*a2 + 0.303573*a ) / a2_13 ) * ( 1 - pow( 1 - NoV, 5 - 4*a2 ) / a2_13 ) * ( ( -0.733996*a2*a
// + 1.50912*a2 - 1.16402*a ) * pow( 1 - NoV, 1 + rcp(39*a2*a2+1) ) + 1 ); 	float Lm = ( max( 1 - 2*a, 0 ) * ( 1 - Pow5(
// 1 - NoL ) ) + min( 2*a, 1 ) ) * ( 1 - 0.5*a + 0.5*a * NoL ); 	float Vd = ( a2 / ( (a2 + 0.09) * (1.31072 + 0.995584 *
// NoV) ) ) * ( 1 - pow( 1 - NoL, ( 1 - 0.3726732 * NoV * NoV ) / ( 0.188566 + 0.38841 * NoV ) ) ); 	float Bp = Cosri < 0
// ? 1.4 * NoV * Cosri : Cosri / max( NoL, 1e-8 ); 	float Lr = (21.0 / 20.0) * (1 - F0) * ( Fr * Lm + Vd + Bp ); 	return
// DiffuseColor / PI * Lr; #endif
// }

// // [ Chan 2018, "Material Advances in Call of Duty: WWII" ]
// // It has been extended here to fade out retro reflectivity contribution from area light in order to avoid visual
// artefacts. float3 Diffuse_Chan( float3 DiffuseColor, float a2, float NoV, float NoL, float VoH, float NoH, float
// RetroReflectivityWeight)
// {
// 	// We saturate each input to avoid out of range negative values which would result in weird darkening at the
// edge of meshes (resulting from tangent space interpolation). 	NoV = saturate(NoV); 	NoL = saturate(NoL); 	VoH =
// saturate(VoH); 	NoH = saturate(NoH);

// 	// a2 = 2 / ( 1 + exp2( 18 * g )
// 	float g = saturate( (1.0 / 18.0) * log2( 2 / a2 - 1 ) );

// 	float F0 = VoH + Pow5( 1 - VoH );
// 	float FdV = 1 - 0.75 * Pow5( 1 - NoV );
// 	float FdL = 1 - 0.75 * Pow5( 1 - NoL );

// 	// Rough (F0) to smooth (FdV * FdL) response interpolation
// 	float Fd = lerp( F0, FdV * FdL, saturate( 2.2 * g - 0.5 ) );

// 	// Retro reflectivity contribution.
// 	float Fb = ( (34.5 * g - 59 ) * g + 24.5 ) * VoH * exp2( -max( 73.2 * g - 21.2, 8.9 ) * sqrt( NoH ) );
// 	// It fades out when lights become area lights in order to avoid visual artefacts.
// 	Fb *= RetroReflectivityWeight;

// 	return DiffuseColor * ( (1 / PI) * ( Fd + Fb ) );
// }

// // [Blinn 1977, "Models of light reflection for computer synthesized pictures"]
// float D_Blinn( float a2, float NoH )
// {
// 	float n = 2 / a2 - 2;
// 	return (n+2) / (2*PI) * pow(max(abs(NoH),POW_CLAMP),n);		// 1 mad, 1 exp, 1 mul, 1 log
// }

// // [Beckmann 1963, "The scattering of electromagnetic waves from rough surfaces"]
// float D_Beckmann( float a2, float NoH )
// {
// 	float NoH2 = NoH * NoH;
// 	return exp( (NoH2 - 1) / (a2 * NoH2) ) / ( PI * a2 * NoH2 * NoH2 );
// }

// // GGX / Trowbridge-Reitz
// // [Walter et al. 2007, "Microfacet models for refraction through rough surfaces"]
// float D_GGX( float a2, float NoH )
// {
// 	float d = ( NoH * a2 - NoH ) * NoH + 1;	// 2 mad
// 	return a2 / ( PI*d*d );					// 4 mul, 1 rcp
// }

// // Anisotropic GGX
// // [Burley 2012, "Physically-Based Shading at Disney"]
// float D_GGXaniso( float ax, float ay, float NoH, float XoH, float YoH )
// {
// // The two formulations are mathematically equivalent
// #if 1
// 	float a2 = ax * ay;
// 	float3 V = float3(ay * XoH, ax * YoH, a2 * NoH);
// 	float S = dot(V, V);

// 	return (1.0f / PI) * a2 * Pow2(a2 / S);
// #else
// 	float d = XoH*XoH / (ax*ax) + YoH*YoH / (ay*ay) + NoH*NoH;
// 	return 1.0f / ( PI * ax*ay * d*d );
// #endif
// }

// float Vis_Implicit()
// {
// 	return 0.25;
// }

// // [Neumann et al. 1999, "Compact metallic reflectance models"]
// float Vis_Neumann( float NoV, float NoL )
// {
// 	return 1 / ( 4 * max( NoL, NoV ) );
// }

// // [Kelemen 2001, "A microfacet based coupled specular-matte brdf model with importance sampling"]
// float Vis_Kelemen( float VoH )
// {
// 	// constant to prevent NaN
// 	return rcp( 4 * VoH * VoH + 1e-5);
// }

// // Tuned to match behavior of Vis_Smith
// // [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
// float Vis_Schlick( float a2, float NoV, float NoL )
// {
// 	float k = sqrt(a2) * 0.5;
// 	float Vis_SchlickV = NoV * (1 - k) + k;
// 	float Vis_SchlickL = NoL * (1 - k) + k;
// 	return 0.25 / ( Vis_SchlickV * Vis_SchlickL );
// }

// // Smith term for GGX
// // [Smith 1967, "Geometrical shadowing of a random rough surface"]
// float Vis_Smith( float a2, float NoV, float NoL )
// {
// 	float Vis_SmithV = NoV + sqrt( NoV * (NoV - NoV * a2) + a2 );
// 	float Vis_SmithL = NoL + sqrt( NoL * (NoL - NoL * a2) + a2 );
// 	return rcp( Vis_SmithV * Vis_SmithL );
// }

// // Appoximation of joint Smith term for GGX
// // [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
// float Vis_SmithJointApprox( float a2, float NoV, float NoL )
// {
// 	float a = sqrt(a2);
// 	float Vis_SmithV = NoL * ( NoV * ( 1 - a ) + a );
// 	float Vis_SmithL = NoV * ( NoL * ( 1 - a ) + a );
// 	return 0.5 * rcp( Vis_SmithV + Vis_SmithL );
// }

// // [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
// float Vis_SmithJoint(float a2, float NoV, float NoL)
// {
// 	float Vis_SmithV = NoL * sqrt(NoV * (NoV - NoV * a2) + a2);
// 	float Vis_SmithL = NoV * sqrt(NoL * (NoL - NoL * a2) + a2);
// 	return 0.5 * rcp(Vis_SmithV + Vis_SmithL);
// }

// // [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
// float Vis_SmithJointAniso(float ax, float ay, float NoV, float NoL, float XoV, float XoL, float YoV, float YoL)
// {
// 	float Vis_SmithV = NoL * length(float3(ax * XoV, ay * YoV, NoV));
// 	float Vis_SmithL = NoV * length(float3(ax * XoL, ay * YoL, NoL));
// 	return 0.5 * rcp(Vis_SmithV + Vis_SmithL);
// }

// // [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
// float3 F_Schlick( float3 SpecularColor, float VoH )
// {
// 	float Fc = Pow5( 1 - VoH );					// 1 sub, 3 mul
// 	//return Fc + (1 - Fc) * SpecularColor;		// 1 add, 3 mad

// 	// Anything less than 2% is physically impossible and is instead considered to be shadowing
// 	return saturate( 50.0 * SpecularColor.g ) * Fc + (1 - Fc) * SpecularColor;
// }

// float3 F_Schlick(float3 F0, float3 F90, float VoH)
// {
// 	float Fc = Pow5(1 - VoH);
// 	return F90 * Fc + (1 - Fc) * F0;
// }

// struct BXDF
// {
//     float NoV;
//     float NoL;
//     float VoL;
//     float NoH;
//     float VoH;
//     float XoV;
//     float XoL;
//     float XoH;
//     float YoV;
//     float YoL;
//     float YoH;
// };

// void InitBXDF( inout(BXDF) context, float3 N, float3 V, float3 L )
// {
// 	context.NoL = dot(N, L);
// 	context.NoV = dot(N, V);
// 	context.VoL = dot(V, L);
// 	float InvLenH = rsqrt( 2 + 2 * context.VoL );
// 	context.NoH = saturate( ( context.NoL + context.NoV ) * InvLenH );
// 	context.VoH = saturate( InvLenH + InvLenH * context.VoL );
// 	context.NoL = saturate( context.NoL );
// 	context.NoV = saturate( abs( context.NoV ) + 1e-5 );
// 	context.XoV = 0.0f;
// 	context.XoL = 0.0f;
// 	context.XoH = 0.0f;
// 	context.YoV = 0.0f;
// 	context.YoL = 0.0f;
// 	context.YoH = 0.0f;
// }

// void InitBXDF( inout(BXDF) context, float3 N, float3 X, float3 Y, float3 V, float3 L )
// {
// 	context.NoL = dot(N, L);
// 	context.NoV = dot(N, V);
// 	context.VoL = dot(V, L);
// 	float InvLenH = rsqrt( 2 + 2 * context.VoL );
// 	context.NoH = saturate( ( context.NoL + context.NoV ) * InvLenH );
// 	context.VoH = saturate( InvLenH + InvLenH * context.VoL );
// 	//NoL = saturate( NoL );
// 	//NoV = saturate( abs( NoV ) + 1e-5 );

// 	context.XoV = dot(X, V);
// 	context.XoL = dot(X, L);
// 	context.XoH = (context.XoL + context.XoV) * InvLenH;
// 	context.YoV = dot(Y, V);
// 	context.YoL = dot(Y, L);
// 	context.YoH = (context.YoL + context.YoV) * InvLenH;
// }
#line 3 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/shading_models.h"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/material_params_defination.hsl"

#line 4 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/shading_models.h"

// UnLit

// opaque
float3 Brdf_Opaque_Default(MaterialProperties mat, BXDF bxdf) {
    float D = NDF_GGX(mat.roughness2, bxdf.NoM);
    float G = Vis_SmithGGXCombined(mat.roughness2, bxdf.NoV, bxdf.NoL);
    float3 F = Fresnel_Schlick(mat.f0, bxdf.NoM);

    float3 diffuse = Diffuse_Lambert(mat.albedo);

    float3 specular = D * G * F;

    return diffuse + specular;
}

// Masked()

// SubSurface

// ClearCoat

// Hair

// Skin

// Eye

// cloth

float3 Brdf_Standard_Burley12(MaterialProperties mat, BXDF bxdf) {

    // gbuffer save tangent

    // float aspect = SqrtFast2(1 - mat.anisotropic * 0.9);
    // aspect = 1.0;
    // float ax = max(0.001, Pow2(mat.roughness) / aspect);
    // float ay = max(0.001, Pow2(mat.roughness) * aspect);
    // float D = NDF_GTR2_Aniso(mat.roughness2, bxdf.XoM, bxdf.YoM, ax, ay);

    // float G = Vis_Smith_GGX_Aniso(bxdf.NoV, bxdf.XoV, bxdf.YoV, ax, ay) *
    //           Vis_Smith_GGX_Aniso(bxdf.NoL, bxdf.XoL, bxdf.YoL, ax, ay);
    float D = NDF_GGX(mat.roughness2, bxdf.NoM);
    float G = Vis_SmithGGXCombined(mat.roughness2, bxdf.NoV, bxdf.NoL);
    float3 F = Fresnel_Schlick(mat.f0, bxdf.NoM);

    float3 kd = float3(1.0, 1.0, 1.0) - F;
    kd *= 1.0 - mat.metallic;

    float3 diffuse = kd * DiffuseBurley12(mat.albedo, mat.roughness, bxdf.VoM, bxdf.NoL, bxdf.NoV);

    float3 specular = D * G * F;

    return diffuse + specular;
}

float3 Brdf_Subsurface_Burley12(MaterialProperties mat, BXDF bxdf) {

    // float aspect = SqrtFast2(1 - mat.anisotropic * 0.9);
    // float ax = max(0.001, Pow2(mat.roughness) / aspect);
    // float ay = max(0.001, Pow2(mat.roughness) * aspect);
    // float D = NDF_GTR2_Aniso(mat.roughness2, bxdf.XoM, bxdf.YoM, ax, ay);

    // float G = Vis_Smith_GGX_Aniso(bxdf.NoV, bxdf.XoV, bxdf.YoV, ax, ay) *
    //           Vis_Smith_GGX_Aniso(bxdf.NoL, bxdf.XoL, bxdf.YoL, ax, ay);

    float D = NDF_GGX(mat.roughness2, bxdf.NoM);
    float G = Vis_SmithGGXCombined(mat.roughness2, bxdf.NoV, bxdf.NoL);

    float3 F = Fresnel_Schlick(mat.f0, bxdf.NoM);

    float3 kd = float3(1.0, 1.0, 1.0) - F;
    kd *= 1.0 - mat.metallic;

    float3 diffuse =
        kd * (lerp(DiffuseBurley12(mat.albedo, mat.roughness, bxdf.VoM, bxdf.NoL, bxdf.NoV),
                   SubsurfaceBurley12(mat.albedo, mat.roughness, bxdf.NoL, bxdf.NoV, bxdf.VoM), mat.subsurface));

    float3 specular = D * G * F;

    return diffuse + specular;
}

float3 Brdf_Cloth_Burley12(MaterialProperties mat, BXDF bxdf) {

    float3 standard = Brdf_Standard_Burley12(mat, bxdf);

    float f = Pow5(1.0 - bxdf.NoM);

    float3 sheen_color = (1.0 - mat.metallic) * Sheen_Burley12(mat.albedo, mat.sheen_tint, mat.sheen, f);

    return standard + sheen_color;
}

float3 Brdf_ClearCoat_Burley12(MaterialProperties mat, BXDF bxdf) {

    float3 standard = Brdf_Standard_Burley12(mat, bxdf);

    // clearcoat (ior = 1.5 -> F0 = 0.04)
    float3 clearcoat = ClearCoat_Burley12(bxdf.NoL, bxdf.NoV, bxdf.NoM, mat.clearcoat, mat.clearcoat_gloss);

    return standard + clearcoat;
}

#line 2 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/lighting.h"

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

    if (asuint(light.position_type).w == DIRECTIONAL_LIGHT) {
        light_dir = -normalize(light.direction.xyz);
        attenuation = light.color_intensity.xyz * light.color_intensity.w;
    } else if (asuint(light.position_type).w == POINT_LIGHT) {
        light_dir = light.position_type.xyz - world_pos;
        float dist = length(light_dir);
        light_dir = normalize(light_dir);

        float lightAttenuation = DistanceFalloff(dist, light.radius_inner_outer.x, light_dir);

        attenuation = lightAttenuation * light.color_intensity.xyz * light.color_intensity.w;
    } else if (asuint(light.position_type).w == SPOT_LIGHT) {

        light_dir = light.position_type.xyz - world_pos;
        float dist = length(light_dir);
        light_dir = normalize(light_dir);

        float lightAttenuation =
            DistanceFalloff(dist, light.radius_inner_outer.x, light_dir) *
            AngleFalloff(light.radius_inner_outer.y, light.radius_inner_outer.z, light.direction.xyz, light_dir);

        attenuation = lightAttenuation * light.color_intensity.xyz * light.color_intensity.w;
    }

    if (dot(n, light_dir) < 0.0) {
        return float4(0.0, 0.0, 0.0, 0.0);
    }

    BXDF bxdf;
    InitBXDF(bxdf, n, v, light_dir);

    float3 radiance = Brdf_Opaque_Default(mat, bxdf) * attenuation * bxdf.NoL;
    return float4(radiance, 1.0);
}

#line 10 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/decal.frag.hsl"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/shading/material_params_defination.hsl"

#line 11 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/decal.frag.hsl"
#line 1 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/include/translation/translation.h"
float3 ReconstructWorldPos(float4x4 inverse_vp, float depth, float2 fragCoord) {
    float2 ndc = float2(fragCoord.x * 2.0 - 1.0, 1.0 - fragCoord.y * 2.0); // unify this in different graphic api
    float4 pos =  inverse_vp * float4(ndc, depth, 1.0);
    pos.xyz /= pos.w;
    return pos.xyz; 
}

float LinearDepth(float depth, float _near, float _far)
{
    float z = depth * 2.0f - 1.0f; // �ص�NDC
    return (2.0f * _near * _far) / (_far + _near - z * (_far - _near));  
}

#line 12 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/decal.frag.hsl"
// per material resources TODO: register for dx12

struct MaterialDescription {
    uint base_color_texture_index;
    uint normal_texture_index;
    uint metallic_roughness_texture_index;
    uint emissive_textue_index;
    uint alpha_mask_texture_index;
    uint subsurface_scattering_texture_index;
    uint param_bitmask;
    uint blend_state;
    float3 base_color;
    float pad1;
    float3 emissive;
    float pad2;
    float2 metallic_roughness;
    float2 pad3;
};

#define _Getdecal_material_textures decal_material_textures
#line 31
RES(Tex2D(float4), decal_material_textures[], UPDATE_FREQ_BINDLESS, t3, binding = 0);

STRUCT(InstanceParameter)
{
    DATA(float4x4,  model, None);
    DATA(float4x4,  decal_to_world, None);
    DATA(float4x4,  world_to_decal, None);
    DATA(uint, material_id, None);
};
#line 40

#define _Getdecal_instance_parameter decal_instance_parameter
layout (std430, UPDATE_FREQ_PER_FRAME, binding = 1) readonly buffer decal_instance_parameter
{
	InstanceParameter decal_instance_parameter_data[];
};
#define decal_instance_parameter decal_instance_parameter_data

#define _Getdecal_material_descriptions decal_material_descriptions
layout (std430, UPDATE_FREQ_PER_FRAME, binding = 3) readonly buffer decal_material_descriptionsBlock
{
	MaterialDescription _data[];
} decal_material_descriptions[];

#define _Getdefault_sampler default_sampler
#line 45
RES(SamplerState, default_sampler, UPDATE_FREQ_PER_FRAME, s0, binding = 4);

#define _Getdepth_tex depth_tex
#line 47
RES(Tex2D(float), depth_tex, UPDATE_FREQ_PER_FRAME, t3, binding = 5);

CBUFFER(DecalConstants, UPDATE_FREQ_PER_FRAME, b0, binding = 6)
{
#define _Getinverse_vp inverse_vp
#line 51
    DATA(float4x4, inverse_vp, None);
#define _Getresolution resolution
#line 52
    DATA(uint4, resolution, None);
};

// PUSH_CONSTANT(ShadingModeID, b0)
// {
//     DATA(uint, shading_model_id, None);
// }

// per frame resources

STRUCT(VSOutput) 
{
#define _position_2169
#line 64
    DATA(float4, position, SV_Position);
#define _world_pos_2174
#line 65
    DATA(float3, world_pos, POSITION);
#define _normal_2179
#line 66
    DATA(float3, normal, NORMAL);
#define _uv_2184
#line 67
    DATA(float2, uv, TEXCOORD0);
#define _tangent_2189
#line 68
    DATA(float3, tangent, TANGENT);
#define _instance_id_2194
#line 69
    uint instance_id;
#ifdef VULKAN
#define _material_id_2200
#line 71
    uint material_id;
#endif

#line 73 "C:/hanyanglu/horizon/assets/C:/hanyanglu/horizon/assets/shaders/decal.frag.hsl"
};
#ifdef _world_pos_2174
layout(location = 0) in(float3) vsout_world_pos;
#endif
#ifdef _normal_2179
layout(location = 1) in(float3) vsout_normal;
#endif
#ifdef _uv_2184
layout(location = 2) in(float2) vsout_uv;
#endif
#ifdef _tangent_2189
layout(location = 3) in(float3) vsout_tangent;
#endif
#ifdef _instance_id_2194
layout(location = 4)flat  in(uint) vsout_instance_id;
#endif
#ifdef _material_id_2200
layout(location = 5)flat  in(uint) vsout_material_id;
#endif
#line 74

STRUCT(PSOutput) {
#define _gbuffer0_2230
#line 76
    DATA(float4, gbuffer0, SV_Target0);
#define _gbuffer1_2235
#line 77
    DATA(float4, gbuffer1, SV_Target0);
#define _gbuffer2_2240
#line 78
    DATA(float4, gbuffer2, SV_Target0);
#define _gbuffer3_2245
#line 79
    DATA(float4, gbuffer3, SV_Target0);
#define _gbuffer4_2250
#line 80
    DATA(float2, gbuffer4, SV_Target0);
    //DATA(uint2, vbuffer0, SV_Target0);
};
#ifdef _gbuffer0_2230
layout(location = 0) out(float4) out_PSOutput_gbuffer0;
#endif
#ifdef _gbuffer1_2235
layout(location = 1) out(float4) out_PSOutput_gbuffer1;
#endif
#ifdef _gbuffer2_2240
layout(location = 2) out(float4) out_PSOutput_gbuffer2;
#endif
#ifdef _gbuffer3_2245
layout(location = 3) out(float4) out_PSOutput_gbuffer3;
#endif
#ifdef _gbuffer4_2250
layout(location = 4) out(float2) out_PSOutput_gbuffer4;
#endif
#line 83

void main()
#line 84
//PSOutput PS_MAIN(VSOutput vsout, SV_PrimitiveID(uint) tri_id) 
{
	VSOutput vsout;
#ifdef _position_2169
	vsout.position = float4(float4(gl_FragCoord.xyz, 1.0f / gl_FragCoord.w));
#endif
#ifdef _world_pos_2174
	vsout.world_pos = vsout_world_pos;
#endif
#ifdef _normal_2179
	vsout.normal = vsout_normal;
#endif
#ifdef _uv_2184
	vsout.uv = vsout_uv;
#endif
#ifdef _tangent_2189
	vsout.tangent = vsout_tangent;
#endif
#ifdef _instance_id_2194
	vsout.instance_id = vsout_instance_id;
#endif
#ifdef _material_id_2200
	vsout.material_id = vsout_material_id;
#endif
	const uint tri_id = uint(SV_PRIMITIVEID);
#line 86
//    INIT_MAIN;
    PSOutput psout;

    // float2 uv = vsout.position.xy / float2(Get(resolution.xy));
    // float3 world_pos = ReconstructWorldPos(Get(inverse_vp), SampleTex2D(Get(depth_tex), default_sampler, uv).r, uv);

    // float4x4 world_to_decal = Get(decal_instance_parameter)[vsout.instance_id].world_to_decal;
    // float4 decal_pos = world_to_decal * float4(world_pos, 1.0);

    // decal_pos.xyz /= decal_pos.w;

    // if (decal_pos.x < -1.0 || decal_pos.x > 1.0 || decal_pos.y < -1.0 || decal_pos.y > 1.0)
    //     discard;

    // //vec2 decal_tex_coord = ;
    // //decal_tex_coord.x    = 1.0 - decal_tex_coord.x;

    // //float2 decal_tc = decal_pos * mvp /w .xy;
    // float2 decal_tc = decal_pos.xy * 0.5 + 0.5;
    // uint material_id = vsout.material_id;

    // MaterialDescription material = Get(decal_material_descriptions)[0][material_id];
    // uint param_bitmask = material.param_bitmask;

    // // uint has_metallic_roughness = param_bitmask & HAS_METALLIC_ROUGHNESS;
    // // uint has_normal = param_bitmask & HAS_NORMAL;
    // // uint has_base_color = param_bitmask & HAS_BASE_COLOR;
    // // uint has_emissive = param_bitmask & HAS_EMISSIVE;

    // float3 albedo =
    //     pow(SampleTex2D(Get(decal_material_textures)[material.base_color_texture_index], default_sampler, decal_tc).xyz,
    //         float3(2.2));
    // float alpha = 1.0;
    
    // // // uniform branching
    // // if (material.blend_state == BLEND_STATE_MASKED) {
    //     alpha =
    //         SampleLvlTex2D(Get(decal_material_textures)[material.base_color_texture_index], default_sampler, decal_tc, 0).w;
    //     if (alpha<0.5) {
    //         discard;
    //     }
    // // }

    // // float3 normal_map =
    // //     has_normal != 0
    // //         ? SampleTex2D(Get(material_textures)[material.normal_texture_index], default_sampler, vsout.uv).xyz
    // //         : float3(0.0, 0.0, 0.0); // normal map
    // // float2 mr =
    // //     has_metallic_roughness != 0
    // //         ? SampleTex2D(Get(material_textures)[material.metallic_roughness_texture_index], default_sampler, vsout.uv)
    // //               .yz
    // //         : material.metallic_roughness;
    // // float3 emissive =
    // //     has_emissive != 0
    // //         ? pow(SampleTex2D(Get(material_textures)[material.emissive_textue_index], default_sampler, vsout.uv).xyz,
    // //               float3(2.2))
    // //         : material.emissive;

    // // normal_map = (2.0 * normal_map - 1.0); // [-1, 1]

    // // float3 gbuffer_normal;

    // // if (has_normal != 0) {
    // //     // construct TBN
    // //     float3 normal = normalize(vsout.normal);
    // //     float3 tangent = normalize(vsout.tangent);
    // //     float3 bitangent = normalize(cross(tangent, normal));
    // //     // Calculate pixel normal using the normal map and the tangent space vectors
    // //     float3x3 tbn = make_f3x3_rows(tangent, bitangent, normal);
    // //     gbuffer_normal = normalize(mul(normal_map, tbn));
    // // } else {
    // //     gbuffer_normal = normalize(vsout.normal);
    // // }
    // // psout.gbuffer0 = float4(gbuffer_normal.x, gbuffer_normal.y, gbuffer_normal.z, asfloat(0));
    // // psout.gbuffer1 = float4(albedo.x, albedo.y, albedo.z, 0.0);
    // // psout.gbuffer2 = float4(emissive.x, emissive.y, emissive.z, 0.0);
    // // psout.gbuffer3 = float4(mr.y, mr.x, alpha, 0.0);


    // //psout.gbuffer0.xyz = normalize(vsout.normal); // normal
    // psout.gbuffer1.xyz = albedo; // albedo

    {
    	PSOutput out_PSOutput = psout;
#ifdef _gbuffer0_2230
    	out_PSOutput_gbuffer0 = out_PSOutput.gbuffer0;
#endif
#ifdef _gbuffer1_2235
    	out_PSOutput_gbuffer1 = out_PSOutput.gbuffer1;
#endif
#ifdef _gbuffer2_2240
    	out_PSOutput_gbuffer2 = out_PSOutput.gbuffer2;
#endif
#ifdef _gbuffer3_2245
    	out_PSOutput_gbuffer3 = out_PSOutput.gbuffer3;
#endif
#ifdef _gbuffer4_2250
    	out_PSOutput_gbuffer4 = out_PSOutput.gbuffer4;
#endif
    	return;
    }
#line 168
//    RETURN(psout);
}
