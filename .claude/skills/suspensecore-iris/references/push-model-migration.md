# Push-Model migration — `ASuspenseCorePickupItem` and custom Iris states

Load when migrating a UPROPERTY-heavy actor or component to Push-Model, or when introducing
a non-UObject struct that needs to participate in Iris replication via `UE_NET_DECLARE_IRIS_STATE`.

---

## 1. Concept — why Push-Model exists

Legacy replication walks every UPROPERTY every frame to check `OldValue != NewValue`. For a server
holding ~10 000 loot actors in a Tarkov-style raid, this is millions of virtual comparisons per
second — and most of them return "unchanged".

Push-Model inverts the contract: **the gameplay code tells the network layer when something
changed**, by setting a dirty bit in the actor's `ReplicationFragment`. Iris polls nothing; it
only serializes properties whose bits are set, then clears them.

The catch: if you forget the `MARK_PROPERTY_DIRTY_FROM_NAME` call, the property mutates locally
on the server and is **never** transmitted. Compile passes, replication silently breaks. The
mitigation is `AP-IRIS-2` in the post-flight grep suite.

---

## 2. Worked example — `ASuspenseCorePickupItem`

Loot items are the highest-volume replicated actor in SuspenseCore. Below is the canonical
SuspenseCore shape. Use as a template for any pickup-style class.

### Header

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Net/Core/PushModel/PushModel.h"   // MARK_PROPERTY_DIRTY_FROM_NAME
#include "SuspenseCorePickupItem.generated.h"

UCLASS()
class SUSPENSECORE_API ASuspenseCorePickupItem : public AActor
{
    GENERATED_BODY()

public:
    ASuspenseCorePickupItem();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Authoritative mutators — only path that may modify state on the server.
    void SetQuantity(int32 NewQuantity);
    void SetDurability(float NewDurability);
    void SetLootedState(bool bLooted);

protected:
    UPROPERTY(ReplicatedUsing = OnRep_ItemState)
    int32 ItemID;

    UPROPERTY(ReplicatedUsing = OnRep_ItemState)
    float Durability;

    UPROPERTY(ReplicatedUsing = OnRep_ItemState)
    int32 Quantity;

    UPROPERTY(Replicated)
    bool bIsLooted;

    UPROPERTY(Replicated)
    FVector LocationOffset;

    UFUNCTION()
    void OnRep_ItemState();
};
```

### Implementation

```cpp
#include "SuspenseCorePickupItem.h"

#include "Net/UnrealNetwork.h"
#include "SuspenseEventBusSubsystem.h"
#include "Events/SuspenseItemStateChangedEvent.h"

ASuspenseCorePickupItem::ASuspenseCorePickupItem()
{
    bReplicates = true;

    // Static-density optimization: spawn dormant so Iris does not poll until first wake-up.
    NetDormancy = DORM_Initial;
}

void ASuspenseCorePickupItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams PushParams;
    PushParams.bIsPushBased = true;          // Iris contract: no polling

    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, ItemID,         PushParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, Durability,     PushParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, Quantity,       PushParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsLooted,      PushParams);
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, LocationOffset, PushParams);
}

void ASuspenseCorePickupItem::SetQuantity(int32 NewQuantity)
{
    if (HasAuthority() && Quantity != NewQuantity)
    {
        Quantity = NewQuantity;
        // Iris-mandatory: flip dirty bit so the fragment serializes on Pre-Send Update.
        MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, Quantity, this);
    }
}

void ASuspenseCorePickupItem::SetDurability(float NewDurability)
{
    if (HasAuthority() && !FMath::IsNearlyEqual(Durability, NewDurability))
    {
        Durability = NewDurability;
        MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, Durability, this);
    }
}

void ASuspenseCorePickupItem::SetLootedState(bool bLooted)
{
    if (HasAuthority() && bIsLooted != bLooted)
    {
        bIsLooted = bLooted;
        MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, bIsLooted, this);
    }
}

