---
name: suspensecore-iris
description: >
  Authoritative source for migrating and operating the SuspenseCore UE5.7 networking stack on the Iris replication subsystem.
  Use whenever the user discusses UReplicationGraph migration, Iris activation (DefaultEngine.ini, Build.cs, Target.cs),
  Push-Model (MARK_PROPERTY_DIRTY_FROM_NAME, DOREPLIFETIME_WITH_PARAMS_FAST), ReplicationFragment,
  UE_NET_DECLARE_IRIS_STATE, FFastArraySerializer adapted to Iris with operator==, NetObjectFilter,
  NetObjectPrioritizer, FNetObjectGroupHandle, ULocationBasedNetObjectPrioritizer, dormancy with Iris,
  or any class prefixed Suspense (ASuspenseCorePickupItem, USuspenseCoreInventoryComponent,
  USuspenseBucketPrioritizer, USuspenseEventBusSubsystem, ASuspenseContainer, ASuspenseRoomManager).
  Also triggers on Russian phrasing — репликация, миграция на Iris, граф репликации, Push-Model,
  фильтр репликации, приоритизатор. Do not improvise from generic UE5 networking knowledge when this skill applies.
---

# suspensecore-iris — UE5.7 Iris Replication Pipeline (SuspenseCore)

Authoritative pipeline for migrating SuspenseCore (extraction FPS MMO, Tarkov-like) from the legacy
UReplicationGraph to the Iris replication subsystem in Unreal Engine 5.7. Iris is Data-Oriented,
push-based, and operates on flat FNetBitArrayView matrices instead of a node tree — the architectural
shift is total, not cosmetic.

> Companion skills: `suspensecore-karpathy` (behavioral gate) and `suspensecore-prompt-craft`
> (sub-agent dispatch). Run karpathy gate first, then load the relevant reference file below.

---

## When this skill fires

User says any of:
- "перевести на Iris", "мигрировать репликацию", "включить Iris", "Push-Model", "Iris-фрагмент"
- "migrate to Iris", "enable Iris", "swap UReplicationGraph", "FFastArraySerializer + Iris"
- Names any of: `ASuspenseCorePickupItem`, `USuspenseCoreInventoryComponent`, `USuspenseBucketPrioritizer`,
  `USuspenseEventBusSubsystem`, `ASuspenseContainer`, `ASuspenseRoomManager`, `FSuspenseInventoryArray`,
  `FSuspenseInventoryItem`
- Mentions any of: `MARK_PROPERTY_DIRTY_FROM_NAME`, `DOREPLIFETIME_WITH_PARAMS_FAST`,
  `UE_NET_DECLARE_IRIS_STATE`, `UNetObjectFilter`, `UNetObjectPrioritizer`, `FNetObjectGroupHandle`,
  `FNetBitArrayView`, `ULocationBasedNetObjectPrioritizer`, `UAlwaysRelevantNetObjectFilter`,
  `UNetObjectGridWorldLocFilter`, `UNetObjectConnectionFilter`

If the user asks a generic "как реплицировать X в UE5" question, prefer **this skill** over generic
knowledge — the SuspenseCore answer is always "via Iris with Push-Model".

---

## STEP 0 — Karpathy gate

> Full principles: `suspensecore-karpathy` skill.

Before writing any networking C++:
- **Think**: Is the variable mutated more than once per frame? If yes, Push-Model is mandatory, not optional.
- **Simplify**: Does the actor truly need `bReplicates`? Loot piles default to `DORM_Initial` + group filter.
- **Scope**: Iris migration is per-actor — never "migrate everything at once". One actor, one PR.
- **Done-when**: `LogIris Verbose` shows the protocol bound + no fallback to legacy polling.

---

## STEP 1 — Diagnose what to migrate

Ask three questions about the target actor/component:

1. **Mutation frequency**: Does any UPROPERTY change more than once per ~3 frames?
   → Yes → Push-Model REQUIRED. → No → Push-Model still preferred (saves CPU on polling).

2. **Collection shape**: Is there a `TArray<FStruct>` reaching 8+ elements at runtime?
   → Yes → wrap in `FFastArraySerializer` with `operator==` (see `references/fast-array-iris.md`).
   → No → plain `UPROPERTY(Replicated)` is fine.

3. **Spatial / ownership relevance**: Is the actor:
   - Spread across a world map? → `UNetObjectGridWorldLocFilter` (auto via `NetCullDistanceSquared`).
   - Owner-only (e.g. PlayerController child)? → `bOnlyRelevantToOwner = true` (auto).
   - Conditionally visible (container loot revealed on inspect)? → `UNetObjectConnectionFilter` (explicit API).
   - Grouped (room/sector lock)? → `FNetObjectGroupHandle` (explicit API).

