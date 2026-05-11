# Templates — full XML bodies (SuspenseCore UE5.7)

Copy the matching block, fill the placeholders. Every template assumes the sub-agent obeys
the SuspenseCore mandatory rules: senior UE5.7 multiplayer engineer, full files only, no
truncation, English inline comments, Russian explanations, no Blueprints.

---

## Template A — Micro-Prompt

For single-file mechanical changes. Target: Haiku 4.5.

```xml
<task>Add three new values (Resource=14, Scrap=15, Component=16) to EItemCategory in SuspenseCorePickupItem.h.</task>
<model>claude-haiku-4-5-20251001</model>
<effort>medium</effort>

<files>
  <modify>Source/SuspenseCore/Items/SuspenseCorePickupItem.h</modify>
</files>

<requirements>
1. Insert after existing value LightPart = 13.
2. Keep underlying type uint8 unchanged.
3. No other files touched, no extra includes, no UPROPERTY metadata changes.
</requirements>

<constraints>
- Namespace and naming match existing project conventions (E prefix, MetaData annotations).
- Must compile against UE 5.7 with current SuspenseCore.Build.cs.
- No Blueprint exposure changes — keep existing UMETA flags.
</constraints>

<done_when>
- [ ] grep "Resource = 14" path/SuspenseCorePickupItem.h → 1 match
- [ ] grep "Scrap = 15" same file → 1 match
- [ ] grep "Component = 16" same file → 1 match
- [ ] git diff shows ONLY 3 inserted lines in that file
</done_when>
```

---

## Template B — Single-Task Prompt

For 3–8 files, one system. Target: Sonnet 4.6 (template-following) or Opus 4.7 (atomic core,
cross-system reasoning, replication design).

