// Copyright(c) 2021 Turánszki János
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define NOMINMAX
#include <Windows.h>

#include "JobSystem.h"

#include <runtime/core/log/Log.h>

#include <assert.h>
namespace Horizon
{
	JobSystem2::JobSystem2() noexcept
	{
	}
	JobSystem2::~JobSystem2() noexcept
	{
	}
	void JobSystem2::Initialize(uint32_t maxThreadCount) noexcept
	{
		if (internal_state.numThreads > 0)
			return;
		maxThreadCount = std::max(1u, maxThreadCount);

		// Retrieve the number of hardware threads in this system:
		internal_state.numCores = std::thread::hardware_concurrency();

		// Calculate the actual number of worker threads we want (-1 main thread):
		internal_state.numThreads = std::min(maxThreadCount, std::max(1u, internal_state.numCores - 1));
		internal_state.jobQueuePerThread.reset(new JobQueue[internal_state.numThreads]);

		for (uint32_t threadID = 0; threadID < internal_state.numThreads; ++threadID)
		{
			std::thread worker([threadID, this]
							   {
								   std::shared_ptr<WorkerState> worker_state = internal_state.worker_state; // this is a copy of shared_ptr<WorkerState>, so it will remain alive for the thread's lifetime

								   while (worker_state->alive.load())
								   {
									   work(threadID);

									   // finished with jobs, put to sleep
									   std::unique_lock<std::mutex> lock(worker_state->wakeMutex);
									   worker_state->wakeCondition.wait(lock);
								   }
							   });

#ifdef _WIN32
			// Do Windows-specific thread setup:
			HANDLE handle = (HANDLE)worker.native_handle();

			// Put each thread on to dedicated core:
			DWORD_PTR affinityMask = 1ull << threadID;
			DWORD_PTR affinity_result = SetThreadAffinityMask(handle, affinityMask);
			assert(affinity_result > 0);

			//// Increase thread priority:
			// BOOL priority_result = SetThreadPriority(handle, THREAD_PRIORITY_HIGHEST);
			// assert(priority_result != 0);

			// Name the thread:
			std::wstring wthreadname = L"wi::jobsystem_" + std::to_wstring(threadID);
			HRESULT hr = SetThreadDescription(handle, wthreadname.c_str());
			assert(SUCCEEDED(hr));
#endif // _WIN32

			worker.detach();
		}

		LOG_INFO("jobsystem Initialized with [{} cores] [{} threads]",
				 internal_state.numCores,
				 internal_state.numThreads);
	}

	uint32_t JobSystem2::GetThreadCount() noexcept
	{
		return internal_state.numThreads;
	}

	void JobSystem2::Execute(context &ctx, const std::function<void(JobArgs)> &task) noexcept
	{
		// Context state is updated:
		ctx.counter.fetch_add(1);

		Job2 job;
		job.ctx = &ctx;
		job.task = task;
		job.groupID = 0;
		job.groupJobOffset = 0;
		job.groupJobEnd = 1;
		job.sharedmemory_size = 0;

		internal_state.jobQueuePerThread[internal_state.nextQueue.fetch_add(1) % internal_state.numThreads].push_back(job);
		internal_state.worker_state->wakeCondition.notify_one();
	}

	void JobSystem2::Dispatch(context &ctx, uint32_t jobCount, uint32_t groupSize, const std::function<void(JobArgs)> &task, size_t sharedmemory_size) noexcept
	{
		if (jobCount == 0 || groupSize == 0)
		{
			return;
		}

		const uint32_t groupCount = DispatchGroupCount(jobCount, groupSize);

		// Context state is updated:
		ctx.counter.fetch_add(groupCount);

		Job2 job;
		job.ctx = &ctx;
		job.task = task;
		job.sharedmemory_size = (uint32_t)sharedmemory_size;

		for (uint32_t groupID = 0; groupID < groupCount; ++groupID)
		{
			// For each group, generate one real job:
			job.groupID = groupID;
			job.groupJobOffset = groupID * groupSize;
			job.groupJobEnd = std::min(job.groupJobOffset + groupSize, jobCount);

			internal_state.jobQueuePerThread[internal_state.nextQueue.fetch_add(1) % internal_state.numThreads].push_back(job);
		}

		internal_state.worker_state->wakeCondition.notify_all();
	}

	uint32_t JobSystem2::DispatchGroupCount(uint32_t jobCount, uint32_t groupSize) noexcept
	{
		// Calculate the amount of job groups to dispatch (overestimate, or "ceil"):
		return (jobCount + groupSize - 1) / groupSize;
	}

	bool JobSystem2::IsBusy(const context &ctx) noexcept
	{
		// Whenever the context label is greater than zero, it means that there is still work that needs to be done
		return ctx.counter.load() > 0;
	}

	void JobSystem2::Wait(const context &ctx) noexcept
	{
		if (IsBusy(ctx))
		{
			// Wake any threads that might be sleeping:
			internal_state.worker_state->wakeCondition.notify_all();

			// work() will pick up any jobs that are on stand by and execute them on this thread:
			work(internal_state.nextQueue.fetch_add(1) % internal_state.numThreads);

			while (IsBusy(ctx))
			{
				// If we are here, then there are still remaining jobs that work() couldn't pick up.
				//	In this case those jobs are not standing by on a queue but currently executing
				//	on other threads, so they cannot be picked up by this thread.
				//	Allow to swap out this thread by OS to not spin endlessly for nothing
				std::this_thread::yield();
			}
		}
	}
}
