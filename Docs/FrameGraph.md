# Horizon FrameGraph Roadmap

[TOC]

## Purpose

This document captures a practical roadmap for building a production-ready
framegraph in Horizon.

The intent is not to replace the existing RHI. The framegraph should sit above
it and take ownership of frame orchestration: pass ordering, resource lifetime,
state transitions, transient allocation, and queue submission planning.

## Why This Is The Right Time

The current renderer already exposes most of the low-level building blocks a
framegraph needs:

- explicit resource states and barrier structs in `RHI/IGraphics.h`
- render target load/store descriptions through `BindRenderTargetsDesc`
- explicit queue submission and present control
- resource heap and placement APIs for transient memory
- debug markers and query pools for instrumentation

At the same time, frame orchestration is still mostly handwritten in client and
feature code. Typical frame flow currently looks like:

- acquire swapchain image
- manually emit barriers
- bind render targets
- record draw or dispatch work
- manually transition back to present
- submit and present

Examples of this manual pattern exist today in:

- `Source/Tools/UIRemoteControl/src/UIRemoteControl.cpp`
- `Source/Runtime/Graphics/Private/Particle/ParticleSystem.cpp`

That combination makes the engine a good candidate for a graph compiler layer.

## Goals

The framegraph should provide the following capabilities:

- central pass scheduling based on declared data dependencies
- automatic barrier generation from resource usage declarations
- first-class handling of imported, history, and transient resources
- dead pass and dead resource culling
- a clear path toward transient memory reuse and aliasing
- instrumentation hooks for debug markers, timestamps, and graph dumps
- deterministic compilation behavior suitable for runtime and tooling use

## Non-Goals

The first version should not try to solve everything:

- it should not replace the RHI abstraction
- it should not hide pipeline or descriptor binding policy
- it should not require subresource-perfect tracking on day one
- it should not start with multi-queue scheduling or aliasing as a hard
  requirement

The right first step is a small, correct graph compiler that can own one full
frame on a single graphics queue.

## Current Engine Support

The existing RHI already supports several concepts the graph can build on:

- `ResourceState`, `TextureBarrier`, and `RenderTargetBarrier`
- `QueueSubmitDesc` and explicit `queueSubmit` / `queuePresent`
- `ResourceHeap` and `ResourcePlacement` for placed resources
- render target descriptors and load/store actions
- debug marker and query APIs for observability

This is enough to ship a useful first version without destabilizing the backend.

## Known Gaps

Some production-grade capabilities will require additional work:

- There is no graph layer today, so pass/resource declaration, compilation, and
  execution all need to be introduced.
- Explicit aliasing barriers are not modeled in the public RHI interface yet.
  That will matter once transient memory aliasing is enabled.
- The barrier structs expose acquire/release and queue type information for
  textures and render targets, but the current Direct3D 12 path only clearly
  maps acquire/release to state transitions. Multi-queue ownership transfer
  should be treated as incomplete until the graph and backend agree on a full
  cross-queue contract.

## High-Level Architecture

### Core Objects

The framegraph should be built around a small set of stable concepts:

- `FrameGraph`
  Owns pass declarations, logical resource declarations, compilation, and
  execution for one frame.
- `PassHandle`
  Identifies a logical pass node in the graph.
- `ResourceHandle`
  Identifies a logical graph resource.
- `ImportedResource`
  Wraps externally owned resources such as swapchain render targets, persistent
  scene textures, readback buffers, or middleware-owned resources.
- `HistoryResource`
  Represents cross-frame resources such as TAA history, exposure history, or
  temporal reservoirs.
- `TransientResource`
  Represents a per-frame logical resource that can later be backed by pooled or
  aliased physical memory.

### Pass Contract

Each pass should declare:

- a stable debug name
- the target queue type
- read usages
- write usages
- whether it has side effects that prevent culling
- an execute callback that records commands through an execution context

The execute callback should resolve logical handles to concrete RHI objects, but
it should not decide synchronization manually. Once a pass is graph-managed, the
graph must own barrier emission.

### Resource Contract

Each logical resource should carry:

- a stable name
- a descriptor derived from `TextureDesc`, `RenderTargetDesc`, or `BufferDesc`
- a lifetime category: imported, history, or transient
- usage metadata gathered during compilation

