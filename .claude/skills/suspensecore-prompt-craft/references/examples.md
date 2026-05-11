# Examples — three worked prompts for SuspenseCore

Load when you want to see real prompts that worked on SuspenseCore tasks.

---

## Example 1 — Micro-Prompt (Template A)

Source: trivial enum addition in the items module.

```xml
<task>Add Resource=14, Scrap=15, Component=16 to EItemCategory enum in SuspenseCorePickupItem.h.</task>
<model>claude-haiku-4-5-20251001</model>
<effort>medium</effort>

<files>
  <modify>Source/SuspenseCore/Items/SuspenseCorePickupItem.h</modify>
</files>

<requirements>
1. Insert after existing value LightPart = 13.
2. Keep the underlying type uint8 unchanged.
3. Mirror existing UMETA(DisplayName=...) style for the three new values.
</requirements>

<done_when>
- [ ] Three new enum values visible in file.
- [ ] git diff --stat shows only SuspenseCorePickupItem.h changed.
- [ ] grep "UMETA" on the new lines confirms metadata attached.
</done_when>
```

**Why it works:** single file, single goal, objectively verifiable done-state. No
`<read_first>` because Haiku auto-loads `CLAUDE.md` and the file path is explicit.

---

## Example 2 — Single-Task Prompt (Template B, Opus)

Source: hot-path replication migration. Hot-path actors (PlayerCharacter, Weapon, hit-marker
buffer) earn Opus because a missed `MARK_PROPERTY_DIRTY_FROM_NAME` here is a player-facing bug,
not a loot rendering glitch.

