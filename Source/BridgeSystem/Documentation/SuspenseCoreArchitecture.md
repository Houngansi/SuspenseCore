# SuspenseCore Architecture Guide

## Overview

SuspenseCore is an EventBus-driven architecture for Unreal Engine that provides:
- Decoupled systems communication via GameplayTags
- Centralized data management
- Project Settings configuration (no Blueprint GameInstance required)
- Full GAS (Gameplay Ability System) integration
- **Dependency Injection via ServiceProvider** (NEW)
- **MMO-scale Replication Graph for 100+ players** (NEW)
- **Centralized Security Validator with AAA-standard anti-cheat** (NEW)

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

### 5. USuspenseCoreServiceProvider (GameInstanceSubsystem) — NEW

**Location:** `BridgeSystem/Public/SuspenseCore/Services/SuspenseCoreServiceProvider.h`

Centralized Dependency Injection for all SuspenseCore services.

```cpp
// Static Access (recommended)
USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(WorldContextObject);

// Service Access
USuspenseCoreEventBus* EventBus = Provider->GetEventBus();
USuspenseCoreDataManager* DataManager = Provider->GetDataManager();
USuspenseCoreEventManager* EventManager = Provider->GetEventManager();

// Generic Service Access
template<typename T>
T* Provider->GetService() const;

// Using Macros (convenience)
SUSPENSE_GET_EVENTBUS(WorldContextObject, EventBus);
SUSPENSE_GET_DATAMANAGER(WorldContextObject, DataManager);
SUSPENSE_PUBLISH_EVENT(WorldContextObject, EventTag, EventData);
```

**Lifecycle:**
1. GameInstance creates subsystem
2. `Initialize()` initializes core services in correct order
3. Broadcasts `SuspenseCore.Event.Services.Initialized`
4. Provides service access throughout session

**EventBus Events:**
| Tag | Description |
|-----|-------------|
| `SuspenseCore.Event.Services.Initialized` | ServiceProvider ready |
| `SuspenseCore.Event.Services.ServiceRegistered` | New service registered |
| `SuspenseCore.Event.Services.ServiceMissing` | Required service not found |

---

### 6. USuspenseCoreReplicationGraph — NEW

**Location:** `BridgeSystem/Public/SuspenseCore/Replication/SuspenseCoreReplicationGraph.h`

Custom Replication Graph for MMO-scale networking (100+ concurrent players).

```cpp
// Enable in DefaultEngine.ini:
[/Script/OnlineSubsystemUtils.IpNetDriver]
ReplicationDriverClassName="/Script/BridgeSystem.SuspenseCoreReplicationGraph"

// Configuration in Project Settings → Game → SuspenseCore Replication
```

**Node Architecture:**
| Node | Purpose | Bandwidth Reduction |
|------|---------|-------------------|
| AlwaysRelevantNode | GameState, GameMode | N/A |
| PlayerStateFrequencyNode | Distance-based PlayerState | ~80% |
| SpatialGridNode | Characters, Pickups | Distance-based |
| EquipmentDormancyNode | Equipment with dormancy | ~80% |
| OwnerOnlyNode | Inventory (per-connection) | ~99% |

**Configuration (USuspenseCoreReplicationGraphSettings):**
```cpp
// Spatial Grid
float SpatialGridCellSize = 10000.0f;    // 100m cells
float SpatialGridExtent = 500000.0f;     // 5km world radius

// Cull Distances
float CharacterCullDistance = 15000.0f;  // 150m
float PickupCullDistance = 5000.0f;      // 50m
float ProjectileCullDistance = 20000.0f; // 200m

// Frequency Buckets
float NearDistanceThreshold = 2000.0f;   // 20m
float MidDistanceThreshold = 5000.0f;    // 50m
float FarDistanceThreshold = 10000.0f;   // 100m
int32 NearReplicationPeriod = 1;         // Every frame
int32 FarReplicationPeriod = 5;          // Every 5th frame

// Dormancy
float EquipmentDormancyTimeout = 5.0f;   // 5 seconds
```