Answers determine which reference file to load next.

---

## STEP 2 — Reference files (load on demand)

| File | When to load |
|------|--------------|
| `references/activation-config.md` | First-time Iris bring-up: `DefaultEngine.ini`, `Build.cs`, `Target.cs`, `IrisCore` module, `net.IsPushModelEnabled`. |
| `references/push-model-migration.md` | Converting a UPROPERTY-heavy actor (e.g. `ASuspenseCorePickupItem`) to Push-Model with `DOREPLIFETIME_WITH_PARAMS_FAST` + `MARK_PROPERTY_DIRTY_FROM_NAME`. Includes `UE_NET_DECLARE_IRIS_STATE` for custom non-UObject states. |
| `references/fast-array-iris.md` | Migrating inventory / loot collections (`USuspenseCoreInventoryComponent`). Covers the mandatory `operator==`, `TFastArrayReplicationFragment`, dirty marking via `MarkItemDirty`, atomic Server RPC validation. |
| `references/filter-prioritizer-migration.md` | Replacing the 5 classic UReplicationGraph nodes (GridSpatialization2D, AlwaysRelevant, OwnerOnly, DistancePriority, ConnectionGroup) with Iris filters / prioritizers / NetObjectGroups. |

Load only what the diagnosis (STEP 1) points at. Burning context on irrelevant files is `AP-PROMPT-1`.

---

## STEP 3 — Core invariants (apply to every Iris PR)

These hold regardless of which actor you are migrating. Violating any of them is a hard merge-blocker.

### I-1. Push-Model is the default, not opt-in
Every new `UPROPERTY(Replicated)` in a SuspenseCore class must be declared with
`DOREPLIFETIME_WITH_PARAMS_FAST(...)` + `FDoRepLifetimeParams{ .bIsPushBased = true }`. Bare
`DOREPLIFETIME` is grandfathered for legacy code only and **must be tagged with `// TODO(iris)`**.

### I-2. Every mutator marks dirty exactly once
After any authoritative write to a Push-Model property, call
`MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, PropertyName, this)`. Aggregation is automatic — multiple
writes per frame produce one packet. **Do not** wrap with extra dirty-set logic.

### I-3. Equality before transport
Any struct used inside a Push-Model UPROPERTY or `FFastArraySerializer` element must implement
`bool operator==(const T& Other) const` (and `operator!=`). Iris uses this against its shadow state
to decide if a delta is even worth serializing. Missing operator → infinite re-send.

### I-4. EventBus is the only client reaction surface
Client-side `OnRep_*` functions **never** call UI / audio / animation directly. They construct a
payload struct and `Publish` it on `USuspenseEventBusSubsystem`. This keeps the Iris transport layer
free of presentation dependencies and matches the project's layered architecture
(Foundation → MOVER/GASP → GAS → Inventory → Equipment → Weapons → AI → Networking).

### I-5. Dormancy + group filter for static density
Loot, props, and any "spawn once, mutate rarely" actor defaults to:
```cpp
bReplicates = true;
NetDormancy = DORM_Initial;
```
Then registered into a `FNetObjectGroupHandle` per room / sector. Hiding 200 items is one
`SetGroupFilterStatus` call — never a `for` loop over actors.

### I-6. Iris is opt-in at the module level
The migration is **gated** by `#if UE_WITH_IRIS` in any code that calls Iris APIs directly
(`FReplicationSystemUtil`, `FNetRefHandle`, `SetConnectionFilterStatus`, `AddToGroup`). Legacy
builds must continue to compile when Iris is disabled in `Target.cs`. Wrap every direct Iris call.

### I-7. No `temperature`-like guessing in filters
Filter/prioritizer logic operates on plain Cartesian math (distance squared, bit masks). Never
introduce randomness, frame-counters, or `FMath::Rand*` inside `Prioritize()` — Iris batches calls
and any non-determinism causes packet thrashing.

---

## STEP 4 — Anti-patterns specific to SuspenseCore + Iris