The graph should distinguish logical resources from physical allocations. That
separation is what makes culling, transient reuse, and aliasing possible later.

## Recommended Rollout

### Phase 1: Minimal Viable Graph

The first milestone should be intentionally conservative:

- single graphics queue only
- whole-resource state tracking only
- no transient aliasing yet
- no pass merging yet
- one command list submission path
- swapchain imported as an external graph resource

Phase 1 compiler responsibilities:

- build a DAG from declared read and write edges
- cull passes and resources that do not contribute to exported outputs
- infer required states from pass usage
- emit barriers automatically
- execute passes in topological order
- transition exported resources to their required final state, including present

This phase is already valuable because it removes handwritten frame plumbing
from feature code while preserving explicit control at the RHI boundary.

### Phase 2: Transient Allocator

Once the graph is stable, add physical resource planning:

- compute first and last use for transient logical resources
- reuse physical allocations for non-overlapping lifetimes
- back transient allocations with `ResourceHeap` plus `ResourcePlacement`
- track peak transient memory and reuse efficiency

Only after that is working should the engine move to memory aliasing. Aliasing
is a production optimization, not the correct starting point.

### Phase 3: Production Features

The next wave can add the features that make the system production-ready:

- async compute and copy queue scheduling
- explicit cross-queue ownership transfer
- subresource-level tracking where it materially helps
- pass merge opportunities and load/store optimization
- graph dumps in human-readable and machine-readable formats
- pass-level timestamp integration
- robust resize, reload, and history invalidation behavior

## First Recommended Milestone

The best initial integration target is not a large deferred renderer. It should
be a small but complete graph that exercises the full orchestration path:

- import the swapchain backbuffer
- create one offscreen color target
- run one raster pass that writes the offscreen target
- run one pass that exercises SRV/UAV style dependencies
- run one composite pass that writes the backbuffer
- let the graph own every transition, submission, and present dependency

This is large enough to validate the architecture and small enough to debug
quickly.

## Migration Candidates In This Repository

The following code paths are good early migration targets:

- `UIRemoteControl.cpp`
  A compact acquire, render, submit, and present loop with manual render target
  transitions.
- `ParticleSystem.cpp`
  A feature path with explicit UAV barriers and a natural compute-to-render data
  dependency.

These two paths together cover most of the state tracking behavior needed for a
useful first graph implementation.

## Suggested Repository Layout

One reasonable starting layout is:

- `Source/Runtime/Graphics/Public/Graphics/IFrameGraph.h`
- `Source/Runtime/Graphics/Private/FrameGraph/FrameGraph.cpp`
- `Source/Runtime/Graphics/Private/FrameGraph/FrameGraphCompiler.cpp`
- `Source/Runtime/Graphics/Private/FrameGraph/FrameGraphResources.cpp`
- `Source/Runtime/Graphics/Private/FrameGraph/FrameGraphExecution.cpp`

The public interface should stay small. Most of the implementation should live
in `Runtime/Graphics/Private`.

## Validation And Tooling

Production readiness depends on tooling, not only correctness:

- emit debug markers per pass
- support per-pass timestamps
- dump the compiled graph as text and JSON or DOT
- log inferred barriers in a debug mode
- expose transient memory statistics

The graph should be easy to inspect when something goes wrong.

## Testing Strategy

The framegraph should ship with targeted tests for:

- dependency ordering
- barrier inference
- dead pass culling
- imported resource handling
- history resource persistence and invalidation
- resize and reload behavior
- transient reuse accounting

Smoke tests should also exercise at least one real frame path through the
runtime.

## Guardrails

To keep the design healthy, the implementation should follow a few rules:

- do not allow graph-managed passes to emit manual barriers
- keep imported and history resources explicit
- start with whole-resource tracking before adding subresource complexity
- treat aliasing as an optimization layer, not a design foundation
- keep the compiler deterministic so graph dumps and tests stay reliable

## Summary

The right way to start is:

1. add a small framegraph layer above the current RHI
2. compile a single-queue graph with automatic barriers
3. migrate one complete but compact frame path
4. only then add transient allocation, aliasing, and multi-queue features

That approach matches the engine's current capabilities and minimizes risk while
still building toward a production-ready renderer architecture.
