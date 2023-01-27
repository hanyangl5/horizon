
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

#include "common_math.hlsl"

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