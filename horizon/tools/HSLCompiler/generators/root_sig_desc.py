

from ply.lex import TOKEN
import ply.lex as lex
from json import JSONEncoder
import re
import json
import sys
sys.path.insert(0, '../')
from utils import Descriptor, PushConstant, RootSignatureDesc, getMacro, ShaderResourceDescriptorType, PushConstantShaderStage, get_stage_from_entry, Stages

# should consistant with defination in cpp code
def GetIntShaderType(shader_stage):
    if shader_stage == Stages.VERT:
        return 1
    if shader_stage == Stages.FRAG:
        return 2
    if shader_stage == Stages.COMP:
        return 4

def StrToDescriptorType(in_str):
    if in_str.startswith('SamplerState'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_SAMPLER
    elif in_str.startswith('Buffer'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_BUFFER
    elif in_str.startswith('WBuffer'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_RW_BUFFER
    elif in_str.startswith('RWBuffer'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_RW_BUFFER
    elif in_str.startswith('ByteBuffer'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_UNDEFINED # TODO: 
    elif in_str.startswith('RWByteBuffer'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_UNDEFINED # TODO:
    elif in_str.startswith('Tex1D'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_TEXTURE
    elif in_str.startswith('Tex2D'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_TEXTURE
    elif in_str.startswith('Tex3D'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_TEXTURE
    elif in_str.startswith('TexCube'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_TEXTURE
        # rw texture
    elif in_str.startswith('RTex1D'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_RW_TEXTURE
    elif in_str.startswith('RTex2D'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_RW_TEXTURE
    elif in_str.startswith('RTex3D'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_RW_TEXTURE
    elif in_str.startswith('RTexCube'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_RW_TEXTURE

    elif in_str.startswith('WTex1D'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_RW_TEXTURE
    elif in_str.startswith('WTex2D'):   
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_RW_TEXTURE  
    elif in_str.startswith('WTex3D'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_RW_TEXTURE
    elif in_str.startswith('WTexCube'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_RW_TEXTURE
                
    elif in_str.startswith('RWTex1D'):
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_RW_TEXTURE
    elif in_str.startswith('RWTex2D'):  
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_RW_TEXTURE
    elif in_str.startswith('RWTex3D'):  
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_RW_TEXTURE
    elif in_str.startswith('RWTexCube'):    
        return ShaderResourceDescriptorType.DESCRIPTOR_TYPE_RW_TEXTURE
        # TODO texture array

def getFloatSize1(data_type):
    _map = {
    'bool'      : 1,
    'int'       : 1,
    'int2'      : 2, 
    'int3'      : 4,
    'int4'      : 4,
    'uint'      : 1,
    'uint2'     : 2,
    'uint3'     : 4,
    'uint4'     : 4,
    'float'     : 1,
    'float2'    : 2,
    'float3'    : 4,
    'float4'    : 4,
    'float2x2'  : 4,
    'float3x3'  : 12,
    'float4x4'  : 16,
    'double2'   : 2,
    'double3'   : 4,
    'double4'   : 4,
    'double2x2' : 4,
    'double3x3' : 12,
    'double4x4' : 16,
    }
    return _map[data_type]

def generate_root_signature_description(text, out_path):
    
    parsing_pushconstant = None
    push_constant_size = 0
    root_signature_desc = RootSignatureDesc()
    lines = text.splitlines()
    shader_stage, _ = get_stage_from_entry(text)
    shader_stage = GetIntShaderType(shader_stage)

    for l in lines:
        elements = getMacro(l)
        if l.startswith('CBUFFER'):
            assert (len(elements) == 4)
            [name, freq, reg, binding] = elements
            binding = int(binding.split('=')[1])
            root_signature_desc.data[freq].append(
                Descriptor(name, ShaderResourceDescriptorType.DESCRIPTOR_TYPE_CONSTANT_BUFFER, binding, reg))
        elif l.startswith('RES'):
            assert (len(elements) == 5)
            [ds_type, name, freq, reg, binding] = elements
            binding = int(binding.split('=')[1])
            root_signature_desc.data[freq].append(
                Descriptor(name, StrToDescriptorType(ds_type), binding,  reg))

        # parse root constant / push constant
        if l.startswith('PUSH_CONSTANT'):
            parsing_pushconstant = tuple(getMacro(l))
        if 'DATA' in l and parsing_pushconstant:
            push_constant_size += 4 * getFloatSize1(getMacro(l)[0])
        if '};' in l and parsing_pushconstant:
            root_signature_desc.data['PUSH_CONSTANT'].append(PushConstant(parsing_pushconstant[0], push_constant_size, parsing_pushconstant[1], shader_stage))
            parsing_pushconstant = None
            push_constant_size = 0


    with open(out_path, 'w', encoding='UTF-8') as fp:
        fp.write(json.dumps(root_signature_desc.data,
                 indent=2, ensure_ascii=False, default = lambda obj : obj.__dict__))
        print("")
