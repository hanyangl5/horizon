# Agent Notes for Horizon

本文件给后续编码代理使用。开始动手前请先读当前工作区状态：这个仓库经常处于开发中的 dirty worktree，修改时只处理任务相关文件，不要回滚用户已有改动。

## 项目概览

Horizon 是一个 CMake 驱动的 C++20 图形/运行时引擎项目，代码风格和大量底层接口延续 The Forge。当前主要平台是 Windows，主要图形后端是 Direct3D 12。

核心产物是 `Runtime` 静态库，示例程序和测试都链接到它。根 `CMakeLists.txt` 只设置项目、`ENGINE_DIR`/`PROJECT_ROOT` 和模块路径，然后加载 `CMake/Horizon.cmake`。

## 目录地图

- `CMake/`：工程模块。`Horizon.cmake` 是总入口，`Runtime.cmake` 创建 `Runtime` 库，`Tests.cmake`/`Examples.cmake` 创建测试和示例目标。
- `Source/Runtime/`：运行时主体，按 `Core`、`Platform`、`RHI`、`Profiler`、`Resources`、`Graphics`、`Application`、`Scripting` 分模块。
- `Source/Runtime/*/Public/<Module>/`：公共接口头文件。新增公共 API 时优先放这里。
- `Source/Runtime/*/Private/`：模块实现和内部头文件。
- `Source/Shaders/`：HLSL shader 资源。
- `Source/ThirdParty/`：第三方依赖和子模块。除非任务明确要求，不要格式化或重构这里。
- `Source/Tools/`：引擎工具和辅助程序，例如资源管线、热重载服务、材质编译脚本。
- `Examples/`：可执行示例，目前包括 `HelloTriangle` 和 `DeferredRenderer`。
- `Tests/`：GoogleTest 测试，按 `Core`、`RHI`、`Smoke` 等分类。
- `Scripts/`：项目脚本。`BuildEngine.py` 会生成并构建 MSVC 与 Clang/Ninja 配置；`UpdateGpuDeps.py`/`VerifyGpuDeps.py` 管理 GPU 相关第三方包。
- `Docs/`：Doxygen 和设计文档。

## 构建与测试

首次 checkout 或依赖缺失时：

```powershell
git submodule update --init --recursive
git lfs pull
python Scripts/VerifyGpuDeps.py
```

常用 Ninja 配置：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INCLUDE_SHADERS=false -DHORIZON_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

MSVC/Visual Studio 配置：

```powershell
cmake -S . -B build_msvc -G "Visual Studio 17 2022" -DCMAKE_INCLUDE_SHADERS=false -DHORIZON_BUILD_TESTS=ON
cmake --build build_msvc --config Debug
ctest --test-dir build_msvc --build-config Debug --output-on-failure
```

常用目标：

```powershell
cmake --build build --target Runtime
cmake --build build --target CoreTests
cmake --build build --target RHITests
cmake --build build --target SmokeTests
cmake --build build --target HelloTriangle
cmake --build build --target DeferredRenderer
```

CI 在 Windows 上用 Ninja 构建 Debug/Release，覆盖 MSVC 和 LLVM 20，并运行 `ctest --output-on-failure`。CI 还会对 `Source/Runtime` 下的 `h,hpp,c,cpp` 自动运行 clang-format。

## CMake 约定

- 新运行时代码通常不需要手动加入源文件列表：模块 CMake 文件使用 `file(GLOB_RECURSE ...)` 收集对应 `Public`/`Private` 文件。
- 新测试文件需要加入对应的 `CMake/Tests/*.cmake` 变量，再由 `add_test_target(...)` 创建目标。
- 新示例需要更新 `CMake/Examples.cmake`，并在 Windows 下复制 Agility SDK、DirectStorage、DXC 等运行时 DLL。
- 顶层选项包括 `HORIZON_BUILD_TESTS`、`HORIZON_BUILD_DOCS`、`HORIZON_BUILD_EXAMPLES`、`HORIZON_ENABLE_TRACY` 和 `CMAKE_INCLUDE_SHADERS`。
- 构建目录应放在源码树外或根部 `build*` 目录，不要把生成文件写进 `Source/`。

## 代码风格

- 使用仓库根目录 `.clang-format`。关键点：4 空格缩进、Allman 大括号、列宽 140、指针靠左、禁用 tab、CRLF。
- Runtime 目标使用 C++20，MSVC 下开启 `/W3 /WX`，并禁用 RTTI 和异常：不要引入 `throw`、`try/catch`、`dynamic_cast` 或依赖 RTTI 的设计。
- 优先沿用现有 The Forge 风格：C 风格 API、显式 `init/add/remove/exit` 生命周期、`p`/`pp` 指针命名、`m` 成员命名、`LOGF` 日志。
- 禁用 STL 作为默认实现选择。不要新增 `std::vector`、`std::string`、`std::function`、iostream、algorithm 等依赖；优先使用 C API、项目已有数组/内存/字符串工具、`stb_ds` 动态数组或现有第三方基础设施。
- 尽量避免新增 C++ 标准库头文件，例如 `<vector>`、`<string>`、`<memory>`、`<functional>`、`<algorithm>`、`<type_traits>` 等。包含头应优先选择 C 头、项目公共头和已有轻量第三方头，减少模板实例化带来的编译时间成本。
- 图形资源和 GPU 对象要有清晰 ownership。新增 `add*` 时要有对应 `remove*`，失败路径也要释放已创建资源。
- 公共头文件保持轻量，尽量只暴露稳定接口；实现细节放 `Private`。
- 不要把宽泛重构、格式化第三方代码、命名大迁移和功能改动混在同一个任务里。

## 测试与验证策略

- Core 或普通工具逻辑改动：优先补/跑 `CoreTests`。
- RHI、shader reflection、GPU config、ring buffer 等改动：补/跑 `RHITests`；涉及实际 D3D12 初始化时要注意 Windows runner 和 GPU/SDK 依赖。
- 启动路径、示例、窗口/应用层改动：跑 `SmokeTests`，必要时构建对应示例。
- 只改文档或注释时通常不需要完整构建，但仍应检查 Markdown/路径是否准确。

## 第三方与 GPU 依赖

- 子模块见 `.gitmodules`：`tracy`、`D3D12MemoryAllocator`、`ags`、`nvapi`。
- `Scripts/GpuDeps.json` 固定 Agility SDK 与 DirectStorage 版本。更新这些包应使用 `python Scripts/UpdateGpuDeps.py`，验证用 `python Scripts/VerifyGpuDeps.py`。
- `Source/ThirdParty` 中既有子模块也有 vendored 源码。除依赖更新任务外，不要改第三方源码；若必须 patch，说明原因并保持最小 diff。

## 给代理的工作流程

1. 先运行 `git status --short`，确认已有改动范围。
2. 用 `rg`/`rg --files` 找相关代码，先读相邻模块和 CMake 入口。
3. 修改前确认目标模块的 public/private 边界、生命周期模式和测试入口。
4. 编辑时保持小范围 diff。不要清理无关 TODO、注释或格式。
5. 改完后运行与风险匹配的最小验证命令，并在回复中说明跑了什么、是否通过。
6. 如果验证因为本机缺工具、SDK、GPU 或已有工作区问题失败，记录具体命令和失败点。
