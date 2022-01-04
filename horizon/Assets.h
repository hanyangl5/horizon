#pragma once
#include "Vertex.h"
#include "VertexBuffer.h"
#include <memory>
#include "Device.h"
class Assest
{
public:
	Assest() = default;
	~Assest();
	void prepare(std::shared_ptr<Device> device);
	std::shared_ptr<VertexBuffer> vbuffer;

	const std::vector<Vertex> vertices = {
		{{0.0f, -0.5f,0.0}, {1.0f, 0.0f, 0.0f},{0.0,0.0},{0.0,0.0,0.0},{0.0,0.0,0.0}},
		{{0.5f, 0.5f,0.0}, {0.0f, 1.0f, 0.0f},{0.0,0.0},{0.0,0.0,0.0},{0.0,0.0,0.0}},
		{{-0.5f, 0.5f,0.0}, {0.0f, 0.0f, 1.0f},{0.0,0.0},{0.0,0.0,0.0},{0.0,0.0,0.0}}
	};
};
