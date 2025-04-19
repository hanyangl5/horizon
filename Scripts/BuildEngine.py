import os
import subprocess

# Define directories
build_dir = "build"
msvc_build_dir = os.path.join(build_dir, "msvc")
clang_build_dir = os.path.join(build_dir, "clang")
msvc_log_file = os.path.join(msvc_build_dir, "vs_build.log")
clang_log_file = os.path.join(clang_build_dir, "ninja_build.log")

# Ensure build directories exist
os.makedirs(msvc_build_dir, exist_ok=True)
os.makedirs(clang_build_dir, exist_ok=True)

# Generate Visual Studio project
print("Generating Visual Studio project...")
vs_gen_cmd = ["cmake", "-G", "Visual Studio 17 2022", "-DCMAKE_INCLUDE_SHADERS=false", "-B", msvc_build_dir]
with open(msvc_log_file, "w") as vs_log:
    subprocess.run(vs_gen_cmd, stdout=vs_log, stderr=subprocess.STDOUT, check=True)

# Build Visual Studio project
print("Building Visual Studio project...")
vs_build_cmd = ["cmake", "--build", msvc_build_dir, "--config", "Debug"]
with open(msvc_log_file, "a") as vs_log:
    subprocess.run(vs_build_cmd, stdout=vs_log, stderr=subprocess.STDOUT, check=True)

# Generate Clang Ninja project
print("Generating Clang Ninja project...")
ninja_gen_cmd = ["cmake", "-G", "Ninja", "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++", "-DCMAKE_INCLUDE_SHADERS=false", "-DCMAKE_TYPE=Debug", "-B", clang_build_dir]
with open(clang_log_file, "w") as ninja_log:
    subprocess.run(ninja_gen_cmd, stdout=ninja_log, stderr=subprocess.STDOUT, check=True)

# Build Clang Ninja project
print("Building Clang Ninja project...")
ninja_build_cmd = ["cmake", "--build", clang_build_dir]
with open(clang_log_file, "a") as ninja_log:
    subprocess.run(ninja_build_cmd, stdout=ninja_log, stderr=subprocess.STDOUT, check=True)

print("Build process completed. Logs saved in the respective build directories.")