# Legacy Modules - Complete Directory Structure

## 1. MedComShared (`Source/BridgeSystem/MedComShared`)

Shared interfaces, types, and contracts for all legacy systems.

```
MedComShared/
├── Private/
│   ├── Abilities/
│   │   └── Inventory/
│   ├── Core/
│   │   ├── Services/
│   │   └── Utils/
│   ├── Delegates/
│   ├── Interfaces/
│   │   ├── Abilities/
│   │   ├── Core/
│   │   ├── Equipment/
│   │   ├── Interaction/
│   │   ├── Inventory/
│   │   ├── UI/
│   │   └── Weapon/
│   ├── ItemSystem/
│   └── Types/
│       ├── Animation/
│       ├── Inventory/
│       └── Loadout/
└── Public/
    ├── Abilities/
    │   └── Inventory/
    ├── Core/
    │   ├── Services/
    │   └── Utils/
    ├── Delegates/
    ├── Input/
    ├── Interfaces/
    │   ├── Abilities/
    │   ├── Core/
    │   ├── Equipment/
    │   ├── Interaction/
    │   ├── Inventory/
    │   ├── Screens/
    │   ├── Tabs/
    │   ├── UI/
    │   └── Weapon/
    ├── ItemSystem/
    ├── Operations/
    └── Types/
        ├── Animation/
        ├── Equipment/
        ├── Events/
        ├── Inventory/
        ├── Loadout/
        ├── Network/
        ├── Rules/
        ├── Transaction/
        ├── UI/
        └── Weapon/
```

**Key Characteristics:**
- 86 header files, 37 source files
- 85 USTRUCT declarations (data structures)
- 60 UINTERFACE declarations (contracts)
- 26,680 lines of code total
- Serves as the foundation for all other legacy modules

---

## 2. MedComGAS (`Source/GAS/MedComGAS`)

Gameplay Ability System (GAS) implementation with abilities, attributes, and effects.

```
MedComGAS/
├── Private/
│   ├── Abilities/ (7 files)
│   ├── Attributes/ (5 files)
│   ├── Components/
│   ├── Effects/ (8 files)
│   └── Subsystems/
└── Public/
    ├── Abilities/ (7 files)
    ├── Attributes/ (5 files)
    ├── Components/
    ├── Effects/ (8 files)
    └── Subsystems/
```

**Key Characteristics:**
- 23 header files, 23 source files
- 22 UCLASS declarations
- 8,003 lines of code
- Symmetric private/public structure
- Focused on ability and attribute systems

---

## 3. MedComCore (`Source/PlayerCore/MedComCore`)

Core player character and system definitions.

```
MedComCore/
├── Private/
│   ├── Characters/ (2 files)
│   └── Core/ (5 files)
└── Public/
    ├── Characters/ (2 files)
    └── Core/ (5 files)
```

**Key Characteristics:**
- 8 header files, 8 source files
- 7 UCLASS declarations
- 8,638 lines of code
- Minimal but focused core system
- Lightweight character definitions

---

## 4. MedComEquipment (`Source/EquipmentSystem/MedComEquipment`)

Complex equipment system with multiple layers: components, services, and subsystems.

```
MedComEquipment/
├── Private/
│   ├── Base/ (2 files)
│   ├── Components/
│   │   ├── Coordination/
│   │   ├── Core/ (5 files)
│   │   ├── Integration/ (2 files)
│   │   ├── Network/ (3 files)
│   │   ├── Presentation/ (3 files)
│   │   ├── Rules/ (6 files)
│   │   ├── Transaction/
│   │   └── Validation/
│   ├── Services/ (7 files)
│   └── Subsystems/
└── Public/
    ├── Base/ (2 files)
    ├── Components/
    │   ├── Coordination/
    │   ├── Core/ (5 files)
    │   ├── Integration/ (2 files)
    │   ├── Network/ (3 files)
    │   ├── Presentation/ (3 files)
    │   ├── Rules/ (6 files)
    │   ├── Transaction/
    │   └── Validation/
    ├── Services/ (7 files)
    └── Subsystems/
```

**Key Characteristics:**
- 40 header files, 40 source files
- 38 UCLASS declarations (highest)
- 77 USTRUCT declarations
- 54,213 lines of code (largest module)
- Complex multi-layered architecture
- Organized by concerns: Coordination, Core, Integration, Network, Presentation, Rules, Transaction, Validation

