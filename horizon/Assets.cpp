#include "Assets.h"

//Assest::Assest(std::shared_ptr<Device> device)
//{
//	vbuffer = std::make_shared<VertexBuffer>(device, vertices);
//}

Assest::~Assest()
{
	;
}

void Assest::prepare(std::shared_ptr<Device> device,VkCommandPool cmdpool)
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
	meshes.emplace_back(Mesh{ device,cmdpool,vertices, indices });
	
}
