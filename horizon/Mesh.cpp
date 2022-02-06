#include "Mesh.h"

Mesh::Mesh(Device* device, CommandBuffer* commandBuffer, std::vector<Vertex> vertices, std::vector<u32> indices)
{
	vertexBuffer = new VertexBuffer(device, commandBuffer, vertices);
	indexBuffer = new IndexBuffer(device, commandBuffer, indices);
}

Mesh::~Mesh()
{

}

void Mesh::ReleaseBuffer()
{
	delete vertexBuffer;
	delete indexBuffer;
}

VkBuffer Mesh::getVertexBuffer() const
{
	return vertexBuffer->get();
}

VkBuffer Mesh::getIndexBuffer() const
{
	return indexBuffer->get();
}

u32 Mesh::getIndexCount() const
{
	return indexBuffer->getIndicesCount();
}
