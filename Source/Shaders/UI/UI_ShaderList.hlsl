#frag FT_MULTIVIEW imgui_SAMPLE_COUNT_1.frag
#define SAMPLE_COUNT 1
#include "imgui.frag.hlsl"
#end
#frag FT_MULTIVIEW imgui_SAMPLE_COUNT_2.frag
#define SAMPLE_COUNT 2
#include "imgui.frag.hlsl"
#end
#frag FT_MULTIVIEW imgui_SAMPLE_COUNT_4.frag
#define SAMPLE_COUNT 4
#include "imgui.frag.hlsl"
#end
#frag FT_MULTIVIEW imgui_SAMPLE_COUNT_8.frag
#define SAMPLE_COUNT 8
#include "imgui.frag.hlsl"
#end
#frag FT_MULTIVIEW imgui_SAMPLE_COUNT_16.frag
#define SAMPLE_COUNT 16
#include "imgui.frag.hlsl"
#end

#vert FT_VDP FT_MULTIVIEW imgui.vert
#include "imgui.vert.hlsl"
#end

#frag textured_mesh.frag
#include "textured_mesh.frag.hlsl"
#end

#vert textured_mesh.vert
#include "textured_mesh.vert.hlsl"
#end
