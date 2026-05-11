# SuspenseCore — Project Architecture & Rules for Claude Code

> **Engine:** Unreal Engine 5.7 | **Type:** Extraction PvE MMO Shooter (Tarkov-style)
> **Scale:** 627 C++ files, 141k lines, 9 active modules, 46 docs
> **Last Audit:** 2026-02-06 | **Overall Grade:** B+/A-

---

## What This Project Is

SuspenseCore is a Tarkov-style extraction PvE MMO FPS **Unreal Engine plugin** (not a game project).
It provides the complete game framework: weapons, inventory, equipment, enemy AI, abilities, UI.

**Core loop:** Hideout → Deployment → Raid → Extraction (like Escape from Tarkov).

---

## Module Map

Load order matters. Never introduce circular dependencies.

```
Phase 1 (PreDefault):
  BridgeSystem   — Core foundation: EventBus, ServiceLocator, interfaces, DataManager, types
  GAS            — Gameplay Ability System: abilities, effects, AttributeSets, services

Phase 2 (Default):
  PlayerCore     — Character, Controller, PlayerState, AnimInstance, movement
  InteractionSystem — Pickup actors, interaction component, helpers
  InventorySystem — Grid inventory, item slots, serialization, validation
  EquipmentSystem — Equipment slots (17), weapon actors, handlers, operation service
  UISystem       — All widgets: HUD, inventory, equipment, context menus, drag-drop
  EnemySystem    — AI: FSM states, perception, patrol, patrol, boss AI
  SuspenseCore   — Re-export module only (nearly empty, consider removing)

Disabled (do not touch):
  DisabledModules/LegasyEnemyCoreSystem  — legacy, migrated to EnemySystem
  DisabledModules/LegasyWeaponGAS        — legacy, migrated to GAS module
  DisabledModules/UISystem               — old UI, migrated to UISystem module
```

**Dependency rule:** Higher modules depend on lower, never the reverse.
`BridgeSystem` has zero dependencies on other SuspenseCore modules.

---

## Naming Conventions (Strict — 100% Enforced)

| Type | Prefix | Example |
|------|--------|---------|
| UObject class | `USuspenseCore*` | `USuspenseCoreDataManager` |
| Actor class | `ASuspenseCore*` | `ASuspenseCoreGrenadeProjectile` |
| Interface | `ISuspenseCore*` | `ISuspenseCoreItemUseHandler` |
| Struct | `FSuspenseCore*` | `FSuspenseCoreEventData` |
| Enum | `ESuspenseCore*` | `ESuspenseCoreInventoryResult` |
| GameplayEffect | `UGE_*` | `UGE_BleedingEffect_Light` |
| GameplayAbility | `UGA_*` | `UGA_MedicalUse` |
| File (header) | `SuspenseCore*.h` | `SuspenseCoreDoTService.h` |
| File (source) | `SuspenseCore*.cpp` | `SuspenseCoreDoTService.cpp` |

**Non-negotiable:** every new file, class, struct, enum must follow this table exactly.

---

## Core Patterns (What Works — Preserve These)

### 1. Native GameplayTags — MANDATORY

```cpp
// CORRECT — compile-time validated, zero runtime lookup
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
EventBus->Publish(SuspenseCoreTags::Event::Weapon::Fired, Data);

// FORBIDDEN — runtime string lookup, silent bug on typo
FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Weapon.Fired")); // NO
```

Tags are declared with `UE_DECLARE_GAMEPLAY_TAG_EXTERN` in namespace headers per domain
(e.g. `SuspenseCoreMedicalNativeTags.h`, `SuspenseCoreEquipmentNativeTags.h`).
All tags must also be registered in `Config/DefaultGameplayTags.ini`.

### 2. EventBus for Cross-Module Communication — MANDATORY

Never call methods across modules directly. Use EventBus.

```cpp
// Get EventBus
USuspenseCoreEventBus* EventBus = USuspenseCoreHelpers::GetEventBus(this);

// Publish
FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(this)
    .SetString(TEXT("ItemID"), ItemID.ToString())
    .SetObject(TEXT("Target"), TargetActor);
EventBus->Publish(SuspenseCoreTags::Event::Inventory::ItemAdded, Data);

// Subscribe (C++ native)
EventBus->SubscribeNative(Tag, this,
    FSuspenseCoreNativeEventCallback::CreateUObject(this, &UMyClass::OnEvent));

// Always store handle and clean up in EndPlay/Deinitialize
SubscriptionHandles.Add(Handle);
```

