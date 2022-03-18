import sys
import os

def glslc(input,output):
    cmd="glslc"+ " " + input + " -o " + output
    os.system(cmd)

def main():
    path, _ = os.path.split(os.path.abspath(sys.argv[0]))
    glslc(os.path.join(path,"defaultlit.vert"),os.path.join(path,"defaultlit.vert.spv"))
    glslc(os.path.join(path,"defaultlit.frag"),os.path.join(path,"defaultlit.frag.spv"))
    
if __name__ == '__main__':
    main()
