# SuspenseCore Architecture Guide

## Overview

SuspenseCore is an EventBus-driven architecture for Unreal Engine that provides:
- Decoupled systems communication via GameplayTags
- Centralized data management
- Project Settings configuration (no Blueprint GameInstance required)
- Full GAS (Gameplay Ability System) integration

---

## Core Components

### 1. USuspenseCoreSettings (UDeveloperSettings)

**Location:** `BridgeSystem/Public/SuspenseCore/Settings/SuspenseCoreSettings.h`

Central configuration accessible via **Project Settings → Game → SuspenseCore**.

```cpp
// C++ Access
const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();

// Configuration Properties
TSoftObjectPtr<UDataTable> ItemDataTable;           // Item definitions
TSoftObjectPtr<UDataAsset> CharacterClassesDataAsset; // Character classes
TSoftObjectPtr<UDataTable> LoadoutDataTable;        // Loadout configurations
TSoftObjectPtr<UDataTable> WeaponAnimationsTable;   // Weapon animations
```

**Best Practices:**
- ALL data configuration goes here (single source of truth)
- Use soft references (TSoftObjectPtr) to avoid load-time dependencies
- Never configure DataTables in Blueprint GameInstance

---

### 2. USuspenseCoreDataManager (GameInstanceSubsystem)

**Location:** `BridgeSystem/Public/SuspenseCore/Data/SuspenseCoreDataManager.h`

Runtime data management with EventBus integration.

```cpp
// C++ Access
USuspenseCoreDataManager* DataManager = USuspenseCoreDataManager::Get(WorldContextObject);

// Or via helpers
USuspenseCoreDataManager* DataManager = USuspenseCoreHelpers::GetDataManager(WorldContextObject);

// Usage
FSuspenseUnifiedItemData ItemData;
if (DataManager->GetItemData(ItemID, ItemData))
{
    // Use item data
}
```

**Lifecycle:**
1. GameInstance creates subsystem
2. `Initialize()` loads data from USuspenseCoreSettings
3. Builds item cache from DataTable
4. Broadcasts `SuspenseCore.Event.Data.Initialized`
5. Provides cached data access throughout session

**EventBus Events:**
| Tag | Description |
|-----|-------------|
| `SuspenseCore.Event.Data.Initialized` | DataManager ready |
| `SuspenseCore.Event.Data.ItemLoaded` | Item data retrieved |
| `SuspenseCore.Event.Data.ItemNotFound` | Item ID not in database |
| `SuspenseCore.Event.Data.ValidationPassed` | All validations passed |
| `SuspenseCore.Event.Data.ValidationFailed` | Validation errors found |

---

### 3. USuspenseCoreEventManager (GameInstanceSubsystem)

**Location:** `BridgeSystem/Public/SuspenseCore/Events/SuspenseCoreEventManager.h`

EventBus provider subsystem.

```cpp
// Get EventBus
USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(WorldContextObject);
USuspenseCoreEventBus* EventBus = Manager->GetEventBus();

// Or via helpers
USuspenseCoreEventBus* EventBus = USuspenseCoreHelpers::GetEventBus(WorldContextObject);
```

---

### 4. USuspenseCoreEventBus

**Location:** `BridgeSystem/Public/SuspenseCore/Events/SuspenseCoreEventBus.h`

Publish/Subscribe event system using GameplayTags.

```cpp
// Publishing
FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(Source);
EventData.SetString(TEXT("ItemID"), ItemID.ToString());
EventBus->Publish(EventTag, EventData);

// Subscribing
FDelegateHandle Handle = EventBus->Subscribe(EventTag,
    FOnSuspenseCoreEvent::CreateUObject(this, &MyClass::OnEvent));

// Unsubscribing
EventBus->Unsubscribe(EventTag, Handle);
```

---

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                    PROJECT SETTINGS                                  │
│                 (Editor Configuration)                               │
├─────────────────────────────────────────────────────────────────────┤
│  USuspenseCoreSettings (UDeveloperSettings)                         │
│  ├── ItemDataTable                                                  │
│  ├── CharacterClassesDataAsset                                      │
│  ├── LoadoutDataTable                                               │
│  └── WeaponAnimationsTable                                          │
└────────────────────────────┬────────────────────────────────────────┘
                             │ Loads at Init
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│              GAMEINSTANCE SUBSYSTEMS                                 │
├─────────────────────────────────────────────────────────────────────┤
│  USuspenseCoreDataManager         USuspenseCoreEventManager         │
│  ├── ItemCache                    └── EventBus                      │
│  ├── GetItemData()                    ├── Publish()                 │
│  ├── CreateItemInstance()             ├── Subscribe()               │
│  └── ValidateItem()                   └── Unsubscribe()             │
└────────────────────────────┬────────────────────────────────────────┘
                             │ Events via GameplayTags
                             ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    GAME SYSTEMS                                      │
