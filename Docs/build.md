- shared pch
- batch unitybuild

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
