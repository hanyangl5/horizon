# Production-Ready Multithreading

[TOC]

This document describes how Horizon should grow from its current threading
primitives into a production-ready game engine multithreading model.

The goal is not to maximize thread count first. The goal is deterministic frame
ownership, clear synchronization contracts, practical observability, and a safe
single-threaded fallback for debugging and automated testing.

## Current baseline

Horizon already has the low-level pieces needed to start:

- `Core/IThread.h` exposes OS threads, mutexes, condition variables, thread
  naming, affinity, sleeps, and CPU core counts.
- `Core/Private/Threading/ThreadSystem.h` and `.c` provide an internal worker
  pool abstraction with task submission, assisted waiting, idle waiting, and a
  dummy single-threaded mode.
- `Resources/Private/ResourceLoader/ResourceLoader.cpp` owns a dedicated
  streaming thread and token-based synchronization for GPU resource uploads.
- The application loop currently runs app update and draw serially, which makes
  CPU update work the safest first target for parallelization.

This is a good starting point: the engine has primitives and a small task pool,
but it still needs public scheduling semantics, frame ownership rules, tooling,
and tests before it should be treated as a production job system.

## Design principles

- Keep the main thread responsible for platform messages, high-level frame
  orchestration, and APIs that are documented as main-thread-only.
- Keep rendering submission explicit. Build command data in parallel only after
  ownership rules are documented for the RHI and graphics layer.
- Prefer immutable frame snapshots and single-writer output buffers over shared
  mutable state.
- Make every blocking wait either a documented frame barrier or an assisted wait
  where the waiting thread can execute ready work.
- Keep a single-threaded mode that runs the same code path and produces the same
  results.
- Treat resource loading, shader compilation, file IO, and GPU fences as
  asynchronous systems with clear handoff tokens instead of arbitrary worker
  jobs.

## Threading contract

Before adding more worker code, document the engine-wide contract in code and in
the API docs:

- Long-lived threads: `Main`, `Worker N`, `ResourceLoader`, and future dedicated
  threads such as `Audio`, `Network`, or `ShaderCompile`.
- Thread affinity: platform-specific affinity is optional and should be hidden
  behind initialization descriptors.
- Main-thread-only APIs: windowing, input pump integration, lifecycle reloads,
  and any rendering APIs that mutate global device state.
- Worker-safe APIs: pure CPU transforms, animation sampling, culling, asset
  decode, CPU particle simulation, compression, and data preparation.
- Allowed frame barriers: begin frame, update phase boundaries, render handoff,
  resource token waits, and shutdown.
- Forbidden behavior: blocking waits from worker jobs unless the wait is
  assisted or the dependency is guaranteed to complete on another dedicated
  thread.

## Job system v1

The first production step should be a small public job system built on top of,
or evolved from, the existing `ThreadSystem`.

Minimum API surface:

```cpp
struct JobSystemDesc
{
    uint32_t mWorkerCount;
    bool     mSingleThreaded;
    bool     mEnableThreadAffinity;
};

struct JobHandle;

void initJobSystem(const JobSystemDesc* pDesc);
void exitJobSystem();

JobHandle dispatchJob(const char* name, JobPriority priority, JobFunction fn, void* userData);
JobHandle parallelFor(const char* name, uint32_t itemCount, uint32_t grainSize, ParallelForFunction fn, void* userData);

bool isJobComplete(const JobHandle* handle);
void waitJob(const JobHandle* handle);
void assistJobSystem();
```

Required behavior:

- `waitJob` must assist the scheduler while waiting when called from a worker or
  from the main thread during an allowed barrier.
- `parallelFor` must support stable chunk ranges so systems can preallocate
  per-chunk outputs.
- Job names must be visible in profiler events and logs.
- The single-threaded mode must run through the same call sites as the threaded
  mode.
- Shutdown must support draining outstanding work and abandoning work during
  fatal exit.

Nice-to-have after v1:

- Priority queues: `High`, `Normal`, `Background`.
- Per-thread temporary allocators.
- Job tags or lanes for profiling and budget tracking.
- Cancellation tokens for long-running background work.

## Frame integration

The first integration point should be CPU update, not render submission.

Recommended frame shape:

