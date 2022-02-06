#pragma once
#include "Vertex.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include "utils.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "CommandBuffer.h"
#include <memory>
// struct Transform
// {
//   glm::vec3 pos = glm::vec3(0.0); // described as xyz
//   glm::vec3 rot = glm::vec3(0.0); // rotate by x/y/z 0-90
//   glm::vec3 scale = glm::vec3(1.0);
// };

class Mesh
{
public:
  Mesh(Device* device, CommandBuffer* commandBuffer,std::vector<Vertex> vertices, std::vector<u32> indices);
  ~Mesh();
  // Mesh(std::vector<Vertex> vertices, std::vector<u32> indices,
  //      std::vector<Texture> textures);
  void ReleaseBuffer();
  // //void Draw(Shader &shader, RenderMode renermode);
  // glm::mat4 GetModelMat();
  VkBuffer getVertexBuffer()const;
  VkBuffer getIndexBuffer()const;
  u32 getIndexCount()const;
private:
  //void setupMesh();

public:
  // glm::vec3 translation;
  //glm::mat4 model_mat = glm::mat4(1.0);
  std::vector<Vertex> vertices;
  std::vector<u32> indices;
  std::string meshname;

  VertexBuffer* vertexBuffer;
  IndexBuffer* indexBuffer;

};