**EventBus Events:**
| Tag | Description |
|-----|-------------|
| `SuspenseCore.Event.Replication.Initialized` | ReplicationGraph ready |
| `SuspenseCore.Event.Replication.ActorAdded` | Actor added to replication |
| `SuspenseCore.Event.Replication.DormancyChanged` | Dormancy state changed |

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

### Extended Architecture (with MMO Scalability)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         PROJECT SETTINGS                                     │
├───────────────────────────────────┬─────────────────────────────────────────┤
│  USuspenseCoreSettings            │  USuspenseCoreReplicationGraphSettings  │
│  ├── ItemDataTable                │  ├── SpatialGridCellSize               │
│  ├── CharacterClassesDataAsset    │  ├── CullDistances                     │
│  └── LoadoutDataTable             │  └── DormancyTimeouts                  │
└───────────────────────────────────┴─────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                 USuspenseCoreServiceProvider (DI Hub)                        │
├─────────────────────────────────────────────────────────────────────────────┤
│  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐                 │
│  │ DataManager    │  │ EventManager   │  │ EventBus       │                 │
│  │ GetItemData()  │  │ GetEventBus()  │  │ Publish()      │                 │
│  └────────────────┘  └────────────────┘  └────────────────┘                 │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                 USuspenseCoreReplicationGraph (Network)                      │
├─────────────────────────────────────────────────────────────────────────────┤
│  AlwaysRelevant → PlayerStateFrequency → SpatialGrid → Dormancy → OwnerOnly │
│  (GameState)      (distance buckets)    (Characters)   (Equipment) (Inventory)│
└─────────────────────────────────────────────────────────────────────────────┘
```

### Bandwidth Optimization Impact (100 Players)

| Feature | Without Optimization | With Optimization | Reduction |
|---------|---------------------|-------------------|-----------|
| PlayerState | 9,900/frame | ~50/frame | 99.5% |
| Inventory | 495,000/frame | 5,000/frame | 99% |
| Equipment | 36,000/sec | ~7,300/sec | 80% |

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
| 1.0 | 2025-12-01 | Initial SuspenseCore architecture |
| 1.1 | 2025-12-02 | Added USuspenseCoreSettings, USuspenseCoreDataManager |
| 2.0 | 2025-12-05 | **MMO Scalability Update**: ServiceProvider DI, ReplicationGraph |
| 2.1 | 2025-12-05 | **Security Update (Phase 6)**: SecurityValidator, Server RPCs, Rate Limiting |

---

## Related Files

### Core Services
- `Config/DefaultGameplayTags.ini` - All GameplayTag definitions
- `BridgeSystem/Public/SuspenseCore/Settings/SuspenseCoreSettings.h`
- `BridgeSystem/Public/SuspenseCore/Data/SuspenseCoreDataManager.h`
- `BridgeSystem/Public/SuspenseCore/Events/SuspenseCoreEventManager.h`
- `BridgeSystem/Public/SuspenseCore/Events/SuspenseCoreEventBus.h`
- `InteractionSystem/Public/SuspenseCore/Utils/SuspenseCoreHelpers.h`

### ServiceProvider (DI System) — NEW
- `BridgeSystem/Public/SuspenseCore/Services/SuspenseCoreServiceProvider.h`
- `BridgeSystem/Public/SuspenseCore/Services/SuspenseCoreServiceInterfaces.h`
- `BridgeSystem/Public/SuspenseCore/Services/SuspenseCoreServiceMacros.h`

### ReplicationGraph (MMO Scalability) — NEW
- `BridgeSystem/Public/SuspenseCore/Replication/SuspenseCoreReplicationGraph.h`
- `BridgeSystem/Public/SuspenseCore/Replication/SuspenseCoreReplicationGraphSettings.h`
- `BridgeSystem/Public/SuspenseCore/Replication/Nodes/SuspenseCoreRepNode_AlwaysRelevant.h`
- `BridgeSystem/Public/SuspenseCore/Replication/Nodes/SuspenseCoreRepNode_PlayerStateFrequency.h`
- `BridgeSystem/Public/SuspenseCore/Replication/Nodes/SuspenseCoreRepNode_OwnerOnly.h`
- `BridgeSystem/Public/SuspenseCore/Replication/Nodes/SuspenseCoreRepNode_Dormancy.h`
- `BridgeSystem/Public/SuspenseCore/Replication/README_ReplicationGraph.md`

### Security Validator (Phase 6) — NEW
- `BridgeSystem/Public/SuspenseCore/Security/SuspenseCoreSecurityValidator.h`
- `BridgeSystem/Public/SuspenseCore/Security/SuspenseCoreSecurityTypes.h`
- `BridgeSystem/Public/SuspenseCore/Security/SuspenseCoreSecurityMacros.h`
- `BridgeSystem/Private/SuspenseCore/Security/SuspenseCoreSecurityValidator.cpp`

---

## Security Architecture (Phase 6)

### USuspenseCoreSecurityValidator (GameInstanceSubsystem)

**Location:** `BridgeSystem/Public/SuspenseCore/Security/SuspenseCoreSecurityValidator.h`

Centralized security validation for all SuspenseCore operations.

```cpp
// C++ Access
USuspenseCoreSecurityValidator* Security = USuspenseCoreSecurityValidator::Get(WorldContextObject);

