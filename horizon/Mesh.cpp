#include "Mesh.h"

Mesh::Mesh(std::shared_ptr<Device> device, VkCommandPool cmdpool, std::vector<Vertex> vertices, std::vector<u32> indices)
{
	vertexBuffer = std::make_shared<VertexBuffer>(device, cmdpool, vertices);
	indexBuffer = std::make_shared<IndexBuffer>(device, cmdpool, indices);

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
