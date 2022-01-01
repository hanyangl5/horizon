import sys
import os

def glslc(input,output):
    cmd="glslc"+ " " + input + " -o " + output
    os.system(cmd)

def main():
    path, _ = os.path.split(os.path.abspath(sys.argv[0]))
    glslc(os.path.join(path,"basicvert.vert"),os.path.join(path,"basicvert.spv"))
    glslc(os.path.join(path,"basicfrag.frag"),os.path.join(path,"basicfrag.spv"))
    
if __name__ == '__main__':
    main()
