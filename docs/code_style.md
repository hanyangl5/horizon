# Horizon Code Style

This document defines the baseline style used for code in `framework/`, `samples/`, and `unit_tests/`.

## 1. Formatting

- Use `.clang-format` with `BasedOnStyle: Microsoft`.
- Use 4 spaces for indentation, no tabs.
- Use Allman brace style (opening brace on a new line).

## 2. Language and Tooling

- Use C++17.
- Keep code warning-clean under configured compilers.
- Treat warnings as errors in CI/local verification builds.
- Run `clang-tidy` checks configured in `.clang-tidy` for non-third-party code.

## 3. Naming

- Types (`class`, `struct`, `enum class`) use `PascalCase`.
- Functions and methods use `PascalCase`.
- Local variables and parameters use `snake_case`.
- Member fields use `m_` prefix + `snake_case` (for example: `m_camera_speed`, `m_resource_manager`).
- Constants and bitmask-style enum values use `UPPER_CASE`.
- Avoid introducing new misspelled identifiers or file names.

## 4. File Organization

- Use `#pragma once` in headers.
- Keep `.h` and `.cpp` paired by feature/module when practical.
- Keep include paths consistent with module layout (for example: `<core/math.h>`).

## 5. API and Class Conventions

- Add `noexcept` where behavior is non-throwing and stable.
- Prefer explicit ownership: use smart pointers for owned resources and raw pointers for non-owning references.
- Delete copy/move operations when an object should not be copied/moved.
- Use early return for invalid state checks.

## 6. Comments

- Keep comments short and technical.
- Use `TODO(name): ...` format for pending work.
- Add comments only where intent is not obvious from code.

## 7. Scope

- Apply this style to first-party code only.
- Do not reformat or restyle `third_party/` code.

## PR Checklist

- [ ] `clang-format` style preserved.
- [ ] New names follow the naming rules above.
- [ ] No new warnings.
- [ ] No style-only changes mixed with unrelated functional changes.
- [ ] No edits under `third_party/`.
