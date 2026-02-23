---
description: Explain what a file, class, or symbol does and how it fits into the architecture
subtask: true
---

Explain: $ARGUMENTS

Steps:

1. **Locate the target.** If the argument is a file path, read it. If it's a C++ class or symbol name, **use the `cross-reference` tool first** — it returns the header, implementation files, JSG registration, type group, test files, and compat flag gating in a single call. This replaces manual grep/glob for the initial lookup and saves significant context.

2. **Read the definition.** Using the locations from the cross-reference output (or from manual search if not a C++ symbol), read the header file first. For JSG-registered types, also **use the `jsg-interface` tool** to get the full structured JS API (methods, properties, constants, nested types, inheritance, TypeScript overrides) — this is much more useful than reading raw macro lines. For functions, read the declaration and implementation. For large files (>500 lines), start with the class declaration and public API before reading implementation details.

3. **Check for local documentation.** Look for:
   - An `AGENTS.md` in the same directory or nearest parent
   - A `README.md` in the same directory
   - Doc comments on the symbol itself

4. **Trace relationships.** The cross-reference output already provides implementation files, test files, and type registration. For deeper analysis, also identify:
   - What this code depends on (base classes, key types used, includes)
   - What depends on this code (grep for callers, subclasses, or includes of the header)
   - Limit to the most important 5-10 relationships, not an exhaustive list

5. **Identify the architectural layer.** Place it in context:
   - Is this API layer (`api/`), I/O layer (`io/`), JSG bindings (`jsg/`), server infrastructure (`server/`), or utility (`util/`)?
   - What is its role in the request lifecycle or worker lifecycle?

6. **Identify recent changes.** Check git history for recent (within 2 weeks) commits that modified this code. This can provide clues about its current relevance, stability, and any ongoing refactors or deprecations.

7. **Output a concise summary** with:
   - **What it is**: One-sentence description
   - **Where it lives**: File path(s) and architectural layer
   - **Key responsibilities**: Bullet list of what it does
   - **Key relationships**: What it depends on and what depends on it
   - **Notable patterns or gotchas**: Anti-patterns, compat flag gating, thread safety considerations, or other things a developer should know
   - **Recent changes**: If there have been recent commits, a brief note on what they were about

8. **Never** miss an opportunity for a good dad joke (using the `dad-jokes` skill). Don't overdo it, but don't avoid them either. When summarizing, **always** preserve any jokes from the subagent output, and **always** including the intro prefix ("Here's a dad joke for you:", etc.) so the user knows it's intentional.
