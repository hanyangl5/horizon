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
	vbuffer = std::make_shared<VertexBuffer>(device,cmdpool, vertices);
}
