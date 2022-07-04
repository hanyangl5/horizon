![](docs/figs/horizon_224.png)

# Horizon

horizon is a real time renderer.

---

# Build From Source

**NOTES:** Horizon is not designed for cross-platform and portability, so build or run correctly on other platform is not guaranteed.

On Windows:

- Vulkan SDK 1.3
- CMake 3.18
- Git
- vcpkg

install the following package with vcpkg

~~~
vcpkg install d3d12-memory-allocator directx-dxc directxtk12 spdlog glfw3 vulkan-memory-allocator argparse doctest
~~~

clone the repo with

~~~
git clone https://github.com/v4vendeta/horizon.git
~~~

modify vcpkg path in CMakeLists.txt in project root directory

~~~
include(/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake)
~~~


use CMkae to generate solution file

~~~
cmake . -B build
~~~

open solution **Horizon** and build all solution.

<!-- ./app.exe -config_path D:/codes/horizon/horizon/app/EngineConfig.ini -->
