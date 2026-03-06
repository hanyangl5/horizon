# Profile Guided Optimization (PGO)

PGO improves runtime performance by compiling with real execution profiles. The process has three phases:

1. **Instrument** – build with profiling instrumentation
2. **Profile** – run the instrumented binary on representative workloads
3. **Optimize** – rebuild using the collected profiles

Supported platforms: macOS / iOS (Clang), Android NDK (Clang), Windows (MSVC).

---

## CMake Integration

### Options

| CMake Variable | Default | Description |
|---|---|---|
| `HORIZON_PGO_GENERATE` | `OFF` | Instrument build for profile collection |
| `HORIZON_PGO_USE` | `OFF` | Optimized build consuming collected profiles |
| `HORIZON_PGO_PROFILE_DIR` | `<root>/pgo_profiles` | Directory for profile data |

Add to `framework/CMakeLists.txt`:

```cmake
option(HORIZON_PGO_GENERATE "Instrument build for PGO profiling" OFF)
option(HORIZON_PGO_USE      "Optimized build using PGO profile"  OFF)
set(HORIZON_PGO_PROFILE_DIR "${CMAKE_SOURCE_DIR}/pgo_profiles"
    CACHE PATH "Directory containing merged profile data")

# ── PGO ──────────────────────────────────────────────────────────
if(HORIZON_PGO_GENERATE OR HORIZON_PGO_USE)
    if(MSVC)
        add_compile_options(/GL)
        add_link_options(/LTCG)
        if(HORIZON_PGO_GENERATE)
            add_link_options(/GENPROFILE:PGD=${HORIZON_PGO_PROFILE_DIR}/horizon.pgd)
        elseif(HORIZON_PGO_USE)
            add_link_options(/USEPROFILE:PGD=${HORIZON_PGO_PROFILE_DIR}/horizon.pgd)
        endif()

    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        if(HORIZON_PGO_GENERATE)
            add_compile_options(-fprofile-instr-generate)
            add_link_options(-fprofile-instr-generate)
        elseif(HORIZON_PGO_USE)
            set(_profdata "${HORIZON_PGO_PROFILE_DIR}/merged.profdata")
            if(NOT EXISTS "${_profdata}")
                message(FATAL_ERROR
                    "PGO profile not found: ${_profdata}\n"
                    "Run: llvm-profdata merge -output=${_profdata} ${HORIZON_PGO_PROFILE_DIR}/*.profraw")
            endif()
            add_compile_options(-fprofile-instr-use=${_profdata} -fprofile-correction)
            add_link_options(-fprofile-instr-use=${_profdata})
        endif()
    endif()
endif()
```

### CMakePresets.json

Add the following presets (one pair per platform):

```jsonc
{
  "name": "macos_pgo_gen",
  "inherits": "macos_clang",
  "displayName": "macOS PGO – Instrument",
  "cacheVariables": {
    "HORIZON_PGO_GENERATE": "ON",
    "CMAKE_BUILD_TYPE": "RelWithDebInfo"
  }
},
{
  "name": "macos_pgo_use",
  "inherits": "macos_clang",
  "displayName": "macOS PGO – Optimize",
  "cacheVariables": {
    "HORIZON_PGO_USE": "ON",
    "CMAKE_BUILD_TYPE": "Release"
  }
}
```

Repeat with `inherits` pointing to `android_framework`, `msvcwin64`, etc.

---

## Platform Workflows

### macOS

```bash
# 1. Instrument build
cmake --preset macos_pgo_gen
cmake --build --preset macos_pgo_gen

# 2. Run representative workloads
LLVM_PROFILE_FILE="$(pwd)/pgo_profiles/%p.profraw" \
    ./build/macos_pgo_gen/samples/your_sample

# 3. Merge raw profiles
llvm-profdata merge \
    -output=pgo_profiles/merged.profdata \
    pgo_profiles/*.profraw

# 4. Optimized build
cmake --preset macos_pgo_use
cmake --build --preset macos_pgo_use
```

---

### iOS Simulator

Same as macOS. Use the simulator scheme in Xcode or `xcodebuild`.

Set the `LLVM_PROFILE_FILE` environment variable via the scheme's **Run → Arguments → Environment Variables**.

### iOS Device

Profile data is written to the app's sandbox. You must flush it manually before the app exits.

**Step 1 – Flush profile in app code** (guard with `HORIZON_PGO_GENERATE`):

