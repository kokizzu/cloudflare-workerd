# Code Review Checklist

When reviewing workerd C++ code, check for each of these items.

---

1. **STL leaking in**: `std::string`, `std::vector`, `std::optional`, `std::unique_ptr`, etc.
2. **Raw `new`/`delete`**: Should be `kj::heap<T>()` or similar
3. **`throw` statements**: Should use `KJ_ASSERT`/`KJ_REQUIRE`/`KJ_FAIL_ASSERT`
4. **`noexcept` declarations**: Should not be present
5. **Destructor missing `noexcept(false)`**: Required on all explicit destructors
6. **`[=]` lambda captures**: Never allowed
7. **Nullable raw pointers (`T*`)**: Should be `kj::Maybe<T&>`
8. **Mixed interface/implementation classes**: No data members in interfaces, no non-final virtuals in implementations
9. **Singletons or mutable globals**: Not allowed
10. **Missing `.attach()` on promises**: Objects must stay alive for the promise duration
11. **Background promise without `.eagerlyEvaluate()`**: Lazy continuations may never execute
12. **Manual errno checks**: Should use `KJ_SYSCALL` / `KJ_SYSCALL_HANDLE_ERRORS`
13. **`static_cast` for downcasting**: Should be `kj::downcast<T>` (debug-checked)
14. **`std::to_string` or `+` string concatenation**: Should be `kj::str()`
15. **`dynamic_cast` for dispatch**: Extend the interface instead
16. **`/* */` block comments**: Use `//` line comments
17. **Naming**: TitleCase types, camelCase functions/variables, CAPS constants
18. **Missing braces**: Required unless entire statement is on one line
19. **`bool` function parameter**: Prefer `enum class` or `WD_STRONG_BOOL` for clarity at call sites. E.g., `void connect(bool secure)` should be `void connect(SecureMode mode)`.
20. **Missing `[[nodiscard]]`**: Functions returning error codes, `kj::Maybe`, or success booleans that callers must check should be `[[nodiscard]]`.
21. **Promise chain where coroutine would be clearer**: Nested `.then()` chains with complex error handling that would be more readable as a coroutine with `co_await`. But avoid suggesting sweeping rewrites.
22. **Missing `constexpr` / `consteval`**: Compile-time evaluable functions or constants not marked accordingly.
23. **Reinvented utility**: Custom code duplicating functionality already in `src/workerd/util/` (e.g., custom ring buffer, small set, state machine, weak reference pattern). Check the util directory before suggesting a new abstraction.
24. **Missing `override`**: Virtual method overrides missing the `override` specifier.
25. **Direct `new`/`delete` (via `new` expression)**: Should use `kj::heap<T>()`, `kj::heapArray<T>()`, or other KJ memory utilities.
26. **Explicit `throw` statement**: Should use `KJ_ASSERT`, `KJ_REQUIRE`, `KJ_FAIL_ASSERT`, or `KJ_EXCEPTION` instead of bare `throw`.
27. **Magic numbers**: Numeric literals without explanation or named constants.
28. **Copyright header on new files**: Every new `.c++` and `.h` file must begin with the project copyright/license header using the current year (not copied from older files). Expected format:
    ```
    // Copyright (c) <current-year> Cloudflare, Inc.
    // Licensed under the Apache 2.0 license found in the LICENSE file or at:
    //     https://opensource.org/licenses/Apache-2.0
    ```
    Flag any new file that uses a stale year (e.g., `2017-2022` in a file created in 2026) or omits the header entirely.
