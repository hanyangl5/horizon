- shared pch
- batch unitybuild

## Slang shaders

Slang is built from `Source/ThirdParty/slang`; initialize submodules with
`git submodule update --init --recursive`. Its tests, examples, standalone tools,
and LLVM backend are disabled. Building the compiler adds to the initial build time.

`ShaderDesc` selects Slang for `.slang` files and DXC for `.hlsl` files. For in-memory
Slang source, set `.language = hz::ShaderLanguage::Slang`. Both paths currently target
D3D12. Slang stages are linked together before generating DXIL, so VS/PS in one module
can share automatically assigned resource bindings. Matrices retain the existing
column-major layout. Renderer uses Slang for both Geometry and Lighting.

## Tracy profiling

Tracy v0.13.1 (`Source/ThirdParty/tracy`) is enabled by default through
`HORIZON_ENABLE_TRACY`. The client starts with the application and marks a frame on
every present. Connect a 0.13.1 viewer; other versions use an incompatible protocol.
The viewer and the `tracy-capture` command-line tool build from
`Source/ThirdParty/tracy/profiler` and `capture` with CMake, which downloads their
dependencies. Without a connected viewer the client keeps queued events in memory;
set `HORIZON_ENABLE_TRACY=OFF` to compile Tracy out.

CPU zones use `PROFILER_SET_CPU_SCOPE_AUTO_NAMED`; `HORIZON_ENABLE_TRACY_VERBOSE`
adds the verbose scopes. GPU zones come from `CommandList::beginGpuTimestamp` when
the context sets `enableGpuProfiler`. Renderer times its Geometry, Lighting and
PostProcess passes, and enables GPU-based validation only in Debug builds because
it distorts GPU timings.

## Tracy memory tracking

Memory tracking is optional and disabled by default. Enable it with:

```powershell
cmake -S . -B build -DHORIZON_ENABLE_TRACY=ON -DHORIZON_ENABLE_TRACY_MEMORY=ON
cmake --build build --config Release --target Renderer
```

Connect Tracy to the application to inspect `CPU/tf` allocations and the
`GPU/D3D12` allocation pools. `HORIZON_TRACY_MEMORY_CALLSTACK_DEPTH` controls
allocation callstack depth (default: 16; 0 disables allocation callstacks).
`memGetTrackingStats()` reports CPU allocation counts, requested/usable bytes,
peaks, and allocation slack. These figures cover the `tf_*` allocation wrappers.
Slack is allocator overhead, not a measurement of external heap fragmentation.

RenderContext publishes CPU and D3D12 memory plots on presentation, including
when GPU timing is disabled. GPU plots describe D3D12MA allocations and reserved
blocks; explicitly placed resources are accounted for by their owning heap.
Tracking adds bookkeeping and callstack capture overhead. Set
`HORIZON_ENABLE_TRACY_MEMORY=OFF` to compile out the tracking hooks.