```cpp
#ifdef HORIZON_PGO_GENERATE
extern "C" int  __llvm_profile_write_file(void);
extern "C" void __llvm_profile_set_filename(const char *);

void FlushPGOProfile()
{
    // Write to Documents/ (accessible via Xcode "Download Container")
    NSString *docs = NSSearchPathForDirectoriesInDomains(
        NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSString *path = [docs stringByAppendingPathComponent:@"profile.profraw"];
    __llvm_profile_set_filename(path.UTF8String);
    __llvm_profile_write_file();
}
#endif
```

Call `FlushPGOProfile()` from `applicationWillTerminate` / scene disconnect.

**Step 2 – Extract profile:**

In Xcode: **Devices and Simulators → your device → your app → Download Container**.  
The `.profraw` file is inside the container at `AppData/Documents/profile.profraw`.

**Step 3 – Merge and rebuild** (same as macOS step 3–4).

---

### Android (NDK Clang)

```bash
# 1. Instrument build
cmake --preset android_pgo_gen
cmake --build --preset android_pgo_gen

# 2. Push binary / library, run on device
adb shell mkdir -p /data/local/tmp/pgo
adb push build/android_pgo_gen/samples/your_sample /data/local/tmp/
adb shell "LLVM_PROFILE_FILE=/data/local/tmp/pgo/%p.profraw \
           /data/local/tmp/your_sample"

# For APK builds the profile lands in the app's files dir:
#   /sdcard/Android/data/<package>/files/*.profraw
# Pull with:
#   adb pull /sdcard/Android/data/<package>/files/ pgo_profiles/android/

# 3. Pull profiles
adb pull /data/local/tmp/pgo/ pgo_profiles/android/

# 4. Merge using NDK's llvm-profdata
$ANDROID_NDK/toolchains/llvm/prebuilt/darwin-arm64/bin/llvm-profdata \
    merge -output=pgo_profiles/merged.profdata \
    pgo_profiles/android/*.profraw

# 5. Optimized build
cmake --preset android_pgo_use
cmake --build --preset android_pgo_use
```

> **Note:** The `llvm-profdata` binary must match the Clang version used by the NDK.  
> Find it at `$ANDROID_NDK/toolchains/llvm/prebuilt/<host>/bin/llvm-profdata`.

---

### Windows (MSVC)

```bat
:: 1. Instrument build
cmake --preset msvcwin64_pgo_gen
cmake --build --preset msvcwin64_pgo_gen

:: 2. Run – .pgc files are written next to the .exe automatically
build\msvcwin64_pgo_gen\samples\your_sample.exe

:: 3. Merge .pgc into .pgd
::    pgomgr.exe is in the VS tools directory (add to PATH or use full path)
pgomgr /merge build\msvcwin64_pgo_gen\horizon.pgd pgo_profiles\horizon.pgd

:: 4. Optimized build
cmake --preset msvcwin64_pgo_use
cmake --build --preset msvcwin64_pgo_use
```

> **Note:** `/GL` (Whole Program Optimization) is required for MSVC PGO and implies `/LTCG` at link time.  
> Incremental linking is disabled automatically.

---

## Helper Script

`tools/pgo_merge.sh` – wraps the merge step for Clang platforms:

```bash
#!/usr/bin/env bash
# Usage: pgo_merge.sh <profile_dir> [llvm-profdata]
set -euo pipefail

PROFILE_DIR="${1:?Usage: $0 <profile_dir> [llvm-profdata]}"
PROFDATA="${2:-llvm-profdata}"
OUTPUT="${PROFILE_DIR}/merged.profdata"

RAW_FILES=("${PROFILE_DIR}"/*.profraw)
if [ ${#RAW_FILES[@]} -eq 0 ]; then
    echo "No .profraw files found in ${PROFILE_DIR}" >&2
    exit 1
fi

"${PROFDATA}" merge -output="${OUTPUT}" "${RAW_FILES[@]}"
echo "Merged profile written to: ${OUTPUT}"
```

---

## Tips

- **Representative workloads matter.** Profile with your most common runtime scenarios (e.g., rendering a real scene, not just a blank frame).
- **`-fprofile-correction`** silences warnings about mismatched counters that sometimes appear when merging profiles from multiple runs.
- **Cross-compilation note (Android/iOS):** always use the `llvm-profdata` that ships with the same toolchain used to compile — host and device Clang versions must match.
- **Iterative PGO:** re-profile after major code refactors; stale profiles can slightly hurt rather than help.
