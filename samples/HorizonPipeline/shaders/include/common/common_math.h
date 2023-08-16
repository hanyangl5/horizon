#define _PI 3.141592654f
#define _2PI 6.283185307f
#define _1DIVPI 0.318309886f
#define _1DIV2PI 0.159154943f
#define _PIDIV2 1.570796327f
#define _PIDIV4 0.785398163f

#define POW_CLAMP 0.000001f

float Pow2(float x) { return x * x; }

float Pow3(float x) { return x * x * x; }

float Pow4(float x) { return x * x * x * x; }

float Pow5(float x) { return x * x * x * x * x; }

float SmoothStep(float e0, float e1, float x) {
    float t = saturate((x - e0) / e1 - e0);
    return t * t * (3.0 - 2.0 * t);
}