**Decision tree:**
- Blueprint exposure → `DECLARE_DYNAMIC_MULTICAST_DELEGATE`
- Widget-to-widget → `DECLARE_MULTICAST_DELEGATE`
- Everything else → **EventBus**

### 3. ServiceLocator / DI — MANDATORY

```cpp
// Get any service via static Get()
USuspenseCoreDoTService* DoTService = USuspenseCoreDoTService::Get(this);
USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(this);
USuspenseCoreEventBus* EventBus = USuspenseCoreEventBus::Get(this);

// Services are UGameInstanceSubsystems — registered automatically
```

Never construct services manually. Never store raw pointers to services — use `TWeakObjectPtr`.

### 4. SSOT — All Game Data from DataManager

```
JSON SSOT Files → DataTable → USuspenseCoreDataManager → Runtime Systems
```

Data files live in `Content/Data/`:
- `ItemDatabase/SuspenseCoreWeaponAttributes.json`
- `ItemDatabase/SuspenseCoreAmmoAttributes.json`
- `ItemDatabase/SuspenseCoreArmorAttributes.json`
- `ItemDatabase/SuspenseCoreConsumableAttributes.json`
- `ItemDatabase/SuspenseCoreThrowableAttributes.json`
- `ItemDatabase/SuspenseCoreMagazineData.json`
- `ItemDatabase/SuspenseCoreAttachmentAttributes.json`
- `StatusEffects/SuspenseCoreStatusEffectVisuals.json`

Never hardcode game values in C++. Always read from DataManager.

### 5. Handler Pattern for Item Use

All item use flows implement `ISuspenseCoreItemUseHandler` (defined in BridgeSystem):

```cpp
class ISuspenseCoreItemUseHandler
{
    virtual bool CanHandle(const FSuspenseCoreItemUseRequest& Request) = 0;
    virtual FSuspenseCoreItemUseResponse Execute(const FSuspenseCoreItemUseRequest& Request) = 0;
};
```

Handlers: `MedicalUseHandler`, `GrenadeHandler`, `MagazineSwapHandler`, `AmmoToMagazineHandler`.

### 6. TWeakObjectPtr for Cross-Object References

```cpp
// CORRECT
TWeakObjectPtr<ASuspenseCoreEnemyCharacter> WeakEnemy(Enemy);
[WeakEnemy]() {
    if (ASuspenseCoreEnemyCharacter* StrongEnemy = WeakEnemy.Get()) {
        StrongEnemy->Destroy();
    }
}

// DANGEROUS — raw pointer in lambda with deferred execution
[Enemy]() { if (Enemy) Enemy->Destroy(); } // NO — can be dangling
```

### 7. Interface Implementation Pattern

```cpp
class UMyComponent : public UActorComponent
    , public ISuspenseCoreEventSubscriber
    , public ISuspenseCoreEventEmitter
{
    // ISuspenseCoreEventSubscriber
    virtual void SetupEventSubscriptions(USuspenseCoreEventBus* EventBus) override;
    virtual void TeardownEventSubscriptions(USuspenseCoreEventBus* EventBus) override;
    virtual TArray<FSuspenseCoreSubscriptionHandle> GetSubscriptionHandles() const override;

protected:
    TArray<FSuspenseCoreSubscriptionHandle> SubscriptionHandles;
    mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;
};
```

### 8. GAS Architecture (ASC on PlayerState)

Follows Fortnite/Lyra pattern:
- `OwnerActor = PlayerState` (survives respawn)
- `AvatarActor = Character` (physical body)
- AttributeSets on PlayerState: Health, Stamina, Movement, Weapon, Armor, Recoil, Economy

When checking authority in abilities, use Avatar role, not Owner role.

---

## Anti-Patterns (Never Do These)

### Anti-Pattern 1: God Objects (CRITICAL to Fix)

Files already identified as too large — these are refactoring targets:

