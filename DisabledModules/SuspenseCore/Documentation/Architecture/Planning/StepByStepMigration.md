# Step-by-Step Migration Guide

**Document Version:** 1.0
**Created:** 2025-11-24
**For:** SuspenseCore Legacy Code Migration
**Target Platform:** Unreal Engine 5.7+

---

## Table of Contents

1. [Prerequisites](#1-prerequisites)
2. [General Migration Procedure](#2-general-migration-procedure)
3. [Wave 1: MedComShared Migration](#3-wave-1-medcomshared-migration)
4. [Wave 2: Core Systems Migration](#4-wave-2-core-systems-migration)
5. [Wave 3: MedComInventory Migration](#5-wave-3-medcominventory-migration)
6. [Wave 4: MedComEquipment Migration](#6-wave-4-medcomequipment-migration)
7. [Wave 5: MedComUI Migration](#7-wave-5-medcomui-migration)
8. [Automation Scripts](#8-automation-scripts)
9. [Troubleshooting Guide](#9-troubleshooting-guide)

---

## 1. Prerequisites

### 1.1 Required Setup

**Before starting ANY migration work:**

- [ ] **Version Control Clean State**
  ```bash
  cd /home/user/SuspenseCore
  git status
  # Ensure: "nothing to commit, working tree clean"
  ```

- [ ] **Backup Current State**
  ```bash
  # Tag current state
  git tag -a pre-migration-baseline -m "Before migration: All legacy code baseline"
  git push origin pre-migration-baseline

  # Create backup branch
  git branch backup/pre-migration
  git push origin backup/pre-migration
  ```

- [ ] **Testing Environment Ready**
  - [ ] Project compiles successfully (Ctrl+F5 in Visual Studio)
  - [ ] All existing tests pass
  - [ ] Editor launches without errors
  - [ ] Test map loads correctly

- [ ] **Documentation Ready**
  - [ ] Read: SuspenseNamingConvention.md
  - [ ] Read: MigrationPipeline.md
  - [ ] Read: ProjectSWOT.md
  - [ ] Have this guide open

- [ ] **Tools Installed**
  - [ ] Visual Studio 2022 (or Rider)
  - [ ] Unreal Engine 5.7
  - [ ] Git + Git LFS
  - [ ] Text editor with regex support (VSCode, Notepad++, etc.)

### 1.2 Create Migration Tracking Spreadsheet

**Create a spreadsheet to track progress:**

| Class Name | Legacy Name | New Name | Status | Files Changed | Tests Passing | Notes |
|------------|-------------|----------|--------|---------------|---------------|-------|
| Interface 1 | IMedComInventoryInterface | ISuspenseInventory | ✅ Done | 45 | ✅ | - |
| Component 1 | UMedComInventoryComponent | USuspenseInventoryComponent | 🔄 In Progress | 12/50 | ⚠️ | BP issues |
| ... | ... | ... | ... | ... | ... | ... |

### 1.3 Performance Baseline

**Measure current performance BEFORE migration:**

```bash
# In Unreal Editor Console
stat fps
stat unit
stat memory
stat game

# Record baseline:
# - FPS: _____
# - Frame time: _____ ms
# - Memory: _____ MB
# - Inventory operations: _____ ms
```

**Save these numbers** - you'll compare after migration to ensure no regression.

---

## 2. General Migration Procedure

### 2.1 Standard Workflow for Each Class

**Follow this procedure for EVERY class migration:**

#### Step 1: Preparation (5-10 minutes)

```bash
# 1. Create feature branch
git checkout -b migrate/[module-name]-[class-name]

# Example:
git checkout -b migrate/inventory-component

# 2. Find all references
grep -r "UMedComInventoryComponent" Source/
# Note the count - you'll verify this many changes later
```

#### Step 2: Header File Rename (10-30 minutes)

**File:** `Source/InventorySystem/MedComInventory/Public/Components/MedComInventoryComponent.h`

**Actions:**
```cpp
// 1. Update class name
// OLD:
class MEDCOMINVENTORY_API UMedComInventoryComponent : public UActorComponent

// NEW:
class SUSPENSECORE_API USuspenseInventoryComponent : public UActorComponent

// 2. Update API macro (if module changed)
// Check .Build.cs for correct macro

// 3. Update constructor
// OLD:
UMedComInventoryComponent();

// NEW:
USuspenseInventoryComponent();

// 4. Update UCLASS meta
UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent, DisplayName = "Inventory Component"))
// Add DisplayName to maintain Blueprint compatibility

// 5. Update all member functions
// OLD:
void UMedComInventoryComponent::Initialize();

// NEW:
void USuspenseInventoryComponent::Initialize();

// 6. Update all includes that reference this class
// Update forward declarations
class USuspenseInventoryStorage;  // Was: class UMedComInventoryStorage;
```

#### Step 3: Source File Rename (10-30 minutes)

**File:** `Source/InventorySystem/MedComInventory/Private/Components/MedComInventoryComponent.cpp`

**Actions:**
```cpp
// 1. Update include path
// OLD:
#include "Components/MedComInventoryComponent.h"

// NEW:
#include "Inventory/Components/SuspenseInventoryComponent.h"

// 2. Update all function implementations
// OLD:
UMedComInventoryComponent::UMedComInventoryComponent()
{
    // ...
}

// NEW:
USuspenseInventoryComponent::USuspenseInventoryComponent()
{
    // ...
}

// 3. Update all method definitions
// Use Find & Replace (Ctrl+H):
// Find: UMedComInventoryComponent::
// Replace: USuspenseInventoryComponent::
```

#### Step 4: Update All References (30-120 minutes)

**Automated Find & Replace:**

```bash
# In VSCode/Visual Studio:
# 1. Find in Files (Ctrl+Shift+F)
# Search: UMedComInventoryComponent
# Replace: USuspenseInventoryComponent

# 2. Review EACH file before replacing
# Some may need manual fixes

# 3. Common locations:
# - Other component headers
# - Blueprint libraries
# - Character classes
# - UI widgets
# - Build.cs files
```

#### Step 5: Rename Physical Files (5 minutes)

```bash
# In file explorer or terminal:

# Header file
mv Source/InventorySystem/MedComInventory/Public/Components/MedComInventoryComponent.h \
   Source/InventorySystem/MedComInventory/Public/Components/SuspenseInventoryComponent.h

# Source file
mv Source/InventorySystem/MedComInventory/Private/Components/MedComInventoryComponent.cpp \
   Source/InventorySystem/MedComInventory/Private/Components/SuspenseInventoryComponent.cpp
```

#### Step 6: Update Build Configuration (5 minutes)

**File:** `Source/InventorySystem/InventorySystem.Build.cs`

```csharp
// Update module dependencies if module name changed

PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "SuspenseCore",  // Was: "MedComShared"
    // ...
});
```

#### Step 7: Compile & Test (10-30 minutes)

```bash
# 1. Generate project files
# Right-click .uproject → "Generate Visual Studio project files"

# 2. Compile in Visual Studio
# Build → Build Solution (Ctrl+Shift+B)

# 3. Fix compilation errors
# - Update any missed references
# - Fix include paths
# - Update forward declarations

# 4. Launch editor
# - Verify no errors in Output Log
# - Check Blueprint classes still exist
# - Test basic functionality
```

#### Step 8: Run Tests (15-30 minutes)

```bash
# Unit tests (if implemented)
# In Editor: Session Frontend → Automation → Run Tests

# Manual tests:
# - Load test map
# - Spawn test actor with component
# - Verify basic operations work
# - Check replication (if applicable)
```

#### Step 9: Commit Changes (5 minutes)

```bash
git add .
git commit -m "Migrate UMedComInventoryComponent → USuspenseInventoryComponent

- Renamed class and all references
- Updated include paths
- Renamed physical files
- Tests passing: [✅/⚠️/❌]
- Performance: [no regression/+X%/-X%]

References updated: [COUNT] files
"
git push origin migrate/inventory-component
```

#### Step 10: Validation Checklist

- [ ] Project compiles without errors
- [ ] No warnings about deprecated classes
- [ ] Blueprint classes compile
- [ ] Editor launches successfully
- [ ] Test map loads
- [ ] Component appears in editor
- [ ] Basic functionality works
- [ ] Network replication works (if applicable)
- [ ] Performance unchanged
- [ ] All tests passing

---

## 3. Wave 1: MedComShared Migration

**Duration:** 4 weeks
**Complexity:** HIGH (foundation for all other modules)
**Dependencies:** None (must be done first)

### 3.1 Prerequisites

```bash
# Create Wave 1 branch
git checkout -b migrate/wave1-shared-foundation

# Verify starting point
cd Source/BridgeSystem/MedComShared
ls -la
# Should see: 86 headers, 37 source files
```

### 3.2 Phase 1: Interfaces Migration

#### Step 1.1: Migrate Core Inventory Interface

**Time Estimate:** 2 hours

**File:** `Source/BridgeSystem/MedComShared/Public/Interfaces/Inventory/MedComInventoryInterface.h`

**Procedure:**

1. **Open header file**

2. **Update UINTERFACE class:**
```cpp
// OLD:
UINTERFACE(MinimalAPI, Blueprintable)
class UMedComInventoryInterface : public UInterface
{
    GENERATED_BODY()
};

// NEW:
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseInventory : public UInterface
{
    GENERATED_BODY()
};
```

3. **Update implementation interface:**
```cpp
// OLD:
class IMedComInventoryInterface
{
    GENERATED_BODY()

public:
    // Interface methods...
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Inventory")
    bool AddItem(const FMedComUnifiedItemData& ItemData, int32 Quantity);
};

// NEW:
class ISuspenseInventory
{
    GENERATED_BODY()

public:
    // Interface methods...
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "SuspenseCore|Inventory")
    bool AddItem(const FSuspenseItemData& ItemData, int32 Quantity);

    // Note: Renamed FMedComUnifiedItemData → FSuspenseItemData
    // This will be done in Phase 2
};
```

4. **Rename file:**
```bash
cd Source/BridgeSystem/MedComShared/Public/Interfaces/Inventory/

# Rename header
mv MedComInventoryInterface.h SuspenseInventoryInterface.h

# Update include guards if present
# Find: MEDCOMINVENTORYINTERFACE_H
# Replace: SUSPENSEINVENTORYINTERFACE_H
```

5. **Update all references:**
```bash
# Find all files that include this interface
grep -r "MedComInventoryInterface.h" Source/

# Update each file:
# OLD: #include "Interfaces/Inventory/MedComInventoryInterface.h"
# NEW: #include "Interfaces/Inventory/SuspenseInventoryInterface.h"
```

6. **Find all implementations:**
```bash
# Find all classes that implement this interface
grep -r "IMedComInventoryInterface" Source/

# Update class declarations:
# OLD: class UMedComInventoryComponent : public UActorComponent, public IMedComInventoryInterface
# NEW: class USuspenseInventoryComponent : public UActorComponent, public ISuspenseInventory
```

7. **Update virtual function implementations:**
```cpp
// In implementation files (.cpp)

// OLD:
bool UMedComInventoryComponent::AddItem_Implementation(
    const FMedComUnifiedItemData& ItemData, int32 Quantity)
{
    // ...
}

// NEW:
bool USuspenseInventoryComponent::AddItem_Implementation(
    const FSuspenseItemData& ItemData, int32 Quantity)
{
    // ...
}
```

8. **Compile & Test:**
```bash
# Regenerate project files
# Build solution
# Check for errors
# Fix any missed references
```

9. **Validation:**
- [ ] Interface compiles
- [ ] All implementations compile
- [ ] Blueprint classes implementing interface still work
- [ ] No "unresolved external symbol" errors

**Repeat for all 60 interfaces** (estimated 2 days total for all interfaces)

---

### 3.3 Phase 2: Types Migration

#### Step 2.1: Migrate FInventoryItemInstance (CRITICAL)

**Time Estimate:** 4 hours

**File:** `Source/BridgeSystem/MedComShared/Public/Types/Inventory/InventoryItemInstance.h`

**This is CRITICAL - used 500+ times across codebase**

**Decision Point:**
- **Option A:** Rename completely (clean break, more work)
- **Option B:** Keep old name and add alias (backward compatibility)

**Recommended: Option B (with migration plan)**

**Procedure:**

1. **Update struct definition:**
```cpp
// In InventoryItemInstance.h

USTRUCT(BlueprintType)
struct SUSPENSECORE_API FSuspenseItemInstance
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Item")
    FName ItemID;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Item")
    int32 Amount = 1;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Item")
    TMap<FName, float> RuntimeProperties;

    // ... rest of struct
};

// Backward compatibility alias (temporary - remove in 6 months)
using FInventoryItemInstance = FSuspenseItemInstance;
```

2. **Update all includes:**
```bash
# If moving file location:
# OLD: #include "Types/Inventory/InventoryItemInstance.h"
# NEW: #include "Items/SuspenseItemInstance.h"
```

3. **Gradual migration strategy:**
```cpp
// Phase 1: Add new name, keep old as alias (NOW)
using FInventoryItemInstance = FSuspenseItemInstance;

// Phase 2: Mark old as deprecated (after 2 weeks)
UE_DEPRECATED(5.7, "Use FSuspenseItemInstance instead")
using FInventoryItemInstance = FSuspenseItemInstance;

// Phase 3: Remove alias completely (after all code updated)
// Remove the using statement
```

4. **Create migration helper script:**
```bash
#!/bin/bash
# migrate-item-instance.sh

# Find all usages
echo "Finding all FInventoryItemInstance usages..."
grep -r "FInventoryItemInstance" Source/ --include="*.h" --include="*.cpp" > item-instance-refs.txt

# Count total
TOTAL=$(wc -l < item-instance-refs.txt)
echo "Total references: $TOTAL"

# Gradually replace
# (Do this manually or semi-automated with review)
```

5. **Validation:**
- [ ] Struct replicates correctly
- [ ] Save/load works
- [ ] Blueprint access maintained
- [ ] Network serialization intact

---

#### Step 2.2: Migrate All Other Structs

**For each struct:**

1. **Update struct name:**
```cpp
// Example: FInventoryCell → FSuspenseInventoryCell

USTRUCT(BlueprintType)
struct SUSPENSECORE_API FSuspenseInventoryCell
{
    GENERATED_BODY()

    UPROPERTY()
    FIntPoint Position;

    UPROPERTY()
    int32 ItemIndex = INDEX_NONE;

    // ...
};
```

2. **Update all references:**
```bash
grep -r "FInventoryCell" Source/
# Update each file
```

3. **Update serialization version (if struct is saved):**
```cpp
// In serializer class
UPROPERTY()
int32 Version = 3;  // Bump from 2 → 3 for SuspenseCore

// Migration code
if (Data.Version == 2)
{
    // Convert old struct names to new
    Data.Version = 3;
}
```

**Estimated Time for All Structs:** 1 week (196 structs total)

---

### 3.4 Phase 3: Core Services Migration

#### Step 3.1: Migrate UMedComItemManager → USuspenseCoreItemManager

**Time Estimate:** 6 hours

**File:** `Source/BridgeSystem/MedComShared/Public/ItemSystem/MedComItemManager.h`

**Procedure:**

1. **Update class declaration:**
```cpp
// OLD:
UCLASS()
class MEDCOMSHARED_API UMedComItemManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UMedComItemManager();

    // Methods...
};

// NEW:
UCLASS()
class SUSPENSECORE_API USuspenseCoreItemManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    USuspenseCoreItemManager();

    // Methods... (same signatures)
};
```

2. **Update source file:**
```cpp
// File: MedComItemManager.cpp → SuspenseItemManager.cpp

#include "ItemSystem/SuspenseItemManager.h"

USuspenseCoreItemManager::USuspenseCoreItemManager()
{
    // Implementation stays the same
}

// Update all method implementations
bool USuspenseCoreItemManager::GetUnifiedItemData(FName ItemID, FSuspenseItemData& OutData) const
{
    // ...
}
```

3. **Update all GetSubsystem calls:**
```cpp
// Find all usages:
grep -r "UMedComItemManager::Get" Source/
grep -r "GetGameInstance()->GetSubsystem<UMedComItemManager>" Source/

// OLD:
UMedComItemManager* ItemMgr = GetWorld()->GetGameInstance()->GetSubsystem<UMedComItemManager>();

// NEW:
USuspenseCoreItemManager* ItemMgr = GetWorld()->GetGameInstance()->GetSubsystem<USuspenseCoreItemManager>();
```

4. **Update cached references:**
```cpp
// In classes that cache the manager:

// OLD:
UPROPERTY(Transient)
TWeakObjectPtr<UMedComItemManager> CachedItemManager;

// NEW:
UPROPERTY(Transient)
TWeakObjectPtr<USuspenseCoreItemManager> CachedItemManager;
```

5. **Validation:**
- [ ] ItemManager initializes on game start
- [ ] DataTable loads correctly
- [ ] Item lookup works
- [ ] Cache invalidation works
- [ ] No performance regression

---

#### Step 3.2: Migrate EventDelegateManager

**Similar procedure to ItemManager**

**Additional considerations:**
- Many delegates registered at runtime
- Verify all event names still work
- Check multicast delegate bindings
- Test event broadcasting

---

### 3.5 Wave 1 Final Steps

#### Integration Testing

```bash
# Full compilation test
Build → Rebuild Solution

# Editor launch test
Launch editor, check for errors

# Runtime test
PIE (Play In Editor)
- Spawn test character
- Verify item manager working
- Test event system
- Check interfaces functioning
```

#### Performance Validation

```bash
# In Editor Console
stat fps
stat unit
stat memory

# Compare to baseline
# Should be within 2% of baseline
```

#### Commit Wave 1

```bash
git add .
git commit -m "Complete Wave 1: MedComShared → SuspenseCore foundation

Migration Summary:
- 60 interfaces migrated
- 196 structs migrated
- 7 core service classes migrated
- All includes updated
- All references updated

Testing:
✅ Compilation successful
✅ All interfaces functional
✅ DataTables loading correctly
✅ Event system working
✅ Performance: no regression

Files changed: [COUNT]
LOC modified: ~26,680
"

git push origin migrate/wave1-shared-foundation

# Create tag
git tag -a wave1-complete -m "Wave 1 (MedComShared) migration complete"
git push origin wave1-complete
```

---

## 4. Wave 2: Core Systems Migration

### 4.1 MedComGAS Migration

**Duration:** 1 week
**Dependencies:** Wave 1 complete

#### Step 1: Create Branch

```bash
git checkout -b migrate/wave2-gas
```

#### Step 2: Migrate Abilities (2 days)

**For each ability class:**

1. **Update class name:**
```cpp
// Example: UMedComGameplayAbility_Inventory → USuspenseAbilityInventory

// OLD:
UCLASS()
class MEDCOMGAS_API UMedComGameplayAbility_Inventory : public UGameplayAbility
{
    GENERATED_BODY()
};

// NEW:
UCLASS()
class SUSPENSECORE_API USuspenseAbilityInventory : public UGameplayAbility
{
    GENERATED_BODY()
};
```

2. **Update ability granting:**
```cpp
// OLD:
ASC->GiveAbility(FGameplayAbilitySpec(UMedComGameplayAbility_Inventory::StaticClass(), 1));

// NEW:
ASC->GiveAbility(FGameplayAbilitySpec(USuspenseAbilityInventory::StaticClass(), 1));
```

3. **Update GameplayTags:**
```cpp
// Verify tags still reference correct abilities
// Check DefaultGameplayTags.ini or C++ tag definitions
```

#### Step 3: Migrate Attributes (1 day)

**For each attribute set:**

```cpp
// Example: UMedComHealthAttributeSet → USuspenseHealthAttributeSet

UCLASS()
class SUSPENSECORE_API USuspenseHealthAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes")
    FGameplayAttributeData Health;

    ATTRIBUTE_ACCESSORS(USuspenseHealthAttributeSet, Health)

    UFUNCTION()
    virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);

protected:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
```

**Important:** Update all GetAttributeSet calls:
```cpp
// OLD:
const UMedComHealthAttributeSet* HealthSet = ASC->GetSet<UMedComHealthAttributeSet>();

// NEW:
const USuspenseHealthAttributeSet* HealthSet = ASC->GetSet<USuspenseHealthAttributeSet>();
```

#### Step 4: Migrate Effects (2 days)

**For each gameplay effect:**

```cpp
// Example: UGE_MedCom_HealthRegen → UGE_Suspense_HealthRegen

UCLASS()
class SUSPENSECORE_API UGE_Suspense_HealthRegen : public UGameplayEffect
{
    GENERATED_BODY()

public:
    UGE_Suspense_HealthRegen();
};
```

**Update effect applications:**
```cpp
// OLD:
ASC->ApplyGameplayEffectToSelf(
    UGE_MedCom_HealthRegen::StaticClass()->GetDefaultObject<UGameplayEffect>(),
    1.0f,
    ASC->MakeEffectContext()
);

// NEW:
ASC->ApplyGameplayEffectToSelf(
    UGE_Suspense_HealthRegen::StaticClass()->GetDefaultObject<UGameplayEffect>(),
    1.0f,
    ASC->MakeEffectContext()
);
```

---

### 4.2 MedComCore Migration

**Similar procedure - 1 week**

**Key classes:**
- `AMedComCharacter` → `ASuspenseCharacter`
- `AMedComPlayerCharacter` → `ASuspensePlayerCharacter`
- Core components

---

### 4.3 MedComInteraction Migration

**Quickest module - 3 days**

**Key classes:**
- `UMedComInteractionComponent` → `USuspenseInteractionComponent`
- `AMedComBasePickupItem` → `ASuspensePickupItem`
- `UMedComItemFactory` → `USuspenseItemFactory`

---

## 5. Wave 3: MedComInventory Migration

**Duration:** 4 weeks
**Dependencies:** Wave 1, Wave 2

### 5.1 Create Branch

```bash
git checkout -b migrate/wave3-inventory
```

### 5.2 Phase 1: Core Component Migration

#### Step 1: Migrate Main Component (3 days)

**File:** `UMedComInventoryComponent` → `USuspenseInventoryComponent`

**This is the MOST REFERENCED class in inventory system**

**Procedure:**

1. **Find all references first:**
```bash
grep -rn "UMedComInventoryComponent" Source/ > inventory-component-refs.txt
wc -l inventory-component-refs.txt
# Expected: 200-300 references
```

2. **Update header file:**
```cpp
// File: MedComInventoryComponent.h → SuspenseInventoryComponent.h

UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent, DisplayName = "Inventory Component"))
class SUSPENSECORE_API USuspenseInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USuspenseInventoryComponent();

    // All methods stay the same (just rename class)

    // Add meta redirect for Blueprint compatibility
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory", meta = (DeprecatedFunction))
    bool AddItem_DEPRECATED(FName ItemID, int32 Amount);  // Temporary compatibility layer
};
```

3. **Update source file:**
```cpp
// File: MedComInventoryComponent.cpp → SuspenseInventoryComponent.cpp

#include "Inventory/Components/SuspenseInventoryComponent.h"
#include "Inventory/Storage/SuspenseInventoryStorage.h"
#include "Inventory/Validation/SuspenseInventoryValidator.h"
// ... all other includes

USuspenseInventoryComponent::USuspenseInventoryComponent()
{
    // Implementation unchanged
}

// Update all method implementations
bool USuspenseInventoryComponent::AddItem(FName ItemID, int32 Amount)
{
    // Implementation unchanged
}
```

4. **Update all character classes:**
```cpp
// In character header:

// OLD:
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
UMedComInventoryComponent* InventoryComponent;

// NEW:
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
USuspenseInventoryComponent* InventoryComponent;

// In character constructor:

// OLD:
InventoryComponent = CreateDefaultSubobject<UMedComInventoryComponent>(TEXT("InventoryComponent"));

// NEW:
InventoryComponent = CreateDefaultSubobject<USuspenseInventoryComponent>(TEXT("InventoryComponent"));
```

5. **Update Blueprint parent class redirects:**
```ini
# In DefaultEngine.ini or Config/DefaultEngine.ini
[/Script/Engine.Engine]
+ActiveClassRedirects=(OldClassName="MedComInventoryComponent",NewClassName="SuspenseInventoryComponent")
```

6. **Compile & Test:**
```bash
# Build solution
# Launch editor
# Check all Blueprint classes
# Verify component appears in editor
# Test PIE functionality
```

**Validation:**
- [ ] Component compiles
- [ ] Blueprints using component still work
- [ ] Component appears in editor Add Component menu
- [ ] Default values preserved
- [ ] Replication working
- [ ] All tests passing

---

### 5.3 Phase 2: Storage & Operations

**Migrate supporting classes:**
- `UMedComInventoryStorage` → `USuspenseInventoryStorage`
- `UMedComInventoryTransaction` → `USuspenseInventoryTransaction`
- `FInventoryOperation` → `FSuspenseInventoryOperation`
- All operation classes (Move, Rotate, Stack)

**Follow general procedure for each**

---

### 5.4 Phase 3: Network Migration

#### Critical: Network Replication Update

**File:** `UInventoryReplicator` → `USuspenseInventoryReplicator`

**This is CRITICAL for multiplayer**

**Procedure:**

1. **Update replication structures:**
```cpp
// OLD:
USTRUCT()
struct FReplicatedItemMeta : public FFastArraySerializerItem
{
    GENERATED_BODY()
    // ...
};

USTRUCT()
struct FReplicatedItemsMetaState : public FFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FReplicatedItemMeta> Items;

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize<FReplicatedItemMeta, FReplicatedItemsMetaState>(
            Items, DeltaParams, *this);
    }
};

// NEW:
USTRUCT()
struct FSuspenseReplicatedItemData : public FFastArraySerializerItem
{
    GENERATED_BODY()
    // Same fields
};

USTRUCT()
struct FSuspenseReplicatedItemsState : public FFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FSuspenseReplicatedItemData> Items;

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize<FSuspenseReplicatedItemData, FSuspenseReplicatedItemsState>(
            Items, DeltaParams, *this);
    }
};
```

2. **Update network protocol version:**
```cpp
// In replicator class
static constexpr uint32 NETWORK_VERSION = 3;  // Bump from 2 → 3

// Add version check
if (ReceivedVersion != NETWORK_VERSION)
{
    UE_LOG(LogInventory, Warning, TEXT("Network version mismatch: %d vs %d"), ReceivedVersion, NETWORK_VERSION);
    // Handle gracefully or reject
}
```

3. **TEST EXTENSIVELY IN MULTIPLAYER:**
```bash
# Test scenarios:
# 1. Client joins server
# 2. Client picks up item
# 3. Client moves item in inventory
# 4. Client drops item
# 5. Multiple clients simultaneously

# Monitor network bandwidth
stat net
```

**Validation:**
- [ ] Client-server synchronization working
- [ ] No replication errors
- [ ] Bandwidth usage acceptable
- [ ] Late join works correctly
- [ ] Item state preserved across network

---

### 5.5 Wave 3 Completion

```bash
# Final integration test
# Performance check
# Multiplayer stress test

git add .
git commit -m "Complete Wave 3: MedComInventory → SuspenseCore

Migration Summary:
- 23 main classes migrated
- Network replication updated
- Protocol version bumped
- All operations functional

Testing:
✅ Compilation successful
✅ Single-player working
✅ Multiplayer stable
✅ Save/load functional
✅ Performance: no regression

Files changed: [COUNT]
LOC modified: ~27,862
"

git tag -a wave3-complete -m "Wave 3 (Inventory) migration complete"
git push origin migrate/wave3-inventory wave3-complete
```

---

## 6. Wave 4: MedComEquipment Migration

**Duration:** 8 weeks (LARGEST MODULE)
**Dependencies:** All previous waves

**This is the most complex migration - take your time!**

### 6.1 Create Branch

```bash
git checkout -b migrate/wave4-equipment
```

### 6.2 Subsystem-by-Subsystem Approach

**Break into manageable pieces:**

#### Week 1-2: Base Actors & Data Store
- `AEquipmentActorBase` → `ASuspenseEquipmentActor`
- `AWeaponActor` → `ASuspenseWeapon`
- `UMedComEquipmentDataStore` → `USuspenseEquipmentDataStore`

#### Week 3-4: Service Layer (7 services)
- `UEquipmentOperationService` → `USuspenseEquipmentOperationService`
- `UEquipmentDataProviderService` → `USuspenseEquipmentDataService`
- `UEquipmentValidationService` → `USuspenseEquipmentValidationService`
- `UEquipmentNetworkService` → `USuspenseEquipmentNetworkService`
- `UEquipmentVisualService` → `USuspenseEquipmentVisualService`
- `UEquipmentAbilityService` → `USuspenseEquipmentAbilityService`
- Service Locator update

#### Week 5-6: Component Subsystems
- Core/ (5 components)
- Network/ (3 components)
- Presentation/ (3 components)
- Integration/ (2 components)

#### Week 7-8: Rules & Transaction
- Rules/ (6 components)
- Transaction/ (1 component)
- Final integration & testing

**Use general procedure for each class**

**Key Challenges:**
- Service initialization order
- Thread safety verification
- Component dependency management
- Network replication complexity

**Extensive testing required at each stage**

---

## 7. Wave 5: MedComUI Migration

**Duration:** 4 weeks
**Dependencies:** All previous waves

### 7.1 Create Branch

```bash
git checkout -b migrate/wave5-ui
```

### 7.2 Phase-by-Phase Migration

#### Week 1: UI Bridges & Managers
- `UMedComInventoryUIBridge` → `USuspenseInventoryUIAdapter`
- `UMedComEquipmentUIBridge` → `USuspenseEquipmentUIAdapter`
- `UMedComUIManager` → `USuspenseUIManager`
- `UMedComDragDropHandler` → `USuspenseDragDropHandler`
- `UMedComTooltipManager` → `USuspenseTooltipManager`

#### Week 2: Base Widget Classes
- `UMedComBaseWidget` → `USuspenseBaseWidget`
- `UMedComBaseContainerWidget` → `USuspenseBaseContainerWidget`
- `UMedComBaseSlotWidget` → `USuspenseBaseSlotWidget`
- `UMedComBaseLayoutWidget` → `USuspenseBaseLayoutWidget`

#### Week 3: Specialized Widgets
- Inventory widgets (2 classes)
- Equipment widgets (3 classes)
- HUD widgets (4 classes)
- Tooltip widgets (1 class)
- Tab/Layout widgets (3 classes)

#### Week 4: Blueprint Migration
- Update all Blueprint widgets (WBP_*)
- Update parent class references
- Test UI flow end-to-end
- Performance optimization

### 7.3 Blueprint Widget Migration Procedure

**For each Blueprint widget:**

1. **Open in editor:**
   - Right-click WBP_MyWidget → Edit

2. **Reparent Blueprint:**
   - File → Reparent Blueprint
   - Select new parent (e.g., USuspenseInventoryWidget)
   - Click Reparent

3. **Fix broken references:**
   - Check for red nodes
   - Update variable types if needed
   - Recompile Blueprint

4. **Test:**
   - PIE
   - Verify visual appearance
   - Test interactions

**Automation script for bulk reparenting:**
```python
# UE Python script - run in editor
import unreal

# Get all widget blueprints
asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
all_assets = asset_registry.get_assets_by_class("WidgetBlueprint")

for asset_data in all_assets:
    widget_bp = asset_data.get_asset()
    parent_class = widget_bp.get_parent_class()

    # Check if parent needs updating
    if "MedCom" in parent_class.get_name():
        new_parent_name = parent_class.get_name().replace("MedCom", "Suspense")
        # Find new parent class
        # ... reparent logic
```

---

## 8. Automation Scripts

### 8.1 Class Rename Script

**File:** `scripts/rename-class.sh`

```bash
#!/bin/bash
# Usage: ./rename-class.sh OldClassName NewClassName

OLD_CLASS=$1
NEW_CLASS=$2

if [ -z "$OLD_CLASS" ] || [ -z "$NEW_CLASS" ]; then
    echo "Usage: ./rename-class.sh OldClassName NewClassName"
    exit 1
fi

echo "Renaming $OLD_CLASS → $NEW_CLASS"

# Find all references
echo "Finding references..."
grep -rl "$OLD_CLASS" Source/ > /tmp/rename-refs.txt
REF_COUNT=$(wc -l < /tmp/rename-refs.txt)
echo "Found $REF_COUNT files with references"

# Ask for confirmation
read -p "Proceed with rename? (y/n) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Cancelled"
    exit 0
fi

# Perform rename in all files
while IFS= read -r file; do
    echo "Updating: $file"
    sed -i "s/$OLD_CLASS/$NEW_CLASS/g" "$file"
done < /tmp/rename-refs.txt

echo "Rename complete!"
echo "Don't forget to:"
echo "1. Rename physical files"
echo "2. Update includes"
echo "3. Regenerate project files"
echo "4. Compile"
```

**Make executable:**
```bash
chmod +x scripts/rename-class.sh
```

**Usage:**
```bash
./scripts/rename-class.sh UMedComInventoryComponent USuspenseInventoryComponent
```

---

### 8.2 Include Path Update Script

**File:** `scripts/update-includes.sh`

```bash
#!/bin/bash
# Update include paths after file moves

OLD_PATH=$1
NEW_PATH=$2

find Source/ -type f \( -name "*.h" -o -name "*.cpp" \) -exec sed -i \
    "s|#include \"$OLD_PATH\"|#include \"$NEW_PATH\"|g" {} +

echo "Updated all includes: $OLD_PATH → $NEW_PATH"
```

**Usage:**
```bash
./scripts/update-includes.sh \
    "Components/MedComInventoryComponent.h" \
    "Inventory/Components/SuspenseInventoryComponent.h"
```

---

### 8.3 Reference Counter Script

**File:** `scripts/count-references.sh`

```bash
#!/bin/bash
# Count references to a class

CLASS_NAME=$1

if [ -z "$CLASS_NAME" ]; then
    echo "Usage: ./count-references.sh ClassName"
    exit 1
fi

echo "Counting references to: $CLASS_NAME"
echo ""

# Headers
H_COUNT=$(grep -r "$CLASS_NAME" Source/ --include="*.h" | wc -l)
echo "Header files: $H_COUNT"

# Sources
CPP_COUNT=$(grep -r "$CLASS_NAME" Source/ --include="*.cpp" | wc -l)
echo "Source files: $CPP_COUNT"

# Total
TOTAL=$((H_COUNT + CPP_COUNT))
echo "Total: $TOTAL"

# By module
echo ""
echo "By module:"
for dir in Source/*/; do
    MODULE_NAME=$(basename "$dir")
    COUNT=$(grep -r "$CLASS_NAME" "$dir" --include="*.h" --include="*.cpp" | wc -l)
    if [ $COUNT -gt 0 ]; then
        echo "  $MODULE_NAME: $COUNT"
    fi
done
```

---

### 8.4 Blueprint Parent Class Finder

**UE Python Script** (run in editor):

**File:** `scripts/find_blueprint_parents.py`

```python
import unreal

def find_blueprints_with_parent(parent_class_name):
    """Find all Blueprints that inherit from a specific class"""

    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    all_blueprints = asset_registry.get_assets_by_class("Blueprint", True)

    results = []

    for asset_data in all_blueprints:
        asset = asset_data.get_asset()
        if asset:
            parent = asset.get_parent_class()
            if parent and parent_class_name in parent.get_name():
                results.append({
                    'asset': asset,
                    'path': asset_data.package_name,
                    'parent': parent.get_name()
                })

    return results

# Usage
blueprints = find_blueprints_with_parent("MedComInventoryComponent")
for bp in blueprints:
    print(f"Blueprint: {bp['path']}")
    print(f"  Parent: {bp['parent']}")
```

---

## 9. Troubleshooting Guide

### 9.1 Common Issues & Solutions

#### Issue: "Unresolved external symbol" linker error

**Symptoms:**
```
Error LNK2019: unresolved external symbol "public: __cdecl UMedComInventoryComponent::UMedComInventoryComponent(void)"
```

**Cause:** Header file updated but source file not updated, or vice versa

**Solution:**
1. Verify class name matches in both .h and .cpp
2. Check constructor name matches class name
3. Verify all method declarations match implementations
4. Clean solution and rebuild

```bash
# Clean build
Build → Clean Solution
# Then rebuild
Build → Rebuild Solution
```

---

#### Issue: Blueprint won't compile after class rename

**Symptoms:**
```
Blueprint 'BP_MyCharacter' failed to compile: Cannot find parent class 'MedComInventoryComponent'
```

**Solution:**
1. Add class redirect in DefaultEngine.ini:
```ini
[/Script/Engine.Engine]
+ActiveClassRedirects=(OldClassName="MedComInventoryComponent",NewClassName="SuspenseInventoryComponent")
```

2. Or manually reparent Blueprint:
   - Open Blueprint
   - File → Reparent Blueprint
   - Select new parent class

---

#### Issue: Include file not found

**Symptoms:**
```
fatal error C1083: Cannot open include file: 'Components/MedComInventoryComponent.h': No such file or directory
```

**Cause:** File renamed/moved but includes not updated

**Solution:**
```bash
# Find all references to old include
grep -r "MedComInventoryComponent.h" Source/

# Update to new path
# OLD: #include "Components/MedComInventoryComponent.h"
# NEW: #include "Inventory/Components/SuspenseInventoryComponent.h"
```

---

#### Issue: Network replication not working after rename

**Symptoms:**
- Clients don't see inventory changes
- "NetSerialize mismatch" errors in log

**Cause:** Replication struct names changed but network still expects old names

**Solution:**
1. Bump network protocol version:
```cpp
static constexpr uint32 NETWORK_VERSION = 3;  // Increment
```

2. Ensure client and server versions match

3. Add struct redirects if needed:
```ini
[/Script/Engine.Engine]
+ActiveStructRedirects=(OldStructName="ReplicatedItemMeta",NewStructName="SuspenseReplicatedItemData")
```

---

#### Issue: Save game won't load after migration

**Symptoms:**
- "Failed to load SaveGame" error
- Save file appears corrupted

**Cause:** Struct names changed, serialization can't match old data

**Solution:**
1. Implement save file migration:
```cpp
bool MigrateSaveGame(USaveGame* SaveGame, int32 OldVersion)
{
    if (OldVersion == 2)  // MedCom version
    {
        // Convert old struct names to new
        // Update version to 3
        return true;
    }
    return false;
}
```

2. Or provide "Clear Save Data" option in game menu

---

#### Issue: Performance regression after migration

**Symptoms:**
- FPS drop
- Increased memory usage
- Longer load times

**Cause:** Unintentional changes during migration

**Solution:**
1. Profile before and after:
```bash
# In editor console
stat startfile
# Play for 1 minute
stat stopfile
# Compare .ue4stats files
```

2. Check for:
   - Removed optimization flags
   - Changed tick intervals
   - Added unnecessary includes
   - Cache invalidation issues

3. Revert problematic changes

---

#### Issue: Circular dependency detected

**Symptoms:**
```
Circular dependency detected for 'SuspenseInventoryComponent'
```

**Cause:** Header includes creating circular reference

**Solution:**
1. Use forward declarations instead of includes in headers:
```cpp
// In .h file:
// ❌ Don't do this:
#include "SuspenseEquipmentComponent.h"

// ✅ Do this instead:
class USuspenseEquipmentComponent;  // Forward declaration

// In .cpp file, you can include:
#include "Equipment/SuspenseEquipmentComponent.h"
```

---

### 9.2 Emergency Rollback

**If migration goes wrong:**

1. **Check current state:**
```bash
git status
git diff
```

2. **Rollback to last good state:**
```bash
# If not committed yet:
git reset --hard HEAD
git clean -fd

# If committed but not pushed:
git reset --hard HEAD~1  # Go back 1 commit

# If pushed to branch:
git revert <commit-hash>
```

3. **Rollback to wave tag:**
```bash
# Go back to last completed wave
git checkout wave2-complete
# Create new branch from there
git checkout -b migrate/retry-wave3
```

---

### 9.3 Debug Checklist

**If something isn't working after migration:**

- [ ] All files renamed correctly?
- [ ] All includes updated?
- [ ] All forward declarations updated?
- [ ] Constructor names match class names?
- [ ] UCLASS macros correct?
- [ ] Build.cs dependencies updated?
- [ ] Project files regenerated?
- [ ] Clean build performed?
- [ ] Blueprint redirects added?
- [ ] Struct redirects added (if needed)?
- [ ] Network version bumped (if network code changed)?
- [ ] Save version bumped (if serialization changed)?
- [ ] All tests passing?
- [ ] Editor launches without errors?
- [ ] PIE works?
- [ ] Multiplayer tested (if applicable)?

---

## Summary

This step-by-step guide provides detailed procedures for migrating all 7 legacy modules to SuspenseCore naming conventions. Key points:

1. **Follow the wave order** - Don't skip waves (dependencies matter!)
2. **Test extensively** - After each class, phase, and wave
3. **Use automation** - Scripts save time and reduce errors
4. **Track progress** - Use spreadsheet and git tags
5. **Monitor performance** - Compare to baseline regularly
6. **Document issues** - Keep notes on problems and solutions
7. **Ask for help** - Code review, team collaboration
8. **Take your time** - This is 18-22 weeks of work, don't rush

**Estimated Timeline:**
- Wave 1 (Shared): 4 weeks
- Wave 2 (Core): 3 weeks
- Wave 3 (Inventory): 4 weeks
- Wave 4 (Equipment): 8 weeks
- Wave 5 (UI): 4 weeks
- **Total: 23 weeks** (with some buffer)

**Success Criteria:**
- ✅ All legacy "MedCom" references gone
- ✅ Consistent "Suspense" naming throughout
- ✅ All tests passing
- ✅ No performance regression
- ✅ Clean architecture maintained
- ✅ Full documentation

---

**Document Status:** ✅ Complete
**For Questions:** Contact tech lead or reference other migration docs
**Good luck!** 🚀
