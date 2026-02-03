// #pragma once
// #include "rhi/rhi.h"

// enum class RDGResourceType
// {
//     TEXTURE,
//     BUFFER,
//     RENDER_TARGET,
// };

// class RDGResource
// {
//   public:
//     RDGResource(std::string name);
//     ~RDGResource();
//     bool IsValid() const { return index != UINT32_MAX; }
//     static RDGResource Invalid() { return {UINT32_MAX}; }
//     u32 index;
//     std::string m_name;
// };

// // Resource handle types
// struct TextureHandle : public RDGResource
// {
// };

// struct BufferHandle : public RDGResource
// {
// };

// struct RenderTargetHandle : public RDGResource
// {
// };

// class RDGPass
// {
//   public:
//     RDGPass(std::string name, const RDG& rdg);
//     ~RDGPass();
//     virtual void Setup();
//     virtual void Execute(CommandList *command_list);
//     RDGResource * DeclareResource(RDGResourceType type, std::string name, bool import = false, const std::string&
//     import_pass_name = "", const std::string& import_resource_name = "");

//     const RDG& m_rdg;
//     std::string m_name;
//     std::vector<RDGResource *> m_resources; //owned resources

//     // Resource dependencies
//     std::vector<TextureHandle> read_textures;
//     std::vector<TextureHandle> write_textures;
//     std::vector<BufferHandle> read_buffers;
//     std::vector<BufferHandle> write_buffers;
//     std::vector<RenderTargetHandle> render_targets;

//     // Resource usage information
//     std::unordered_map<u32, ResourceUsage> texture_usages;
//     std::unordered_map<u32, ResourceUsage> buffer_usages;
// };

// class RDG
// {
//   public:
//     RDG();
//     ~RDG();
//     void AddPass(RDGPass *pass);
//     // build dependency graph, sort pass execution order, track resource state, allocate actual resources
//     void Compile();
//     // execute passes in order, insert barriers between passes, submit command lists
//     void Execute();
//     //void RegisterResource(RDGResource *resource, );
//     template<typename T>
//     RDGPass* CreatePass();
//     std::vector<RDGPass*> m_passes;
// }