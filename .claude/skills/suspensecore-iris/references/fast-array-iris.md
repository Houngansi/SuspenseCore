# FFastArraySerializer under Iris — `USuspenseCoreInventoryComponent`

Load when migrating any replicated collection (inventory grid, equipped slots, status effects,
hit-marker buffer) to Iris. The critical contract is **`operator==` on the element type** — Iris
uses it against the shadow state to decide whether a delta is even worth serializing.

---

## 1. Why a plain `TArray<UPROPERTY(Replicated)>` is unacceptable

Default replication for `TArray<T>` is "ship the whole array on any change". A 60-slot inventory
in a Tarkov-like game with stack-count updates would saturate the per-actor bundle in seconds.

`FFastArraySerializer` exists to ship **only the delta** (added, changed, removed items). Under
Iris, this is upgraded further: Iris registers a `TFastArrayReplicationFragment` that consumes
`OldArraySize` + `WriteSparseBitArrayDelta` for tight bit-level packing. Under the hood Iris
calls your element's `operator==` to skip elements whose quantized shadow already matches.

**Without `operator==`, Iris assumes every element changed every frame.** Bandwidth blows up.
This is `AP-IRIS-3`.

---

## 2. Element struct — every field, the equality operator

```cpp
USTRUCT()
struct FSuspenseInventoryItem : public FFastArraySerializerItem
{
    GENERATED_BODY()

    UPROPERTY()
    int32 ItemID = 0;

    UPROPERTY()
    int32 GridPositionX = 0;

    UPROPERTY()
    int32 GridPositionY = 0;

    UPROPERTY()
    int32 StackCount = 0;

    UPROPERTY()
    float Durability = 1.0f;

    // Iris fast-array contract: required for TFastArrayReplicationFragment delta-skip.
    // Compare every field that participates in replication. Floats compared with epsilon.
    bool operator==(const FSuspenseInventoryItem& Other) const
    {
        return ItemID         == Other.ItemID
            && GridPositionX  == Other.GridPositionX
            && GridPositionY  == Other.GridPositionY
            && StackCount     == Other.StackCount
            && FMath::IsNearlyEqual(Durability, Other.Durability);
    }

    bool operator!=(const FSuspenseInventoryItem& Other) const
    {
        return !(*this == Other);
    }
};
```

Notes:
- `FFastArraySerializerItem` base provides `ReplicationID` / `ReplicationKey` — leave them alone.
- Every UPROPERTY field that drifts at runtime **must** be in `operator==`. Forgetting one means
  that field updates never transmit.
- Floats: epsilon comparison or quantize before storage. Bit-exact float equality is the most
  common source of "delta storms".

---

## 3. Array wrapper — `FSuspenseInventoryArray`

```cpp
USTRUCT()
struct FSuspenseInventoryArray : public FFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FSuspenseInventoryItem> Items;

    // Engine delta-serialization entry point. Iris reads this via TFastArrayReplicationFragment.
    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize<
            FSuspenseInventoryItem,
            FSuspenseInventoryArray>(Items, DeltaParms, *this);
    }

    // -------- Authoritative mutators (server-only) --------

    void AddItem(const FSuspenseInventoryItem& NewItem)
    {
        Items.Add(NewItem);
        MarkItemDirty(Items.Last());
    }

    bool UpdateItemStack(int32 TargetItemID, int32 DeltaStack)
    {
        const int32 Index = Items.IndexOfByPredicate(
            [TargetItemID](const FSuspenseInventoryItem& I) { return I.ItemID == TargetItemID; });

        if (Index == INDEX_NONE)
        {
            return false;
        }

        Items[Index].StackCount += DeltaStack;
        MarkItemDirty(Items[Index]);     // Iris contract: bump ReplicationKey
        return true;
    }

    bool RemoveByItemID(int32 TargetItemID)
    {
        const int32 Index = Items.IndexOfByPredicate(
            [TargetItemID](const FSuspenseInventoryItem& I) { return I.ItemID == TargetItemID; });

        if (Index == INDEX_NONE)
        {
            return false;
        }

        Items.RemoveAt(Index);
        MarkArrayDirty();                // Iris contract: structural change
        return true;
    }
};

// UStruct trait — enables NetDeltaSerialize for this struct.
template<>
struct TStructOpsTypeTraits<FSuspenseInventoryArray> : public TStructOpsTypeTraitsBase2<FSuspenseInventoryArray>
{
    enum { WithNetDeltaSerializer = true };
};
```

**The two dirty calls and when to use each:**
- `MarkItemDirty(Item)` — element field changed (StackCount, Durability, position). One element's
  delta is shipped.
- `MarkArrayDirty()` — element added or removed. Iris ships the structural change. Required after
  any `Items.Add` / `Items.RemoveAt` / `Items.Insert`.

---

## 4. Component — owner and Server RPC entry points

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Net/Core/PushModel/PushModel.h"
#include "SuspenseInventoryTypes.h"
#include "SuspenseCoreInventoryComponent.generated.h"

UCLASS(ClassGroup = (SuspenseCore), meta = (BlueprintSpawnableComponent))
class SUSPENSECORE_API USuspenseCoreInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USuspenseCoreInventoryComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Client requests; server validates and mutates.
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_AddStackToItem(int32 ItemID, int32 Amount);

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_RemoveItem(int32 ItemID);

