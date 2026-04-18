# Horizon Runtime API Documentation

[TOC]

This site is generated with Doxygen from the public runtime headers under
`Source/Runtime/*/Public`.

The goal is to expose the engine-facing API surface, not every private
implementation detail.

## Runtime modules

The runtime is split into these public modules:

- `Application`
  Windowing, app lifecycle, and user-facing runtime integration.
- `Core`
  Configuration, memory, logging, filesystem, threading, timing, math, and
  related foundational services.
- `Graphics`
  Higher-level rendering structures that sit above the RHI layer.
- `Platform`
  Platform-facing public interfaces used by the runtime.
- `Profiler`
  Profiling hooks and profiling-related runtime APIs.
- `Resources`
  Resource loading, asset data, and streaming-oriented interfaces.
- `RHI`
  Rendering hardware interface types and device-facing abstractions.
- `Scripting`
  Public scripting-facing runtime interfaces.

## Building the docs

If Doxygen is installed and CMake is configured with `HORIZON_BUILD_DOCS=ON`,
the project exposes a `Docs` target.

Typical build flow:

1. Configure the project with CMake.
2. Build the `Docs` target.
3. Open `build/docs/doxygen/html/index.html`.

The generated output is written to the build tree so it does not pollute the
source tree.
