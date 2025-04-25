# Example usage: python GenProjectFile.py -Project="Path"

import os
import sys

# Get the directory of the current file
engine_path = os.path.abspath(os.path.join(__file__, os.pardir, os.pardir))
project_path = None



# Parse input parameter -Project="Path" to project_path

for arg in sys.argv:
    if arg.startswith("-Project="):
        project_path = arg.split("=", 1)[1].strip('"')
        break

if not project_path:
    raise ValueError("Missing required parameter: -Project=\"Path\"")

engine_path = engine_path.replace("\\", "/")
project_path = project_path.replace("\\", "/")

print("PROJECT PATH:", project_path)
# Run cmake with Dengine_path set to engine_path
project_build_path = os.path.join(project_path, "build")

os.makedirs(project_build_path, exist_ok=True)

command = f'cmake -S "{project_path}" \
    -DENGINE_PATH="{engine_path}" \
    -DPROJECT_PATH="{project_path}" \
    -B "{project_build_path}"'
print("Executing command:", command)
os.system(command)