protected:
    UPROPERTY(Replicated)
    FSuspenseInventoryArray InventoryData;
};
```

```cpp
#include "SuspenseCoreInventoryComponent.h"

#include "Net/UnrealNetwork.h"
#include "SuspenseEventBusSubsystem.h"
#include "Events/SuspenseInventoryChangedEvent.h"

USuspenseCoreInventoryComponent::USuspenseCoreInventoryComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void USuspenseCoreInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    FDoRepLifetimeParams Params;
    Params.bIsPushBased = true;
    DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, InventoryData, Params);
}

// ---- Anti-cheat validation ----
bool USuspenseCoreInventoryComponent::Server_AddStackToItem_Validate(int32 ItemID, int32 Amount)
{
    // Hard bounds — anything outside is treated as a malicious client.
    return Amount > 0 && Amount <= 60 && ItemID > 0;
}

void USuspenseCoreInventoryComponent::Server_AddStackToItem_Implementation(int32 ItemID, int32 Amount)
{
    if (!InventoryData.UpdateItemStack(ItemID, Amount))
    {
        return;  // Item not in inventory — silent reject, log via separate analytics path.
    }

    // The wrapper struct itself is the replicated UPROPERTY — flag it dirty so Iris
    // runs NetDeltaSerialize for the next outbound bundle.
    MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, InventoryData, this);
}

bool USuspenseCoreInventoryComponent::Server_RemoveItem_Validate(int32 ItemID)
{
    return ItemID > 0;
}

void USuspenseCoreInventoryComponent::Server_RemoveItem_Implementation(int32 ItemID)
{
    if (!InventoryData.RemoveByItemID(ItemID))
    {
        return;
    }
    MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, InventoryData, this);
}
```

---

## 5. Client reaction — `PostReplicatedAdd` / `PostReplicatedChange` / `PreReplicatedRemove`

`FFastArraySerializerItem` exposes optional virtual overrides you can use on the client side to
fire EventBus messages instead of polling for changes. Put these on the **element**, not the
component, so each item self-reports.

```cpp
USTRUCT()
struct FSuspenseInventoryItem : public FFastArraySerializerItem
{
    GENERATED_BODY()

    // ... fields + operator== as above ...

    void PostReplicatedAdd(const struct FSuspenseInventoryArray& InArray)
    {
        FSuspenseInventoryChangedEvent Payload{ EChangeType::Added, *this };
        if (auto* Bus = USuspenseEventBusSubsystem::Get(GWorld))
        {
            Bus->Publish(Payload);
        }
    }

    void PostReplicatedChange(const struct FSuspenseInventoryArray& InArray)
    {
        FSuspenseInventoryChangedEvent Payload{ EChangeType::Modified, *this };
        if (auto* Bus = USuspenseEventBusSubsystem::Get(GWorld))
        {
            Bus->Publish(Payload);
        }
    }

    void PreReplicatedRemove(const struct FSuspenseInventoryArray& InArray)
    {
        FSuspenseInventoryChangedEvent Payload{ EChangeType::Removed, *this };
        if (auto* Bus = USuspenseEventBusSubsystem::Get(GWorld))
        {
            Bus->Publish(Payload);
        }
    }
};
```

`GWorld` is a placeholder — in production resolve the world via the array's owning object.
The point is: **UI never queries the inventory directly**; it subscribes to
`FSuspenseInventoryChangedEvent` on the EventBus.

---

## 6. Performance audit checklist

For any inventory-style migration, verify:
- [ ] `operator==` covers every UPROPERTY field of the element.
- [ ] Floats compared with `FMath::IsNearlyEqual` or quantized.
- [ ] Every element-field mutation calls `MarkItemDirty(Items[i])`.
- [ ] Every `Add` / `Remove` / `Insert` calls `MarkArrayDirty()`.
- [ ] Component-level mutation triggers `MARK_PROPERTY_DIRTY_FROM_NAME(..., InventoryData, this)`.
- [ ] `Server_*` RPCs have `WithValidation` + hard bounds checks.
- [ ] Client never writes to `InventoryData` directly — only via Server RPC.
- [ ] EventBus payloads carry **the changed element + change type**, not the whole array.

---

## 7. Common pitfalls

| Symptom | Cause | Fix |
|---------|-------|-----|
| Bandwidth grows linearly with array size every frame | Missing `operator==` or it forgets a field | Audit `operator==` against the UPROPERTY list |
| Add/Remove not arriving on client | Only `MarkItemDirty`, not `MarkArrayDirty` | Add `MarkArrayDirty()` after structural ops |
| Same field updated twice per frame produces two packets | It does not — Iris aggregates. If you observe this, you have two separate writes from two systems | Funnel both through one setter |
| `PostReplicatedChange` never called on client | Component `bReplicates` false, or `SetIsReplicatedByDefault(true)` missing | Re-enable replication on the component |
| `Quantity` change ships but `Durability` change doesn't | `Durability` missing from `operator==` | Add the field |
| Client edit "rolls back" on next replication | Client wrote locally; server is authoritative | Route through Server RPC, never write client-side |