├─────────────────────────────────────────────────────────────────────┤
│  Pickups    │  Inventory  │  Equipment  │  GAS Abilities           │
│  ─────────  │  ──────────  │  ──────────  │  ──────────────          │
│  Broadcast  │  Subscribe   │  Subscribe   │  Broadcast              │
│  events     │  to events   │  to events   │  ability events         │
└─────────────────────────────────────────────────────────────────────┘
```

---

## GameplayTag Naming Convention

All SuspenseCore events follow the pattern:
```
SuspenseCore.Event.{System}.{Action}
```

Examples:
- `SuspenseCore.Event.Data.Initialized`
- `SuspenseCore.Event.Pickup.Collected`
- `SuspenseCore.Event.Inventory.ItemAdded`
- `SuspenseCore.Event.Ability.CharacterJump.Activated`

---

## Best Practices

### DO:
1. **Use DataManager for all item data access**
   ```cpp
   USuspenseCoreDataManager* DataManager = USuspenseCoreHelpers::GetDataManager(this);
   DataManager->GetItemData(ItemID, OutData);
   ```

2. **Configure all DataTables in Project Settings**
   - Never hardcode DataTable paths
   - Never configure in Blueprint GameInstance

3. **Broadcast events through EventBus**
   ```cpp
   FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
   EventData.SetString(TEXT("Key"), Value);
   GetEventBus()->Publish(EventTag, EventData);
   ```

4. **Use SetAssetTags() for GAS abilities**
   ```cpp
   FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Movement.Jump"));
   SetAssetTags(FGameplayTagContainer(Tag));
   ```

5. **Register all GameplayTags in DefaultGameplayTags.ini**

### DON'T:
1. **Don't use legacy ItemManager directly**
   ```cpp
   // DEPRECATED - Don't do this:
   USuspenseItemManager* ItemManager = USuspenseCoreHelpers::GetItemManager(this);

   // DO THIS INSTEAD:
   USuspenseCoreDataManager* DataManager = USuspenseCoreHelpers::GetDataManager(this);
   ```

2. **Don't configure data in Blueprint GameInstance**
   - All configuration goes in USuspenseCoreSettings

3. **Don't use AbilityTags.AddTag() (deprecated)**
   ```cpp
   // DEPRECATED:
   AbilityTags.AddTag(Tag);

   // USE:
   SetAssetTags(FGameplayTagContainer(Tag));
   ```

---

## Migration from Legacy Code

### Before (Legacy):
```cpp
// Getting item data
USuspenseItemManager* ItemManager = GameInstance->GetSubsystem<USuspenseItemManager>();
ItemManager->GetUnifiedItemData(ItemID, OutData);

// Configuration in Blueprint GameInstance
UPROPERTY(EditDefaultsOnly)
UDataTable* ItemDataTable;
```

### After (SuspenseCore):
```cpp
// Getting item data
USuspenseCoreDataManager* DataManager = USuspenseCoreHelpers::GetDataManager(this);
DataManager->GetItemData(ItemID, OutData);

// Configuration in Project Settings → Game → SuspenseCore
// No Blueprint configuration needed!
```

---

## Adding New Data Types

To add a new data type (e.g., Quests):

1. **Add to USuspenseCoreSettings:**
   ```cpp
   UPROPERTY(Config, EditAnywhere, Category = "Quest System")
   TSoftObjectPtr<UDataTable> QuestDataTable;
   ```

2. **Add loading in USuspenseCoreDataManager::Initialize():**
   ```cpp
   bool InitializeQuestSystem();
   ```

3. **Add GameplayTags in DefaultGameplayTags.ini:**
   ```ini
   +GameplayTagList=(Tag="SuspenseCore.Event.Quest.Started",DevComment="Quest started")
   +GameplayTagList=(Tag="SuspenseCore.Event.Quest.Completed",DevComment="Quest completed")
   ```

4. **Add access methods:**
   ```cpp
   bool GetQuestData(FName QuestID, FQuestData& OutData) const;
   ```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-12 | Initial SuspenseCore architecture |
| 1.1 | 2025-12 | Added USuspenseCoreSettings, USuspenseCoreDataManager |

---

## Related Files

- `Config/DefaultGameplayTags.ini` - All GameplayTag definitions
- `BridgeSystem/Public/SuspenseCore/Settings/SuspenseCoreSettings.h`
- `BridgeSystem/Public/SuspenseCore/Data/SuspenseCoreDataManager.h`
- `BridgeSystem/Public/SuspenseCore/Events/SuspenseCoreEventManager.h`
- `BridgeSystem/Public/SuspenseCore/Events/SuspenseCoreEventBus.h`
- `InteractionSystem/Public/SuspenseCore/Utils/SuspenseCoreHelpers.h`