```text
BeginFrame
  Pump platform messages
  Publish input snapshot

Update
  Phase 1: scripts and gameplay intent
  Phase 2: animation, transforms, particles, visibility
  Phase 3: finalize scene snapshot

RenderHandoff
  Wait for required update jobs
  Submit draw using finalized frame data

EndFrame
  Retire frame resources
  Advance frame index
```

Rules:

- Each phase publishes immutable data to the next phase.
- Systems that write into shared frame state must either own disjoint ranges or
  write into per-thread buffers that are merged at the phase boundary.
- Draw should consume a finalized scene snapshot. It should not chase unfinished
  update jobs.
- Frame resources should be double or triple buffered, matching the renderer's
  frames-in-flight model.

## First systems to parallelize

Start with systems that are CPU-heavy, deterministic, and naturally partitioned:

- Transform propagation for independent roots or hierarchy ranges.
- Animation sampling and blending per animated object.
- CPU visibility and light/object culling.
- CPU particles and gameplay-side simulation.
- Asset decode, mesh processing, compression, and offline import steps.

Avoid these as first targets:

- Global ECS mutation without a deferred command buffer.
- RHI object creation unless the RHI explicitly documents thread safety.
- Render command submission until command pool ownership and synchronization are
  clear.
- File IO in the general worker pool unless the IO layer has throttling and
  cancellation.

## Resource loading

The existing resource loader is already closer to a production pattern than a
generic worker job because it owns GPU copy queues, staging buffers, tokens, and
semaphores.

Recommended direction:

- Keep the dedicated `ResourceLoader` thread for GPU upload and copy queue work.
- Expose resource loads to gameplay through tokens or futures, not direct waits.
- Make blocking token waits rare and visible in profiler traces.
- Let background decode jobs prepare CPU data, then hand upload requests to the
  resource loader.
- Keep `mSingleThreaded` behavior for deterministic startup and tests.

The resource loader can later become a job graph node, but it should still own
its GPU queue synchronization internally.

## Observability

A production multithreading system is only as useful as its debugging surface.
Add these early:

- Thread names for all long-lived threads.
- Job names and phase names in the profiler.
- Queue depth, active worker count, idle worker count, and longest wait time.
- Stall reasons: waiting on job, waiting on resource token, waiting on GPU
  fence, waiting on IO, waiting on mutex.
- Optional debug logging for long waits over a configurable threshold.
- Debug assertions for invalid thread ownership.

## Testing plan

Tests should cover correctness before performance:

- Single-threaded and multithreaded execution produce identical results.
- `parallelFor` handles zero items, one item, uneven grain sizes, and many small
  items.
- `waitJob` completes when called from main thread and worker thread.
- Assisted waits cannot deadlock on nested job dependencies.
- Shutdown drains queued work by default.
- Shutdown can abandon queued work when explicitly requested.
- Resource loader token waits remain valid in both threaded and single-threaded
  modes.

Add stress tests after basic correctness:

- Thousands of tiny jobs.
- A mix of high-priority and background jobs.
- Repeated init/exit cycles.
- Worker jobs that enqueue more jobs.
- Long-running background work during app shutdown.

## Staged rollout

1. Publish `IJobSystem` in `Source/Runtime/Core/Public/Core`.
2. Move the existing `ThreadSystem` behavior behind the new API while preserving
   dummy single-threaded execution.
3. Add focused unit tests for dispatch, `parallelFor`, wait, assisted wait, and
   shutdown.
4. Add profiler events and thread/job names.
5. Convert one CPU-only system, preferably animation sampling or culling.
6. Add frame phase barriers to the app update path.
7. Convert more CPU update systems once the first migration is stable.
8. Revisit render command building and task graph dependencies after update
   parallelism is proven.

## Definition of production ready

The threading system can be considered production ready when:

- The engine can run the same scene in single-threaded and multithreaded modes.
- Thread ownership is documented for public runtime APIs.
- Frame phase barriers are explicit and observable.
- Worker starvation, long waits, and deadlocks have useful diagnostics.
- Shutdown is deterministic.
- At least one real gameplay or rendering-adjacent CPU system uses the job
  system in shipping-style code.
- The resource loader integrates through tokens without forcing arbitrary worker
  threads to block on GPU work.
