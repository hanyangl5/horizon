/*
we can generate model from dcc program, and when import mesh into horizon, we
specify the mesh as clustered and non clustered for clustered meshed, we use a
gpu-driven culling, rasterize, draw call render pipeline for none clustered
mesh, we use a original render pipeline

horzion mesh description v0.1

// a 16 byte vertex description
struct Vertex {
    vec3 pos;
    f32 pad0;
    vec3 normal;
    f32 pad1;
    vec4 tbn;
    vec2 uv1, uv2;
}

// mesh description for horizon runtime

struct MeshNode{
  u32 index_offset;
  u32 index_count;
  u32 material_id;
  u32 parent_id;
  u32* childs_id;
  u32[2] pad0;
};

struct Mesh{
  u32 vertices_count;
  u32 indices_count;
  u32 stride;
  f32 aabb[6];
  MeshNode* mesh_node_arr;
  Vertex* vertices;
  Indices* indices;
  Texture* textures;
  Material* materials;
  u32 pad0;
}
*/