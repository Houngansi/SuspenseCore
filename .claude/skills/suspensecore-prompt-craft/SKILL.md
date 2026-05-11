---
name: suspensecore-prompt-craft
description: >
  ALWAYS use FIRST when the user asks to write, craft, design, or draft a prompt for another
  Claude agent (Haiku 4.5, Sonnet 4.6, Opus 4.7) for SuspenseCore or FoundationGame UE5.7 C++
  work. Tiered model router, XML-structured universal template, anti-timeout rules for Opus 4.7
  on multiplayer C++ tasks, token budgets, task decomposition into sequential sub-tasks.
  Triggers on phrases like "напиши промпт", "сделай промпт", "разбей на таски", "draft a task
  for Opus", "split this sprint", "decompose this feature". Use BEFORE reading suspensecore-iris
  references — those are loaded ON DEMAND per crafted task.
---

# suspensecore-prompt-craft — Universal Prompt Pipeline (Opus 4.7 tuned, UE5.7)

## When this skill fires

User says any of:
- "напиши промпт", "сделай промпт", "подготовь промпт", "оформи задачу"
- "write a prompt", "craft a prompt", "design a prompt", "draft a task"
- "split this into tasks", "decompose this sprint", "разбей на саб-таски"
- "prepare a sub-agent run for Iris migration", "Opus task for the inventory refactor"

**Rule:** read THIS file first. Do NOT preload `suspensecore-iris/SKILL.md`, references, or
architecture docs. Those are loaded on demand — the crafted prompt itself instructs the
sub-agent to read them.

---

## STEP 0 — Karpathy gate (before crafting)

> Full principles: `suspensecore-karpathy` skill.

A prompt that encodes the wrong goal wastes the whole downstream run. Before writing any XML,
answer:

1. **Think** — Is the user's goal unambiguous? If not → ask the user, do not invent a goal for
   the sub-agent.
2. **Simplify** — Is this task actually needed, or can an existing pattern / one-line edit solve
   it? If simpler works → say so, do not craft.
3. **Scope** — What is explicitly **out** of scope? Opus 4.7 follows literally; if you do not
   state `<exclude>`, you get drive-by edits across MOVER/GASP/Foundation layers.
4. **Done-when** — Can the sub-agent self-verify with grep / build / PIE smoke / LogIris check?
   If not, the task is under-specified — tighten before shipping.

Every crafted prompt MUST therefore contain four Karpathy hooks: `<assumptions>`,
`<clarify_first>`, `<simplicity_rules>`, `<done_when>`.

---

## STEP 1 — Classify the task (30 seconds)

Answer these 5 questions before touching a template:

```
Q1. Goal: one sentence — what must exist after?
Q2. Files touched: count + list (estimate). Source/SuspenseCore/* or Plugins/FoundationGame/*?
Q3. Surface: 1 system (Inventory) / cross-system (Inventory + GAS) / full sprint?
Q4. Has ambiguity? (Iris vs legacy graph, replication side, plugin layer)
Q5. Known blockers / existing code patterns to mirror? (cite file:line where possible)
```

If Q4 = yes → you decide, not the sub-agent. Resolve with the user BEFORE crafting.

---

## STEP 2 — Route to model

| Task shape | Model | Effort | Example |
|------------|-------|--------|---------|
| 1 file, mechanical (enum add, `UPROPERTY` add, comment block, header rename) | **Haiku 4.5** `claude-haiku-4-5-20251001` | `medium` | "Add 3 enum values to `EItemCategory`" |
| 2–5 files, single system, template-following (new `ASuspenseCorePickupItem` subclass, new Server RPC pair, push-model migration of one class) | **Sonnet 4.6** `claude-sonnet-4-6` | `high` | "Migrate `ASuspenseCorePickupItem` to Push-Model with `MARK_PROPERTY_DIRTY_FROM_NAME`" |
| Architecture / cross-system / atomic logic / replication design / final review | **Opus 4.7** `claude-opus-4-7` | `xhigh` | "Design and implement `ASuspenseRoomManager` with `FNetObjectGroupHandle` per-room loot lock + dormancy + visibility API" |

**Why Opus 4.7 needs care (real constraints):**
- 1M context, 128k output — do not abuse; large context = slower + higher timeout risk.
- New tokenizer uses ~1.35x more tokens vs 4.6 — budget accordingly.
- Adaptive thinking only (no `budget_tokens`); `temperature/top_p/top_k` removed (400 errors).
- Follows instructions **literally** at lower effort → write explicit scope.
- Spawns fewer sub-agents by default → ask explicitly when fan-out helps.