// Authority Check
if (!Security->CheckAuthority(GetOwner(), TEXT("AddItem")))
{
    // Client has no authority - redirect to Server RPC
    return;
}

// Rate Limiting
if (!Security->CheckRateLimit(GetOwner(), TEXT("AddItem"), 10.0f))
{
    // Rate limited - operation blocked
    return;
}
```

**Using Macros:**
```cpp
#include "SuspenseCore/Security/SuspenseCoreSecurityMacros.h"

bool UMyComponent::DoOperation()
{
    SUSPENSE_CHECK_COMPONENT_AUTHORITY(this, DoOperation);
    SUSPENSE_CHECK_RATE_LIMIT(GetOwner(), DoOperation, 10.0f);

    // Rest of implementation (only runs on server)
    return true;
}
```

**Security Features:**
| Feature | Description |
|---------|-------------|
| Authority Check | Blocks client-side execution of server operations |
| Rate Limiting | Prevents spam/DoS (configurable per-operation) |
| RPC Validation | Validates all Server RPC parameters |
| Suspicious Activity | Detects abnormal patterns |
| Violation Logging | Records all security violations |
| Auto-kick | Kicks players after max violations |

**EventBus Events:**
| Tag | Description |
|-----|-------------|
| `SuspenseCore.Event.Security.ViolationDetected` | Security violation detected |
| `SuspenseCore.Event.Security.RateLimitExceeded` | Rate limit exceeded |
| `SuspenseCore.Event.Security.SuspiciousActivity` | Suspicious pattern detected |
| `SuspenseCore.Event.Security.AuthorityDenied` | Authority check failed |
| `SuspenseCore.Event.Security.PlayerKicked` | Player kicked for violations |

### Server RPC Pattern

All modifying operations in SuspenseCore follow this pattern:

```cpp
// 1. Public API - checks authority, redirects to RPC if client
bool UInventoryComponent::AddItem(FName ItemID, int32 Quantity)
{
    if (!CheckInventoryAuthority(TEXT("AddItem")))
    {
        Server_AddItem(ItemID, Quantity);
        return false; // Client returns false
    }

    return AddItemInternal(ItemID, Quantity);
}

// 2. Server RPC with validation
UFUNCTION(Server, Reliable, WithValidation)
void Server_AddItem(FName ItemID, int32 Quantity);

bool Server_AddItem_Validate(FName ItemID, int32 Quantity)
{
    // Parameter validation + rate limiting
    return SUSPENSE_VALIDATE_ITEM_RPC(ItemID, Quantity);
}

void Server_AddItem_Implementation(FName ItemID, int32 Quantity)
{
    AddItemInternal(ItemID, Quantity);
}
```
