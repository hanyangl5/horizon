#define PI 3.14159265359
#define eps 1e-6
#define MAX 1e6
#define rad 1.0

#define TRANSMITTANCE_TEXTURE_WIDTH  256
#define TRANSMITTANCE_TEXTURE_HEIGHT  64
#define SCATTERING_TEXTURE_R_SIZE  32
#define SCATTERING_TEXTURE_MU_SIZE  128
#define SCATTERING_TEXTURE_MU_S_SIZE  32
#define SCATTERING_TEXTURE_NU_SIZE  8
#define SCATTERING_TEXTURE_WIDTH  SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_MU_S_SIZE
#define SCATTERING_TEXTURE_HEIGHT  SCATTERING_TEXTURE_MU_SIZE
#define SCATTERING_TEXTURE_DEPTH  SCATTERING_TEXTURE_R_SIZE
#define IRRADIANCE_TEXTURE_WIDTH  64
#define IRRADIANCE_TEXTURE_HEIGHT  16

struct DensityProfileLayer {
    float width;
    float exp_term;
    float exp_scale;
    float linear_term;
    float constant_term;
};

struct DensityProfile {
    DensityProfileLayer layers[2];
};

struct AtmosphereParameters {
    float bottom_radius;
    float top_radius;
    float mie_g;
    float sun_angular_radius;
    vec3 solar_irradiance;
    vec3 rayleigh_scattering, mie_scattering, mie_extinction, absorption_extinction;
    DensityProfile rayleigh_density, mie_density, absorption_density;
    float mu_s_min;
    vec3 ground_albedo;
};

#define COMBINED_SCATTERING_TEXTURES

#define Length float
#define Wavelength float
#define Angle float
#define SolidAngle float
#define Power float
#define LuminousPower float

#define Number float
#define InverseLength float
#define Area float
#define Volume float
#define NumberDensity float
#define Irradiance float
#define Radiance float
#define SpectralPower float
#define SpectralIrradiance float
#define SpectralRadiance float
#define SpectralRadianceDensity float
#define ScatteringCoefficient float
#define InverseSolidAngle float
#define LuminousIntensity float
#define Luminance float
#define Illuminance float

// A generic function from Wavelength to some other type.
#define AbstractSpectrum vec3
// A function from Wavelength to Number.
#define DimensionlessSpectrum vec3
// A function from Wavelength to SpectralPower.
#define PowerSpectrum vec3
// A function from Wavelength to SpectralIrradiance.
#define IrradianceSpectrum vec3
// A function from Wavelength to SpectralRadiance.
#define RadianceSpectrum vec3
// A function from Wavelength to SpectralRadianceDensity.
#define RadianceDensitySpectrum vec3
// A function from Wavelength to ScaterringCoefficient.
#define ScatteringSpectrum vec3

// A position in 3D (3 length values).
#define Position vec3
// A unit direction vector in 3D (3 unitless values).
#define Direction vec3
// A vector of 3 luminance values.
#define Luminance3 vec3
// A vector of 3 illuminance values.
#define Illuminance3 vec3

/*
<p>Finally, we also need precomputed textures containing physical quantities in
each texel. Since we can't define custom sampler types to enforce the
homogeneity of expressions at compile time in GLSL, we define these texture
types as <code>sampler2D</code> and <code>sampler3D</code>, with preprocessor
macros. The full definitions are given in the
<a href="reference/definitions.h.html">C++ equivalent</a> of this file).
*/

#define TransmittanceTexture sampler2D
#define AbstractScatteringTexture sampler3D
#define ReducedScatteringTexture sampler3D
#define ScatteringTexture sampler3D
#define ScatteringDensityTexture sampler3D
#define IrradianceTexture sampler2D