---

## STEP 3 — Choose a template (decision tree)

```
files ≤ 2 AND single-pass edit?    → Template A: Micro-Prompt        (≤30 lines)
3–8 files, one feature, one model? → Template B: Single-Task Prompt  (≤120 lines)
> 8 files OR 2+ models needed?     → Template C: Multi-Task Sprint   (N tasks)
```

All three live in XML-structured form (Claude models parse XML cleaner than prose).

Full bodies in `references/templates.md`. Quick overview below.

---

## Template A — Micro-Prompt (Haiku, ≤2 files)

```xml
<task>ONE sentence goal.</task>
<model>claude-haiku-4-5-20251001</model>
<effort>medium</effort>

<files>
  <modify>Source/SuspenseCore/Items/SuspenseCorePickupItem.h</modify>
</files>

<requirements>
1. ...concrete change 1...
2. ...concrete change 2...
</requirements>

<constraints>
- UE 5.7, no Blueprints, full file output
- Inline comments in English, UE-style
- Must compile against current SuspenseCore.Build.cs
</constraints>

<done_when>
- [ ] File compiles
- [ ] grep "&lt;pattern&gt;" path/File.h returns expected lines
</done_when>
```

---

## Template B — Single-Task Prompt (Sonnet / Opus, 3–8 files)

Use for: one-class Iris migration, new replicated component, new Server RPC pair, new GAS
ability set, new pickup type with full lifecycle.

Skeleton fields (full body in `references/templates.md`):
- `<task>`, `<model>`, `<effort>`, `<branch>`, `<budget>` (advisory token cap)
- `<read_first>` — only the docs the sub-agent actually needs
  (e.g. `suspensecore-karpathy/SKILL.md`, `suspensecore-iris/SKILL.md`,
  `suspensecore-iris/references/push-model-migration.md`)
- `<assumptions>` — state assumptions; if any wrong, STOP and ask
- `<clarify_first>` — open questions; if any, do not proceed
- `<scope>` with `<include>` and `<exclude>` — be explicit; Opus 4.7 is literal
- `<files>` with `<create>` and `<modify>` lists
- `<requirements>` — numbered steps, each with a verification clause
- `<simplicity_rules>` — no abstractions, no drive-by refactors
- `<anti_patterns>` — only the APs relevant to THIS task (cite IDs from
  `suspensecore-iris` AP-IRIS-1..AP-IRIS-10 or `suspensecore-karpathy` AP-K-SC-1..AP-K-SC-8)
- `<pattern_references>` — file:line citations of existing patterns to mirror
- `<output_format>` — one commit, conventional message, push to branch
- `<done_when>` — grep/build/PIE/LogIris verifiable criteria

---

## Template C — Multi-Task Sprint (Opus decomposition)

When a feature crosses ≥ 8 files or needs mixed effort levels, **split into N self-contained
sub-tasks**. Each sub-task fits into Template B and targets a specific model. Opus times out on
mega-prompts; a chain of 6 focused Sonnet/Haiku prompts plus 1 Opus prompt for the hard core
is faster and more reliable than one big Opus prompt.

Skeleton — save as `.claude/sprints/S<NN>_<FEATURE>.md`:

```markdown
# S<NN> <Feature> — Implementation Prompts
Branch: claude/<slug>
Docs: ARCHITECTURE_RULES.md §<n>, suspensecore-iris STEP <n>

## TASK-A — Data types & events                (Haiku 4.5, ~20 min)
## TASK-B — Replication core (Push-Model)       (Opus 4.7 xhigh, ~60 min)
## TASK-C — Component + Server RPCs             (Sonnet 4.6 high, ~50 min)
## TASK-D — Filter / Prioritizer subclass       (Sonnet 4.6 high, ~40 min)
## TASK-E — Wiring, EventBus, smoke test        (Sonnet 4.6 high, ~30 min)
## TASK-F — Tech debt entry + docs              (Haiku 4.5, ~15 min)

## Final checklist (before merge)
[ ] All TASK-X committed
[ ] grep suite passes (suspensecore-iris STEP 5)
[ ] PIE smoke (host + 2 clients) passes
[ ] LogIrisReplication shows protocol bound
```

Each `TASK-X` body is a full Template B XML block. Run them **sequentially**, verifying commits
between. If a task fails → shrink scope and re-issue, do not stuff more into the next one.

