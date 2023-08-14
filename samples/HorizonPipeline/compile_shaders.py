# -*- coding=utf-8 -*-
import os, sys
import shutil

def delete_folder(folder_path):
    try:
        shutil.rmtree(folder_path)
        print(f"Folder '{folder_path}' deleted successfully.")
    except OSError as e:
        print(f"Error deleting folder '{folder_path}': {e}")

from threading import Thread
_filedir = os.path.abspath(os.path.dirname(__file__))
# directly import hsl as module, calling the tool directly is much faster that way than spawning a process for each invocation
sys.path.append("../../tools/HSLCompiler")
from hsl import main

argv = sys.argv[:]

output_dir = 'shaders'

languages = ['VULKAN']

delete_folder('shaders/generated')
delete_folder('shaders/bin')

if not os.path.exists('shaders/generated/rsd'):
    os.makedirs('shaders/generated/rsd')
if not os.path.exists('shaders/bin'):
    os.makedirs('shaders/bin')
if not os.path.exists('shaders/bin/VULKAN'):
    os.makedirs('shaders/bin/VULKAN')

shader_dir = "shaders"

def compile_hsl(lang, hsl_file):
    print("compile hsl:", hsl_file)
    sys.argv = argv[:]
    sys.argv += ['-d', os.path.join('shaders','generated')]
    sys.argv += ['-b', os.path.join('shaders', 'bin', lang)]
    sys.argv += ['-l', lang]
    sys.argv += ['--compile']
    sys.argv += [hsl_file]
    status = main()
    if status != 0: return status

threads = []
for lang in languages:
    #project = os.path.normpath(shader_dir).split(os.path.sep)[-3]
    shader_dir = os.path.join(_filedir, shader_dir)

    for hsl_file in [os.path.join(shader_dir, _file) for _file in os.listdir(shader_dir) if _file.endswith('.hsl')]:
        compile_hsl(lang, hsl_file)



