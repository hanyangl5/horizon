#include "DX12CommandContext.h"
#include "DX12Buffer.h"

namespace Horizon {
	namespace RHI {

		//DX12CommandList::DX12CommandList() noexcept
		//{
		//}

		//DX12CommandList::~DX12CommandList() noexcept
		//{
		//}
		//void DX12CommandList::BeginRecording() noexcept
		//{
		//	is_recording = true;
		//}
		//void DX12CommandList::EndRecording() noexcept
		//{
		//	is_recording = false;
		//}
		//void DX12CommandList::Draw() noexcept
		//{
		//	//assert(is_recording, "command list isn't recording");
		//}
		//void DX12CommandList::DrawIndirect() noexcept
		//{
		//	//assert(is_recording, "command list isn't recording");
		//}
		//void DX12CommandList::Dispatch() noexcept
		//{
		//	//assert(is_recording, "command list isn't recording");
		//}
		//void DX12CommandList::DispatchIndirect() noexcept
		//{
		//	//assert(is_recording, "command list isn't recording");

		//}
		//void DX12CommandList::UpdateBuffer(Buffer* buffer, void* data, u64 size) noexcept
		//{
		//	//assert(is_recording, "command list isn't recording");
		//	assert(buffer->GetBufferSize() == size);

		//	auto dx12_buffer = dynamic_cast<DX12Buffer*>(buffer);
		//	auto stage_buffer = new DX12Buffer(dx12_buffer->m_allocator, BufferCreateInfo{ BufferUsage::BUFFER_USAGE_TRANSFER_SRC, size }, MemoryFlag::CPU_VISABLE_MEMORY);

		//	const auto& resource = stage_buffer->m_allocation->GetResource();

		//	void* mapped_data;
		//	CHECK_DX_RESULT(resource->Map(0, nullptr, &mapped_data));
		//	memcpy(mapped_data, data, size);
		//	resource->Unmap(0, nullptr);
		//}

		//void DX12CommandList::UpdateTexture() noexcept
		//{
		//	//assert(is_recording, "command list isn't recording");
		//}

		//void DX12CommandList::CopyBuffer(Buffer* dst_buffer, Buffer* src_buffer) noexcept
		//{
		//	//assert(is_recording, "command list isn't recording");
		//	assert(dst_buffer->GetBufferSize() == src_buffer->GetBufferSize());
		//	auto dx12_src_buffer = dynamic_cast<DX12Buffer*>(src_buffer);
		//	auto dx12_dst_buffer = dynamic_cast<DX12Buffer*>(dst_buffer);

		//}
		//void DX12CommandList::CopyTexture() noexcept
		//{
		//	//assert(is_recording, "command list isn't recording");
		//}
		//void DX12CommandList::InsertBarrier(const BarrierDesc& desc) noexcept
		//{

		//}
		//void DX12CommandList::Submit() noexcept
		//{
		//	//assert(is_recording, "command list isn't recording");
		//}
		//void DX12CommandList::CopyBuffer(DX12Buffer* src_buffer, DX12Buffer* dst_buffer) noexcept
		//{
		//	//assert(is_recording, "command list isn't recording");
		//	assert(dst_buffer->GetBufferSize() == src_buffer->GetBufferSize());

		//}
	}
}

