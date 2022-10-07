

from ply.lex import TOKEN
import ply.lex as lex
from json import JSONEncoder
import re
import json
import sys
sys.path.insert(0, '../')
from utils import Descriptor, DescriptorSets, getMacro, serialize_descriptor, ShaderResourceDescriptorType


class HSLLexer(object):
    tokens = (

        #'PUSH_CONSTANT',
        'RES',
        'CBUFFER',
        #'DATA',
        #'UPDATE_FREQ_NONE',
        #'UPDATE_FREQ_PER_FRAME',
        #'UPDATE_FREQ_PER_BATCH',
        #'UPDATE_FREQ_PER_DRAW',
        #'BINDING',
        #'register',
        #'LPAREN',
        #'RPAREN',
    )

    # recognize keyword: CBUFFER(...), PUSH_CONSTANT(...) and RES(...).
    #t_PUSH_CONSTANT = r'PUSH_CONSTANT'
    t_RES = r'RES'
    t_CBUFFER = r'CBUFFER'
    #t_DATA = r'DATA'
    #t_BINDING = r'binding = \d+'
    #t_register = r'[buts]\d+'
    #t_UPDATE_FREQ_NONE = r'UPDATE_FREQ_NONE'
    #t_UPDATE_FREQ_PER_FRAME = r'UPDATE_FREQ_PER_FRAME'
    #t_UPDATE_FREQ_PER_BATCH = r'UPDATE_FREQ_PER_BATCH'
    #t_UPDATE_FREQ_PER_DRAW = r'UPDATE_FREQ_PER_DRAW'

    # t_LPAREN  = r'\('
    # t_RPAREN  = r'\)'
    # t_COMMA = r','
    # t_LBRACE = r'\{'
    # t_RBRACE = r'\}'
    # t_SEG = r';'

    def t_newline(self, t):
        r'\n+'
        t.lexer.lineno += len(t.value)

    def t_error(self, t):
        #print("Illegal character '%s'" % t.value[0])
        t.lexer.skip(1)

    # C or C++ comment (ignore)
    def t_ccode_comment(self, t):
        r'(/\*(.|\n)*?\*/)|(//.*)'
        pass

    # Build the lexer
    def build(self, **kwargs):
        self.lexer = lex.lex(module=self, **kwargs)
        self.tks = []

    def test(self, data):
        self.lexer.input(data)
        while True:
            tok = self.lexer.token()
            if not tok:
                break
            #print(tok)
            self.tks.append(tok)

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

def generate_root_signature_description(text, out_path):
    sets_descriptions = DescriptorSets()
    json_blob = []
    hsl_lexer = HSLLexer()
    hsl_lexer.build()
    hsl_lexer.test(text)
    lines = text.splitlines()
    for i in range(len(hsl_lexer.tks)):
        lines[i] = lines[hsl_lexer.tks[i].lineno-1].replace(" ", "")
    lines = list(set(lines[0:len(hsl_lexer.tks)]))
    for l in lines:
        elements = getMacro(l)
        if l.startswith('CBUFFER'):
            assert (len(elements) == 4)
            [name, freq, reg, binding] = elements
            binding = binding.split('=')[1]
            sets_descriptions.data[freq].append(
                Descriptor(name, ShaderResourceDescriptorType.DESCRIPTOR_TYPE_CONSTANT_BUFFER, binding, reg))
        elif l.startswith('RES'):
            assert (len(elements) == 5)
            [ds_type, name, freq, reg, binding] = elements
            binding = binding.split('=')[1]
            sets_descriptions.data[freq].append(
                Descriptor(name, StrToDescriptorType(ds_type), binding,  reg))
        # TODO: push constant

    with open(out_path, 'w', encoding='UTF-8') as fp:
        fp.write(json.dumps(sets_descriptions.data,
                 indent=2, ensure_ascii=False, default= serialize_descriptor))
        print("")