| File | Lines | Problem |
|------|-------|---------|
| `InventorySystem/.../SuspenseCoreInventoryComponent.cpp` | 3823 | Too many responsibilities |
| `EquipmentSystem/.../SuspenseCoreEquipmentOperationService.cpp` | 3519 | God service |
| `EquipmentSystem/.../SuspenseCoreEquipmentInventoryBridge.cpp` | 2744 | Too large |
| `EquipmentSystem/.../SuspenseCoreEquipmentTransactionProcessor.cpp` | 2585 | Too large |
| `EquipmentSystem/.../SuspenseCoreEquipmentDataService.cpp` | 2509 | Too large |
| `BridgeSystem/.../SuspenseCoreDataManager.cpp` | 2350 | Handles all data domains |
| `GAS/.../SuspenseCoreBaseFireAbility.h+cpp` | ~2400 | 5-6 responsibilities |

**Rule:** No `.cpp` file over 800 lines. If you're writing more, extract sub-components.

### Anti-Pattern 2: FCriticalSection on Read-Heavy Data

```cpp
// WRONG — blocks ALL readers even for read-only access
mutable FCriticalSection SubscriptionLock;
FScopeLock Lock(&SubscriptionLock); // blocks concurrent reads

// CORRECT — concurrent reads, exclusive writes
mutable FRWLock SubscriptionLock;
FRWScopeLock Lock(SubscriptionLock, SLT_ReadOnly);   // concurrent
FRWScopeLock Lock(SubscriptionLock, SLT_Write);      // exclusive
```

Structures that need FRWLock: EventBus subscriptions, ServiceLocator map, DataManager caches.

### Anti-Pattern 3: String-Keyed EventBus Payload (Type Unsafe)

```cpp
// CURRENT (bad) — typo = silent bug
Data.SetFloat(TEXT("Damge"), 50.0f);   // typo, nobody notices, returns 0

// TARGET — typed payload structs
USTRUCT(BlueprintType)
struct FSuspenseCoreDamageEventPayload {
    GENERATED_BODY()
    UPROPERTY() float Damage = 0.0f;
    UPROPERTY() float CurrentHealth = 0.0f;
    UPROPERTY() TWeakObjectPtr<AActor> DamageSource;
};
EventBus->PublishTyped(DamageTag, Payload);
```

### Anti-Pattern 4: Hardcoded Game Values

```cpp
// WRONG
SightConfig->SightRadius = 2000.0f;      // hardcoded in AIController ctor
HearingConfig->HearingRange = 1500.0f;   // ignore BehaviorData

// CORRECT — read from BehaviorData DataAsset
SightConfig->SightRadius = BehaviorData->GetEffectiveSightRange();
```

### Anti-Pattern 5: Raw Pointer Capture in Deferred Lambdas

```cpp
// WRONG — dangling pointer risk
Enemy->GetWorldTimerManager().SetTimer(Handle, [Enemy]() { Enemy->Destroy(); }, 3.0f, false);

// CORRECT
TWeakObjectPtr<ASuspenseCoreEnemyCharacter> Weak(Enemy);
Enemy->GetWorldTimerManager().SetTimer(Handle, [Weak]() {
    if (auto* E = Weak.Get()) E->Destroy();
}, 3.0f, false);
```

### Anti-Pattern 6: Synchronous Asset Loading

```cpp
// WRONG — causes hitches on init
DataTable = Cast<UDataTable>(DataTableRef.LoadSynchronous());

// CORRECT
FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
Streamable.RequestAsyncLoad(Paths, FStreamableDelegate::CreateUObject(this, &UClass::OnLoaded));
```

### Anti-Pattern 7: Cross-Module Direct Includes

```cpp
// WRONG — GAS module including EquipmentSystem
#include "SuspenseCore/Handlers/ItemUse/SuspenseCoreMedicalUseHandler.h" // in GAS module

// CORRECT — publish event, handler subscribes
EventBus->Publish(SuspenseCoreMedicalTags::Event::TAG_Event_Medical_ApplyEffect, Data);
```

---

## Known Bugs (Fix Priority)

### CRITICAL

1. **DeathState dangling pointer** — `Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyDeathState.cpp:82-92`
   Raw pointer capture in timer lambda. Fix: use `TWeakObjectPtr`.

