# Legacy Modules Statistics Report

## Summary Table

| Module | .h Files | .cpp Files | Total LOC | UCLASS | USTRUCT | UINTERFACE |
|--------|----------|-----------|-----------|--------|---------|-----------|
| MedComShared | 86 | 37 | 26,680 | 7 | 85 | 60 |
| MedComGAS | 23 | 23 | 8,003 | 22 | 1 | 0 |
| MedComCore | 8 | 8 | 8,638 | 7 | 1 | 0 |
| MedComEquipment | 40 | 40 | 54,213 | 38 | 77 | 0 |
| MedComInventory | 22 | 21 | 27,862 | 16 | 16 | 0 |
| MedComInteraction | 6 | 5 | 3,486 | 5 | 1 | 0 |
| MedComUI | 24 | 24 | 26,706 | 23 | 15 | 0 |
| **TOTAL** | **209** | **158** | **155,588** | **118** | **196** | **60** |

## Module Details

### 1. MedComShared
**Path:** `/home/user/SuspenseCore/Source/BridgeSystem/MedComShared`

**Statistics:**
- Header Files: 86
- Source Files: 37
- Total Lines of Code: 26,680
- UCLASS declarations: 7
- USTRUCT declarations: 85
- UINTERFACE declarations: 60

**Main Subdirectories:**
- **Private/Abilities/Inventory/** - Inventory abilities
- **Private/Core/Services/** - Core service implementations
- **Private/Core/Utils/** - Utility functions
- **Private/Delegates/** - Delegate definitions
- **Private/Interfaces/** - Interface definitions (Abilities, Core, Equipment, Interaction, Inventory, UI, Weapon)
- **Private/ItemSystem/** - Item system implementations
- **Private/Types/** - Type definitions (Animation, Inventory, Loadout)
- **Public/Core/Services/** - Public core services
- **Public/Core/Utils/** - Public utilities
- **Public/Input/** - Input handling
- **Public/Interfaces/** - Public interfaces (comprehensive)
- **Public/ItemSystem/** - Public item system
- **Public/Operations/** - Operation definitions
- **Public/Types/** - Public type definitions (Animation, Equipment, Events, Inventory, Loadout, Network, Rules, Transaction, UI, Weapon)

---

### 2. MedComGAS
**Path:** `/home/user/SuspenseCore/Source/GAS/MedComGAS`

**Statistics:**
- Header Files: 23
- Source Files: 23
- Total Lines of Code: 8,003
- UCLASS declarations: 22
- USTRUCT declarations: 1
- UINTERFACE declarations: 0

**Main Subdirectories:**
- **Private/Abilities/** - Ability implementations (7 files)
- **Private/Attributes/** - Attribute definitions (5 files)
- **Private/Components/** - Component implementations
- **Private/Effects/** - Gameplay effect implementations (8 files)
- **Private/Subsystems/** - Subsystem implementations
- **Public/Abilities/** - Public abilities (7 files)
- **Public/Attributes/** - Public attributes (5 files)
- **Public/Components/** - Public components
- **Public/Effects/** - Public effects (8 files)
- **Public/Subsystems/** - Public subsystems

---

### 3. MedComCore
**Path:** `/home/user/SuspenseCore/Source/PlayerCore/MedComCore`

**Statistics:**
- Header Files: 8
- Source Files: 8
- Total Lines of Code: 8,638
- UCLASS declarations: 7
- USTRUCT declarations: 1
- UINTERFACE declarations: 0

**Main Subdirectories:**
- **Private/Characters/** - Character implementations (2 files)
- **Private/Core/** - Core implementations (5 files)
- **Public/Characters/** - Public character definitions (2 files)
- **Public/Core/** - Public core definitions (5 files)

---

### 4. MedComEquipment
**Path:** `/home/user/SuspenseCore/Source/EquipmentSystem/MedComEquipment`

**Statistics:**
- Header Files: 40
- Source Files: 40
- Total Lines of Code: 54,213
- UCLASS declarations: 38
- USTRUCT declarations: 77
- UINTERFACE declarations: 0

**Main Subdirectories:**
- **Private/Base/** - Base classes (2 files)
- **Private/Components/** - Component system (7 files)
  - **Coordination/** - Component coordination
  - **Core/** - Core component functionality (5 files)
  - **Integration/** - Integration components (2 files)
  - **Network/** - Network-related components (3 files)
  - **Presentation/** - Presentation components (3 files)
  - **Rules/** - Rules system (6 files)
  - **Transaction/** - Transaction handling
  - **Validation/** - Validation system
- **Private/Services/** - Service implementations (7 files)
- **Private/Subsystems/** - Subsystem implementations
- **Public/Base/** - Public base classes (2 files)
- **Public/Components/** - Public components (7 files)
- **Public/Services/** - Public services (7 files)
- **Public/Subsystems/** - Public subsystems

---

### 5. MedComInventory
**Path:** `/home/user/SuspenseCore/Source/InventorySystem/MedComInventory`

**Statistics:**
- Header Files: 22
- Source Files: 21
- Total Lines of Code: 27,862
- UCLASS declarations: 16
- USTRUCT declarations: 16
- UINTERFACE declarations: 0

**Main Subdirectories:**
- **Private/Base/** - Base classes (5 files)
- **Private/Components/** - Component implementations
- **Private/Debug/** - Debug utilities
- **Private/Events/** - Event handling
- **Private/Network/** - Network synchronization
- **Private/Operations/** - Operation implementations (5 files)
- **Private/Serialization/** - Data serialization
- **Private/Storage/** - Storage systems
- **Private/Templates/** - Template implementations
- **Private/UI/** - UI-related implementations
- **Private/Utility/** - Utility functions
- **Private/Validation/** - Validation systems
- **Public/Base/** - Public base classes (5 files)
- **Public/Operations/** - Public operations (6 files)
- **Public/** directories mirror private structure

---

### 6. MedComInteraction
**Path:** `/home/user/SuspenseCore/Source/InteractionSystem/MedComInteraction`

**Statistics:**
- Header Files: 6
- Source Files: 5
- Total Lines of Code: 3,486
- UCLASS declarations: 5
- USTRUCT declarations: 1
- UINTERFACE declarations: 0

**Main Subdirectories:**
- **Private/Components/** - Component implementations
- **Private/Pickup/** - Pickup system
- **Private/Utils/** - Utility functions (2 files)
- **Public/Components/** - Public components
- **Public/Pickup/** - Public pickup definitions
- **Public/Utils/** - Public utilities (3 files)

---

### 7. MedComUI
**Path:** `/home/user/SuspenseCore/Source/UISystem/MedComUI`

**Statistics:**
- Header Files: 24
- Source Files: 24
- Total Lines of Code: 26,706
- UCLASS declarations: 23
- USTRUCT declarations: 15
- UINTERFACE declarations: 0

**Main Subdirectories:**
- **Private/Components/** - UI components (3 files)
- **Private/DragDrop/** - Drag & drop system
- **Private/Tooltip/** - Tooltip implementations
- **Private/Widgets/** - Widget hierarchy
  - **Base/** - Base widgets (3 files)
  - **DragDrop/** - Drag & drop widgets (2 files)
  - **Equipment/** - Equipment UI (2 files)
  - **HUD/** - HUD widgets (4 files)
  - **Inventory/** - Inventory UI (2 files)
  - **Layout/** - Layout widgets (2 files)
  - **Screens/** - Screen definitions
  - **Tabs/** - Tab widgets
  - **Tooltip/** - Tooltip widgets
- **Public/Components/** - Public components (3 files)
- **Public/Widgets/** - Public widget hierarchy (mirrors private structure)

---

## Key Insights

### Code Distribution
- **Largest module:** MedComEquipment (54,213 LOC, 54% of equipment system complexity)
- **Second largest:** MedComInventory (27,862 LOC)
- **Third largest:** MedComShared (26,680 LOC)
- **Smallest module:** MedComInteraction (3,486 LOC, lightweight system)

### UClass Distribution
- **Most UCLASS declarations:** MedComEquipment (38 classes)
- **Second most:** MedComGAS (22 classes)
- **Third most:** MedComUI (23 classes)

### USTRUCT Distribution
- **Highest:** MedComShared (85 structs) - data structure heavy
- **Second highest:** MedComEquipment (77 structs)
- **Total structs:** 196 across all modules

### UINTERFACE Distribution
- **Only MedComShared uses interfaces:** 60 UINTERFACE declarations
- All other modules use direct class inheritance instead

### Architecture Patterns
1. **MedComShared:** Interface-heavy, provides contracts for all systems
2. **MedComGAS:** Ability system focused with attributes and effects
3. **MedComCore:** Lightweight core with character and system definitions
4. **MedComEquipment:** Complex multi-layered system with services and subsystems
5. **MedComInventory:** Operation-driven with validation and serialization
6. **MedComInteraction:** Minimal, focused pickup interaction system
7. **MedComUI:** Widget-heavy UI system with multiple widget types
