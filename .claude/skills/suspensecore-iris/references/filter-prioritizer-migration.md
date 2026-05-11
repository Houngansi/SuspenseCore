# UReplicationGraph → Iris filter / prioritizer / group migration

Load when removing legacy `UReplicationGraph` nodes from SuspenseCore and standing up the
Iris equivalents. Each section is one of the five canonical nodes and how to replace it.

The mental shift: **the graph is gone**. Iris does not have nodes. It has flat per-connection
bitmasks (`FNetBitArrayView`) and batched location-based prioritizers. Anything you used to
express as "the actor walks through Node A, then Node B" is now expressed as "the actor sets
bit B in connection C's filter mask".

---

## Node 1 — `GridSpatialization2D` → `UNetObjectGridWorldLocFilter`

**What it did:** sliced the world into 2D cells; only actors in cells overlapping the player's
view were replicated.

**Iris replacement:** an automatic, built-in filter. No C++ work. Configure via INI.

```ini
[/Script/IrisCore.NetObjectGridWorldLocFilter]
CellSizeX=10000.0
CellSizeY=10000.0
DefaultCullDistance=25000.0
```

Any actor with `NetCullDistanceSquared > 0` is automatically routed through this filter by the
`UActorReplicationBridge` on `BeginReplication`. Set per-class via:

```cpp
ASuspenseCorePickupItem::ASuspenseCorePickupItem()
{
    bReplicates = true;
    NetCullDistanceSquared = 25000.0f * 25000.0f;   // 250m cull radius
}
```

No filter subclass, no registration, no graph traversal. Iris keeps the cell→object mapping
in a flat array; relevance is a bitmask AND.

---

## Node 2 — `AlwaysRelevantNode` → `UAlwaysRelevantNetObjectFilter`

**What it did:** force-included GameState, PlayerState, top-level managers in every connection's
relevance set.

**Iris replacement:** automatic. Set `bAlwaysRelevant = true` on the actor (or
`AActor::IsNetRelevantFor` overridden if needed). Iris detects the flag in
`UActorReplicationBridge::BeginReplication` and registers the actor into the always-relevant
filter group. No code change beyond the bool.

```cpp
ASuspenseGameState::ASuspenseGameState()
{
    bReplicates = true;
    bAlwaysRelevant = true;
}
```

Use sparingly. Each always-relevant actor consumes bandwidth on every connection. GameState,
PlayerStates, and the raid coordinator are typical; nothing else should be on this list.

---

## Node 3 — `OwnerOnlyNode` → built-in flag OR `UNetObjectConnectionFilter`

Two sub-cases here. Pick by ownership semantics.

### 3a. Standard owner-only (built-in)

For an actor whose `GetNetOwner()` is the target connection — e.g. `APlayerController` and any
component owned by it:

```cpp
ASuspenseHUDController::ASuspenseHUDController()
{
    bReplicates = true;
    bOnlyRelevantToOwner = true;     // Iris auto-routes via OwnerOnlyFilter
}
```

That's it. No custom filter.

### 3b. Conditional per-connection visibility (custom)

For actors where ownership is **not** the criterion — e.g. a `ASuspenseContainer` whose contents
should reveal only to the player currently inspecting it — use `UNetObjectConnectionFilter` via
the explicit API.

```cpp
#include "Net/Iris/ReplicationSystem/ReplicationSystemUtil.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"

void ASuspenseContainer::OpenForPlayer(APlayerController* PC)
{
#if UE_WITH_IRIS
    if (!PC || !PC->GetNetConnection())
    {
        return;
    }

    UReplicationSystem* RepSys = UE::Net::FReplicationSystemUtil::GetReplicationSystem(this);
    UActorReplicationBridge* Bridge = UE::Net::FReplicationSystemUtil::GetActorReplicationBridge(this);
    if (!RepSys || !Bridge)
    {
        return;
    }

    UE::Net::FNetRefHandle Handle = Bridge->GetReplicatedRefHandle(this);
    const uint32 ConnectionId = PC->GetNetConnection()->GetConnectionId();

    // Allow this connection to see the container's contents.
    RepSys->SetConnectionFilterStatus(Handle, ConnectionId, UE::Net::ENetFilterStatus::Allow);
#endif
}

void ASuspenseContainer::CloseForPlayer(APlayerController* PC)
{
#if UE_WITH_IRIS
    // Symmetric: disallow to hide on close.
    if (!PC || !PC->GetNetConnection()) return;

    UReplicationSystem* RepSys = UE::Net::FReplicationSystemUtil::GetReplicationSystem(this);
    UActorReplicationBridge* Bridge = UE::Net::FReplicationSystemUtil::GetActorReplicationBridge(this);
    if (!RepSys || !Bridge) return;

    UE::Net::FNetRefHandle Handle = Bridge->GetReplicatedRefHandle(this);
    RepSys->SetConnectionFilterStatus(
        Handle, PC->GetNetConnection()->GetConnectionId(),
        UE::Net::ENetFilterStatus::Disallow);
#endif
}
```

