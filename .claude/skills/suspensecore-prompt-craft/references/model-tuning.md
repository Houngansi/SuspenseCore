# Model tuning — per-model knobs (April 2026)

Source of truth: Anthropic docs as of April 2026 (Opus 4.7, Sonnet 4.6, Haiku 4.5).

---

## Claude Opus 4.7 — `claude-opus-4-7`

**Strengths:** long-horizon agentic coding, complex reasoning, atomic / rollback logic,
cross-system integration, replication design, vision (up to 2576px / 3.75MP),
memory / scratchpad.

**Context:** 1M tokens input, 128k tokens output.

**Effort recommendation (`effort` param via API, `--effort` in CLI):**

| Level | Use when | Cost |
|-------|----------|------|
| `max` | Intelligence-demanding, research-grade — but can over-think | Highest |
| `xhigh` | **Default for SuspenseCore coding / agentic work** | High |
| `high` | Balanced — minimum for intelligence-sensitive tasks | Medium-high |
| `medium` | Cost-sensitive, simple reasoning | Medium |
| `low` | Short scoped tasks only — risks under-thinking on complex multiplayer logic | Low |

**Key behavior shifts vs 4.6 (breaks old prompts):**
- Follows instructions **literally** — write scope explicitly, do not rely on "judgment".
- Response length scales with perceived complexity — add "Provide concise, focused responses."
  if you want terser output.
- Fewer tool calls by default — raise `effort` or ask explicitly.
- Fewer sub-agents spawned — steer with "Spawn sub-agents in parallel when fanning out across
  N files".
- Thinking content omitted from response by default — set `thinking.display = "summarized"`
  if the UI needs progress.
- More direct, opinionated tone — restore warmth with an explicit style instruction.
- Progress updates are native — remove `"After every 3 tool calls, summarize"` scaffolding.

**Removed / rejected (will 400 error):**
- `temperature`, `top_p`, `top_k` — omit entirely.
- `thinking: {"type": "enabled", "budget_tokens": N}` — use `{"type": "adaptive"}`.

**Adaptive thinking:**
```python
thinking = {"type": "adaptive", "display": "summarized"}   # omitted is default
output_config = {"effort": "xhigh"}
```

**Task budgets (beta, flag `task-budgets-2026-03-13`):**
Advisory token cap across the full agentic loop (thinking + tools + output). Minimum 20k.
Not a hard cap — use `max_tokens` for that.
```python
output_config = {
    "effort": "xhigh",
    "task_budget": {"type": "tokens", "total": 80000}
}
```
Do **not** set `task_budget` for open-ended quality-critical work. Use it when you need the
model to self-moderate.

**Tokenizer note:** New tokenizer — up to ~1.35x more tokens than 4.6. Raise `max_tokens`
headroom by ~35% when migrating prompts, especially for compaction triggers.

**Pricing:** 1M context at standard rate (no long-context premium).

---

## Claude Sonnet 4.6 — `claude-sonnet-4-6`

**Strengths:** cost-effective mid-tier, template-following, multi-file edits, replicated
component scaffolding, well-defined refactors, single-class Push-Model migration.

**When to choose over Opus:** the task has a clear template to mirror (e.g.
`suspensecore-iris/references/push-model-migration.md` §2) and no atomic / rollback /
cross-system reasoning required.

**Effort:** `high` is the default. `xhigh` only for HLSL / compute / shader work or
performance-critical math.

**No breaking differences** from 4.7 for basic XML structure. Legacy `temperature` / `top_p`
still accepted but rarely needed.

---

## Claude Haiku 4.5 — `claude-haiku-4-5-20251001`

**Strengths:** mechanical edits, documentation, enum additions, rename refactors, single-file
changes with explicit instructions.

**Cost:** ~25x cheaper than Opus per token on extraction / classification / mechanical tasks.

**Limits — do NOT use for:**
- Multi-file atomic logic (rollback flows).
- Architecture decisions.
- Iris replication design.
- Bug hunts with unknown root cause.
- Shader / HLSL.
- Anything that requires reading 3+ files before editing.

**Effort:** `medium` is the default. `low` only for truly trivial edits.

---

## Cross-model prompt economics

Rough cost multipliers (output token, April 2026):
- Haiku 4.5 ≈ 1x baseline
- Sonnet 4.6 ≈ 4x
- Opus 4.7 ≈ 25x

**Implication for the SuspenseCore pipeline:**
- Push as much mechanical work as possible to Haiku (TASK-A, TASK-F).
- Keep Opus ONLY for the atomic / replication / architectural sub-task.
- Sonnet for component scaffolding, RPC pairs, prioritizer subclass, UI binders.

A 6-task multi-task sprint costs ~10-15% of a single mega-Opus prompt and almost never times out.

---

## When Opus 4.7 DOES time out / API errors — remediation ladder

1. **Shorten the prompt body** — target ≤ 400 lines; split `<scope>` if wider.
2. **Shrink `<read_first>`** — replace full-file reads with line ranges or single sections.
3. **Drop `max` effort → `xhigh`** — max can over-think into timeouts.
4. **Remove thinking-display="summarized"** — less streaming overhead.
5. **Split the task into two sequential sub-tasks** (Template C pattern).
6. **Switch to Sonnet 4.6** if the task does not actually need Opus-level reasoning.

---

## Sampling parameters (DO NOT SET for Opus 4.7)

| Param | Opus 4.6 | Opus 4.7 |
|-------|---------|---------|
| `temperature` | allowed | **400 error** |
| `top_p` | allowed | **400 error** |
| `top_k` | allowed | **400 error** |

If you need variety, ask for it in prose:
> "Before implementing, propose 3 distinct approaches (one line each). Ask me to pick one,
>  then implement only that."

This replaces the old `temperature=0.8` trick.

---

## Per-task model choice cheat sheet (SuspenseCore)

| Task type | Default model | Effort | Notes |
|-----------|---------------|--------|-------|
| Add enum / constant / UPROPERTY | Haiku 4.5 | medium | Template A |
| Push-Model migration of one actor | Sonnet 4.6 | high | Template B, mirror existing class |
| Push-Model migration of hot-path actor (PlayerCharacter, Weapon) | Opus 4.7 | xhigh | Template B, replication-critical |
| New `FFastArraySerializer` collection | Sonnet 4.6 | high | Template B, mirror inventory pattern |
| New `ULocationBasedNetObjectPrioritizer` subclass | Sonnet 4.6 | high | Template B, no randomness |
| New `FNetObjectGroupHandle` system | Opus 4.7 | xhigh | Template B, cross-system |
| Custom `FNetSerializer` with bit packing | Opus 4.7 | max | One-off, expensive |
| Server RPC with `WithValidation` | Sonnet 4.6 | high | Template B |
| Server RPC with atomic rollback (purchase, craft) | Opus 4.7 | xhigh | Atomic logic |
| New `UGameplayAbility` subclass | Sonnet 4.6 | high | Template B |
| GAS attribute set + `OnRep_` chain | Sonnet 4.6 | high | Template B |
| AI / `StateTree` task implementation | Sonnet 4.6 | high | Template B |
| Inventory grid algorithm | Opus 4.7 | xhigh | Algorithmic |
| Documentation / README / TDD update | Haiku 4.5 | medium | Template A |
| Bug investigation (read-only, no code) | Opus 4.7 | high | Investigate-only Template B |
| PIE smoke-test scaffolding | Sonnet 4.6 | high | Template B |
