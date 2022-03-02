import sys
import os

def glslc(input,output):
    cmd="glslc"+ " " + input + " -o " + output
    os.system(cmd)

def main():
    path, _ = os.path.split(os.path.abspath(sys.argv[0]))
    # glslc(os.path.join(path,"basicvert.vert"),os.path.join(path,"basicvert.spv"))
    # glslc(os.path.join(path,"basicfrag.frag"),os.path.join(path,"basicfrag.spv"))
    # glslc(os.path.join(path,"vertexshader.vert"),os.path.join(path,"vertexshader.spv"))
    # glslc(os.path.join(path,"fragshader.frag"),os.path.join(path,"fragshader.spv"))
    glslc(os.path.join(path,"model.vert"),os.path.join(path,"model.vert.spv"))
    glslc(os.path.join(path,"model.frag"),os.path.join(path,"model.frag.spv"))
    
if __name__ == '__main__':
    main()
