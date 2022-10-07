HSL(Horizon Shading Language) is a modified version of [HSL(ForgeShadingLanguage)](https://github.com/ConfettiFX/The-Forge/wiki/The-Forge-Shading-Language-(FSL))

### Shader Resource Binding in Horizon

Previously I adopt FSL as shading language for cross platform usage. FSL expose the update frequency and binding/register when declare shader resources.

compared with reflect pipeline layout / root signature from compiled shaders (spirv and dxil), I thinks its better to parse the shader source file directly,

so a .rsd file are generated after shader compiled 