**Critical:** the `#if UE_WITH_IRIS` guard is non-negotiable. Without it the file fails to compile
when Iris is disabled (e.g. legacy server builds during the transition window).

---

## Node 4 — `DistancePriorityNode` → `ULocationBasedNetObjectPrioritizer` subclass

**What it did:** weighted actor priority by distance, so close actors got bandwidth before far ones.

**Iris replacement:** subclass `ULocationBasedNetObjectPrioritizer` and implement `Prioritize()`.
Iris caches actor positions in a flat `FObjectLocationInfo` array — your override iterates this
array directly, no virtual calls per-actor.

### Bucketed priority — the SuspenseCore default

```cpp
// SuspenseBucketPrioritizer.h
#pragma once

#include "CoreMinimal.h"
#include "Iris/ReplicationSystem/Prioritization/LocationBasedNetObjectPrioritizer.h"
#include "SuspenseBucketPrioritizer.generated.h"

UCLASS(Transient, MinimalAPI)
class USuspenseBucketPrioritizer : public ULocationBasedNetObjectPrioritizer
{
    GENERATED_BODY()

protected:
    virtual void Prioritize(FNetObjectPrioritizationParams& Params) override;
};
```

```cpp
// SuspenseBucketPrioritizer.cpp
#include "SuspenseBucketPrioritizer.h"

void USuspenseBucketPrioritizer::Prioritize(FNetObjectPrioritizationParams& Params)
{
    // Single-viewer simplification. For multi-viewer, iterate Params.Viewers.
    const FVector ViewLocation = Params.Viewers[0].ViewLocation;

    constexpr float Near_SqDist = 50.f  * 50.f  * 100.f * 100.f;   // ~50m
    constexpr float Mid_SqDist  = 100.f * 100.f * 100.f * 100.f;   // ~100m

    for (uint32 i = 0; i < Params.ObjectCount; ++i)
    {
        const uint32 ObjIdx = Params.ObjectIndices[i];
        const FVector ObjLoc = GetObjectLocation(ObjIdx);          // O(1) cached read
        const float DistSq = FVector::DistSquared(ViewLocation, ObjLoc);

        float Priority;
        if      (DistSq < Near_SqDist) Priority = 1.0f;            // Hot ring
        else if (DistSq < Mid_SqDist)  Priority = 0.5f;            // Mid ring
        else                           Priority = 0.1f;            // Cold ring

        Params.Priorities[i] = Priority;
    }
}
```

### Registration — INI only

```ini
[/Script/IrisCore.ReplicationSystemConfig]
+NetObjectPrioritizerDefinitions=(PrioritizerName="BucketPrioritizer",ClassName=/Script/SuspenseCore.SuspenseBucketPrioritizer)
```

Then assign per-class or per-actor with `RepSys->SetPrioritizer(Handle, PrioritizerName)`.

**Determinism rule (`AP-IRIS-7`):** never inject randomness or frame counters into `Prioritize()`.
Iris batches and re-runs the function; non-deterministic output produces packet thrashing.

---

## Node 5 — `ConnectionGroupNode` → `FNetObjectGroupHandle`

**What it did:** grouped actors and toggled visibility for a connection in one shot (room-based
loot reveal, instance switching).

**Iris replacement:** `FNetObjectGroupHandle` — a handle that bundles N actors and exposes
`SetGroupFilterStatus(group, connection, Allow|Disallow)` for O(1) bitmask operations.

### Pattern — per-room loot lock