### HIGH

2. **EnemySystem bypasses EventBus** — FSM states call methods directly instead of publishing events.
   All `OnStateChanged`, player detection, death, attack must go through EventBus.

3. **FCriticalSection everywhere** — EventBus, ServiceLocator, DataManager all use exclusive locks
   for structures that are read 100-1000× more than written. Replace with `FRWLock`.

4. **GAS simulated proxy optimization missing** — Full ASC replicated to all proxies.
   Should skip ASC for simulated proxies (Fortnite pattern). ~60-80% bandwidth reduction.

5. **Perception config hardcoded** — `SuspenseCoreEnemyAIController.cpp:15-30`
   BehaviorData already has sight/hearing ranges but they're ignored.

### MEDIUM

6. **IdleState rotation is frame-rate dependent** — `SuspenseCoreEnemyIdleState.cpp:53-61`
   `RInterpTo` called once per look interval, not every frame → jerky rotation.

7. **PatrolState delegate not cleaned on destruction** — `SuspenseCoreEnemyPatrolState.cpp`
   Missing `BeginDestroy()` override to unbind `ReceiveMoveCompleted`.

8. **ChaseState excessive logging** — `SuspenseCoreEnemyChaseState.cpp:130-155`
   100 log messages/sec at 50 enemies. Change to `UE_LOG(Verbose)`.

9. **ExecuteAttack is empty** — `SuspenseCoreEnemyCharacter.cpp:160-165`
   Only logs. No GAS ability activation, no damage, no animation.

10. **BridgeSystem depends on Niagara/UMG** — Foundation module shouldn't reference rendering.

### LOW

11. Mixed Russian/English comments throughout codebase.
12. Copyright header says 2025, should be 2025-2026.
13. `SuspenseCore.uplugin` still marked `IsBetaVersion: true`.
14. `DisabledModules/` contains 3 legacy modules — delete after verifying migration complete.
15. Typo: `Legasy` → `Legacy` in disabled module names.

---

## TODO/FIXME Inventory

Active technical debt markers in source:

| File | Line | Type | Description |
|------|------|------|-------------|
| `SuspenseCoreWeaponActor.cpp` | 1058-1060 | TODO | Attach mesh, apply modifiers, broadcast event |
| `SuspenseCoreWeaponActor.cpp` | 1077-1079 | TODO | Detach mesh, remove modifiers, broadcast event |
| `SuspenseCoreGrenadeHandler.cpp` | 535 | TODO | Request unequip via EventBus on cancel |
| `SuspenseCoreGrenadeHandler.cpp` | 1266 | TODO | Map GrenadeType tag to ItemID via DataManager |
| `SuspenseCoreMedicalUseHandler.cpp` | 681,718 | TODO (2026-Q2) | Remove legacy bleeding fallback code |
| `SuspenseCoreEquipmentOperationService.cpp` | 2379 | TODO | Advanced queue optimization |
| `SuspenseCoreMagazineComponent.cpp` | 261 | TODO | Spawn pickup actor with magazine data |
| `SuspenseCoreEquipmentAttributeComponent.cpp` | 313 | TODO | Create armor AttributeSet using SSOT class |
| `SuspenseCoreQuickSlotComponent.cpp` | 752,890 | TODO | Query inventory for validation + quantity |
| `SuspenseCoreHelpers.cpp` | 346,483,510,542,559 | TODO | Implement via ISuspenseCoreInventory |
| `SuspenseCoreInventoryWidget.cpp` | multiple | TODO | DragDrop container detection, slot search |
| `SuspenseCoreEquipmentWidget.cpp` | 643,803,877 | DEPRECATED/TODO | Remove deprecated method, implement preview |
| `SuspenseCoreCharacterSprintAbility.cpp` | 197,210 | DEPRECATED | SprintBuffEffectClass for speed is deprecated |
| `SuspenseCoreInventoryComponent.h` | 524,533 | DEPRECATED | GridSlots_DEPRECATED — remove in v2.0 |

---

## Refactoring Priorities (Ordered)

