# SuspenseCore Developer Guide

> **Version:** 1.0
> **Last Updated:** December 2024
> **Target:** Unreal Engine 5.x | AAA Project Standards

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Module Structure](#module-structure)
3. [Naming Conventions](#naming-conventions)
4. [GameplayTags System](#gameplaytags-system)
5. [EventBus Pattern](#eventbus-pattern)
6. [Service Locator / DI](#service-locator--di)
7. [Code Patterns](#code-patterns)
8. [Adding New Features](#adding-new-features)
9. [Checklist for New Code](#checklist-for-new-code)

---

## Architecture Overview

SuspenseCore follows a **decoupled event-driven architecture** designed for:
- Scalability (MMO-ready)
- Testability (loose coupling)
- Maintainability (single responsibility)

### Core Principles

| Principle | Implementation |
|-----------|----------------|
| **Single Source of Truth (SSOT)** | DataManager for item data, PlayerState for player data |
| **Event-Driven Communication** | EventBus with GameplayTags (not direct calls) |
| **Dependency Injection** | ServiceProvider/ServiceLocator pattern |
| **Server Authority** | All mutations go through server with validation |

### System Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                     USuspenseCoreServiceProvider                │
│  ┌───────────────┐  ┌───────────────┐  ┌───────────────────┐   │
│  │  EventBus     │  │  DataManager  │  │  ItemManager      │   │
│  │  (Pub/Sub)    │  │  (SSOT)       │  │  (Item Factory)   │   │
│  └───────┬───────┘  └───────────────┘  └───────────────────┘   │
└──────────┼──────────────────────────────────────────────────────┘
           │ GameplayTag Events
           ▼
┌──────────────────────────────────────────────────────────────────┐
│                         SUBSCRIBERS                               │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐ │
│  │ PlayerCore │  │    GAS     │  │ Interaction│  │    UI      │ │
│  │  Module    │  │   Module   │  │   Module   │  │  Widgets   │ │
│  └────────────┘  └────────────┘  └────────────┘  └────────────┘ │
└──────────────────────────────────────────────────────────────────┘
```

---

## Module Structure

### Active Modules (Source/)

| Module | LoadingPhase | Purpose |
|--------|--------------|---------|
| **BridgeSystem** | PreDefault | Core infrastructure, EventBus, ServiceProvider, interfaces |
| **GAS** | PreDefault | Gameplay Ability System integration, abilities, effects |
| **PlayerCore** | Default | Character, Controller, PlayerState, player logic |
| **InteractionSystem** | Default | Interaction component, pickups, helpers |

### Module Dependencies

```
BridgeSystem (base)
    ↑
    ├── GAS (depends on BridgeSystem)
    │     ↑
    │     └── PlayerCore (depends on GAS + BridgeSystem)
    │           ↑
    │           └── InteractionSystem (depends on all above)
```

### Directory Structure

```
Plugins/SuspenseCore/
├── Config/
│   └── DefaultGameplayTags.ini          # ALL GameplayTags definitions
├── Documentation/
│   └── SuspenseCoreDeveloperGuide.md    # This file
├── Source/
│   ├── BridgeSystem/
│   │   ├── Public/SuspenseCore/
│   │   │   ├── Events/                  # EventBus, EventManager
│   │   │   ├── Services/                # ServiceProvider, ServiceLocator
│   │   │   ├── Interfaces/              # All ISuspenseCore* interfaces
│   │   │   ├── Tags/                    # SuspenseCoreGameplayTags.h
│   │   │   └── Types/                   # Shared structs, enums
│   │   └── Private/SuspenseCore/
│   ├── GAS/
│   │   ├── Public/SuspenseCore/
│   │   │   ├── Abilities/               # Base abilities
│   │   │   ├── Attributes/              # Attribute sets
│   │   │   └── Components/              # ASC wrapper
│   │   └── Private/SuspenseCore/
│   ├── PlayerCore/
│   │   ├── Public/SuspenseCore/
│   │   │   ├── Characters/              # SuspenseCoreCharacter
│   │   │   └── Core/                    # Controller, PlayerState
│   │   └── Private/SuspenseCore/
│   └── InteractionSystem/
│       ├── Public/SuspenseCore/
│       │   ├── Components/              # InteractionComponent
│       │   ├── Pickup/                  # PickupItem
│       │   └── Utils/                   # Helpers, Factory
│       └── Private/SuspenseCore/
└── SuspenseCore.uplugin
```

---

## Naming Conventions

### Classes

| Type | Prefix | Example |
|------|--------|---------|
| UObject classes | `USuspenseCore` | `USuspenseCoreItemManager` |
| Actor classes | `ASuspenseCore` | `ASuspenseCorePickupItem` |
| Interfaces | `ISuspenseCore` | `ISuspenseCoreInteractable` |
| Structs | `FSuspenseCore` | `FSuspenseCoreEventData` |
| Enums | `ESuspenseCore` | `ESuspenseCoreInventoryResult` |

### Files

```
// Header: PascalCase with SuspenseCore prefix
SuspenseCoreItemManager.h

// Source matches header
SuspenseCoreItemManager.cpp

// Interface files start with I
ISuspenseCoreInteractable.h
```

### GameplayTags

```
// Event tags: SuspenseCore.Event.{Domain}.{Action}
SuspenseCore.Event.Player.Spawned
SuspenseCore.Event.Weapon.Fired
SuspenseCore.Event.Inventory.ItemAdded

// Ability tags: SuspenseCore.Ability.{AbilityName}
SuspenseCore.Ability.Jump
SuspenseCore.Ability.Interact

// State tags: State.{StateName}
State.Dead
State.Stunned
State.Sprinting

// Item tags: Item.{Category}.{Type}
Item.Weapon.Rifle
Item.Armor.Helmet

// Equipment slots: Equipment.Slot.{SlotName}
Equipment.Slot.PrimaryWeapon
Equipment.Slot.BodyArmor
```

---

## GameplayTags System

### Configuration File

All tags MUST be defined in `Config/DefaultGameplayTags.ini`:

```ini
[/Script/GameplayTags.GameplayTagsSettings]
+GameplayTagList=(Tag="SuspenseCore.Event.Player.Spawned",DevComment="Player spawned event")
```

### Native Tags (C++)

Use the centralized header for frequently-used tags:

```cpp
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"

// Usage - compile-time validated, no string lookups
EventBus->Publish(SuspenseCoreTags::Event::Player::Spawned, EventData);

// NOT recommended - runtime string lookup, possible typos
EventBus->Publish(FGameplayTag::RequestGameplayTag("SuspenseCore.Event.Player.Spawned"), EventData);
```

### Adding New Tags

1. **Add to config first:**
```ini
+GameplayTagList=(Tag="SuspenseCore.Event.MyFeature.Action",DevComment="Description")
```

2. **If frequently used, add to SuspenseCoreGameplayTags.h:**
```cpp
// In header
namespace SuspenseCoreTags::Event::MyFeature
{
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Action);
}

// In cpp
namespace SuspenseCoreTags::Event::MyFeature
{
    UE_DEFINE_GAMEPLAY_TAG(Action, "SuspenseCore.Event.MyFeature.Action");
}
```

3. **Add to validation if critical:**
```cpp
// In SuspenseCoreGameplayTags.cpp - GetCriticalTagNames()
TEXT("SuspenseCore.Event.MyFeature.Action"),
```

---

## EventBus Pattern

### Getting EventBus

**Preferred method** - via ServiceProvider:

```cpp
#include "SuspenseCore/Utils/SuspenseCoreHelpers.h"

USuspenseCoreEventBus* EventBus = USuspenseCoreHelpers::GetEventBus(this);
```

**In components implementing ISuspenseCoreEventEmitter:**

```cpp
USuspenseCoreEventBus* EventBus = GetEventBus(); // Interface method
```

### Publishing Events

```cpp
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"

// Create event data
FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this)
    .SetString(TEXT("ItemID"), ItemID.ToString())
    .SetInt(TEXT("Quantity"), Quantity)
    .SetObject(TEXT("TargetActor"), TargetActor);

// Publish using native tag
EventBus->Publish(SuspenseCoreTags::Event::Inventory::ItemAdded, EventData);
```

### Subscribing to Events

```cpp
// In SetupEventSubscriptions()
void UMyComponent::SetupEventSubscriptions(USuspenseCoreEventBus* EventBus)
{
    if (!EventBus) return;

    FSuspenseCoreSubscriptionHandle Handle = EventBus->Subscribe(
        SuspenseCoreTags::Event::Inventory::ItemAdded,
        FSuspenseCoreEventDelegate::CreateUObject(this, &UMyComponent::HandleItemAdded)
    );

    SubscriptionHandles.Add(Handle);
}

void UMyComponent::HandleItemAdded(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    FString ItemID = EventData.GetString(TEXT("ItemID"));
    int32 Quantity = EventData.GetInt(TEXT("Quantity"));
    // Handle event...
}
```

---

## Service Locator / DI

### Getting Services

```cpp
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"

// Get ServiceProvider
USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(this);

// Get specific services
USuspenseCoreEventBus* EventBus = Provider->GetEventBus();
USuspenseCoreDataManager* DataManager = Provider->GetDataManager();
```

### Service Pattern

All services should be GameInstance Subsystems:

```cpp
UCLASS()
class BRIDGESYSTEM_API UMySuspenseCoreService : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    static UMySuspenseCoreService* Get(const UObject* WorldContextObject);
    // ...
};
```

---

## Code Patterns

### Delegate vs EventBus Decision Tree

```
Is it for Blueprint exposure?
├── YES → Use DECLARE_DYNAMIC_MULTICAST_DELEGATE + BlueprintAssignable
│
└── NO → Is it UI widget-to-widget communication?
         ├── YES → Use DECLARE_MULTICAST_DELEGATE (performance)
         │
         └── NO → Use EventBus (GameplayTags)
```

### Interface Implementation Pattern

```cpp
UCLASS()
class UMyComponent : public UActorComponent
    , public ISuspenseCoreEventSubscriber
    , public ISuspenseCoreEventEmitter
{
    GENERATED_BODY()

public:
    // ISuspenseCoreEventSubscriber
    virtual void SetupEventSubscriptions(USuspenseCoreEventBus* EventBus) override;
    virtual void TeardownEventSubscriptions(USuspenseCoreEventBus* EventBus) override;
    virtual TArray<FSuspenseCoreSubscriptionHandle> GetSubscriptionHandles() const override;

    // ISuspenseCoreEventEmitter
    virtual void EmitEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& Data) override;
    virtual USuspenseCoreEventBus* GetEventBus() const override;

protected:
    TArray<FSuspenseCoreSubscriptionHandle> SubscriptionHandles;
    mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;
};
```

### Error Handling Pattern

```cpp
// Use EventBus for error reporting
if (!CanPerformOperation())
{
    FSuspenseCoreEventData ErrorData = FSuspenseCoreEventData::Create(this)
        .SetString(TEXT("Reason"), TEXT("Validation failed"))
        .SetString(TEXT("Operation"), TEXT("AddItem"));

    EmitEvent(SuspenseCoreTags::Event::Inventory::OperationFailed, ErrorData);
    return false;
}
```

---

## Adding New Features

### Step 1: Plan the Feature

- [ ] Identify which module it belongs to
- [ ] Define required GameplayTags
- [ ] Define required interfaces
- [ ] Plan EventBus events

### Step 2: Add GameplayTags

```ini
# Config/DefaultGameplayTags.ini
+GameplayTagList=(Tag="SuspenseCore.Event.MyFeature.Started",DevComment="Feature started")
+GameplayTagList=(Tag="SuspenseCore.Event.MyFeature.Completed",DevComment="Feature completed")
```

### Step 3: Create Interface (if needed)

```cpp
// ISuspenseCoreMyFeature.h
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreMyFeature : public UInterface { GENERATED_BODY() };

class ISuspenseCoreMyFeature
{
    GENERATED_BODY()
public:
    virtual void DoSomething() = 0;
};
```

### Step 4: Implement Component/Actor

```cpp
// Follow patterns from existing components
class UMyFeatureComponent
    : public UActorComponent
    , public ISuspenseCoreEventSubscriber
    , public ISuspenseCoreEventEmitter
    , public ISuspenseCoreMyFeature
{
    // Implementation...
};
```

### Step 5: Update Documentation

- Add tags to this guide if significant
- Update SuspenseCoreGameplayTags.h if frequently used

---

## Checklist for New Code

### Before Committing

- [ ] All classes use `SuspenseCore` prefix
- [ ] All GameplayTags defined in `DefaultGameplayTags.ini`
- [ ] No hardcoded tag strings (use SuspenseCoreGameplayTags.h)
- [ ] EventBus used for cross-system communication
- [ ] No direct module dependencies (use interfaces)
- [ ] Comments updated with `@see` references
- [ ] Blueprint-exposed functions have proper meta tags

### Code Review Checklist

- [ ] Follows naming conventions
- [ ] Uses EventBus, not direct calls
- [ ] Implements proper interfaces
- [ ] Error handling broadcasts events
- [ ] No memory leaks (proper cleanup in EndPlay/Deinitialize)
- [ ] Thread-safe if needed

---

## Quick Reference

### Common Includes

```cpp
// Tags
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"

// EventBus
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"

// Services
#include "SuspenseCore/Services/SuspenseCoreServiceProvider.h"

// Helpers
#include "SuspenseCore/Utils/SuspenseCoreHelpers.h"

// Interfaces
#include "SuspenseCore/SuspenseCoreInterfaces.h"
```

### Event Publishing Template

```cpp
void UMyClass::BroadcastMyEvent(const FString& Param)
{
    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this)
            .SetString(TEXT("Param"), Param);

        EventBus->Publish(SuspenseCoreTags::Event::MyFeature::Action, EventData);
    }
}
```

---

## Contact

For questions about architecture decisions, contact the Tech Lead or reference this document.

**Remember:** When in doubt, use EventBus with GameplayTags. Tight coupling is the enemy of maintainability.
