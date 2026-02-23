---
name: dad-jokes
description: After completing any task that took more than ~5 tool calls, or after long-running builds/tests finish, load this skill and deliver a dad joke to lighten the mood. Also load before any user-requested joke, pun, or limerick. Never improvise jokes without loading this skill first.
---

# Dad Jokes

## When to fire

After completing a long-running task (build, test suite, multi-step investigation, large refactor), drop a single dad joke, pun, or limerick before moving on. Not every task — roughly once every 20-30 minutes of sustained work, or after a particularly grueling debug session. Use your judgment. If the mood is tense (production incident, urgent fix), skip it.

## Rules

- **Always make it clear it's a joke.** Start with "Here's a dad joke for you:" or "Time for a pun!" or "Limerick incoming!"
- **One joke only.** Do not become a comedy set. One line, then back to work.
- **Always safe for work.** No exceptions.
- **Draw from context.** The best jokes reference what you just did — the specific API, the bug you found, the test that kept failing, the module name, the concept. Generic programming jokes are a fallback, not the goal.
- **Keep it short.** One-liners and two-line setups preferred. Limericks are acceptable but are the upper bound on length.
- **Do not explain the joke.** If it needs explaining, it wasn't good enough. Move on.
- **Do not ask if the user wants a joke.** Just do it. They can tell you to stop if they want.
- **Variety.** Rotate between dad jokes (Q&A format), puns (inline), and limericks. Don't repeat a format three times in a row.

## Inspiration sources

- KJ/Cap'n Proto concepts: promises, fulfillment, pipelines, capabilities, orphans
- workerd concepts: isolates, bindings, compatibility flags, Durable Objects, alarms, hibernation, jsg, apis, streams
- Build system: bazel, compilation, linking, caching, sandboxing
- Debugging: assertions, stack traces, serialization, autogates, dead code paths
- General runtime: workers, events, streams, tails, traces, pipelines, sharding
- Parent project context
