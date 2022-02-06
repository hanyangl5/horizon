#include "Assets.h"

//Assest::Assest(Device* device)
//{
//	vbuffer = new VertexBuffer>(device, vertices);
//}

Assest::~Assest()
{
}

void Assest::ReleaseBuffer()
{
	for (auto& mesh : meshes) {
		mesh.ReleaseBuffer();
	}
}

void Assest::prepare(Device* device, CommandBuffer* commandBuffer)
{

	const std::vector<Vertex> vertices = {
		{{-0.5f, -0.5f,0.0}, {1.0f, 0.0f, 0.0f},{0.0,0.0},{0.0,0.0,0.0},{0.0,0.0,0.0}},
		{{0.5f, -0.5f,0.0}, {0.0f, 1.0f, 0.0f},{0.0,0.0},{0.0,0.0,0.0},{0.0,0.0,0.0}},
		{{0.5f, 0.5f,0.0}, {0.0f, 1.0f, 0.0f},{0.0,0.0},{0.0,0.0,0.0},{0.0,0.0,0.0}},
		{{-0.5f, 0.5f,0.0}, {0.0f, 0.0f, 1.0f},{0.0,0.0},{0.0,0.0,0.0},{0.0,0.0,0.0}}
	};
	const std::vector<u32> indices = {
	0, 1, 2, 2, 3, 0
	};
	meshes.emplace_back(Mesh{ device, commandBuffer, vertices, indices });
	
}

void Assest::draw(Device* device, CommandBuffer* commandBuffer)
{
	std::vector<DrawContext> drawContexts;
	for (auto& mesh : meshes) {
		drawContexts.push_back({ mesh.getVertexBuffer(), mesh.getIndexBuffer(), 0, mesh.getIndexCount() });
	}
	commandBuffer->draw(drawContexts);
	
}
