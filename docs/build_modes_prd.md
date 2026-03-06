# Build Modes PRD

## 背景

当前项目只有 CMake 原生的 `Debug` / `Release` 两种构建模式，缺乏中间层级。
参考 Unreal Engine 的 Development / Shipping 分层，引入更清晰的构建语义：

- **Development**：面向开发调试，带完整调试符号但启用优化，性能明显优于 Debug，适合日常开发和性能分析。
- **Shipping**：面向发布，最高优化等级，调试符号单独保存（不打包进二进制），用于线上 crash 定位。

目标平台：macOS / iOS（Clang）、Android NDK（Clang）、Windows（MSVC）。

---

## 构建模式定义

| 模式 | 优化等级 | 调试符号 | Assertions | NDEBUG | 备注 |
|------|---------|---------|-----------|--------|------|
| `Debug` | 无 / `-O0` / `/Od` | 完整内嵌 | ON | 未定义 | 现有，不变 |
| `Development` | `-O2` / `/O2` | 完整内嵌 | ON | 未定义 | 新增 |
| `Shipping` | `-O3` / `/O2 /GL` | 生成后剥离 | OFF | 定义 | 新增 |

---

## 编译 Flag 规范

### Clang（macOS / iOS / Android NDK）

#### Development
```
-O2 -g -fno-omit-frame-pointer
```
- `-O2`：中等优化，inlining 适度，利于 profiler 定位热点
- `-g`：生成完整 DWARF，支持断点调试
- `-fno-omit-frame-pointer`：保留帧指针，`perf` / Instruments 调用栈完整

#### Shipping
```
-O3 -g -fno-omit-frame-pointer -DNDEBUG
```
- `-O3`：最高优化（可选追加 `-flto=thin` 开启 ThinLTO）
- `-g`：仍然生成 DWARF，供后处理提取
- `-DNDEBUG`：关闭 `assert()`
- 构建后：`dsymutil` 提取符号 → `strip -S` 剥离二进制

### MSVC（Windows）

#### Development
```
/O2 /Zi /Ob2 /MD
```
- `/O2`：速度优化
- `/Zi`：生成独立 PDB
- `/Ob2`：积极内联
- `/MD`：Release CRT（不用 debug heap）

#### Shipping
```
/O2 /GL /Zi /MD /DNDEBUG
```
链接参数追加：
```
/LTCG /DEBUG:FULL /OPT:REF /OPT:ICF
```
- `/GL` + `/LTCG`：全程序优化（Link-Time Code Generation）
- `/DEBUG:FULL`：生成完整 PDB（符号与二进制分离）
- `/OPT:REF /OPT:ICF`：移除未引用代码、折叠重复函数

---

## 符号处理方案

### 与 LTO 协同

LTO 由独立开关控制，不改变 Build Mode 语义：

- `HORIZON_ENABLE_LTO=ON`：启用全局 LTO
- `HORIZON_LTO_MODE=AUTO|THIN|FULL`：选择 LTO 模式（`AUTO` 下 Clang 默认 ThinLTO，MSVC 使用 `/GL + /LTCG`）
- `Development` 默认不开启 LTO；`Shipping` 推荐按平台评估后启用

该方案与 PGO 可共存，组合时以 PGO 原有流程为主（仅叠加 LTO 链接期优化能力）。

### macOS / iOS

```
Post-build:
  dsymutil <binary> -o <binary>.dSYM   # 提取 DWARF 到 .dSYM bundle
  strip -S <binary>                      # 从二进制剥离调试段
```

`.dSYM` 归档到 `symbols/` 目录，不随二进制分发。  
Crash report（`.ips` / `PLCrashReporter` 格式）用 `atos` 或 `symbolicatecrash` + `.dSYM` 还原符号。

### Android NDK

```
Post-build:
  保存 unstripped .so → symbols/android/<abi>/lib<name>.so
  llvm-strip <binary>                    # APK 打包用 stripped 版本
```

Gradle 侧配合 `android.packagingOptions` 确保 APK 内 `.so` 为 stripped。  
崩溃分析用 NDK 自带 `ndk-stack` 或 Firebase Crashlytics + unstripped `.so`。

### Windows MSVC

MSVC `/DEBUG:FULL` 自动将符号写入 `.pdb`，与 `.exe` / `.dll` 物理分离。  
部署策略：
- 发布物：只包含 `.exe` / `.dll`
- 符号存档：`.pdb` 上传符号服务器（`symstore` / Azure Artifacts Symbol Server）
- Crash dump 分析：WinDbg / Visual Studio 连接符号服务器自动加载

---

## 宏定义

代码内通过宏区分构建模式，在 `framework/CMakeLists.txt` 中注入：

```cmake
# Development
target_compile_definitions(horizon_framework PUBLIC HZ_BUILD_DEVELOPMENT)

# Shipping
target_compile_definitions(horizon_framework PUBLIC HZ_BUILD_SHIPPING)
```

使用示例：

```cpp
#if defined(HZ_BUILD_SHIPPING)
    // 关闭 in-game console、overlay profiler UI
    // 关闭非必要的日志输出
#endif

#if defined(HZ_BUILD_DEVELOPMENT) || defined(HZ_BUILD_SHIPPING)
    // 优化路径，不走 Debug 慢路径
#endif
```

---

## 文件结构

新增以下文件：

```
cmake/
├── BuildTypes.cmake        ← 注册 Development / Shipping build type
├── CompilerFlags.cmake     ← 各编译器 flag 细节
└── SymbolExtraction.cmake  ← 平台后处理（dsymutil / strip / PDB 归档）
docs/
└── build_modes_prd.md      ← 本文档
```

修改以下文件：

| 文件 | 变更内容 |
|------|---------|
| `CMakeLists.txt`（root） | `include()` 三个新 cmake 模块 |
| `framework/CMakeLists.txt` | 注入 `HZ_BUILD_*` 宏定义 |
| `CMakePresets.json` | 新增各平台 `_development` / `_shipping` preset |

---

## CMakePresets 命名规范

```
<platform>_development    macos_development / android_development / msvcwin64_development
<platform>_shipping       macos_shipping    / android_shipping    / msvcwin64_shipping
```

### 示例（macOS）

```jsonc
{
  "name": "macos_development",
  "inherits": "macos_clang",
  "displayName": "macOS Development",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Development"
  }
},
{
  "name": "macos_shipping",
  "inherits": "macos_clang",
  "displayName": "macOS Shipping",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Shipping"
  }
}
```

---

## 实施优先级

| 优先级 | 任务 |
|-------|------|
| P0 | `cmake/BuildTypes.cmake`：注册 build type + Clang/MSVC flag |
| P0 | `CMakePresets.json`：macOS + Windows preset |
| P1 | `cmake/SymbolExtraction.cmake`：macOS dsymutil/strip post-build |
| P1 | Windows PDB 归档脚本 |
| P2 | Android unstripped `.so` 保存 |
| P2 | iOS Device 支持 |
| P3 | CI 集成（每次 Shipping build 自动归档符号） |

---

## 验证标准

- `Development` build：可用 Instruments / MSVC Profiler 获得完整调用栈
- `Shipping` build：
  - 二进制不含调试信息（`dwarfdump` / `objdump` 无输出）
  - 用 `.dSYM` / `.pdb` + crash dump 能还原到源码行号
  - `assert()` 不触发（`NDEBUG` 已定义）
- 所有平台 `cmake --preset <name>` 一行完成构建，无需手动传 flag
