# Code Style PRD

## 背景

项目跨多人、多平台开发，缺少统一风格规范会导致 code review 摩擦、可读性下降、自动格式化产生无意义 diff。本文档定义 `framework/`、`samples/`、`unit_tests/` 的代码风格基线。

---

## 目标

- 统一格式化风格，消除风格争议
- 减少 code review 中非功能性讨论
- 保持命名一致性，降低阅读理解成本
- 在 CI 中可自动验证（format / lint）

## 非目标

- 不覆盖 `third_party/` 代码
- 不规定算法或架构设计风格

---

## 规范

### 格式化

- 使用 `.clang-format`，`BasedOnStyle: Microsoft`
- 4 空格缩进，禁止 tab
- Allman 大括号风格（开括号另起一行）

### 语言与工具链

| 项目 | 要求 |
|------|------|
| C++ 标准 | C++17 |
| 警告处理 | 全部警告视为错误（`-Werror` / `/WX`） |
| 静态分析 | `clang-tidy`，配置见 `.clang-tidy`，仅对非 third_party 代码运行 |

### 命名规范

| 类别 | 风格 | 示例 |
|------|------|------|
| 类型（`class` / `struct` / `enum class`） | `PascalCase` | `RenderTarget` |
| 函数 / 方法 | `PascalCase` | `CreateBuffer()` |
| 局部变量 / 参数 | `snake_case` | `buffer_size` |
| 成员字段 | `m_` + `snake_case` | `m_camera_speed` |
| 常量 / 位掩码枚举值 | `UPPER_CASE` | `MAX_FRAMES_IN_FLIGHT` |

禁止引入拼写错误的标识符或文件名。

### 文件组织

- 头文件使用 `#pragma once`
- `.h` 与 `.cpp` 按 feature/module 配对
- include 路径与模块布局一致，例如 `<core/math.h>`

### API 与类设计

- 非抛异常的函数加 `noexcept`
- 明确所有权：owned 资源用智能指针，非 owning 引用用裸指针
- 不应被拷贝/移动的对象，显式 `delete` 拷贝和移动操作
- 无效状态检查使用 early return

### 注释

- 注释简短、技术化
- 待处理事项格式：`TODO(name): ...`
- 仅在意图不能从代码直接读出时才写注释

---

## 涉及配置文件

| 文件 | 用途 |
|------|------|
| `.clang-format` | 格式化规则 |
| `.clang-tidy` | 静态分析规则 |

---

## 实施方式

- 本地：提交前运行 `clang-format -i` 和 `clang-tidy`
- CI：格式化检查和 lint 检查作为 PR 必过门槛

---

## 验证标准

- [ ] `clang-format --dry-run` 无 diff
- [ ] 新增命名符合上述规范
- [ ] 编译无新增 warning
- [ ] 风格修改不与功能改动混入同一 PR
- [ ] `third_party/` 目录无改动