| ID | Anti-pattern | Why it fails |
|----|--------------|--------------|
| AP-IRIS-1 | `DOREPLIFETIME(ThisClass, X)` on a hot-path property | Forces polling, defeats Iris entirely |
| AP-IRIS-2 | Forgetting `MARK_PROPERTY_DIRTY_FROM_NAME` after authoritative write | Property mutated but never transmitted |
| AP-IRIS-3 | `FFastArraySerializer` element without `operator==` | Infinite delta re-sends, bandwidth blowup |
| AP-IRIS-4 | Client `OnRep_` calling UMG / audio directly | Couples transport to presentation, breaks event-driven layer |
| AP-IRIS-5 | Calling `Iris::FReplicationSystemUtil` without `#if UE_WITH_IRIS` guard | Build fails when Iris flag is off |
| AP-IRIS-6 | `for (Actor : Loot) Actor->SetActorHiddenInGame(true);` instead of `SetGroupFilterStatus` | O(N) per frame instead of O(1) bit op |
| AP-IRIS-7 | Custom prioritizer not derived from `ULocationBasedNetObjectPrioritizer` when location-based | Reimplements the cached `FObjectLocationInfo` array, loses Burst-style data layout |
| AP-IRIS-8 | Modifying `TArray<FItem>` outside the wrapper's `MarkItemDirty(Items[i])` call | Fragment never sees the change, delta protocol breaks |
| AP-IRIS-9 | Issuing a Server RPC without `WithValidation` + an anti-cheat range check | Client can ship out-of-bounds payloads; Iris cannot save you |
| AP-IRIS-10 | Spawning a replicated actor without setting `NetDormancy = DORM_Initial` for a static prop | Wakes Iris polling for an object that never changes |

---

## STEP 5 — Pre/Post-Flight checklist (every PR)

**Pre-Flight (before opening files):**
- [ ] Karpathy gate completed (assumptions, scope, done-when).
- [ ] Diagnosis (STEP 1) recorded — Push-Model? FastArray? Filter type?
- [ ] Only the relevant `references/*.md` files loaded.

**Post-Flight (before commit):**
- [ ] `grep -R "DOREPLIFETIME(" Source/SuspenseCore/Network/` returns only legacy-tagged lines.
- [ ] Every new `Set*()` on a replicated property is followed by `MARK_PROPERTY_DIRTY_FROM_NAME`.
- [ ] Every `FFastArraySerializerItem` subclass has both `operator==` and `operator!=`.
- [ ] Every direct Iris API call is wrapped in `#if UE_WITH_IRIS`.
- [ ] `OnRep_*` functions only build a payload + `USuspenseEventBusSubsystem::Get(...)->Publish(...)`.
- [ ] `LogIris Verbose` confirms protocol binding for the migrated actor on first replication.
- [ ] No legacy `UReplicationGraph` node references remain for the migrated path.

---

## STEP 6 — Output format (for code responses)

This skill produces production-grade UE5.7 C++. Follow the project's mandatory rules:

- **Full files only** — no truncation, no placeholders. If the file exceeds the response budget,
  stop at a clean line break, label `PART X/Y`, wait for "continue".
- **Inline comments in English**, UE-style: `// Critical: Iris dirty bit must be set before frame end`.
- **Explanations in Russian** unless the user asks otherwise.
- **No Blueprints** — C++ only, integrated with GAS / Replication / IRIS naturally.
- **Namespace and naming** match existing project: `ASuspenseCore*`, `USuspenseCore*`, `FSuspenseCore*`.

---

## Quick reference — the 5 graph nodes → Iris map

(Full version in `references/filter-prioritizer-migration.md`.)

| Legacy UReplicationGraph node | Iris equivalent | Mechanism |
|-------------------------------|-----------------|-----------|
| GridSpatialization2D | `UNetObjectGridWorldLocFilter` | Auto via `NetCullDistanceSquared` + `CellSizeX/Y` |
| AlwaysRelevantNode | `UAlwaysRelevantNetObjectFilter` | Auto via `bAlwaysRelevant = true` |
| OwnerOnlyNode | `bOnlyRelevantToOwner` or `UNetObjectConnectionFilter` | Built-in flag or explicit per-connection bitmask |
| DistancePriorityNode | `ULocationBasedNetObjectPrioritizer` subclass | Cached `FObjectLocationInfo`, batched `Prioritize()` |
| ConnectionGroupNode | `FNetObjectGroupHandle` | `CreateGroup`, `AddToGroup`, `SetGroupFilterStatus` |

---

## Why this skill exists

The Iris migration is irreversible from an architecture standpoint — once a system is on push-model
+ filter API, going back to `UReplicationGraph` requires a rewrite. Getting it right on the first
pass per actor is the cheapest path. This skill encodes the SuspenseCore-specific decisions
(named classes, EventBus integration, dormancy defaults, RPC validation policy) so every sub-agent
or follow-up session produces the same shape of code.

Keep this SKILL.md under 300 lines. Anything bulkier goes into `references/`.