Worked example in `references/examples.md` (suspensecore-iris migration of `ASuspenseRoomManager`).

---

## STEP 4 — Opus 4.7 anti-timeout rules (HARD)

These exist because Opus 4.7 reliably times out on large prompts.

1. **≤ 400 lines** per single sub-task prompt body. If longer → split.
2. **≤ 8 files touched** per sub-task. If more → split.
3. **Explicit scope** in `<exclude>` — name the modules and layers NOT to touch
   (MOVER, GASP, Foundation, AI, Networking).
4. **No open-ended exploration**: never write "figure out the best replication approach".
   Write: "use pattern from `suspensecore-iris/references/push-model-migration.md` §2".
5. **No chained reasoning across files unless explicitly listed**: every file the sub-agent
   needs to read must be in `<read_first>` or `<pattern_references>`.
6. **Token ceiling hint** (advisory): `<budget>~40k tokens target for full loop</budget>`.
   Minimum 20k.
7. **Prefer `xhigh` over `max`** for coding — `max` over-thinks and raises timeout odds.
8. **No sampling knobs** — do NOT mention `temperature` / `top_p` / `top_k` (4.7 rejects them).
9. **One commit per sub-task** — smaller diffs = easier rollback.
10. **No "improve", "refactor for clarity", "clean up"** as side-goals. Each sub-task has ONE
    goal. Side-cleanup goes into its own Haiku task.

---

## STEP 5 — Token economy checklist

Before handing the prompt to the user, verify:

- [ ] Only the docs the sub-agent actually needs are in `<read_first>`
      (not the whole `suspensecore-iris/references/` tree).
- [ ] Only the APs relevant to the task are in `<anti_patterns>`
      (not the full AP-IRIS-1..10 + AP-K-SC-1..8 catalog).
- [ ] `<pattern_references>` names file + line where possible
      (saves the sub-agent a grep pass).
- [ ] No duplicated context already in `CLAUDE.md` / project-level
      (sub-agent auto-loads CLAUDE.md).
- [ ] One clear `done_when` with grep-verifiable + LogIris-verifiable criteria.

**Rule of thumb:** every paragraph in the prompt should either (a) narrow the sub-agent's
search space, or (b) name a concrete artifact. If it does neither — cut it.

---

## STEP 6 — Handing off

Return the prompt to the user in ONE code block, copy-paste ready. Below it, a 2-line summary:

```
MODEL: <id>   EFFORT: <level>   EST. FILES: <n>   EST. TIME: <min>
RUN: claude --model <id> --effort <level> "<<< paste prompt <<<"
```

If multi-task (Template C): return the file path `.claude/sprints/S<NN>_<FEATURE>.md` and list
the run order.

---

## Common anti-patterns in prompt-writing (AP-PROMPT-*)

- **AP-PROMPT-1**: dumping the whole `CLAUDE.md` into every prompt → wasted tokens
- **AP-PROMPT-2**: "follow best practices" without naming them → Opus 4.7 ignores (literal)
- **AP-PROMPT-3**: mixing 3+ unrelated goals into one prompt → timeout + half-done work
- **AP-PROMPT-4**: asking the sub-agent to "decide" architecture → you decide, it implements
- **AP-PROMPT-5**: relying on `temperature` for variety → removed in 4.7; ask for 3 options first
- **AP-PROMPT-6**: forcing verbose scaffolding ("after every tool call summarize") → 4.7 does
  this natively, scaffolding adds tokens
- **AP-PROMPT-7**: no `<exclude>` block → 4.7 respects scope only if stated
- **AP-PROMPT-8**: mega-prompt > 400 lines for Opus → split
- **AP-PROMPT-9**: no `<assumptions>` block → sub-agent invents premises → wrong build
- **AP-PROMPT-10**: no `<simplicity_rules>` block → sub-agent adds "helpful" abstractions
- **AP-PROMPT-11**: vague `<done_when>` ("make it work", "looks good") → infinite loop
- **AP-PROMPT-12**: crafting a prompt when a one-line edit would do → don't craft, just edit

---

## Reference files (load on demand)

- `references/templates.md` — full copy-paste XML bodies of A / B / C
- `references/model-tuning.md` — per-model knobs (effort / budget / tokenizer delta) + cost
- `references/examples.md` — three fully-worked SuspenseCore examples

Keep this SKILL.md under 300 lines. Anything bulkier belongs in `references/`.
