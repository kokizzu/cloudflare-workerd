---
name: workerd-safety-review
description: Memory safety, thread safety, concurrency, and critical detection patterns for workerd code review. Covers V8/KJ boundary hazards, lifetime management, cross-thread safety, and coroutine pitfalls. Load this skill when reviewing code that touches io/, jsg/, async patterns, or V8 integration.
---

## Safety Analysis — Memory, Thread Safety & Concurrency

Apply the checklists and detection patterns below when analyzing code for memory safety, thread safety, or concurrency correctness.

---

### Memory Safety

- Identify potential memory leaks, use-after-free, and dangling pointers or references
- Review ownership semantics and lifetime management
- Analyze smart pointer usage (`kj::Own`, `kj::Rc`, `kj::Maybe`)
- Check for proper RAII, CRTP patterns
- Look for potential buffer overflows and bounds checking issues
- Identify raw pointer usage that could be safer with owning types
- Review destructor correctness and cleanup order
- Analyze lambda captures for safety
- Consider patterns where weakrefs (see `util/weak-refs.h`) or other techniques would be safer
- Methods that return references, pointers, `kj::ArrayPtr`, `kj::StringPtr`, or other non-owning
  views into data owned by `this` (or by a parameter) should be annotated with `KJ_LIFETIMEBOUND`.
  This expands to `[[clang::lifetimebound]]` and enables the compiler to warn when the returned
  view outlives the object it borrows from. Flag missing annotations during review, especially on
  accessors that return `kj::ArrayPtr<T>`, `kj::StringPtr`, `kj::Maybe<T&>`, or raw `T&`/`const T&`.
- Functions that return owned resources, `kj::Maybe`, error indicators, or expensive-to-compute
  results should be annotated with `KJ_WARN_UNUSED_RESULT`. This catches two classes of problems:
  silently discarding results the caller must act on (e.g., error codes, `kj::Promise`), and
  performing expensive computation whose result is thrown away. `kj::Promise` is already
  `[[nodiscard]]` at the type level, but other return types need per-function annotation.
- Lambdas that capture `jsg::Ref<T>` or other GC-traced references must use `JSG_VISITABLE_LAMBDA`
  (see `jsg/function.h`) so V8's GC can trace through the captures. Without it, captured JS-heap
  objects can be collected while the lambda still references them. Flag closures stored for deferred
  execution that capture GC-managed types without using this macro.
- RAII scope guards, locks, and other types with positional semantics must use
  `KJ_DISALLOW_COPY_AND_MOVE` to prevent accidental moves that break scope invariants. Use
  `KJ_DISALLOW_COPY` only when explicit move semantics are intentionally provided. Flag new
  scope-guard or lock types that are missing these annotations.

### Thread Safety & Concurrency

- Identify data races and race conditions
- Review lock ordering and deadlock potential
- Analyze shared state access patterns
- Check for proper synchronization primitives usage
- Review promise/async patterns for correctness
- Identify thread-unsafe code in concurrent contexts
- Analyze KJ event loop interactions
- Ensure that code does not attempt to use isolate locks across suspension points in coroutines.
  Types that must never be held across `co_await` should carry the `KJ_DISALLOW_AS_COROUTINE_PARAM`
  annotation for compile-time enforcement. This is already applied to `jsg::Lock`, `Worker::Lock`,
  `jsg::V8StackScope`, and similar types. Flag new lock or scope types that are missing it.
- Ensure that RAII objects and other types that capture raw pointers or references are not unsafely
  used across suspension points
- When reviewing V8 integration, pay particular attention to GC interactions and cleanup order
- kj I/O objects should never be held by a V8-heap object without use of `IoOwn` or `IoPtr`
  (see `io/io-own.h`) to ensure proper lifetime and thread-safety.
- Watch carefully for places where `kj::Refcounted` is used when `kj::AtomicRefcounted` is required
  for thread safety.

---

### Critical Detection Patterns

Concrete patterns to watch for. When you encounter these, flag them at the indicated severity.

Beyond these specific patterns, also watch for non-obvious complexity at V8/KJ boundaries: re-entrancy bugs where a C++ callback unexpectedly re-enters JavaScript, subtle interactions between KJ event loop scheduling and V8 GC timing, and cases where destruction order depends on runtime conditions.

**CRITICAL / HIGH:**

- **V8 callback throwing C++ exception**: A V8 callback (JSG method, property getter/setter) that can throw a C++ exception without using `liftKj` (see `jsg/util.h`). V8 callbacks must catch C++ exceptions and convert them to JS exceptions.
- **V8 heap object holding kj I/O object directly**: A `jsg::Object` subclass storing `kj::Own<T>`, `kj::Rc<T>`, `kj::Arc<T>` for an I/O-layer object without wrapping in `IoOwn` or `IoPtr` (see `io/io-own.h`). Causes lifetime and thread-safety bugs.
- **`kj::Refcounted` in cross-thread context**: A class using `kj::Refcounted` whose instances can be accessed from both the I/O thread and the JS isolate thread. Needs `kj::AtomicRefcounted`.
- **Isolate lock held across `co_await`**: Holding a `jsg::Lock`, V8 `HandleScope`, or similar V8 scope object across a coroutine suspension point. This is undefined behavior.
- **RAII object with raw pointer/reference across `co_await`**: Any RAII type or variable capturing a raw pointer or reference used across a coroutine suspension point without `kj::coCapture` to ensure correct lifetime.
- **Reference cycle between `jsg::Object` subclasses**: Two or more `jsg::Object` subclasses holding strong references to each other without GC tracing via `JSG_TRACE`. Causes memory leaks invisible to V8's GC.
- **`jsg::Object` destructor accessing another `jsg::Object`**: V8 GC destroys objects in non-deterministic order. A destructor that dereferences another GC-managed object may use-after-free.

**MEDIUM (safety-related):**

- **Broad capture in async lambda**: Lambda passed to `.then()` or stored for deferred execution using `[&]` or `[this]` when only specific members are needed. Prefer explicit captures and carefully consider captured variable lifetimes.
- **Implicit GC trigger in sensitive context**: V8 object allocations (e.g., `ArrayBuffer` backing store creation, string flattening, `v8::Object::New()`) inside hot loops or time-sensitive callbacks may trigger GC unexpectedly.
- **Missing `DISALLOW_KJ_IO_DESTRUCTORS_SCOPE` awareness**: The `DISALLOW_KJ_IO_DESTRUCTORS_SCOPE` macro (see `jsg/wrappable.h`) creates a scope that crashes the process if any KJ async/I/O object is destroyed. It enforces that JS-heap objects use `IoOwn`/`IoPtr` rather than holding I/O objects directly. `IoOwn`'s destructor creates a matching `AllowAsyncDestructorsScope` to permit safe destruction. When reviewing code that introduces new GC or destructor paths, verify these scopes are correctly nested.

---

### Runtime-Specific Safety Notes

- **V8 GC awareness**: V8 may GC at any allocation point. Operations that create V8 objects (including string flattening, ArrayBuffer creation) can trigger GC. Be aware of this when analyzing code that interleaves V8 allocations with raw pointer access to V8-managed objects.
- **Destructors may throw**: workerd follows KJ convention of `noexcept(false)` destructors. Do not flag this as an issue unless there is a specific exception safety problem (e.g., double-exception during stack unwinding).
- **Cross-platform**: workerd runs on Linux in production but builds on macOS and Windows. Flag platform-specific system calls or assumptions (e.g., Linux-only epoll, /proc filesystem) that lack portable alternatives.