### Sprint 1 — Bug Fixes (Safety)
- [ ] Fix `SuspenseCoreEnemyDeathState.cpp` dangling pointer (TWeakObjectPtr)
- [ ] Fix `SuspenseCoreEnemyPatrolState.cpp` BeginDestroy cleanup
- [ ] Fix `SuspenseCoreEnemyIdleState.cpp` frame-rate dependent rotation
- [ ] Fix `SuspenseCoreEnemyChaseState.cpp` excessive logging → Verbose

### Sprint 2 — Architecture (High Impact)
- [ ] Replace all `FCriticalSection` with `FRWLock` in EventBus, ServiceLocator, DataManager
- [ ] Wire `ExecuteAttack()` to GAS ability in EnemyCharacter
- [ ] Add EventBus publishing to all EnemySystem FSM events (death, detection, attack, state change)
- [ ] Apply BehaviorData sight/hearing to AIPerception in `OnPossess`

### Sprint 3 — God Object Decomposition
- [ ] Split `SuspenseCoreDataManager` → domain-specific Data Providers (Weapon, Ammo, Armor, Consumable, StatusEffect)
- [ ] Split `SuspenseCoreBaseFireAbility` → RecoilCalculator + SpreadCalculator + TraceService + FXService + NetValidator
- [ ] Split `SuspenseCoreEquipmentOperationService` → QueueProcessor + HistoryService + CacheService + BatchExecutor
- [ ] Split `SuspenseCoreInventoryComponent` (3823 lines!) → extract serialization, validation, operations

### Sprint 4 — Type Safety & Cleanup
- [ ] Introduce typed EventBus payload structs for all high-traffic events
- [ ] Move Niagara/UMG out of BridgeSystem
- [ ] Remove all `DisabledModules/` after confirming no references
- [ ] Implement `FStreamableManager::RequestAsyncLoad` in DataManager
- [ ] Standardize all comments to English
- [ ] Update plugin status and copyright headers

### Sprint 5 — Networking (Future)
- [ ] Start Iris replication migration research (UE5.7 — ReplicationGraph deprecated)
- [ ] Implement simulated proxy ASC optimization (skip full ASC replication)
- [ ] Evaluate StateTree for complex enemy AI (Scav/PMC/Boss tactics)

---

## GDD Implementation Status

| Feature | Status | Notes |
|---------|--------|-------|
| Tarkov-style ammo system | ✅ Complete | MagazineComponent + QuickSlot (8 phases) |
| Weapon fire system | ✅ Complete | Single/Auto/Burst, GAS-based |
| Recoil system | ✅ Phase 4/6 | Convergence done, visual separation pending |
| Movement abilities | ✅ Complete | Sprint/Crouch/Prone/Jump/Vault + GAS stamina |
| DoT system (bleeding/burning) | ✅ Complete | GrenadeDoT + EventBus + DebuffWidget |
| Status effects v2.0 | ✅ Complete | 17 GE classes, stacking (Tarkov-style) |
| Equipment system (17 slots) | ✅ Complete | Full slot system + rules engine |
| Enemy AI (FSM) | 🔶 Basic | 5 states, needs tactical behavior |
| Medical system | ✅ Complete | IFAK, Medkit, HoT, GAS-integrated |
| Limb damage | 🔴 Not started | GDD exists |
| Level streaming | 🔴 Not started | Design exists |
| Instance zones | 🔴 Not started | Design exists |
| Extraction mechanics | 🔴 Not started | No implementation |
| Enemy AI (Scav/PMC/Boss) | 🔴 Not started | Needs StateTree evaluation |

---

## Checklist for Any New Code

Before committing:
- [ ] Class prefix: `U`/`A`/`I`/`F`/`E` + `SuspenseCore`
- [ ] GameplayEffects: `UGE_*` | GameplayAbilities: `UGA_*`
- [ ] No `FGameplayTag::RequestGameplayTag()` — use native tags
- [ ] Tags added to `Config/DefaultGameplayTags.ini`
- [ ] Cross-module communication via EventBus, not direct includes
- [ ] Timer lambdas use `TWeakObjectPtr`, not raw pointers
- [ ] New services are `UGameInstanceSubsystem` with `static Get()`
- [ ] Delegate cleanup in `EndPlay`/`Deinitialize`/`BeginDestroy`
- [ ] Read-heavy data structures use `FRWLock`, not `FCriticalSection`
- [ ] No game values hardcoded — read from DataManager/BehaviorData
- [ ] No `.cpp` file over 800 lines — extract sub-components if needed
- [ ] Comments in English only

