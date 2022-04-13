import sys
import os


path, _ = os.path.split(os.path.abspath(sys.argv[0]))

def glslc(shaderPath):
    input = os.path.join(path, shaderPath)
    output = os.path.join(os.path.join(path, "spirv"), shaderPath) + ".spv"
    cmd="glslc"+ " " + input + " -o " + output
    os.system(cmd)

def main():
    # glslc("defaultlit.vert")
    # glslc("defaultlit.frag")
    glslc("postprocess.vert")
    glslc("postprocess.frag")
    glslc("geometry.vert")
    glslc("geometry.frag")
    glslc("scatter.vert")
    glslc("scatter.frag")
    glslc("present.vert")
    glslc("present.frag")
    # glslc("scatter.vert")
    # glslc("scatter.frag")
    
if __name__ == '__main__':
    main()
