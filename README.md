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
d3d12-memory-allocator directx-dxc directxtk12 spdlog glm glfw3 vulkan-memory-allocator
~~~

clone the repo with

~~~
git clone https://github.com/v4vendeta/horizon.git
~~~


use CMkae to generate solution file

~~~
cmake . -B build
~~~

open solution **Horizon** and build all to generate execuatble file.