```xml
<task>Migrate ASuspenseCorePickupItem from legacy DOREPLIFETIME to Push-Model with full Iris compatibility.</task>
<model>claude-opus-4-7</model>
<effort>xhigh</effort>
<branch>claude/iris-migrate-pickup-item</branch>
<budget>~50k tokens target for full loop</budget>

<read_first>
- suspensecore-karpathy/SKILL.md
- suspensecore-iris/SKILL.md
- suspensecore-iris/references/push-model-migration.md
- Source/SuspenseCore/Items/SuspenseCorePickupItem.h
- Source/SuspenseCore/Items/SuspenseCorePickupItem.cpp
</read_first>

<assumptions>
- net.IsPushModelEnabled=1 is already set in DefaultEngine.ini (verified in activation-config.md).
- IrisCore is already in SuspenseCore.Build.cs PublicDependencyModuleNames.
- USuspenseEventBusSubsystem and FSuspenseItemStateChangedEvent exist and compile.
- The class currently uses bare DOREPLIFETIME on ItemID, Durability, Quantity, bIsLooted, LocationOffset.
</assumptions>

<clarify_first>
- None. If any of the above assumptions is wrong, STOP and surface it before editing.
</clarify_first>

<scope>
  <include>
    - Source/SuspenseCore/Items/SuspenseCorePickupItem.h (add includes, setter declarations)
    - Source/SuspenseCore/Items/SuspenseCorePickupItem.cpp (convert DOREPLIFETIME, add setters, refactor OnRep_)
  </include>
  <exclude>
    - Do NOT modify SuspenseCore.Build.cs (IrisCore already added in a prior commit).
    - Do NOT modify DefaultEngine.ini.
    - Do NOT modify USuspenseEventBusSubsystem or any event payload struct.
    - Do NOT change MOVER, GASP, GAS, or AI layer files.
    - Do NOT touch other ASuspenseCore* classes — this PR is scoped to PickupItem only.
  </exclude>
</scope>

<files>
  <modify>Source/SuspenseCore/Items/SuspenseCorePickupItem.h</modify>
  <modify>Source/SuspenseCore/Items/SuspenseCorePickupItem.cpp</modify>
</files>

<requirements>
1. In the .cpp, include Net/Core/PushModel/PushModel.h.
   → verify: grep "PushModel.h" SuspenseCorePickupItem.cpp returns 1 line.

2. Replace every DOREPLIFETIME(ThisClass, X) with DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, X, PushParams)
   where PushParams.bIsPushBased = true.
   → verify: grep -E "DOREPLIFETIME\(ThisClass" SuspenseCorePickupItem.cpp returns 0 lines.
   → verify: grep "DOREPLIFETIME_WITH_PARAMS_FAST" SuspenseCorePickupItem.cpp returns 5 lines.

3. Introduce public setters SetQuantity(int32), SetDurability(float), SetLootedState(bool).
   Each setter: HasAuthority() guard + equality short-circuit (use FMath::IsNearlyEqual for float)
   + MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, X, this).
   → verify: grep "MARK_PROPERTY_DIRTY_FROM_NAME" SuspenseCorePickupItem.cpp returns ≥ 3 lines.

4. Constructor sets NetDormancy = DORM_Initial (loot is static-density).
   → verify: grep "DORM_Initial" SuspenseCorePickupItem.cpp returns 1 line.

5. OnRep_ItemState builds an FSuspenseItemStateChangedEvent payload and publishes via
   USuspenseEventBusSubsystem::Get(GetWorld())->Publish(Payload). No direct UI/audio calls.
   → verify: OnRep_ItemState body contains exactly one Publish call and no other side effects.
</requirements>

<simplicity_rules>
- Minimum code to satisfy <requirements>. Nothing speculative.
- Do NOT introduce a new IInteractable or pickup-interface abstraction.
- Do NOT add a "PickupConfig" struct unless one already exists.
- Do NOT add error handling for cases the engine already rejects (null world, missing subsystem
  in PIE shutdown — log and early-return is enough).
- Every changed line must trace to <requirements>.
</simplicity_rules>

<anti_patterns>
- AP-IRIS-1: DOREPLIFETIME on a hot-path property forces polling
- AP-IRIS-2: forgetting MARK_PROPERTY_DIRTY_FROM_NAME after authoritative write
- AP-IRIS-4: client OnRep_ calling UMG/audio directly instead of EventBus
- AP-IRIS-10: spawning a static-density actor without DORM_Initial
- AP-K-SC-1: implementing the wrong RPC direction by guessing authority
</anti_patterns>

<pattern_references>
- Canonical Push-Model shape: suspensecore-iris/references/push-model-migration.md §2
- EventBus payload publish pattern: suspensecore-iris/references/push-model-migration.md §2 (OnRep_ItemState body)
- Setter guard idiom: same file, SetDurability body (IsNearlyEqual short-circuit)
</pattern_references>

<output_format>
- Full files for both .h and .cpp in two ```cpp``` code blocks.
- One commit: feat(net): migrate ASuspenseCorePickupItem to Push-Model + EventBus
- Commit message in English, present tense.
- Push to branch claude/iris-migrate-pickup-item.
</output_format>

<done_when>
- [ ] grep "DOREPLIFETIME(" SuspenseCorePickupItem.cpp → 0
- [ ] grep "DOREPLIFETIME_WITH_PARAMS_FAST" SuspenseCorePickupItem.cpp → 5
- [ ] grep "MARK_PROPERTY_DIRTY_FROM_NAME" SuspenseCorePickupItem.cpp → ≥ 3
- [ ] grep "DORM_Initial" SuspenseCorePickupItem.cpp → 1
- [ ] grep "USuspenseEventBusSubsystem" SuspenseCorePickupItem.cpp → ≥ 1
- [ ] Dedicated-server build compiles with bUseIris=true
- [ ] LogIrisReplication Verbose shows "Bound protocol ASuspenseCorePickupItem" on first replication
- [ ] Diff only touches the 2 files listed in <files>
</done_when>
```

---

## Template C — Multi-Task Sprint

For > 8 files or mixed-effort chain. Save as `.claude/sprints/S<NN>_<FEATURE>.md`. Each TASK
block below = one Template B prompt.

