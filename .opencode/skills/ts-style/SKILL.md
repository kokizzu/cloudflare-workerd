---
name: ts-style
description: JS/TS style guidelines and review checklist for workerd. Covers TypeScript strictness, import conventions, export patterns, private field syntax, error handling, feature gating, and test structure. Load this skill when reviewing or writing JavaScript or TypeScript code in src/node/, src/cloudflare/, or JS/TS test files under src/workerd/.
---

## JS/TS Style Guide — Code Review Reference

ESLint config: `tools/base.eslint.config.mjs`
TypeScript config: `tools/base.tsconfig.json`
Prettier config: `.prettierrc.json`

Many conventions below are enforced by ESLint and the TypeScript compiler. The checklist focuses
on what automated tools miss and on verifying that enforced rules are not being bypassed with
`eslint-disable` comments.

For `src/node/` specifics, see `src/node/AGENTS.md` and `src/node/README.md`.
For `src/cloudflare/` specifics, see `src/cloudflare/AGENTS.md` and `src/cloudflare/README.md`.

---

### TypeScript Strictness

The project uses strict TypeScript with several settings stricter than typical:

- **`exactOptionalPropertyTypes: true`** — optional properties cannot be explicitly set to
  `undefined` unless the type includes `| undefined`. E.g., `{ x?: string }` does not accept
  `{ x: undefined }`.
- **`noUncheckedIndexedAccess: true`** — indexed access on arrays and records returns `T | undefined`.
  Code must narrow or assert before using the value.
- **`verbatimModuleSyntax: true`** — type-only imports **must** use `import type { X }` or inline
  `import { type X, Y }`. A bare `import { X }` where `X` is only used as a type is a compile error.
- **`erasableSyntaxOnly: true`** — no TypeScript enums, no `namespace` declarations, no parameter
  properties. Only syntax that erases cleanly to JavaScript is permitted.
- **`noImplicitReturns: true`**, **`noUnusedLocals: true`**, **`noUnusedParameters: true`** —
  unused variables are prefixed with `_` to signal intent.

### Import Conventions

Import specifiers follow a tiered module system:

| Specifier prefix        | Visibility | Purpose                                                 |
| ----------------------- | ---------- | ------------------------------------------------------- |
| `node:*`                | Public     | Standard Node.js module API                             |
| `node-internal:*`       | Private    | Internal Node.js implementation (not user-facing)       |
| `cloudflare:*`          | Public     | Cloudflare product APIs                                 |
| `cloudflare-internal:*` | Private    | Cloudflare runtime-provided internals (not user-facing) |

- Public modules (`node:*`, `cloudflare:*`) must not import from internal specifiers of the
  other tier. `src/node/` code must not import `cloudflare-internal:*` and vice versa.
- Internal specifiers (`*-internal:*`) are not available to user code. Do not expose them in
  public module exports.
- `.d.ts` files without a matching `.ts` file declare the shape of C++ JSG modules imported
  via internal specifiers.

### Export Patterns

**Public modules** (`src/node/*.ts`, `src/cloudflare/*.ts`) use a dual-export pattern:

```typescript
export { Foo, Bar, type Baz } from 'internal-module';
export default { Foo, Bar };
```

Both named exports and a default export object are required for compatibility with different
import styles.

**Internal modules** export directly:

```typescript
export class Foo { ... }
export function bar(): void { ... }
```

Do not re-export namespace imports as default. The ESLint rule
`workerd/no-export-default-of-import-star` (in `src/node/`) enforces:

```typescript
// BAD
import * as X from 'module';
export default X;

// GOOD — explicit named re-exports
export { foo, bar } from 'module';
export default { foo, bar };
```

### Private Fields

Use `#private` syntax for private members, not the `private` keyword:

```typescript
// GOOD
class Foo {
  #bar: string;
  #doSomething(): void { ... }
}

// BAD — flagged by ESLint no-restricted-syntax
class Foo {
  private bar: string;
  private doSomething(): void { ... }
}
```

Exceptions exist in older code with `eslint-disable` comments (e.g., when `#` syntax conflicts
with `implements` interface constraints). New code should not add new exceptions without
justification.

### Accessibility Modifiers

Do not write the `public` keyword. Members without an accessibility modifier are public by default.

```typescript
// GOOD
class Foo {
  bar: string;
  doSomething(): void { ... }
}

// BAD — flagged by ESLint
class Foo {
  public bar: string;
  public doSomething(): void { ... }
}
```

### Error Handling

**`src/node/`**: Use Node.js-style error classes from `node-internal:internal_errors`:

```typescript
import { ERR_METHOD_NOT_IMPLEMENTED } from 'node-internal:internal_errors';
throw new ERR_METHOD_NOT_IMPLEMENTED('setEngine');
```

Do not throw bare `Error` objects when a matching `ERR_*` class exists.