---

## Quick Reference

### Getting Services

```cpp
USuspenseCoreEventBus*   Bus  = USuspenseCoreEventBus::Get(this);
USuspenseCoreDataManager* DM  = USuspenseCoreDataManager::Get(this);
USuspenseCoreDoTService*  DoT = USuspenseCoreDoTService::Get(this);
```

### Publishing an Event

```cpp
FSuspenseCoreEventData Data = FSuspenseCoreEventData::Create(this)
    .SetFloat(TEXT("Value"), 42.0f)
    .SetObject(TEXT("Target"), TargetActor);
Bus->Publish(SuspenseCoreTags::Event::MyDomain::MyAction, Data);
```

### Safe Timer Lambda

```cpp
TWeakObjectPtr<UMyClass> WeakSelf(this);
GetWorldTimerManager().SetTimer(Handle, [WeakSelf]() {
    if (auto* Self = WeakSelf.Get()) Self->DoWork();
}, Delay, false);
```

### RWLock Pattern

```cpp
mutable FRWLock DataLock;

// Reader:
FRWScopeLock ReadLock(DataLock, SLT_ReadOnly);
return MyMap.FindRef(Key);

// Writer:
FRWScopeLock WriteLock(DataLock, SLT_Write);
MyMap.Add(Key, Value);
```

### New Native Tag

```cpp
// 1. Config/DefaultGameplayTags.ini:
+GameplayTagList=(Tag="MyDomain.Event.MyAction",DevComment="...")

// 2. MyNativeTags.h:
namespace MyDomain::Event { UE_DECLARE_GAMEPLAY_TAG_EXTERN(MyAction); }

// 3. MyNativeTags.cpp:
namespace MyDomain::Event { UE_DEFINE_GAMEPLAY_TAG(MyAction, "MyDomain.Event.MyAction"); }
```

---

## Key Files Map

```
Source/
├── BridgeSystem/Public/SuspenseCore/
│   ├── Tags/SuspenseCoreGameplayTags.h        ← 200+ native tags (master)
│   ├── Events/SuspenseCoreEventBus.h          ← EventBus API
│   ├── Services/SuspenseCoreServiceProvider.h ← Service DI root
│   ├── Data/SuspenseCoreDataManager.h         ← SSOT data access
│   └── Types/SuspenseCoreTypes.h              ← FSuspenseCoreEventData, etc.
│
├── GAS/Public/SuspenseCore/
│   ├── Abilities/Base/SuspenseCoreBaseFireAbility.h  ← GOD OBJECT (refactor)
│   ├── Components/SuspenseCoreAbilitySystemComponent.h
│   ├── Attributes/ (7 AttributeSets)
│   └── Services/SuspenseCoreDoTService.h
│
├── EquipmentSystem/Public/SuspenseCore/
│   ├── Services/SuspenseCoreEquipmentOperationService.h ← GOD OBJECT (refactor)
│   ├── Services/SuspenseCoreEquipmentDataService.h      ← GOD OBJECT (refactor)
│   ├── Components/Core/SuspenseCoreEquipmentDataStore.h
│   └── Handlers/ItemUse/ (Medical, Grenade, Magazine)
│
├── InventorySystem/Public/SuspenseCore/
│   └── Components/SuspenseCoreInventoryComponent.h  ← GOD OBJECT 3823 lines
│
├── EnemySystem/Public/SuspenseCore/
│   ├── Characters/SuspenseCoreEnemyCharacter.h     ← ExecuteAttack() is empty
│   ├── FSM/States/ (Idle, Patrol, Chase, Combat, Death)
│   └── Data/SuspenseCoreEnemyBehaviorData.h
│
└── PlayerCore/Public/SuspenseCore/
    ├── Characters/SuspenseCoreCharacter.h  ← CineCamera for ADS (document why)
    └── Core/SuspenseCorePlayerState.h      ← ASC host (Fortnite pattern)
```