// Client-side reaction surface — strictly EventBus, no direct UI/audio/anim calls.
void ASuspenseCorePickupItem::OnRep_ItemState()
{
    FSuspenseItemStateChangedEvent Payload;
    Payload.TargetItem  = this;
    Payload.ItemID      = ItemID;
    Payload.NewQuantity = Quantity;
    Payload.NewDurability = Durability;

    if (UWorld* World = GetWorld())
    {
        if (auto* Bus = USuspenseEventBusSubsystem::Get(World))
        {
            Bus->Publish(Payload);
        }
    }
}
```

**Things to notice (and copy):**
- `HasAuthority()` guard on every setter — clients cannot drive state.
- `!=` and `IsNearlyEqual` short-circuit before assignment — no dirty bit for no-op writes.
- `DORM_Initial` so freshly spawned loot starts dormant; the spawner wakes it via `FlushNetDormancy`
  when a player interacts.
- `OnRep_ItemState` builds a payload and publishes — never reaches into UMG or audio.

---

## 3. Custom Iris states via `UE_NET_DECLARE_IRIS_STATE`

When you need to replicate a tightly packed, non-UObject struct (telemetry blob, compressed
network token, bitfield mask) and `UPROPERTY` reflection overhead is unacceptable, declare the
state directly:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationSystem/ReplicationFragmentUtil.h"

// Plain C++ struct — no USTRUCT, no UPROPERTY overhead.
struct FSuspenseTelemetryFrame
{
    uint32 Tick;
    uint16 PackedHealth;     // Quantized 0..65535
    uint16 PackedStamina;
    uint8  AmmoCount;
    uint8  Flags;            // Bitfield

    bool operator==(const FSuspenseTelemetryFrame& Other) const
    {
        return Tick == Other.Tick
            && PackedHealth == Other.PackedHealth
            && PackedStamina == Other.PackedStamina
            && AmmoCount == Other.AmmoCount
            && Flags == Other.Flags;
    }
};

// Declares CreateAndRegisterReplicationFragmentFunc + DestructReplicationStateFunc.
// UHT generates a quantized shadow state mirror with per-field dirty bits.
UE_NET_DECLARE_IRIS_STATE(FSuspenseTelemetryFrame, SUSPENSECORE_API);
```

Pair with a custom `FNetSerializer` if you want bit-level packing (e.g. fit `PackedHealth` and
`PackedStamina` into 28 bits combined). For most cases, the default serializer Iris generates is
already an order of magnitude faster than the reflection path.

**When to use:**
- Per-tick telemetry that needs absolute minimum cost.
- Packed bitfields where field boundaries do not align to byte boundaries.
- Structs shared with native networking code outside UE's UStruct system.

**When NOT to use:**
- Plain replicated state — `UPROPERTY` + Push-Model is already fast and friendlier to tooling.
- One-off RPC payloads — use `USTRUCT` parameters.

---

## 4. Common pitfalls

| Symptom | Likely cause | Fix |
|--------|--------------|-----|
| Property mutates on server, client never sees update | Missing `MARK_PROPERTY_DIRTY_FROM_NAME` | Add the macro in every setter |
| Dirty bit set, but no packet sent | `DOREPLIFETIME` instead of `DOREPLIFETIME_WITH_PARAMS_FAST` | Replace with `_FAST` variant |
| Client receives update but `OnRep_` not called | `ReplicatedUsing` typo or stale reflection cache | Recompile after rename, restart editor |
| Custom struct compiles but never arrives | Missing `SetupIrisSupport(Target)` in Build.cs | Add macro, rebuild module |
| `LogIrisReplication: Push model disabled` | `net.IsPushModelEnabled=0` or missing in ini | Add to `DefaultEngine.ini` per `activation-config.md` |
| Two writes per frame produce two packets | They don't — Iris aggregates dirty bits. If you see two packets, you have two distinct dirty-set sites for the same property | Consolidate writes through a single setter |

---

## 5. Migration recipe for any existing actor

1. Add `#include "Net/Core/PushModel/PushModel.h"` to the cpp.
2. Convert all `DOREPLIFETIME(...)` to `DOREPLIFETIME_WITH_PARAMS_FAST(...)` with
   `PushParams.bIsPushBased = true`.
3. For every direct `UPROPERTY` write outside `BeginPlay` / construction, introduce a `Set*()`
   mutator with `HasAuthority()` guard + equality short-circuit + `MARK_PROPERTY_DIRTY_FROM_NAME`.
4. Move any logic from `OnRep_*` into an EventBus payload publish.
5. If the actor is static-density (loot, props), set `NetDormancy = DORM_Initial`.
6. Run the dedicated server with `LogIrisReplication Verbose` and verify the protocol binding
   line appears for your class on first replication.

After step 6 the actor is fully migrated. Move to the next.