```xml
<task>Migrate ASuspensePlayerCharacter replicated state to Push-Model with full Iris compatibility, preserving GAS attribute replication and MOVER integration.</task>
<model>claude-opus-4-7</model>
<effort>xhigh</effort>
<branch>claude/iris-migrate-player-character</branch>
<budget>~70k tokens target for full loop</budget>

<read_first>
- suspensecore-karpathy/SKILL.md
- suspensecore-iris/SKILL.md
- suspensecore-iris/references/push-model-migration.md
- Source/SuspenseCore/Character/SuspensePlayerCharacter.h
- Source/SuspenseCore/Character/SuspensePlayerCharacter.cpp
- Source/SuspenseCore/Character/SuspensePlayerCharacter_Movement.cpp (partial — replication setup only)
</read_first>

<assumptions>
- net.IsPushModelEnabled=1 is set globally; IrisCore is in Build.cs.
- USuspenseEventBusSubsystem + FSuspensePlayerStateChangedEvent already exist.
- MOVER integration is on the C++ side of the project (no Blueprint).
- GAS AttributeSet uses GAMEPLAYATTRIBUTE_REPNOTIFY pattern which is compatible with Push-Model.
</assumptions>

<clarify_first>
- Hot-path question: does the project use COND_OwnerOnly on any of the existing DOREPLIFETIME_CONDITION
  calls? If yes, those become DOREPLIFETIME_WITH_PARAMS_FAST with Params.Condition = COND_OwnerOnly.
  If unclear, STOP and ask before editing.
</clarify_first>

<scope>
  <include>
    - Source/SuspenseCore/Character/SuspensePlayerCharacter.h (setter declarations, include)
    - Source/SuspenseCore/Character/SuspensePlayerCharacter.cpp (DOREPLIFETIME conversion, setters, OnRep_ refactor)
  </include>
  <exclude>
    - Do NOT modify any AttributeSet — GAS handles its own Push-Model path via GAMEPLAYATTRIBUTE_REPNOTIFY.
    - Do NOT modify MOVER component classes.
    - Do NOT touch ASuspenseAICharacter or any non-player character.
    - Do NOT change Build.cs or DefaultEngine.ini.
    - Do NOT introduce new abstractions (IInteractable, IReplicatedActor, etc.).
  </exclude>
</scope>

<files>
  <modify>Source/SuspenseCore/Character/SuspensePlayerCharacter.h</modify>
  <modify>Source/SuspenseCore/Character/SuspensePlayerCharacter.cpp</modify>
</files>

<requirements>
1. Convert every DOREPLIFETIME / DOREPLIFETIME_CONDITION in GetLifetimeReplicatedProps to
   DOREPLIFETIME_WITH_PARAMS_FAST with PushParams.bIsPushBased = true. Preserve any existing
   Condition values.
   → verify: grep "DOREPLIFETIME(" .cpp → 0; grep "DOREPLIFETIME_WITH_PARAMS_FAST" .cpp ≥ N
   (where N is the original DOREPLIFETIME count).

2. For each replicated UPROPERTY mutated outside construction, introduce a Set*() mutator on
   the server side. Each: HasAuthority() guard, equality short-circuit, MARK_PROPERTY_DIRTY_FROM_NAME.

3. OnRep_* functions only build a FSuspensePlayerStateChangedEvent (or domain-specific payload)
   and Publish via USuspenseEventBusSubsystem::Get(GetWorld())->Publish(...). No direct calls
   to UMG, audio, animation BPs.

4. Hot-path properties (Health visual mirror, ammo display) must remain reliable — do not switch
   to unreliable RPC. Push-Model only changes WHEN serialization happens, not RELIABILITY.

5. Authoritative writes from MOVER / movement code path call the new Set*() mutators instead of
   touching the UPROPERTY directly.
</requirements>

<simplicity_rules>
- Mirror the canonical shape from push-model-migration.md §2.
- Do NOT introduce a "replicated state holder" struct unless one is already used by adjacent
  classes (it is not — verified).
- Do NOT add Server RPCs for state the server already owns — these are server→client mirrors,
  not client requests.
- Every changed line traces to <requirements>.
</simplicity_rules>

<anti_patterns>
- AP-IRIS-1, AP-IRIS-2, AP-IRIS-4 (see suspensecore-iris STEP 4).
- AP-K-SC-3: drive-by edits to MOVER/GASP — explicitly excluded above.
- AP-K-SC-7: adding new UPROPERTY(Replicated) without Push-Model parameters.
</anti_patterns>

<pattern_references>
- Canonical Push-Model shape with EventBus: suspensecore-iris/references/push-model-migration.md §2.
- Setter idiom (HasAuthority + IsNearlyEqual + MARK_PROPERTY_DIRTY_FROM_NAME): same file, SetDurability.
</pattern_references>

<output_format>
- Full files for both .h and .cpp in two ```cpp``` code blocks.
- Single commit: feat(net): migrate ASuspensePlayerCharacter to Push-Model + EventBus.
- Push to branch claude/iris-migrate-player-character.
</output_format>

<post_flight>
- [ ] grep "DOREPLIFETIME(" Source/SuspenseCore/Character/SuspensePlayerCharacter.cpp → 0
- [ ] grep "MARK_PROPERTY_DIRTY_FROM_NAME" same file → ≥ 1 per mutator
- [ ] grep "USuspenseEventBusSubsystem" same file → ≥ 1
- [ ] Dedicated-server build with bUseIris=true compiles, 0 new warnings
- [ ] PIE smoke (host + 2 clients): health/ammo updates arrive on all clients within 1 frame
- [ ] LogIrisReplication shows "Bound protocol ASuspensePlayerCharacter" on first replication
</post_flight>
```

**Why it works for Opus 4.7:**
- `<scope><exclude>` explicit — 4.7 is literal, respects boundaries.
- Only 2 files → fits the ≤ 8-file rule.
- `<pattern_references>` names file:section → no exploratory reads.
- No `temperature`, no `thinking.budget_tokens`, no sampling params.
- `<budget>` matches the task-budget API beta (advisory).
- `xhigh` effort — optimal for replication design.
- `<clarify_first>` asks the one question that could derail the run.

---

## Example 3 — Multi-Task Sprint (Template C)

Source: full feature spanning per-room loot lock, container reveal, distance prioritizer.