```cpp
#include "Net/Iris/ReplicationSystem/ReplicationSystemUtil.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"

// ASuspenseRoomManager.h
UCLASS()
class SUSPENSECORE_API ASuspenseRoomManager : public AActor
{
    GENERATED_BODY()

protected:
#if UE_WITH_IRIS
    UE::Net::FNetObjectGroupHandle RoomGroupHandle;
#endif

public:
    virtual void BeginPlay() override;
    void RegisterLoot(AActor* LootActor);
    void SetVisibleForPlayer(APlayerController* PC, bool bVisible);
};
```

```cpp
// ASuspenseRoomManager.cpp
void ASuspenseRoomManager::BeginPlay()
{
    Super::BeginPlay();

#if UE_WITH_IRIS
    if (auto* RepSys = UE::Net::FReplicationSystemUtil::GetReplicationSystem(this))
    {
        RoomGroupHandle = RepSys->CreateGroup();
        // Exclusion group: members are hidden until SetGroupFilterStatus(Allow) flips them on.
        RepSys->AddExclusionFilterGroup(RoomGroupHandle);
    }
#endif
}

void ASuspenseRoomManager::RegisterLoot(AActor* LootActor)
{
#if UE_WITH_IRIS
    if (!LootActor) return;

    auto* RepSys = UE::Net::FReplicationSystemUtil::GetReplicationSystem(this);
    auto* Bridge = UE::Net::FReplicationSystemUtil::GetActorReplicationBridge(this);
    if (!RepSys || !Bridge) return;

    const UE::Net::FNetRefHandle Handle = Bridge->GetReplicatedRefHandle(LootActor);
    RepSys->AddToGroup(RoomGroupHandle, Handle);
#endif
}

void ASuspenseRoomManager::SetVisibleForPlayer(APlayerController* PC, bool bVisible)
{
#if UE_WITH_IRIS
    if (!PC || !PC->GetNetConnection()) return;

    auto* RepSys = UE::Net::FReplicationSystemUtil::GetReplicationSystem(this);
    if (!RepSys) return;

    const uint32 ConnId = PC->GetNetConnection()->GetConnectionId();
    const UE::Net::ENetFilterStatus Status =
        bVisible ? UE::Net::ENetFilterStatus::Allow
                 : UE::Net::ENetFilterStatus::Disallow;

    RepSys->SetGroupFilterStatus(RoomGroupHandle, ConnId, Status);
#endif
}
```

**Why this composes with dormancy:** a `DORM_Initial` actor inside an exclusion group is the
optimal density-1000 loot scenario — Iris does not poll, does not serialize, and the entire
group flips visibility in one bitmask write when a player enters the room.

---

## Decision tree — which Iris primitive for a new actor?

```
Is the actor always relevant to every player?
  YES → bAlwaysRelevant = true                                     (Node 2)
  NO  → continue
        ↓
Is the actor owned by exactly one connection (PC, owned component)?
  YES → bOnlyRelevantToOwner = true                                (Node 3a)
  NO  → continue
        ↓
Is visibility conditional and per-player (container reveal, instance)?
  YES → UNetObjectConnectionFilter via SetConnectionFilterStatus    (Node 3b)
  NO  → continue
        ↓
Does the actor have a meaningful world position?
  YES → NetCullDistanceSquared > 0 → UNetObjectGridWorldLocFilter   (Node 1)
        + optional ULocationBasedNetObjectPrioritizer subclass      (Node 4)
        + optional FNetObjectGroupHandle for bulk room lock          (Node 5)
  NO  → fall back to default Iris polling (rare for SuspenseCore)
```

---

## Post-migration grep suite

After removing a `UReplicationGraph` node, the following greps must all return zero hits in
the migrated path:

```bash
grep -R "GridSpatialization2D"        Source/SuspenseCore/Network/
grep -R "AlwaysRelevantNode"          Source/SuspenseCore/Network/
grep -R "OwnerOnlyNode"               Source/SuspenseCore/Network/
grep -R "DistancePriorityNode"        Source/SuspenseCore/Network/
grep -R "ConnectionGroupNode"         Source/SuspenseCore/Network/
grep -R "UReplicationGraph"           Source/SuspenseCore/Network/
```

If any return non-zero — the migration is incomplete; that path still uses the old graph.