**`src/cloudflare/`**: Define product-specific error classes extending `Error`:

```typescript
export class InferenceUpstreamError extends Error {
  constructor(message: string, name = 'InferenceUpstreamError') {
    super(message);
    this.name = name;
  }
}
```

### Feature Gating

Runtime compat flags are checked via `Cloudflare.compatibilityFlags`:

```typescript
if (!Cloudflare.compatibilityFlags.some_flag) {
  // legacy behavior
}
```

Any behavioral change that could break existing workers must be gated behind a compat flag.
See `src/workerd/io/compatibility-date.capnp` for the flag schema.

### Test Patterns

Tests are plain `.js` files (not `.ts`) using `node:assert`. No test frameworks.

**Named export pattern** (preferred for multiple tests per file):

```javascript
export const someFeatureTest = {
  test() {
    assert.strictEqual(actual, expected);
  },
};
```

**Default export pattern** (for handler-based tests):

```javascript
export default {
  async fetch(request, env, ctx) { ... },
  async test(ctrl, env, ctx) { ... },
};
```

Each test `.js` file is paired with a `.wd-test` Cap'n Proto config. See the `wd-test-format`
skill for config details.

**Mock services** (`*-mock.js`) export a default handler for simulating external services:

```javascript
export default {
  async fetch(request, env, ctx) { ... },
};
```

### Formatting

- **Single quotes** (not double)
- **2-space indentation**
- **Trailing commas** in ES5-valid positions
- **80-char line width**
- Run `just format` before submitting

---

### Review Checklist

When reviewing JS/TS code in workerd, check for each of these items.

---

1. **Copyright header on new files**: Every new `.ts` and `.js` file must begin with the project
   copyright/license header using the current year. Expected format:
   ```
   // Copyright (c) <current-year> Cloudflare, Inc.
   // Licensed under the Apache 2.0 license found in the LICENSE file or at:
   //     https://opensource.org/licenses/Apache-2.0
   ```
   Flag any new file that uses a stale year or omits the header entirely. Files adapted from
   Node.js or Deno should include attribution after the Cloudflare header.
2. **Missing `import type` for type-only imports**: With `verbatimModuleSyntax`, type-only imports
   must use `import type { X }` or `import { type X, Y }`. The compiler catches this, but verify
   it in review since CI may not always run on every file.
3. **Wrong import specifier tier**: Public modules must not import from `*-internal:*` specifiers
   of the other subsystem. Internal implementation details must not leak through public exports.
4. **Missing explicit return type**: All functions in `.ts` files must have explicit return types
   (enforced by ESLint). Check that new functions comply and that `eslint-disable` is not being
   used to bypass this.
5. **`private` keyword instead of `#private`**: New code must use `#` syntax for private members.
   Flag any new `eslint-disable` for `no-restricted-syntax` on private members without clear
   justification.
6. **`public` keyword on class members**: Should be omitted. Flagged by ESLint but verify in review.
7. **Non-null assertion (`!`)**: Banned by ESLint. Prefer proper narrowing or `?? defaultValue`.
   Flag any `eslint-disable` for `@typescript-eslint/no-non-null-assertion`.
8. **TypeScript enum or namespace**: Not allowed (`erasableSyntaxOnly`). Use `as const` objects
   for enum-like patterns.
9. **Missing compat flag gating**: Behavioral changes that could break existing workers must be
   gated behind a compat flag. Check that the flag exists in `compatibility-date.capnp` and is
   checked at runtime.
10. **Dual export pattern in public modules**: Top-level files in `src/node/` and `src/cloudflare/`
    must provide both named exports and a default export object.
11. **Bare `Error` throw in `src/node/`**: Should use the appropriate `ERR_*` class from
    `node-internal:internal_errors` when one exists.
12. **`eslint-disable` without justification**: Every `eslint-disable` comment should name a
    specific rule and explain why the override is necessary. Blanket `eslint-disable` (no rule
    name) is never acceptable.
13. **`@ts-expect-error` without explanation**: Must include a comment explaining why the type
    system cannot express the correct type. Prefer fixing the types over suppressing the error.
14. **`require()` calls**: Banned. Use ESM `import` syntax.
15. **Namespace re-export as default**: In `src/node/`, do not `import * as X` then
    `export default X`. Use explicit named re-exports.
16. **Test file is `.ts` instead of `.js`**: Test files under `src/workerd/api/tests/` and
    `src/cloudflare/internal/test/` should be `.js` unless there is a specific reason for
    TypeScript (rare; only two existing `.ts` test files have the `-ts` suffix convention).
17. **Missing `.wd-test` config for new test**: Every new test `.js` file needs a paired
    `.wd-test` Cap'n Proto configuration file.
18. **Unused `eslint-disable`**: If the suppressed lint no longer fires, the disable comment
    should be removed. Review for stale suppressions.
