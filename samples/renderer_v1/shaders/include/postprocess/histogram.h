
#define EPSILON 0.005
#define GROUP_SIZE 256

uint HDRToHistogramBin(float3 color, float maxLuminance) {
    float luminance = Luminance(color);

    if (luminance < EPSILON) {
        return 0;
    }
    // remap to an 256 uint index
    float logLuminance = saturate(luminance / maxLuminance);
    //float logLuminance = saturate((log2(luminance) - minLogLuminance) * oneOverLogLuminanceRange);
    return uint(logLuminance * 254.0f + 1.0f);
}