# Profile Guided Optimization (PGO) PRD

## 背景

编译器默认优化基于静态分析和启发式规则，无法感知真实运行热点。PGO 通过收集实际执行数据指导编译，可在不改业务代码的前提下进一步提升运行时性能（通常 5–15%），尤其对热路径 inlining 和 branch prediction 有明显收益。

目标平台：macOS / iOS（Clang）、Android NDK（Clang）、Windows（MSVC）。

---

## 目标

- 为所有目标平台提供完整的 PGO 构建流程
- 通过 CMake option + preset 封装，一行命令完成 instrument / optimize 构建
- 符号提取与 profile 管理标准化，便于 CI 集成

## 非目标

- 不自动选择 representative workload，需由开发者人工指定
- 不覆盖 sampling-based profiling（Instruments / Perfetto），PGO 属于 instrumentation-based
- 不强制集成 CI，流程为手动可选

---

## PGO 三阶段

```
Instrument → Profile → Optimize
```

| 阶段 | 操作 | 产物 |
|------|------|------|
| Instrument | 带插桩编译，运行采集数据 | `.profraw`（Clang）/ `.pgc`（MSVC） |
| Profile | 合并多次运行数据 | `merged.profdata`（Clang）/ `.pgd`（MSVC） |
| Optimize | 用 profile 重编译 | 最终优化二进制 |

---

## CMake 集成

### 选项

| CMake 变量 | 默认值 | 说明 |
|-----------|-------|------|
| `HORIZON_PGO_GENERATE` | `OFF` | 开启插桩构建 |
| `HORIZON_PGO_USE` | `OFF` | 使用已收集的 profile 优化构建 |
| `HORIZON_PGO_PROFILE_DIR` | `<root>/pgo_profiles` | profile 数据目录 |

接入位置：根 `CMakeLists.txt` 通过 `include(cmake/pgo.cmake)` 统一启用。
核心逻辑如下：

```cmake
option(HORIZON_PGO_GENERATE "Instrument build for PGO profiling" OFF)
option(HORIZON_PGO_USE      "Optimized build using PGO profile"  OFF)
set(HORIZON_PGO_PROFILE_DIR "${CMAKE_SOURCE_DIR}/pgo_profiles"
    CACHE PATH "Directory containing merged profile data")

if(HORIZON_PGO_GENERATE AND HORIZON_PGO_USE)
    message(FATAL_ERROR "HORIZON_PGO_GENERATE and HORIZON_PGO_USE are mutually exclusive.")
endif()

if(HORIZON_PGO_GENERATE OR HORIZON_PGO_USE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        add_compile_options(/GL)
        add_link_options(/LTCG)
        if(HORIZON_PGO_GENERATE)
            add_compile_definitions(HORIZON_PGO_GENERATE=1)
            add_link_options(/GENPROFILE:PGD=${HORIZON_PGO_PROFILE_DIR}/horizon.pgd)
        else()
            add_link_options(/USEPROFILE:PGD=${HORIZON_PGO_PROFILE_DIR}/horizon.pgd)
        endif()

    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if(HORIZON_PGO_GENERATE)
            add_compile_definitions(HORIZON_PGO_GENERATE=1)
            add_compile_options(-fprofile-instr-generate)
            add_link_options(-fprofile-instr-generate)
        else()
            set(_profdata "${HORIZON_PGO_PROFILE_DIR}/merged.profdata")
            if(NOT EXISTS "${_profdata}")
                message(FATAL_ERROR
                    "PGO profile not found: ${_profdata}\n"
                    "Run: tools/pgo_merge.sh ${HORIZON_PGO_PROFILE_DIR}")
            endif()
            add_compile_options(-fprofile-instr-use=${_profdata})
            if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
                add_compile_options(-fprofile-correction)
            endif()
            add_link_options(-fprofile-instr-use=${_profdata})
        endif()
    endif()
endif()
```

### CMakePresets 命名规范

```
<platform>_pgo_gen    macos_pgo_gen / android_pgo_gen / msvcwin64_pgo_gen
<platform>_pgo_use    macos_pgo_use / android_pgo_use / msvcwin64_pgo_use
```

### 与 LTO 组合

PGO 与 LTO 可以同时启用：

- `HORIZON_PGO_GENERATE`/`HORIZON_PGO_USE` 保持既有语义
- 额外开启 `HORIZON_ENABLE_LTO=ON` 即可叠加 LTO
- Clang 推荐 `HORIZON_LTO_MODE=THIN`（或 `AUTO`），MSVC 使用 `/GL + /LTCG`

示例（macOS PGO Use + LTO）：

```jsonc
{
  "name": "macos_pgo_use_lto",
  "inherits": "macos_pgo_use",
  "cacheVariables": {
    "HORIZON_ENABLE_LTO": "ON",
    "HORIZON_LTO_MODE": "THIN"
  }
}
```

示例（macOS）：

```jsonc
{
  "name": "macos_pgo_gen",
  "inherits": "macos_clang",
  "displayName": "macOS PGO – Instrument",
  "cacheVariables": { "HORIZON_PGO_GENERATE": "ON", "CMAKE_BUILD_TYPE": "RelWithDebInfo" }
},
{
  "name": "macos_pgo_use",
  "inherits": "macos_clang",
  "displayName": "macOS PGO – Optimize",
  "cacheVariables": { "HORIZON_PGO_USE": "ON", "CMAKE_BUILD_TYPE": "Release" }
}
```