```markdown
# S<NN> Iris Migration — Room-Based Loot Group

**Branch:** `claude/iris-room-loot-group`
**Docs:** `suspensecore-iris/SKILL.md`, `references/filter-prioritizer-migration.md` §5
**Run order:** A → B → C → D → E → F (sequential, one commit each)

---

## TASK-A — Event payload struct + EventBus registration
**Model:** `claude-haiku-4-5-20251001` | **Effort:** medium | **ETA:** 20 min | **Files:** 2

(Paste Template B body here, scoped to creating FSuspenseRoomVisibilityChangedEvent struct
+ registering it on the EventBus subsystem.)

---

## TASK-B — ASuspenseRoomManager core with FNetObjectGroupHandle
**Model:** `claude-opus-4-7` | **Effort:** xhigh | **ETA:** 60 min | **Files:** 3

(Template B body — this is the atomic core: CreateGroup, AddExclusionFilterGroup, AddToGroup,
SetGroupFilterStatus, all wrapped in #if UE_WITH_IRIS. Opus only for the atomic logic.)

---

## TASK-C — ASuspenseContainer wiring with per-connection filter
**Model:** `claude-sonnet-4-6` | **Effort:** high | **ETA:** 50 min | **Files:** 2

(Template B body — implement OpenForPlayer / CloseForPlayer using SetConnectionFilterStatus,
mirror the pattern from filter-prioritizer-migration.md §3b.)

---

## TASK-D — USuspenseBucketPrioritizer subclass + INI registration
**Model:** `claude-sonnet-4-6` | **Effort:** high | **ETA:** 40 min | **Files:** 3

(Template B body — subclass ULocationBasedNetObjectPrioritizer, three distance buckets, INI
registration line, no randomness in Prioritize().)

---

## TASK-E — PIE smoke test scaffolding + EventBus subscriber on UI side
**Model:** `claude-sonnet-4-6` | **Effort:** high | **ETA:** 30 min | **Files:** 2

(Template B body — small UI binder that subscribes to FSuspenseRoomVisibilityChangedEvent and
toggles loot panel; smoke-test plan for host + 2 clients.)

---

## TASK-F — Tech debt entry + doc cross-links
**Model:** `claude-haiku-4-5-20251001` | **Effort:** medium | **ETA:** 15 min | **Files:** 2

(Template B body — log the migration in TECH_DEBT_TRACKER.md or its SuspenseCore equivalent,
add doc cross-link in the module README.)

---

## Final checklist (before merge)

- [ ] All TASK-A..F committed in order on `claude/iris-room-loot-group`.
- [ ] grep suite from `suspensecore-iris/SKILL.md` STEP 5 → 0 violations on changed files.
- [ ] Dedicated server build with bUseIris=true compiles.
- [ ] PIE smoke (host + 2 clients): loot inside Room A is invisible to clients until
      RoomManager flips visibility on; flipping it off hides again in ≤ 1 frame.
- [ ] LogIrisFilter Verbose shows AddExclusionFilterGroup + SetGroupFilterStatus lines.
- [ ] No legacy ConnectionGroupNode references in the touched path.
```

---

## Quick-reference: which template when

| Situation | Template | Model hint |
|-----------|----------|------------|
| Rename one symbol in one file | A | Haiku |
| Add UPROPERTY + setter pair | A or B | Haiku / Sonnet |
| Single-class Push-Model migration | B | Sonnet (or Opus for hot-path actor) |
| New `ASuspenseCore*` actor with replication | B | Sonnet |
| Cross-system Iris feature (RoomManager + Container + Prioritizer + UI) | C | Mixed |
| New GAS ability with replicated effects | B | Sonnet |
| Custom `FNetSerializer` with bit packing | B | Opus only |
| Atomic Server RPC with rollback (purchase, craft) | B | Opus only |
| Documentation update | A | Haiku |
| Bug fix with known root cause | A or B | Sonnet |
| Bug hunt with unknown cause (investigate-only) | B (no code) | Opus |
| Performance optimization with measurement | B | Opus |
