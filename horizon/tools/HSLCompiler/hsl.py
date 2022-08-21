

"""
HSL shader generator tool
"""

import os, sys, argparse, subprocess, time
from inspect import currentframe, getframeinfo

# add hsl roots to path
hsl_root = os.path.sep.join(os.path.abspath(__file__).split(os.path.sep)[:-1])
horizon_root = os.path.dirname(os.path.dirname(os.path.dirname(hsl_root)))
hsl_orbis_root    = os.path.join(horizon_root, 'PS4', 'Common_3', 'Tools', 'HorizonShadingLanguage')
hsl_prospero_root = os.path.join(horizon_root, 'Prospero', 'Common_3', 'Tools', 'HorizonShadingLanguage')
hsl_xbox_root     = os.path.join(horizon_root, 'Xbox', 'Common_3', 'Tools', 'HorizonShadingLanguage')
sys.path.extend( [hsl_root, hsl_orbis_root, hsl_prospero_root, hsl_xbox_root])

# set default compiler paths
# if not 'HSL_COMPILER_FXC' in os.environ:
#     os.environ['HSL_COMPILER_FXC'] = os.path.normpath('C:/Program Files (x86)/Windows Kits/10.0.17763.0/x64')
if not 'HSL_COMPILER_DXC' in os.environ:
    os.environ['HSL_COMPILER_DXC'] = os.path.normpath("../3rd_party/DirectXShaderCompiler/bin/x64")


from utils import *
import generators, compilers

def get_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--destination', help='output directory', required=True)
    parser.add_argument('-b', '--binaryDestination', help='output directory', required=True)
    parser.add_argument('-l', '--language', help='language, defaults to all')
    parser.add_argument('hsl_input', help='hsl file to generate from')
    parser.add_argument('--verbose', default=False, action='store_true')
    parser.add_argument('--compile', default=False, action='store_true')
    parser.add_argument('--rootSignature', default=None)
    parser.add_argument('--incremental', default=False, action='store_true')
    args = parser.parse_args()
    args.language = args.language.split()
    return args

def main():
    args = get_args()

    ''' collect shader languages '''
    languages = []
    for language in args.language:
        hsl_assert(language in [l.name for l in Languages], filename=args.hsl_input, message='Invalid target language {}'.format(language))
        languages += [Languages[language]]

    class Gen:
        def __init__(self, g,c):
            self.gen, self.compile = g,c
        gen = None
        compile = None

    gen_map = {
        #Languages.DIRECT3D11:       Gen(generators.d3d,    compilers.d3d11),
        Languages.DIRECT3D12:       Gen(generators.d3d12,  compilers.d3d12),
        Languages.VULKAN:      Gen(generators.vulkan, compilers.vulkan),
        #Languages.METAL:       Gen(generators.metal,  compilers.metal),
        #Languages.ORBIS:       Gen(generators.pssl,   compilers.orbis),
        #Languages.PROSPERO:    Gen(generators.prospero,   compilers.prospero),
        #Languages.XBOX :       Gen(generators.xbox,   compilers.xbox),
        #Languages.SCARLETT :   Gen(generators.scarlett,   compilers.scarlett),
        #Languages.GLES :       Gen(generators.gles,   compilers.gles),
    }

    # Seperate folders per render API
    folder_map = {
        #Languages.DIRECT3D11:       "DIRECT3D11",
        Languages.DIRECT3D12:       "DIRECT3D12",
        Languages.VULKAN:           "VULKAN",
        #Languages.METAL:            "",
        #Languages.ORBIS:            "",
        #Languages.PROSPERO:         "",
        #Languages.XBOX :            "DIRECT3D12",
        #Languages.SCARLETT :        "DIRECT3D12",
        #Languages.GLES :            "GLES",
    }

    if args.hsl_input.endswith('.h.hsl'):
        return 0

    if not os.path.exists(args.hsl_input):
        print(__file__+'('+str(currentframe().f_lineno)+'): error HSL: Cannot open source file \''+args.hsl_input+'\'')
        sys.exit(1)

    hsl_assert(args.destination, filename=args.hsl_input, message='Missing destionation directory')

    for language in languages:
        out_filename = os.path.basename(args.hsl_input).replace('.hsl', '')

        # if language == Languages.METAL:
        #     out_filename = out_filename.replace('.tesc', '.tesc.comp')
        #     out_filename = out_filename.replace('.tese', '.tese.vert')
        #     out_filename += '.metal'

        # Create per-language subdirectories
        dst_dir = os.path.join(args.destination, folder_map[language])
        os.makedirs(dst_dir, exist_ok=True)
        out_filepath = os.path.normpath(os.path.join(dst_dir, out_filename)).replace(os.sep, '/')

        if args.incremental and (max_timestamp(args.hsl_input) < max_timestamp(out_filepath) and os.path.exists(out_filepath) ):
            continue

        if args.verbose:
            print('HSL: Generating {}, from {}'.format(language.name, args.hsl_input))

        rootSignature = None
        if args.rootSignature and args.rootSignature != 'None':
            assert os.path.exists(args.rootSignature)
            rootSignature = open(args.rootSignature).read()
            status = gen_map[language].gen(args.hsl_input, out_filepath, rootSignature=rootSignature)
        else:
            status = gen_map[language].gen(args.hsl_input, out_filepath)
        if status != 0: return 1

        if args.compile:
            hsl_assert(dst_dir, filename=args.hsl_input, message='Missing destination binary directory')
            if not os.path.exists(args.binaryDestination): os.makedirs(args.binaryDestination)
            #bin_filepath = os.path.join( args.binaryDestination, os.path.basename(out_filepath) )
            bin_filepath = args.hsl_input + "." + str(folder_map[language])
            
            status = gen_map[language].compile(out_filepath, bin_filepath)
            if status != 0: return 1

    return 0

if __name__ == '__main__':
    sys.exit(main())