---

## 平台 Workflow

### macOS

```bash
# 1. Instrument build
cmake --preset macos_pgo_gen && cmake --build --preset macos_pgo_gen

# 2. Run representative workloads
LLVM_PROFILE_FILE="$(pwd)/pgo_profiles/%p.profraw" \
    ./build/macos_pgo_gen/samples/your_sample

# 3. Merge raw profiles
tools/pgo_merge.sh pgo_profiles

# 4. Optimized build
cmake --preset macos_pgo_use && cmake --build --preset macos_pgo_use
```

### iOS Simulator

同 macOS。通过 Xcode scheme **Run → Arguments → Environment Variables** 设置 `LLVM_PROFILE_FILE`。

### iOS 真机

Profile 数据写入 app 沙盒，需手动 flush。

**在 app 适当位置调用（仅 `HORIZON_PGO_GENERATE` 构建）：**

```cpp
#ifdef HORIZON_PGO_GENERATE
extern "C" int  __llvm_profile_write_file(void);
extern "C" void __llvm_profile_set_filename(const char *);

void FlushPGOProfile()
{
    NSString *docs = NSSearchPathForDirectoriesInDomains(
        NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString *path = [docs stringByAppendingPathComponent:@"profile.profraw"];
    __llvm_profile_set_filename(path.UTF8String);
    __llvm_profile_write_file();
}
#endif
```

在 `applicationWillTerminate` / scene disconnect 调用 `FlushPGOProfile()`。

**提取 profile：** Xcode → Devices and Simulators → 目标设备 → 目标 App → Download Container，文件在 `AppData/Documents/profile.profraw`。

**后续步骤同 macOS 第 3–4 步。**

### Android NDK

```bash
# 1. Instrument build
cmake --preset android_pgo_gen && cmake --build --preset android_pgo_gen

# 2. Push & run
adb shell mkdir -p /data/local/tmp/pgo
adb push build/android_pgo_gen/samples/your_sample /data/local/tmp/
adb shell "LLVM_PROFILE_FILE=/data/local/tmp/pgo/%p.profraw /data/local/tmp/your_sample"
# APK 场景 profile 路径：/sdcard/Android/data/<package>/files/*.profraw

# 3. Pull profiles
adb pull /data/local/tmp/pgo/ pgo_profiles/android/

# 4. Merge（使用 NDK 自带 llvm-profdata，版本须与编译 Clang 一致）
$ANDROID_NDK/toolchains/llvm/prebuilt/darwin-arm64/bin/llvm-profdata \
    merge -output=pgo_profiles/merged.profdata pgo_profiles/android/*.profraw

# 5. Optimized build
cmake --preset android_pgo_use && cmake --build --preset android_pgo_use
```

### Windows (MSVC)

```bat
:: 1. Instrument build
cmake --preset msvcwin64_pgo_gen
cmake --build --preset msvcwin64_pgo_gen

:: 2. Run — .pgc 文件自动写到 .exe 同目录
build\msvcwin64_pgo_gen\samples\your_sample.exe

:: 3. Merge .pgc → .pgd
pgomgr /merge build\msvcwin64_pgo_gen\horizon.pgd pgo_profiles\horizon.pgd

:: 4. Optimized build
cmake --preset msvcwin64_pgo_use
cmake --build --preset msvcwin64_pgo_use
```

> `/GL`（Whole Program Optimization）是 MSVC PGO 前提，隐含 `/LTCG`，增量链接自动禁用。

---

## 辅助工具

### `tools/pgo_merge.sh`（Clang 平台通用 merge）

仓库已提供可执行脚本：

```bash
tools/pgo_merge.sh pgo_profiles
# 或指定特定 llvm-profdata
tools/pgo_merge.sh pgo_profiles \
  "$ANDROID_NDK/toolchains/llvm/prebuilt/darwin-arm64/bin/llvm-profdata"
```

---

## 实施优先级

| 优先级 | 平台 | 备注 |
|-------|------|------|
| P0 | macOS | 最简单，本地可验证 |
| P1 | Android | 需 ADB，NDK llvm-profdata 版本对齐 |
| P2 | Windows MSVC | pgomgr 工具链依赖 VS 安装 |
| P3 | iOS 真机 | 沙盒限制，需 flush 代码改动 |
| P3 | CI 集成 | 每次 Release build 自动归档 profile |

---

## 注意事项

- **workload 代表性**：用真实场景（渲染完整场景而非空帧），profile 质量决定优化效果
- **`-fprofile-correction`**：多次运行合并时抑制计数不一致告警
- **跨平台工具链匹配**：Android / iOS 必须用与编译器版本匹配的 `llvm-profdata`，位置在 `$ANDROID_NDK/toolchains/llvm/prebuilt/<host>/bin/`
- **定期刷新 profile**：大规模代码重构后重新采集；过期 profile 可能轻微劣化性能

---

## 验证标准

- `cmake --preset <platform>_pgo_gen` 编译无错误
- instrumented binary 运行后生成 `.profraw` / `.pgc` 文件
- merge 步骤产生 `merged.profdata` / `horizon.pgd`
- `cmake --preset <platform>_pgo_use` 编译无错误（Clang 无 "profile data may be out of date" 硬错误）
- optimized binary 与 non-PGO Release 相比，热路径帧时间有可测量改善