```markdown
# S<NN> Iris Loot Density Pipeline — Implementation Prompts

**Branch:** `claude/iris-loot-density-pipeline`
**Docs:** suspensecore-iris STEP 5, references/filter-prioritizer-migration.md §§4-5
**Run order:** A → B → C → D → E → F (sequential, one commit each)

## TASK-A — Event payloads + EventBus registration   (Haiku, medium, 20 min, 4 files)
## TASK-B — ASuspenseRoomManager + FNetObjectGroupHandle   (Opus 4.7, xhigh, 60 min, 3 files)
## TASK-C — ASuspenseContainer per-connection filter API   (Sonnet, high, 50 min, 2 files)
## TASK-D — USuspenseBucketPrioritizer + INI registration   (Sonnet, high, 40 min, 3 files)
## TASK-E — Wiring on UI side + PIE smoke harness   (Sonnet, high, 30 min, 4 files)
## TASK-F — Tech debt entry + cross-doc links   (Haiku, medium, 15 min, 2 files)

Each TASK is a full Template B XML block. Run A → F sequentially, one commit each.
```

**Why it works:**
- Total ≈ 18 files touched across 6 sub-tasks → NO single Opus request > 8 files.
- Opus used for ONE task (TASK-B, atomic FNetObjectGroupHandle + dormancy interplay) →
  minimizes timeout surface.
- Haiku on bookends (event payloads + docs) → cost savings vs. all-Sonnet.
- Sonnet handles container filter, prioritizer subclass, UI wiring → template-following work,
  its sweet spot.
- Sequential execution → if TASK-B times out, TASK-A is already committed; re-issue only B.

---

## Common mistakes — what NOT to do

### Mistake 1 — kitchen-sink Opus prompt
```
❌ "Implement the full Iris loot pipeline: room manager, containers, prioritizer, UI, smoke
    tests, and tech debt entries. Use best practices."
```
Times out on Opus 4.7 because: 15+ files, "best practices" is not literal, no `<exclude>`, no
`<read_first>`, no atomic step boundaries.

### Mistake 2 — asking for architectural decisions
```
❌ "Decide the best way to hide loot per-room and implement it."
```
Opus 4.7 is literal — it will pick something, possibly not what you wanted. Fix:
`"Use FNetObjectGroupHandle with AddExclusionFilterGroup as documented in
suspensecore-iris/references/filter-prioritizer-migration.md §5"`.

### Mistake 3 — legacy sampling knobs
```
❌ "Use temperature=0.3 for deterministic output."
```
Returns 400 on Opus 4.7. If you need variety: ask for 3 options first.

### Mistake 4 — preloading everything
```
❌ <read_first>
     - ALL of suspensecore-iris/references/*.md
     - ALL of suspensecore-prompt-craft/references/*.md
     - Every header in Source/SuspenseCore/Network/
   </read_first>
```
Burns tokens + confuses 4.7 (more literal = distracted by irrelevant context). Fix: name only
the specific sections needed (e.g. `references/push-model-migration.md §2` not the full file).

### Mistake 5 — vague done-when
```
❌ <done_when>Code works and is clean.</done_when>
```
Opus 4.7 will interpret "clean" literally and over-refactor adjacent code. Fix: grep commands
+ compile + PIE smoke result + `LogIrisReplication` line presence.

### Mistake 6 — forgetting #if UE_WITH_IRIS guards in <requirements>
```
❌ "Implement Open/Close container reveal using SetConnectionFilterStatus."
```
Sub-agent will produce code that fails to compile when Iris is disabled.
Fix: add an explicit requirement: "Every direct Iris API call wrapped in #if UE_WITH_IRIS guard".

### Mistake 7 — touching MOVER/GASP during a networking task
```
❌ <scope><include>... + any MOVER replication tweaks needed ...</include></scope>
```
Cross-layer drive-by edit — `AP-K-SC-3`. Fix: keep the PR scoped; if MOVER needs a change,
it is its own sub-task with its own scope and reviewer.
