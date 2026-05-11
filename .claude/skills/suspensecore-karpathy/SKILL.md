---
name: suspensecore-karpathy
description: >
  Universal behavioral guardrails (Karpathy principles) for every UE5.7 C++ task in SuspenseCore:
  Think Before Coding, Simplicity First, Surgical Changes, Goal-Driven Execution. ALWAYS consult
  BEFORE any code edit, BEFORE crafting a prompt for a sub-agent, and BEFORE a simplify pass.
  Triggers on coding work in SuspenseCore / FoundationGame, networking migration, GAS changes,
  Replication Graph or Iris work, inventory/equipment/weapons subsystems. Source of truth
  referenced by suspensecore-iris STEP 0 and suspensecore-prompt-craft STEP 0.
---

# suspensecore-karpathy — Behavioral Guardrails (source of truth)

Four principles, derived from Andrej Karpathy's observations on LLM coding failure modes,
adapted to the SuspenseCore UE5.7 C++ contract. They bias toward caution over speed.
For trivial edits (typo, rename in one file) use judgment — the principles scale to the
weight of the task.

Reference (originator's repo): https://github.com/forrestchang/andrej-karpathy-skills

---

## 1. Think Before Coding

**Do not assume. Do not hide confusion. Surface tradeoffs.**

Before writing a single line of UE5 C++:

- State assumptions explicitly. Networking? Authority-only? Push-Model required?
- If multiple interpretations exist (server-side validation vs. client prediction) — list them,
  let the user pick. Don't pick silently.
- If a simpler approach exists (a `bool` flag instead of a new GAS attribute) — say so.
- If something is unclear (does this Actor live in `FoundationGame` plugin or in
  `SuspenseCore` project?) — stop. Name what is confusing. Ask.

Failure mode this blocks: "ran with wrong assumption, built the wrong thing correctly".

UE-specific watch-outs:
- "Replicate this" — Push-Model or legacy? Owner-only or world-relevant? Iris or graph?
- "Add ability X" — new `UGameplayAbility`, or extension of existing one in the Foundation layer?
- "Server RPC" — Reliable? Validation? Anti-cheat bounds?

---

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code (do not create `IInteractable` for one pickup).
- No "flexibility" or "configurability" that was not requested.
- No error handling for impossible scenarios. Trust internal callers; validate at network
  boundaries only (Server RPC `WithValidation`).
- If you wrote 200 lines and 50 would do — rewrite it.

Senior-engineer test: "Would a senior UE5 multiplayer engineer call this overcomplicated?"
If yes → simplify.

Failure mode this blocks: "1000-line plugin scaffolding when 100 lines in one .cpp would do".

UE-specific watch-outs:
- New `UInterface` for one consumer = overengineered. Just call the function directly.
- `UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(AllowPrivateAccess="true"))` on every
  field = noise. Match the smallest access set the gameplay actually needs.
- New module for one class = overengineered. Put it in the existing module.

---

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

- Do not "improve" adjacent code, comments, or formatting.
- Do not refactor things that are not broken.
- Match existing style, even if you would write it differently
  (the project's "UE-style English inline comments" rule wins).
- Unrelated dead code → mention it, do not delete it.
- Orphan cleanup: remove only includes / vars / functions **your** change made unused.

The test: every changed line traces directly to the stated request.

Failure mode this blocks: "drive-by refactor broke MOVER/GASP integration during a 1-line fix".

UE-specific watch-outs:
- Renaming an `FName` literal everywhere "while I'm here" = drive-by, breaks data tables and
  Blueprint references you cannot see in the .cpp.
- Touching headers of unrelated classes "to add includes" = invalidates 200 other compilation
  units' precompiled headers.
- Reformatting a whole file when only 3 lines changed = unreviewable diff.

---

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform vague tasks into verifiable goals before starting:

- "Add validation" → "Server_AddStackToItem_Validate returns false for `Amount <= 0` or
  `Amount > 60`; manual test with debug payload shows kick."
- "Fix the bug" → "Write a reproduction in a server unit test or PIE script, then make it pass."
- "Refactor X" → "PIE smoke test (host + 2 client) passes before and after."
- "Migrate to Iris" → "`LogIrisReplication Verbose` shows protocol bound for the class on first
  replication; no legacy graph node referenced for that path."

For multi-step tasks, state a brief plan with per-step verification:

```
1. [Step] → verify: [grep / build / PIE smoke / LogIris line]
2. [Step] → verify: [same]
3. [Step] → verify: [same]
```

Strong criteria let the loop run independently. "Make it work" is not a criterion.

Failure mode this blocks: "claimed done, no verification, reopened on first PIE test".

---

## Karpathy gate (30-second preamble for every SuspenseCore task)

Before tool use, answer in ≤ 5 bullets:

```
Goal       : <one sentence — what must exist after>
Assumptions: <list — what I am taking as given (authority, plugin layer, module)>
Ambiguity? : <none | list — escalate to user if any>
Scope      : <files in / files explicitly out — name the module + class>
Done when  : <grep / build / PIE smoke / LogIris criteria>
```

If `Ambiguity? != none` → **ask the user first**, do not implement.

---

## SuspenseCore-specific anti-patterns (AP-K-SC-*)

- **AP-K-SC-1**: Silent assumption about authority side → implemented the wrong RPC direction.
- **AP-K-SC-2**: New `UInterface` / new module for a one-shot use case → ten files instead of one.
- **AP-K-SC-3**: Drive-by edit to MOVER/GASP/Foundation layers during an Inventory change → cross-
  layer break.
- **AP-K-SC-4**: Vague done-criteria ("works in PIE") → not reproducible, reviewer cannot verify.
- **AP-K-SC-5**: Speculative `WithValidation` checks for cases the engine already rejects → dead
  branches, more attack surface to audit.
- **AP-K-SC-6**: Rewriting working Replication Graph paths to "improve clarity" during an Iris
  migration → invalidates the in-flight migration plan.
- **AP-K-SC-7**: Adding `UPROPERTY(Replicated)` without checking whether Push-Model applies →
  pulls the actor back into the polling path.
- **AP-K-SC-8**: "Future-proofing" by adding hooks/interfaces with no consumer → maintenance
  burden with zero value.

---

## How this fits with the SuspenseCore stack

The project enforces architectural rules (LAW-1 through LAW-10, Foundation layering, GAS/MOVER
integration boundaries, Iris migration policy in `suspensecore-iris`). Karpathy rules are
**behavioral**, not architectural. They are orthogonal — run the Karpathy gate **first**
(surface assumptions, scope, done-criteria), then run the architectural Pre-Flight on the files
you have committed to touching.

Concrete order for every SuspenseCore PR:

1. **Karpathy gate** (this skill) — assumptions, scope, done-when. 30 seconds.
2. **Architectural Pre-Flight** — Foundation layer boundaries, Iris invariants
   (`suspensecore-iris` STEP 3), GAS rules.
3. **Code**.
4. **Post-Flight** — grep suite, PIE smoke, LogIris confirmation.

Skipping step 1 is the most common cause of "compiled, shipped, broke in playtest".

Keep this file under 200 lines. Anything bulkier belongs in the project CLAUDE.md, not here.
