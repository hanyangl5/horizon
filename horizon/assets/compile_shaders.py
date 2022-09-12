def CompileShaders():
    import os, sys
    _filedir = os.path.abspath(os.path.dirname(__file__))
    # directly import hsl as module, calling the tool directly is much faster that way than spawning a process for each invocation
    sys.path.append("../tools/HSLCompiler")
    from hsl import main

    argv = sys.argv[:]

    output_dir = 'generated_shaders'
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    languages = ['VULKAN']
    
    shader_dir = "shaders"
    for lang in languages:
        #project = os.path.normpath(shader_dir).split(os.path.sep)[-3]
        shader_dir = os.path.join(_filedir, shader_dir)
        for hsl_file in [os.path.join(shader_dir, _file) for _file in os.listdir(shader_dir) if _file.endswith('.hsl')]:
            print("compile hsl:", hsl_file)
            sys.argv = argv[:]
            sys.argv += ['-d', os.path.join(output_dir, lang)]
            sys.argv += ['-b', shader_dir]
            sys.argv += ['-l', lang]
            sys.argv += ['--compile']
            sys.argv += [hsl_file]

            status = main()
            if status != 0: return status
    return 0

if __name__ == '__main__':
	print(CompileShaders())