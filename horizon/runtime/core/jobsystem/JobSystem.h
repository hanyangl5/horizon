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

#pragma once

#include <functional>
#include <atomic>

#include <memory>
#include <algorithm>
#include <deque>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <runtime/core/utils/Definations.h>

namespace Horizon
{
	struct JobArgs
	{
		uint32_t jobIndex;		// job index relative to dispatch (like SV_DispatchThreadID in HLSL)
		uint32_t groupID;		// group index relative to dispatch (like SV_GroupID in HLSL)
		uint32_t groupIndex;	// job index relative to group (like SV_GroupIndex in HLSL)
		bool isFirstJobInGroup; // is the current job the first one in the group?
		bool isLastJobInGroup;	// is the current job the last one in the group?
		void *sharedmemory;		// stack memory shared within the current group (jobs within a group execute serially)
	};

	// Defines a state of execution, can be waited on
	struct context
	{
		std::atomic<uint32_t> counter{0};
	};

	class SpinLock
	{
	private:
		std::atomic_flag lck = ATOMIC_FLAG_INIT;

	public:
		inline void lock()
		{
			int spin = 0;
			while (!try_lock())
			{
				if (spin < 10)
				{
					_mm_pause(); // SMT thread swap can occur here
				}
				else
				{
					std::this_thread::yield(); // OS thread swap can occur here. It is important to keep it as fallback, to avoid any chance of lockup by busy wait
				}
				spin++;
			}
		}
		inline bool try_lock()
		{
			return !lck.test_and_set(std::memory_order_acquire);
		}

		inline void unlock()
		{
			lck.clear(std::memory_order_release);
		}
	};

	struct Job2
	{
		std::function<void(JobArgs)> task;
		context *ctx;
		uint32_t groupID;
		uint32_t groupJobOffset;
		uint32_t groupJobEnd;
		uint32_t sharedmemory_size;
	};
	struct JobQueue
	{
		std::atomic_bool processing{false};
		std::deque<Job2> queue;
		SpinLock locker;

		inline void push_back(const Job2 &item)
		{
			std::scoped_lock lock(locker);
			queue.push_back(item);
		}

		inline bool pop_front(Job2 &item)
		{
			std::scoped_lock lock(locker);
			if (queue.empty())
			{
				processing.store(false);
				return false;
			}
			item = std::move(queue.front());
			queue.pop_front();
			processing.store(true);
			return true;
		}
	};
	struct WorkerState
	{
		std::atomic_bool alive{true};
		std::condition_variable wakeCondition;
		std::mutex wakeMutex;
	};

	// This structure is responsible to stop worker thread loops.
	//	Once this is destroyed, worker threads will be woken up and end their loops.
	struct InternalState
	{
		uint32_t numCores = 0;
		uint32_t numThreads = 0;
		std::unique_ptr<JobQueue[]> jobQueuePerThread;
		std::shared_ptr<WorkerState> worker_state = std::make_shared<WorkerState>(); // kept alive by both threads and internal_state
		std::atomic<uint32_t> nextQueue{0};
		~InternalState()
		{
			worker_state->alive.store(false);		  // indicate that new jobs cannot be started from this point
			worker_state->wakeCondition.notify_all(); // wakes up sleeping worker threads
			// wait until all currently running jobs finish:
			for (uint32_t i = 0; i < numThreads; ++i)
			{
				while (jobQueuePerThread[i].processing.load())
				{
					std::this_thread::yield();
				}
			}
		}
	}; // static internal_state

	class JobSystem2
	{
	public:
		JobSystem2() noexcept;
		~JobSystem2() noexcept;
		void Initialize(uint32_t maxThreadCount = ~0u) noexcept;

		uint32_t GetThreadCount() noexcept;

		// Add a task to execute asynchronously. Any idle thread will execute this.
		void Execute(context &ctx, const std::function<void(JobArgs)> &task) noexcept;

		// Divide a task onto multiple jobs and execute in parallel.
		//	jobCount	: how many jobs to generate for this task.
		//	groupSize	: how many jobs to execute per thread. Jobs inside a group execute serially. It might be worth to increase for small jobs
		//	task		: receives a JobArgs as parameter
		void Dispatch(context &ctx, uint32_t jobCount, uint32_t groupSize, const std::function<void(JobArgs)> &task, size_t sharedmemory_size = 0) noexcept;

		// Returns the amount of job groups that will be created for a set number of jobs and group size
		uint32_t DispatchGroupCount(uint32_t jobCount, uint32_t groupSize) noexcept;

		// Check if any threads are working currently or not
		bool IsBusy(const context &ctx) noexcept;

		// Wait until all threads become idle
		//	Current thread will become a worker thread, executing jobs
		void Wait(const context &ctx) noexcept;

		// Start working on a job queue
		//	After the job queue is finished, it can switch to an other queue and steal jobs from there
		inline void work(uint32_t startingQueue)
		{
			Job2 job;
			for (uint32_t i = 0; i < internal_state.numThreads; ++i)
			{
				JobQueue &job_queue = internal_state.jobQueuePerThread[startingQueue % internal_state.numThreads];
				while (job_queue.pop_front(job))
				{
					JobArgs args;
					args.groupID = job.groupID;
					if (job.sharedmemory_size > 0)
					{
						thread_local static std::vector<uint8_t> shared_allocation_data;
						shared_allocation_data.reserve(job.sharedmemory_size);
						args.sharedmemory = shared_allocation_data.data();
					}
					else
					{
						args.sharedmemory = nullptr;
					}

					for (uint32_t i = job.groupJobOffset; i < job.groupJobEnd; ++i)
					{
						args.jobIndex = i;
						args.groupIndex = i - job.groupJobOffset;
						args.isFirstJobInGroup = (i == job.groupJobOffset);
						args.isLastJobInGroup = (i == job.groupJobEnd - 1);
						job.task(args);
					}

					job.ctx->counter.fetch_sub(1);
				}
				startingQueue++; // go to next queue
			}
		}

	private:
		InternalState internal_state;
	};


	class Job {
		//std::function
	};

	class JobSystem {
	public:
		JobSystem(u32 max_thread_count) noexcept;
		~JobSystem() noexcept;
		void ExecuteJob(std::function<void()> job) noexcept;
		void Wait(Job& job) noexcept;
		void WaitAll() noexcept;
	private:
		
	};
}