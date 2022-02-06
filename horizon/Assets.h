#pragma once
#include "Vertex.h"
#include "VertexBuffer.h"
#include <memory>
#include "Device.h"
#include "Mesh.h"
#include "CommandBuffer.h"
#include <vector>



class Assest
{
public:
	Assest() = default;
	~Assest();
	void ReleaseBuffer();
	void prepare(Device* device, CommandBuffer* commandBuffer);
	void draw(Device* device, CommandBuffer* commandBuffer);
	//std::shared_ptr<VertexBuffer> vbuffer;
	std::vector<Mesh> meshes;
};
