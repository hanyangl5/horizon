# Coding Guidelines

These rules apply to new or modified first-party Runtime code. Do not rewrite existing or third-party code merely to make its style uniform. Tests, tools, and legacy modules should follow the established style of their directories.

- Use the repository-root `.clang-format`. Do not specify a conflicting format or line length elsewhere.
- Runtime targets use C++20, enable `/W3 /WX` under MSVC, and disable RTTI and exceptions. Do not introduce `throw`, `try`/`catch`, `dynamic_cast`, or designs that depend on RTTI.
- Write simple, efficient, minimal C/C++ code. Prefer the existing The Forge style: C-style APIs, explicit `init/add/remove/exit` lifecycles, `p`/`pp` pointer names, `m` member names, and `LOGF` logging.
- Avoid named variables for trivial expressions used only once. Make an existing local variable `const` when it is not reassigned, but do not extract a meaningless temporary merely to make it `const`.
- Do not use `auto` except when a lambda closure type must be stored. Spell out ordinary value, pointer, structure, iterator, and return-value types. Avoid unnecessary lambdas and complex templates.
- Do not use `goto`.
- Use C++20 designated initializers for aggregates that support them. Give public API structure fields useful defaults; initialize only non-default fields at call sites and name every initialized field.
- Do not use STL containers, strings, algorithms, or function wrappers as the default implementation in new or modified first-party Runtime code. Prefer C APIs, existing project array, memory, and string utilities, `stb_ds` dynamic arrays, or existing third-party infrastructure. Do not rewrite an existing module merely to satisfy this rule.
- Avoid unnecessary C++ standard-library headers such as `<vector>`, `<string>`, `<memory>`, `<functional>`, `<algorithm>`, and `<type_traits>` to reduce template-instantiation build costs.
- Avoid memory allocation in per-frame and hot paths. Do not design elaborate out-of-memory recovery; follow the contracts and checking conventions of the existing allocators.
- Avoid copying large user data structures. Pass structures by reference and array data through the project's existing span type. Always pass `hz::Span` function parameters by value.
- Check errors at the earliest point where failure is known, and prefer early exits over deeply nested control flow. Use `ASSERT` for programming preconditions. Use error returns and `LOGF` for recoverable external-input, initialization, or runtime failures. Do not explicitly call `abort` for programming errors.
- Do not maintain duplicate shadow state solely to validate user data forwarded directly to a low-level graphics API. Enable the appropriate backend debug or validation facilities during development.
- Graphics resources and GPU objects must have clear ownership. Destroy a resource only after recorded and executing GPU work no longer uses it, following the existing fence, semaphore, or queue-wait mechanisms.
- Keep public headers lightweight and expose only stable interfaces. Group APIs by function, such as keeping command-buffer APIs together, and place implementation details in the corresponding module's `Private` directory.
- Do not introduce new complex inheritance or virtual-function hierarchies.
- Follow the organization of the containing shader directory when adding first-party shaders. Closely related stages may share a source file; shaders with different purposes belong in separate files and must not be combined behind preprocessor conditionals.
- Do not add obvious comments, such as stating that arguments must refer to live objects. Do not add assertions or comments for theoretical overflow cases already covered by the existing 32-bit ranges.
- Review changes for performance issues before considering the work complete, especially hot-path allocations, unnecessary data copies, duplicate state tracking, and GPU/CPU synchronization.
- Do not combine broad refactoring, third-party formatting, large naming migrations, and functional changes in one task.

# Documentation Guidelines

- Keep Markdown documentation short, precise, and focused on user-visible behavior and actual design constraints. Describe graphics backends and API contracts according to the repository's current implementation. Avoid internal plumbing, exhaustive `Desc` or API catalogs, and sample-specific asset or format details better left in source files. Keep example descriptions brief.
