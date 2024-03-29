# Precomputed Atmospheric Scattering

![](https://github.com/hanyangl5/horizon/blob/main/docs/figs/samples/atmosphere.png?raw=true)

## Build status

| Platform |       Clang        |        MSVC        |
| -------- | :----------------: | :----------------: |
| Windows  | :heavy_check_mark: | :heavy_check_mark: |


## Build From Source

On Windows:

- Vulkan SDK 1.1
- CMake 3.10
- Git

clone the branch PrecomputeAtmosphericScattering

```
git clone -b PrecomputeAtmosphericScattering https://github.com/hanyangl5/horizon.git
```

use CMake to generate solution file

```
cmake -D build .
```

build and run `example/atmosphere`


## Other Features

latest update is under branch [`main`](https://github.com/hanyangl5/horizon/tree/main)

- Physically Based Rendering
  - physical light unit
  - physical camera and exposure
  - pbr shading with energy compensation
  - IBL diffuse irradiance with spherical harmonics, specular with split sum approximation
 
![](https://raw.githubusercontent.com/hanyangl5/horizon/main/docs/figs/samples/pbs.png)

- SSAO
  - ssao
  - gaussian blur
 
![](https://raw.githubusercontent.com/hanyangl5/horizon/main/docs/figs/samples/ssao.png)

- Temoral Antialiasing

![](https://raw.githubusercontent.com/hanyangl5/horizon/main/docs/figs/samples/taa.png)
