# SuspenseCore Naming Convention Guide

**Document Version:** 1.0
**Last Updated:** 2025-11-24
**Status:** Official Standard
**Applies To:** All SuspenseCore modules and legacy code migration

---

## Table of Contents

1. [AAA Naming Philosophy](#1-aaa-naming-philosophy)
2. [Unreal Engine Naming Prefixes](#2-unreal-engine-naming-prefixes)
3. [Module Naming Rules](#3-module-naming-rules)
4. [Class Renaming Examples](#4-class-renaming-examples)
5. [Namespace Organization](#5-namespace-organization)
6. [Blueprint Naming](#6-blueprint-naming)
7. [Asset Naming Conventions](#7-asset-naming-conventions)
8. [Migration Mapping Reference](#8-migration-mapping-reference)

---

## 1. AAA Naming Philosophy

### 1.1 What is AAA Naming?

**AAA** (Application Architecture Approach) naming is a professional game development standard that emphasizes:

1. **Clarity** - Names immediately convey purpose and type
2. **Consistency** - Predictable patterns across the entire codebase
3. **Searchability** - Easy to find related classes via prefix search
4. **Scalability** - Works for codebases with 1000+ classes
5. **Professionalism** - Industry-standard conventions

### 1.2 Why AAA for SuspenseCore?

**Benefits:**
- ✅ Instantly recognizable class types (Actor, Component, Struct, etc.)
- ✅ Prevents naming conflicts with engine classes
- ✅ Improves code navigation and IntelliSense
- ✅ Easier onboarding for new developers
- ✅ Matches Unreal Engine best practices

**Industry Examples:**
```cpp
// Epic Games (Fortnite, Gears)
AFortniteCharacter, UFortAbility, FFortGameplayTag

// CD Projekt Red (Cyberpunk 2077)
ACyberpunkPlayer, UCyberpunkInventory, FCyberpunkItemData

// SuspenseCore (Our Project)
ASuspenseCharacter, USuspenseInventoryComponent, FSuspenseItemInstance
```

### 1.3 Core Principles

1. **Prefix Over Suffix** - Type information comes first
   - ✅ `USuspenseInventoryComponent`
   - ❌ `InventorySuspenseComponent`

2. **Descriptive Middle Section** - Explains what the class does
   - ✅ `USuspenseEquipmentReplicator` (Equipment + Replicator)
   - ❌ `USuspenseER` (cryptic abbreviation)

3. **No Redundant Type Suffix** - Prefix already indicates type
   - ✅ `ASuspensePickupItem` (A = Actor, implied)
   - ❌ `ASuspensePickupItemActor` (redundant "Actor")

4. **Consistent Brand Prefix** - "Suspense" for all project classes
   - ✅ Separates our code from engine/plugin code
   - ✅ Avoids naming conflicts
   - ✅ Professional branding

---

## 2. Unreal Engine Naming Prefixes

### 2.1 Standard Unreal Prefixes

All Unreal Engine C++ classes **must** use these prefixes:

| Prefix | Type | Example | Usage |
|--------|------|---------|-------|
| **A** | Actor | `ASuspensePickupItem` | Spawnable world objects |
| **U** | UObject (Component/Asset) | `USuspenseInventoryComponent` | Components, assets, subsystems |
| **F** | Struct | `FSuspenseItemInstance` | Plain data structures |
| **E** | Enum | `ESuspenseInventoryError` | Enumerations |
| **I** | Interface | `ISuspenseInventory` | Abstract contracts |
| **T** | Template | `TSuspenseArray<T>` | Generic containers (rare) |
| **S** | Slate Widget | `SSuspenseSlotWidget` | Low-level UI (rare in UMG projects) |

### 2.2 When to Use Each Prefix

#### **A - Actor Classes**

**Use for:** Objects that exist in the world with transform (location, rotation, scale)

```cpp
// ✅ Correct
class SUSPENSECORE_API ASuspensePickupItem : public AActor { ... };
class SUSPENSECORE_API ASuspenseCharacter : public ACharacter { ... };
class SUSPENSECORE_API ASuspenseWeapon : public AActor { ... };

// ❌ Incorrect
class SUSPENSECORE_API USuspensePickupItem : public AActor { ... };  // Wrong prefix!
```

**Common Actor Types:**
- Characters: `ASuspensePlayerCharacter`, `ASuspenseNPC`
- Pickups: `ASuspensePickupItem`, `ASuspenseAmmoBox`
- Weapons: `ASuspenseWeapon`, `ASuspenseGrenade`
- Interactables: `ASuspenseDoor`, `ASuspenseContainer`

#### **U - UObject Classes**

**Use for:** Components, subsystems, assets, and non-actor game objects

```cpp
// Components
class SUSPENSECORE_API USuspenseInventoryComponent : public UActorComponent { ... };
class SUSPENSECORE_API USuspenseEquipmentComponent : public UActorComponent { ... };

// Subsystems
class SUSPENSECORE_API USuspenseInventorySubsystem : public UGameInstanceSubsystem { ... };
class SUSPENSECORE_API USuspenseCoreItemManager : public UGameInstanceSubsystem { ... };

// Blueprint Libraries
class SUSPENSECORE_API USuspenseInventoryLibrary : public UBlueprintFunctionLibrary { ... };

// Data Assets
class SUSPENSECORE_API USuspenseItemDataAsset : public UPrimaryDataAsset { ... };
```

**Common UObject Types:**
- Components: `USuspense*Component`
- Subsystems: `USuspense*Subsystem`
- Libraries: `USuspense*Library`
- Assets: `USuspense*Asset`, `USuspense*DataTable`

#### **F - Struct Types**

**Use for:** Data structures without UObject overhead

```cpp
// ✅ Correct - Core data structures
USTRUCT(BlueprintType)
struct SUSPENSECORE_API FSuspenseItemInstance
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName ItemID;

    UPROPERTY(BlueprintReadOnly)
    int32 Amount;
};

// ✅ Correct - Operation results
USTRUCT(BlueprintType)
struct FSuspenseInventoryResult
{
    GENERATED_BODY()

    bool bSuccess;
    ESuspenseInventoryError ErrorCode;
};

// ❌ Incorrect - Should use struct not class
class FSuspenseItemInstance { ... };  // Wrong! Use struct
```

**Common Struct Types:**
- Data instances: `FSuspenseItemInstance`, `FSuspenseEquipmentSlot`
- Results: `FSuspense*Result`, `FSuspense*Response`
- Configurations: `FSuspense*Config`, `FSuspense*Settings`
- UI Data: `FSuspense*UIData`, `FSuspense*DisplayInfo`

#### **E - Enum Types**

**Use for:** Enumerations

```cpp
// ✅ Correct - Error codes
UENUM(BlueprintType)
enum class ESuspenseInventoryError : uint8
{
    None,
    NotEnoughSpace,
    ItemNotFound,
    InvalidItem
};

// ✅ Correct - Item types
UENUM(BlueprintType)
enum class ESuspenseItemType : uint8
{
    Weapon,
    Armor,
    Consumable,
    Ammo
};

// ❌ Incorrect - Missing prefix
enum class InventoryError : uint8 { ... };  // Wrong!
```

**Naming Pattern:** `ESuspense[Domain][Purpose]`
- Error codes: `ESuspenseInventoryError`, `ESuspenseEquipmentError`
- Types: `ESuspenseItemType`, `ESuspenseSlotType`
- States: `ESuspenseEquipmentState`, `ESuspenseInventoryState`

#### **I - Interface Types**

**Use for:** Abstract contracts (pure virtual classes)

```cpp
// ✅ Correct - UInterface pattern
UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseInventory : public UInterface
{
    GENERATED_BODY()
};

class ISuspenseInventory
{
    GENERATED_BODY()
public:
    virtual bool AddItem(const FSuspenseItemData& ItemData, int32 Quantity) = 0;
    virtual bool RemoveItem(FName ItemID, int32 Quantity) = 0;
};

// ❌ Incorrect - Missing I prefix for implementation
class USuspenseInventory  // Wrong! This should be ISuspenseInventory
{
    virtual bool AddItem(...) = 0;
};
```

**Important:** Unreal interfaces require **TWO** classes:
1. `USuspense*` (with UInterface macro) - For Blueprint compatibility
2. `ISuspense*` (with pure virtuals) - For C++ implementation

**Naming Pattern:** `ISuspense[Domain]` (simplified)
- Inventory: `ISuspenseInventory`, `ISuspenseInventoryItem`
- Equipment: `ISuspenseEquipment`, `ISuspenseEquipable`
- Interaction: `ISuspenseInteractable`, `ISuspenseActivatable`

---

## 3. Module Naming Rules

### 3.1 Module Name Structure

**Pattern:** `[Domain]System` (no brand prefix for modules)

```
Source/
├── InventorySystem/        # ✅ Clear domain
├── EquipmentSystem/        # ✅ Clear domain
├── InteractionSystem/      # ✅ Clear domain
├── UISystem/               # ✅ Clear domain
├── GAS/                    # ✅ Abbreviation acceptable for well-known systems
└── PlayerCore/             # ✅ Clear domain
```

**Reasoning:**
- Module names are folder/project names (organizational)
- Brand prefix ("Suspense") goes in class names, not folder names
- Keeps folder structure clean and focused on functionality

### 3.2 Module Internal Structure

**Standard Layout:**
```
InventorySystem/
├── InventorySystem.Build.cs           # Module build file
├── Public/                            # Public API
│   ├── Components/                    # USuspenseInventory*Component classes
│   ├── Operations/                    # FSuspenseInventory*Operation structs
│   ├── Interfaces/                    # ISuspenseInventory* interfaces
│   └── Types/                         # FSuspense*Data, ESuspense*Error enums
└── Private/                           # Implementation
    ├── Components/                    # .cpp files
    ├── Operations/
    └── Utility/
```

### 3.3 Include Path Conventions

```cpp
// ✅ Correct - Module/Category/Class
#include "Inventory/Components/SuspenseInventoryComponent.h"
#include "Equipment/Services/SuspenseEquipmentService.h"
#include "Interaction/SuspenseInteractionComponent.h"

// ❌ Incorrect - Missing category
#include "Inventory/SuspenseInventoryComponent.h"  // Where is it?

// ❌ Incorrect - Too deep nesting
#include "Inventory/Public/Components/Core/Base/SuspenseInventoryComponent.h"
```

---

## 4. Class Renaming Examples

### 4.1 Component Classes

| Legacy Name | New Name | Reasoning |
|-------------|----------|-----------|
| `UMedComInventoryComponent` | `USuspenseInventoryComponent` | Brand update, keep descriptive middle |
| `UMedComEquipmentComponent` | `USuspenseEquipmentComponent` | Direct rename, clear purpose |
| `UMedComInteractionComponent` | `USuspenseInteractionComponent` | Consistent pattern |
| `UMedComEquipmentDataStore` | `USuspenseEquipmentDataStore` | Preserve "DataStore" suffix (architectural term) |
| `UMedComInventoryStorage` | `USuspenseInventoryStorage` | Keep "Storage" distinction from "Component" |
| `UMedComEquipmentReplicationManager` | `USuspenseEquipmentReplicator` | Simplify "Manager" → "Replicator" |
| `UMedComInventoryConstraints` | `USuspenseInventoryValidator` | Rename for clarity (validates constraints) |

### 4.2 Actor Classes

| Legacy Name | New Name | Reasoning |
|-------------|----------|-----------|
| `AMedComBasePickupItem` | `ASuspensePickupItem` | Remove "Base" prefix (unnecessary) |
| `AMedComInventoryItem` | `ASuspenseInventoryItemActor` | Clarify it's an actor wrapper |
| `AMedComCharacter` | `ASuspenseCharacter` | Direct rename |
| `AEquipmentActorBase` | `ASuspenseEquipmentActor` | Add brand, remove "Base" |
| `AWeaponActor` | `ASuspenseWeapon` | Add brand, remove "Actor" suffix |

**Note:** Remove "Base" prefix for base classes - inheritance is clear from class hierarchy.

### 4.3 Struct Classes

| Legacy Name | New Name | Reasoning |
|-------------|----------|-----------|
| `FInventoryItemInstance` | `FSuspenseItemInstance` | **CRITICAL:** Widely used, consider keeping old name as alias |
| `FInventoryCell` | `FSuspenseInventoryCell` | Add "Inventory" for clarity |
| `FInventoryOperationResult` | `FSuspenseInventoryResult` | Simplify, remove "Operation" |
| `FReplicatedItemMeta` | `FSuspenseReplicatedItemData` | More descriptive: "Meta" → "Data" |
| `FReplicatedCellsState` | `FSuspenseReplicatedGridState` | Better semantics: "Cells" → "Grid" |
| `FMedComUnifiedItemData` | `FSuspenseItemData` | Simplify: remove "Unified" |
| `FSerializedInventoryData` | `FSuspenseInventorySnapshot` | Better intent: "Serialized" → "Snapshot" |

**Special Case - Widely Used Structs:**
```cpp
// Option 1: Keep original name (minimize breaking changes)
using FInventoryItemInstance = FSuspenseItemInstance;  // Alias for compatibility

// Option 2: Full rename (clean break, more work)
// Rename all references to FSuspenseItemInstance
```

### 4.4 Interface Classes

| Legacy Name | New Name | Reasoning |
|-------------|----------|-----------|
| `IMedComInventoryInterface` | `ISuspenseInventory` | Remove redundant "Interface" suffix |
| `IMedComInventoryItemInterface` | `ISuspenseInventoryItem` | Simplify |
| `IMedComEquipmentInterface` | `ISuspenseEquipment` | Simplify |
| `IMedComInteractableInterface` | `ISuspenseInteractable` | Simplify, use adjective form |
| `IMedComInventoryUIBridgeWidget` | `ISuspenseInventoryWidget` | Simplify: UI widget context is clear |
| `IMedComPickupInterface` | `ISuspensePickup` | Simplify |

**Pattern:** `ISuspense[Domain][OptionalSpecialization]`
- Remove "Interface" suffix (I prefix already indicates interface)
- Use clear, concise names
- Prefer adjective forms for behavior interfaces (-able, -ible)

### 4.5 Enum Classes

| Legacy Name | New Name | Reasoning |
|-------------|----------|-----------|
| `EInventoryErrorCode` | `ESuspenseInventoryError` | Add brand, remove "Code" |
| `EInventoryOperationType` | `ESuspenseInventoryOperation` | Add brand, remove "Type" |
| `EEquipmentSlotType` | `ESuspenseEquipmentSlot` | Remove "Type" suffix |
| `EItemType` | `ESuspenseItemType` | Add brand |
| `EEquipmentState` | `ESuspenseEquipmentState` | Add brand |

### 4.6 Subsystem Classes

| Legacy Name | New Name | Reasoning |
|-------------|----------|-----------|
| `UInventoryManager` | `USuspenseInventorySubsystem` | Add brand, clarify as Subsystem |
| `UMedComItemManager` | `USuspenseCoreItemManager` | Keep "Manager" (manages DataTable) |
| `UEventDelegateManager` | `USuspenseEventManager` | Simplify: remove "Delegate" |
| `UMedComItemFactory` | `USuspenseItemFactory` | Direct rename |
| `UMedComSystemCoordinatorSubsystem` | `USuspenseEquipmentSubsystem` | Simplify: remove "Coordinator" |

### 4.7 Service Classes

| Legacy Name | New Name | Reasoning |
|-------------|----------|-----------|
| `UEquipmentOperationService` | `USuspenseEquipmentOperationService` | Add brand, keep full name |
| `UEquipmentDataProviderService` | `USuspenseEquipmentDataService` | Simplify: remove "Provider" |
| `UEquipmentValidationService` | `USuspenseEquipmentValidationService` | Add brand |
| `UEquipmentNetworkService` | `USuspenseEquipmentNetworkService` | Add brand |
| `UEquipmentVisualService` | `USuspenseEquipmentVisualService` | Add brand |

### 4.8 Blueprint Library Classes

| Legacy Name | New Name | Reasoning |
|-------------|----------|-----------|
| `UMedComInventoryFunctionLibrary` | `USuspenseInventoryLibrary` | Remove "Function", add brand |
| `UMedComItemBlueprintLibrary` | `USuspenseItemLibrary` | Simplify |
| `UMedComStaticHelpers` | `USuspenseHelpers` | Remove "Static", add brand |

---

## 5. Namespace Organization

### 5.1 C++ Namespace Convention

**Pattern:** `Suspense::[Module]::[Category]`

```cpp
// Inventory System
namespace Suspense::Inventory
{
    namespace Operations
    {
        struct FMoveOperation { ... };
        struct FStackOperation { ... };
    }

    namespace Validation
    {
        bool ValidateWeight(float Weight);
        bool ValidateSpace(const FVector2D& GridSize);
    }

    namespace Network
    {
        void ReplicateInventory(...);
    }
}

// Equipment System
namespace Suspense::Equipment
{
    namespace Services
    {
        class IOperationService { ... };
        class IDataService { ... };
    }

    namespace Rules
    {
        enum class EValidationResult { ... };
    }
}

// Global Suspense utilities
namespace Suspense
{
    namespace Math
    {
        float CalculateDamage(...);
    }

    namespace Debug
    {
        void DrawDebugInventory(...);
    }
}
```

### 5.2 Usage Examples

```cpp
// ✅ Using namespace
using namespace Suspense::Inventory::Operations;
FMoveOperation Op;

// ✅ Fully qualified
Suspense::Inventory::Validation::ValidateWeight(50.0f);

// ✅ Namespace alias
namespace SInv = Suspense::Inventory;
SInv::Operations::FMoveOperation Op;
```

### 5.3 Blueprint Namespace (Categories)

```cpp
// Blueprint function categories
UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Inventory")
bool AddItem(FName ItemID, int32 Amount);

UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Equipment")
bool EquipItem(FName SlotID, FName ItemID);

UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Interaction")
void StartInteraction(AActor* Target);
```

**Pattern:** `SuspenseCore|[Module]|[OptionalSubcategory]`

---

## 6. Blueprint Naming

### 6.1 Blueprint Class Naming

**Pattern:** `BP_Suspense[Type][Name]`

```
// Blueprints
BP_SuspenseCharacter              # Character BP
BP_SuspensePickupRifle            # Pickup item BP
BP_SuspenseInventoryWidget        # UI widget BP
BP_SuspenseMainHUD                # HUD BP

// Data Assets
DA_Suspense_ItemDatabase          # Data Asset
DT_Suspense_Items                 # Data Table
DT_Suspense_Loadouts              # Data Table

// Materials
M_Suspense_ItemIcon               # Material
MI_Suspense_WeaponMetal           # Material Instance

// Textures
T_Suspense_Rifle_Icon             # Texture
T_Suspense_UI_Background          # Texture
```

### 6.2 Widget Blueprint Naming

```
// Widgets
WBP_Suspense_InventoryGrid        # Widget Blueprint
WBP_Suspense_EquipmentSlot        # Widget Blueprint
WBP_Suspense_ItemTooltip          # Widget Blueprint
WBP_Suspense_MainMenu             # Widget Blueprint
```

### 6.3 Blueprint Variable Naming

```cpp
// ✅ Correct
InventoryComponent                # Clear, no prefix needed
EquipmentSlots                    # Plural for arrays
bIsEquipped                       # Boolean prefix 'b'
CurrentHealth                     # No prefix for numbers
OnItemAdded                       # Delegate/Event prefix 'On'

// ❌ Incorrect
m_InventoryComponent              # No Hungarian notation
inventoryComponent                # Use PascalCase
is_equipped                       # No snake_case
```

---

## 7. Asset Naming Conventions

### 7.1 Asset Prefix Standards

| Asset Type | Prefix | Example |
|------------|--------|---------|
| Blueprint Class | `BP_` | `BP_SuspenseCharacter` |
| Widget Blueprint | `WBP_` | `WBP_SuspenseInventory` |
| Data Table | `DT_` | `DT_SuspenseItems` |
| Data Asset | `DA_` | `DA_SuspenseLoadout` |
| Material | `M_` | `M_SuspenseWeapon` |
| Material Instance | `MI_` | `MI_SuspenseRifle` |
| Texture | `T_` | `T_SuspenseIcon_Rifle` |
| Static Mesh | `SM_` | `SM_SuspenseWeapon_AK47` |
| Skeletal Mesh | `SK_` | `SK_SuspenseCharacter` |
| Animation | `A_` or `AM_` | `A_SuspenseReload`, `AM_SuspenseIdle` |
| Animation Blueprint | `ABP_` | `ABP_SuspenseCharacter` |
| Sound Cue | `SC_` | `SC_SuspenseGunshot` |
| Sound Wave | `S_` | `S_SuspenseReload` |
| Particle System | `P_` | `P_SuspenseMuzzleFlash` |
| Niagara System | `NS_` | `NS_SuspenseBlood` |

### 7.2 Folder Organization

```
Content/
├── SuspenseCore/
│   ├── Characters/
│   │   ├── Player/
│   │   │   ├── BP_SuspensePlayer
│   │   │   ├── ABP_SuspensePlayer
│   │   │   └── SK_SuspensePlayer
│   │   └── NPC/
│   ├── Items/
│   │   ├── Weapons/
│   │   │   ├── Rifles/
│   │   │   │   ├── BP_SuspenseRifle_AK47
│   │   │   │   ├── SM_SuspenseRifle_AK47
│   │   │   │   └── T_SuspenseRifle_AK47_Icon
│   │   │   └── Pistols/
│   │   ├── Consumables/
│   │   └── Armor/
│   ├── UI/
│   │   ├── Inventory/
│   │   │   ├── WBP_SuspenseInventoryGrid
│   │   │   └── WBP_SuspenseInventorySlot
│   │   ├── Equipment/
│   │   └── HUD/
│   └── Data/
│       ├── DT_SuspenseItems
│       ├── DT_SuspenseLoadouts
│       └── DA_SuspenseGameConfig
```

---

## 8. Migration Mapping Reference

### 8.1 Complete Class Mapping Table

**Critical Classes (High Impact):**

| Legacy | New | Impact | Notes |
|--------|-----|--------|-------|
| `UMedComInventoryComponent` | `USuspenseInventoryComponent` | CRITICAL | Main inventory component, 50+ BP references |
| `UMedComEquipmentComponent` | `USuspenseEquipmentComponent` | CRITICAL | Main equipment component |
| `IMedComInventoryInterface` | `ISuspenseInventory` | CRITICAL | Core interface, many implementations |
| `FInventoryItemInstance` | `FSuspenseItemInstance` OR keep as-is | CRITICAL | Used everywhere, consider alias |
| `UInventoryReplicator` | `USuspenseInventoryReplicator` | HIGH | Network critical |
| `UMedComItemManager` | `USuspenseCoreItemManager` | HIGH | DataTable manager |

**Medium Impact Classes:**

| Legacy | New | Impact | Notes |
|--------|-----|--------|-------|
| `UMedComInventoryStorage` | `USuspenseInventoryStorage` | MEDIUM | Internal component |
| `UMedComInventoryTransaction` | `USuspenseInventoryTransaction` | MEDIUM | Transaction system |
| `UMedComInventoryEvents` | `USuspenseInventoryEvents` | MEDIUM | Event broadcaster |
| `UMedComEquipmentDataStore` | `USuspenseEquipmentDataStore` | MEDIUM | Data storage |
| `AMedComBasePickupItem` | `ASuspensePickupItem` | MEDIUM | World pickups |

**Low Impact Classes (Utilities):**

| Legacy | New | Impact | Notes |
|--------|-----|--------|-------|
| `UInventoryLogs` | `FSuspenseInventoryLogs` | LOW | Logging category |
| `UInventoryDebugger` | `USuspenseInventoryDebugger` | LOW | Debug tools |
| `UMedComStaticHelpers` | `USuspenseHelpers` | LOW | Blueprint library |

### 8.2 Quick Reference Chart

```
MedCom Prefix → Suspense Prefix
================================
UMedCom*      → USuspense*
AMedCom*      → ASuspense*
FMedCom*      → FSuspense*
IMedCom*      → ISuspense*
EMedCom*      → ESuspense*

Special Cases:
==============
UInventory*       → USuspenseInventory*  (add brand)
AWeaponActor      → ASuspenseWeapon      (add brand, remove Actor)
UEquipment*       → USuspenseEquipment*  (add brand)
FInventory*       → FSuspenseInventory* OR keep (depends on usage)
```

---

## Summary & Best Practices

### Golden Rules

1. ✅ **Always use Unreal prefixes** (A/U/F/E/I/T/S)
2. ✅ **Always use "Suspense" brand prefix** for project classes
3. ✅ **Be descriptive** but not redundant
4. ✅ **Use PascalCase** for all C++ names
5. ✅ **Remove unnecessary suffixes** ("Interface", "Class", "Type")
6. ✅ **Organize by domain** (Inventory, Equipment, UI, etc.)
7. ✅ **Keep interfaces simple** (ISuspenseInventory, not ISuspenseInventoryInterface)
8. ✅ **Use adjectives for behavior interfaces** (ISuspenseInteractable)

### Common Mistakes to Avoid

❌ Missing Unreal prefix: `SuspenseInventory` (should be `USuspenseInventory`)
❌ Wrong prefix: `USuspensePickup` when it's an Actor (should be `ASuspensePickup`)
❌ Redundant suffix: `ISuspenseInventoryInterface` (should be `ISuspenseInventory`)
❌ No brand prefix: `UInventoryComponent` (should be `USuspenseInventoryComponent`)
❌ Hungarian notation: `m_pInventory`, `iCount` (use PascalCase)
❌ Inconsistent naming: `USuspenseInvComp` (don't abbreviate randomly)

---

**Document Status:** ✅ Complete and ready for use
**Review Cycle:** Quarterly
**Last Reviewed:** 2025-11-24
**Next Review:** 2026-02-24
