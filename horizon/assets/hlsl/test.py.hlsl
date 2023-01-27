import os

for i, e in enumerate(os.listdir()):
    
    e1 = e.split('.hsl')[0]+".hlsl"
    print(e1)
    os.rename(e, e1)