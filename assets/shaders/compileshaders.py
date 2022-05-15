import sys
import os


path, _ = os.path.split(os.path.abspath(sys.argv[0]))

def glslc(shaderPath):
    input = os.path.join(path, shaderPath)
    output = os.path.join(os.path.join(path, "spirv"), shaderPath) + ".spv"
    cmd="glslc"+ " " + input + " -o " + output
    os.system(cmd)

def main():
    glslc("postprocess.vert")
    glslc("postprocess.frag")
    glslc("geometry.vert")
    glslc("geometry.frag")
    glslc("present.vert")
    glslc("present.frag")
    glslc("simplevs.vert")

    # atmosphere
    glslc("atmosphere/transmittance_lut.comp")
    glslc("atmosphere/direct_irradiance_lut.comp")
    glslc("atmosphere/single_scattering_lut.comp")
    glslc("atmosphere/scattering_density.comp")
    glslc("atmosphere/indirect_irradiance_lut.comp")
    glslc("atmosphere/multi_scattering_lut.comp")
    glslc("atmosphere/scatter.vert")
    glslc("atmosphere/scatter.frag")

if __name__ == '__main__':
    main()