---

## 5. MedComInventory (`Source/InventorySystem/MedComInventory`)

Operation-driven inventory system with validation, serialization, and network support.

```
MedComInventory/
├── Private/
│   ├── Base/ (5 files)
│   ├── Components/
│   ├── Debug/
│   ├── Events/
│   ├── Network/
│   ├── Operations/ (5 files)
│   ├── Serialization/
│   ├── Storage/
│   ├── Templates/
│   ├── UI/
│   ├── Utility/
│   └── Validation/
└── Public/
    ├── Base/ (5 files)
    ├── Components/
    ├── Debug/
    ├── Events/
    ├── Network/
    ├── Operations/ (6 files)
    ├── Serialization/
    ├── Storage/
    ├── Templates/
    ├── UI/
    ├── Utility/
    └── Validation/
```

**Key Characteristics:**
- 22 header files, 21 source files
- 16 UCLASS declarations
- 16 USTRUCT declarations
- 27,862 lines of code
- Operation-focused architecture
- Includes debug, validation, and network support

---

## 6. MedComInteraction (`Source/InteractionSystem/MedComInteraction`)

Minimal, focused interaction system for pickups and interactions.

```
MedComInteraction/
├── Private/
│   ├── Components/
│   ├── Pickup/
│   └── Utils/ (2 files)
└── Public/
    ├── Components/
    ├── Pickup/
    └── Utils/ (3 files)
```

**Key Characteristics:**
- 6 header files, 5 source files
- 5 UCLASS declarations
- 3,486 lines of code (smallest module)
- Lightweight and focused
- Simple component and utility structure

---

## 7. MedComUI (`Source/UISystem/MedComUI`)

Widget-heavy UI system with drag & drop, tooltips, and various widget types.

```
MedComUI/
├── Private/
│   ├── Components/ (3 files)
│   ├── DragDrop/
│   ├── Tooltip/
│   └── Widgets/
│       ├── Base/ (3 files)
│       ├── DragDrop/ (2 files)
│       ├── Equipment/ (2 files)
│       ├── HUD/ (4 files)
│       ├── Inventory/ (2 files)
│       ├── Layout/ (2 files)
│       ├── Screens/
│       ├── Tabs/
│       └── Tooltip/
└── Public/
    ├── Components/ (3 files)
    ├── DragDrop/
    ├── Tooltip/
    └── Widgets/
        ├── Base/ (3 files)
        ├── DragDrop/ (2 files)
        ├── Equipment/ (2 files)
        ├── HUD/ (4 files)
        ├── Inventory/ (2 files)
        ├── Layout/ (2 files)
        ├── Screens/
        ├── Tabs/
        └── Tooltip/
```

**Key Characteristics:**
- 24 header files, 24 source files
- 23 UCLASS declarations
- 15 USTRUCT declarations
- 26,706 lines of code
- Rich widget hierarchy
- Organized by UI concerns: Components, DragDrop, Tooltip, and various Widget types

---

## Summary of Directory Patterns

### Common Patterns Across All Modules:
1. **Public/Private Separation** - All modules follow this fundamental pattern
2. **Component-Based Architecture** - Most modules use component pattern (Equipment, Inventory, Interaction, UI)
3. **Service/Subsystem Pattern** - Equipment uses explicit services and subsystems
4. **Interface-Driven Design** - MedComShared provides interfaces for all systems
5. **Type Definitions** - Separate directories for data structures and enums
6. **Organized by Concern** - Equipment uses Coordination, Core, Integration, Network, etc.

### Module Size Categories:
- **Large (20K+ LOC):** MedComEquipment (54K), MedComInventory (28K), MedComShared (27K), MedComUI (27K)
- **Medium (8K-10K LOC):** MedComGAS (8K), MedComCore (9K)
- **Small (<5K LOC):** MedComInteraction (3.5K)

### Reusability Indicators:
- **Highly Reusable:** MedComShared (interfaces and contracts for all)
- **System-Specific:** MedComEquipment (complex multi-layer), MedComUI (widget-heavy)
- **Support Systems:** MedComGAS (abilities), MedComCore (core player)
