# SuspenseCore Migration Pipeline

> **✅ МИГРАЦИЯ ЗАВЕРШЕНА (2025-11-28)**
>
> Этот план миграции был успешно выполнен. Весь код мигрирован и компилируется.
> Документ сохранён для исторических и справочных целей.

**Document Version:** 2.0 (Completed)
**Created:** 2025-11-24
**Completed:** 2025-11-28
**Original Estimate:** 18-22 weeks
**Actual Status:** ✅ COMPLETED
**Final Result:** ~155,000 LOC successfully migrated, compiles without errors

---

## Table of Contents

1. [Migration Overview](#1-migration-overview)
2. [Wave Strategy](#2-wave-strategy)
3. [Wave 1: Foundation (MedComShared)](#3-wave-1-foundation-medcomshared)
4. [Wave 2: Core Systems](#4-wave-2-core-systems)
5. [Wave 3: Primary Systems](#5-wave-3-primary-systems)
6. [Wave 4: Complex Systems](#6-wave-4-complex-systems)
7. [Wave 5: UI Layer](#7-wave-5-ui-layer)
8. [Testing Strategy](#8-testing-strategy)
9. [Rollback Procedures](#9-rollback-procedures)

---

## 1. Migration Overview

### 1.1 Total Scope

| Metric | Count |
|--------|-------|
| **Total Modules** | 7 legacy modules |
| **Total Files** | 209 headers + 158 source = 367 files |
| **Total LOC** | 155,588 lines of code |
| **Total Classes** | 118 UCLASS |
| **Total Structs** | 196 USTRUCT |
| **Total Interfaces** | 60 UINTERFACE |
| **Estimated Developer Time** | 18-22 weeks |
| **Team Size** | 2-3 engineers recommended |

### 1.2 Migration Principles

1. **Bottom-Up Approach** - Migrate dependencies first
2. **Incremental Delivery** - Each wave is testable and deployable
3. **Zero Downtime** - Maintain compatibility during migration
4. **Safety First** - Comprehensive testing at each stage
5. **Reversibility** - Clear rollback procedures for each wave

### 1.3 Success Metrics

- ✅ All unit tests passing
- ✅ All integration tests passing
- ✅ No performance regression (>5%)
- ✅ Blueprint compatibility maintained
- ✅ Network replication working
- ✅ Save file migration successful
- ✅ Zero critical bugs introduced

---

## 2. Wave Strategy

### 2.1 Wave Organization

```
Wave 1: MedComShared (Foundation)
  ├─ 60 interfaces
  ├─ 85 structs
  ├─ 7 classes
  └─ Time: 4 weeks

Wave 2: Core Systems (GAS + Core + Interaction)
  ├─ MedComGAS (8,003 LOC)
  ├─ MedComCore (8,638 LOC)
  ├─ MedComInteraction (3,486 LOC)
  └─ Time: 3 weeks

Wave 3: Primary Systems (Inventory)
  ├─ MedComInventory (27,862 LOC)
  └─ Time: 4 weeks

Wave 4: Complex Systems (Equipment)
  ├─ MedComEquipment (54,213 LOC)
  └─ Time: 8 weeks

Wave 5: UI Layer
  ├─ MedComUI (26,706 LOC)
  └─ Time: 4 weeks
```

### 2.2 Dependency Flow

```
MedComShared (Wave 1)
    ↓
    ├─→ MedComGAS (Wave 2)
    ├─→ MedComCore (Wave 2)
    └─→ MedComInteraction (Wave 2)
         ↓
         MedComInventory (Wave 3)
              ↓
              MedComEquipment (Wave 4)
                   ↓
                   MedComUI (Wave 5)
```

### 2.3 Parallel Work Opportunities

**Can work in parallel:**
- Wave 2: All three modules (GAS, Core, Interaction) can be migrated simultaneously
- Within waves: Different subsystems can be migrated by different engineers

**Must be sequential:**
- Waves must complete in order (Wave 1 → 2 → 3 → 4 → 5)
- Within complex modules: Tier-based migration (Foundation → Business Logic → UI)

---

## 3. Wave 1: Foundation (MedComShared)

**Duration:** 4 weeks
**LOC:** 26,680
**Files:** 86 headers, 37 source
**Complexity:** HIGH (foundation for all other modules)
**Risk:** CRITICAL (breaks everything if done wrong)

### Phase 1: Interfaces Migration (1 week)

**Tasks:**
- [ ] **Day 1: Preparation**
  - [ ] Create feature branch `migrate/wave1-shared-foundation`
  - [ ] Document all 60 interfaces and their implementations
  - [ ] Create interface dependency graph
  - [ ] Set up automated testing for interface compliance
  - [ ] Backup all existing interface files

- [ ] **Day 2-3: Core Interfaces (Priority 1)**
  - [ ] Migrate `IMedComInventoryInterface` → `ISuspenseInventory`
  - [ ] Migrate `IMedComEquipmentInterface` → `ISuspenseEquipment`
  - [ ] Migrate `IMedComInteractableInterface` → `ISuspenseInteractable`
  - [ ] Migrate `IMedComPickupInterface` → `ISuspensePickup`
  - [ ] Update all forward declarations
  - [ ] Test compilation after each interface

- [ ] **Day 4-5: Subsystem Interfaces (Priority 2)**
  - [ ] Migrate all UI interfaces (10 interfaces)
  - [ ] Migrate all ability interfaces (8 interfaces)
  - [ ] Migrate all weapon interfaces (6 interfaces)
  - [ ] Migrate all core service interfaces (12 interfaces)
  - [ ] Update include paths

**Testing:**
- Compilation test after each interface
- Blueprint compatibility check
- Interface inheritance validation

**Acceptance Criteria:**
- ✅ All 60 interfaces renamed and compiling
- ✅ No circular dependencies introduced
- ✅ All Blueprint interfaces accessible
- ✅ Zero compilation errors

**Time Estimate:** 5 working days (1 week)

---

### Phase 2: Types Migration (1 week)

**Tasks:**
- [ ] **Day 1: Core Data Structures**
  - [ ] Migrate `FInventoryItemInstance` → `FSuspenseItemInstance` (CRITICAL)
    - Consider keeping old name as alias temporarily
    - Update all references (500+ locations)
  - [ ] Migrate `FInventoryCell` → `FSuspenseInventoryCell`
  - [ ] Migrate `FMedComUnifiedItemData` → `FSuspenseItemData`
  - [ ] Test DataTable compatibility

- [ ] **Day 2: Inventory Types**
  - [ ] Migrate all inventory structs (16 structs)
  - [ ] Migrate `EInventoryErrorCode` → `ESuspenseInventoryError`
  - [ ] Migrate `FInventoryOperationResult` → `FSuspenseInventoryResult`
  - [ ] Update all enum usage

- [ ] **Day 3: Equipment Types**
  - [ ] Migrate all equipment structs (77 structs)
  - [ ] Migrate equipment enums
  - [ ] Migrate slot type definitions
  - [ ] Test equipment data tables

- [ ] **Day 4: UI Types**
  - [ ] Migrate all UI data structures (15 structs)
  - [ ] Migrate widget data types
  - [ ] Update UI bindings

- [ ] **Day 5: Testing & Validation**
  - [ ] Full compilation test
  - [ ] DataTable loading test
  - [ ] Struct replication test
  - [ ] Blueprint struct usage test

**Testing:**
- Struct replication validation
- DataTable import/export
- Blueprint struct access
- Serialization compatibility

**Acceptance Criteria:**
- ✅ All 196 structs migrated
- ✅ DataTables load correctly
- ✅ Struct replication working
- ✅ Blueprint access maintained

**Time Estimate:** 5 working days (1 week)

---

### Phase 3: Core Services Migration (1 week)

**Tasks:**
- [ ] **Day 1-2: Item Manager**
  - [ ] Migrate `UMedComItemManager` → `USuspenseCoreItemManager`
  - [ ] Update DataTable integration
  - [ ] Update cached item data system
  - [ ] Test item lookup performance
  - [ ] Validate item data loading

- [ ] **Day 3-4: Event System**
  - [ ] Migrate `UEventDelegateManager` → `USuspenseEventManager`
  - [ ] Update all delegate declarations
  - [ ] Migrate event type definitions
  - [ ] Test event broadcasting
  - [ ] Validate subscription system

- [ ] **Day 5: Testing**
  - [ ] Integration test: ItemManager + EventManager
  - [ ] Performance benchmarking
  - [ ] Memory leak check
  - [ ] Thread safety validation

**Testing:**
- DataTable integration test
- Event broadcasting test
- Performance profiling
- Multi-threaded access test

**Acceptance Criteria:**
- ✅ ItemManager fully functional
- ✅ EventManager working correctly
- ✅ No performance regression
- ✅ Thread-safe operations

**Time Estimate:** 5 working days (1 week)

---

### Phase 4: Utilities & Cleanup (1 week)

**Tasks:**
- [ ] **Day 1-2: Helper Classes**
  - [ ] Migrate `UMedComStaticHelpers` → `USuspenseHelpers`
  - [ ] Update Blueprint function library
  - [ ] Migrate logging categories
  - [ ] Update debug utilities

- [ ] **Day 3: Documentation**
  - [ ] Update all header documentation
  - [ ] Generate API documentation
  - [ ] Update migration guide
  - [ ] Document breaking changes

- [ ] **Day 4: Integration Testing**
  - [ ] Full module integration test
  - [ ] Cross-module dependency test
  - [ ] Blueprint compatibility test
  - [ ] Network replication test

- [ ] **Day 5: Code Review & Polish**
  - [ ] Code review session
  - [ ] Fix identified issues
  - [ ] Final compilation test
  - [ ] Prepare for Wave 2

**Testing:**
- Full module compilation
- Blueprint library access
- Integration with dependent modules
- Documentation completeness

**Acceptance Criteria:**
- ✅ All utilities migrated
- ✅ Documentation complete
- ✅ Code review approved
- ✅ Ready for Wave 2

**Time Estimate:** 5 working days (1 week)

---

**Wave 1 Total:** 4 weeks (20 working days)

**Dependencies:** None (foundation)
**Blocks:** All other waves
**Risk Factors:**
- ⚠️ Interface changes break all dependent modules
- ⚠️ Struct changes break save file compatibility
- ⚠️ Event system changes affect all game logic

**Mitigation:**
- Maintain old names as deprecated aliases
- Version bump for serialization
- Extensive integration testing
- Phased rollout with backwards compatibility

---

## 4. Wave 2: Core Systems

**Duration:** 3 weeks total (can parallelize)
**Total LOC:** 20,127
**Modules:** MedComGAS, MedComCore, MedComInteraction

### 4.1 MedComGAS Migration (1 week)

**Duration:** 1 week
**LOC:** 8,003
**Files:** 23 headers, 23 source
**Complexity:** MEDIUM
**Risk:** MEDIUM (GAS integration critical)

#### Phase 1: Abilities Migration (2 days)

**Tasks:**
- [ ] **Day 1: Core Abilities**
  - [ ] Audit all 7 ability classes
  - [ ] Migrate base ability classes
  - [ ] Update ability granting logic
  - [ ] Test ability activation

- [ ] **Day 2: Specialized Abilities**
  - [ ] Migrate inventory abilities
  - [ ] Migrate equipment abilities
  - [ ] Migrate interaction abilities
  - [ ] Integration test with ASC

**Testing:**
- Ability activation test
- Cooldown system test
- Cost validation test
- Network replication test

**Acceptance Criteria:**
- ✅ All 7 abilities migrated
- ✅ Ability system functional
- ✅ No activation issues

**Time Estimate:** 2 days

---

#### Phase 2: Attributes Migration (1 day)

**Tasks:**
- [ ] Migrate all 5 attribute sets
- [ ] Update attribute replication
- [ ] Test attribute clamping
- [ ] Validate gameplay effects

**Testing:**
- Attribute replication test
- Min/max clamping test
- Gameplay effect application test

**Acceptance Criteria:**
- ✅ All 5 attribute sets migrated
- ✅ Replication working
- ✅ No attribute calculation errors

**Time Estimate:** 1 day

---

#### Phase 3: Effects Migration (2 days)

**Tasks:**
- [ ] **Day 1: Core Effects**
  - [ ] Migrate 8 gameplay effects
  - [ ] Update effect calculations
  - [ ] Test duration effects
  - [ ] Test instant effects

- [ ] **Day 2: Integration**
  - [ ] Test ability + effect combinations
  - [ ] Test effect stacking
  - [ ] Performance profiling
  - [ ] Network bandwidth test

**Testing:**
- Effect application test
- Effect stacking test
- Effect removal test
- Performance test

**Acceptance Criteria:**
- ✅ All 8 effects migrated
- ✅ Effect system working
- ✅ No calculation errors

**Time Estimate:** 2 days

---

**MedComGAS Total:** 5 working days (1 week)

**Dependencies:** MedComShared (Wave 1)
**Risk Factors:**
- ⚠️ Attribute calculation errors
- ⚠️ Ability activation failures
- ⚠️ Effect stacking issues

---

### 4.2 MedComCore Migration (1 week)

**Duration:** 1 week
**LOC:** 8,638
**Files:** 8 headers, 8 source
**Complexity:** LOW-MEDIUM
**Risk:** LOW (well-focused module)

#### Phase 1: Character Classes (2 days)

**Tasks:**
- [ ] **Day 1: Base Character**
  - [ ] Migrate `AMedComCharacter` → `ASuspenseCharacter`
  - [ ] Update component initialization
  - [ ] Update GAS integration
  - [ ] Test character spawning

- [ ] **Day 2: Player Character**
  - [ ] Migrate player-specific logic
  - [ ] Update input handling
  - [ ] Test player controller integration
  - [ ] Test character possession

**Testing:**
- Character spawning test
- Component initialization test
- Network replication test
- Input handling test

**Acceptance Criteria:**
- ✅ 2 character classes migrated
- ✅ Characters spawn correctly
- ✅ All components initialized

**Time Estimate:** 2 days

---

#### Phase 2: Core Systems (2 days)

**Tasks:**
- [ ] **Day 1: Core Components**
  - [ ] Migrate 5 core classes
  - [ ] Update subsystem integration
  - [ ] Test component communication

- [ ] **Day 2: Integration**
  - [ ] Full integration test
  - [ ] Blueprint compatibility test
  - [ ] Network test
  - [ ] Performance test

**Testing:**
- System integration test
- Component lifecycle test
- Network synchronization test

**Acceptance Criteria:**
- ✅ All 5 core classes migrated
- ✅ Systems fully integrated
- ✅ No initialization issues

**Time Estimate:** 2 days

---

#### Phase 3: Testing & Documentation (1 day)

**Tasks:**
- [ ] Final integration testing
- [ ] Update documentation
- [ ] Code review
- [ ] Prepare for Wave 3

**Time Estimate:** 1 day

---

**MedComCore Total:** 5 working days (1 week)

**Dependencies:** MedComShared (Wave 1), MedComGAS (Wave 2.1)
**Risk Factors:**
- ⚠️ Character initialization order
- ⚠️ Component dependency issues

---

### 4.3 MedComInteraction Migration (3 days)

**Duration:** 3 days
**LOC:** 3,486
**Files:** 6 headers, 5 source
**Complexity:** LOW
**Risk:** LOW (smallest, well-designed module)

#### Phase 1: Interaction Component (1 day)

**Tasks:**
- [ ] Migrate `UMedComInteractionComponent` → `USuspenseInteractionComponent`
- [ ] Update trace logic
- [ ] Update GAS integration
- [ ] Test interaction detection

**Testing:**
- Trace detection test
- Cooldown test
- GAS ability activation test

**Acceptance Criteria:**
- ✅ Component migrated
- ✅ Tracing working
- ✅ Abilities triggering

**Time Estimate:** 1 day

---

#### Phase 2: Pickup System (1 day)

**Tasks:**
- [ ] Migrate `AMedComBasePickupItem` → `ASuspensePickupItem`
- [ ] Update initialization modes
- [ ] Update weapon state preservation
- [ ] Test pickup/drop

**Testing:**
- Pickup spawn test
- Weapon state persistence test
- Replication test

**Acceptance Criteria:**
- ✅ Pickup actor migrated
- ✅ State preservation working
- ✅ Replication functional

**Time Estimate:** 1 day

---

#### Phase 3: Factory & Utilities (1 day)

**Tasks:**
- [ ] Migrate `UMedComItemFactory` → `USuspenseItemFactory`
- [ ] Migrate `UMedComStaticHelpers` → `USuspenseHelpers`
- [ ] Integration testing
- [ ] Documentation

**Testing:**
- Factory creation test
- Helper function test
- Full system integration test

**Acceptance Criteria:**
- ✅ All utilities migrated
- ✅ Factory working correctly
- ✅ No helper function issues

**Time Estimate:** 1 day

---

**MedComInteraction Total:** 3 working days

**Dependencies:** MedComShared (Wave 1), MedComCore (Wave 2.2)
**Risk Factors:**
- ⚠️ Minimal - well-designed module

---

**Wave 2 Total:** 3 weeks (15 working days with parallelization)

**Note:** MedComGAS, MedComCore, and MedComInteraction can be migrated in parallel by different team members.

---

## 5. Wave 3: Primary Systems (MedComInventory)

**Duration:** 4 weeks
**LOC:** 27,862
**Files:** 22 headers, 21 source
**Complexity:** MEDIUM-HIGH
**Risk:** MEDIUM-HIGH (critical gameplay system)

### Phase 1: Foundation Types (1 week)

**Tasks:**
- [ ] **Day 1-2: Core Data Structures**
  - [ ] Decide on `FInventoryItemInstance` strategy (keep or rename)
  - [ ] Migrate all inventory structs (16 structs)
  - [ ] Update serialization version
  - [ ] Test data compatibility

- [ ] **Day 3-4: Interfaces & Enums**
  - [ ] Migrate all inventory interfaces
  - [ ] Migrate error codes
  - [ ] Migrate operation types
  - [ ] Update result structures

- [ ] **Day 5: Testing**
  - [ ] Struct replication test
  - [ ] Serialization round-trip test
  - [ ] Interface compliance test

**Testing:**
- Data structure integrity
- Replication validation
- Serialization compatibility
- Interface implementation check

**Acceptance Criteria:**
- ✅ All types migrated
- ✅ Serialization working
- ✅ No data loss

**Time Estimate:** 5 days (1 week)

---

### Phase 2: Core Components (1 week)

**Tasks:**
- [ ] **Day 1-2: Storage Layer**
  - [ ] Migrate `UMedComInventoryStorage` → `USuspenseInventoryStorage`
  - [ ] Update grid management
  - [ ] Test placement algorithms
  - [ ] Validate bitmap optimization

- [ ] **Day 3-4: Validation Layer**
  - [ ] Migrate `UMedComInventoryConstraints` → `USuspenseInventoryValidator`
  - [ ] Update weight validation
  - [ ] Update type restrictions
  - [ ] Update grid bounds validation

- [ ] **Day 5: Integration**
  - [ ] Test storage + validation together
  - [ ] Performance profiling
  - [ ] Memory usage check

**Testing:**
- Grid placement test
- Validation logic test
- Integration test
- Performance benchmark

**Acceptance Criteria:**
- ✅ Storage fully functional
- ✅ Validation working correctly
- ✅ No performance regression

**Time Estimate:** 5 days (1 week)

---

### Phase 3: Business Logic (1 week)

**Tasks:**
- [ ] **Day 1-2: Operations System**
  - [ ] Migrate all operation classes (4 operations)
  - [ ] Update undo/redo system
  - [ ] Test operation execution
  - [ ] Test rollback

- [ ] **Day 3-4: Transaction System**
  - [ ] Migrate `UMedComInventoryTransaction` → `USuspenseInventoryTransaction`
  - [ ] Update atomic operations
  - [ ] Test transaction rollback
  - [ ] Integration with operations

- [ ] **Day 5: Events & Serialization**
  - [ ] Migrate `UMedComInventoryEvents` → `USuspenseInventoryEvents`
  - [ ] Migrate `UMedComInventorySerializer` → `USuspenseInventorySerializer`
  - [ ] Test event broadcasting
  - [ ] Test save/load

**Testing:**
- Operation undo/redo test
- Transaction atomicity test
- Event broadcasting test
- Serialization test

**Acceptance Criteria:**
- ✅ Operations working
- ✅ Transactions atomic
- ✅ Events firing correctly
- ✅ Save/load functional

**Time Estimate:** 5 days (1 week)

---

### Phase 4: Network & Main Component (1 week)

**Tasks:**
- [ ] **Day 1-2: Network Replication**
  - [ ] Migrate `UInventoryReplicator` → `USuspenseInventoryReplicator`
  - [ ] Update FastArray serialization
  - [ ] Test delta compression
  - [ ] Test client-server sync

- [ ] **Day 3-4: Main Component**
  - [ ] Migrate `UMedComInventoryComponent` → `USuspenseInventoryComponent`
  - [ ] Update all sub-component references
  - [ ] Test initialization from loadout
  - [ ] Test all public API methods

- [ ] **Day 5: Final Integration**
  - [ ] Full system integration test
  - [ ] Multiplayer stress test
  - [ ] Performance profiling
  - [ ] Code review

**Testing:**
- Network replication test
- Client-server synchronization
- API functionality test
- Multiplayer test

**Acceptance Criteria:**
- ✅ Replication working
- ✅ Main component functional
- ✅ All API methods working
- ✅ Multiplayer stable

**Time Estimate:** 5 days (1 week)

---

**Wave 3 Total:** 4 weeks (20 working days)

**Dependencies:** Wave 1, Wave 2
**Blocks:** Wave 4 (Equipment), Wave 5 (UI)
**Risk Factors:**
- ⚠️ Network replication breaks
- ⚠️ Save file incompatibility
- ⚠️ Performance regression
- ⚠️ Blueprint API changes

**Mitigation:**
- Extensive multiplayer testing
- Save file migration layer
- Performance benchmarking
- Blueprint compatibility layer

---

## 6. Wave 4: Complex Systems (MedComEquipment)

**Duration:** 8 weeks
**LOC:** 54,213 (LARGEST MODULE)
**Files:** 40 headers, 40 source
**Complexity:** VERY HIGH
**Risk:** CRITICAL (most complex system)

### Phase 1: Foundation & Base Classes (2 weeks)

**Tasks:**
- [ ] **Week 1: Base Actors**
  - [ ] Migrate `AEquipmentActorBase` → `ASuspenseEquipmentActor`
  - [ ] Migrate `AWeaponActor` → `ASuspenseWeapon`
  - [ ] Update component initialization
  - [ ] Test actor spawning
  - [ ] Validate visual components

- [ ] **Week 2: Core Data Store**
  - [ ] Migrate `UMedComEquipmentDataStore` → `USuspenseEquipmentDataStore`
  - [ ] Update thread-safe operations
  - [ ] Test data access patterns
  - [ ] Performance profiling
  - [ ] Integration testing

**Testing:**
- Actor spawning test
- Component attachment test
- Data store thread safety test
- Performance benchmark

**Acceptance Criteria:**
- ✅ Base actors working
- ✅ Data store functional
- ✅ Thread-safe operations
- ✅ No initialization issues

**Time Estimate:** 10 working days (2 weeks)

---

### Phase 2: Service Layer (2 weeks)

**Tasks:**
- [ ] **Week 1: Core Services (Part 1)**
  - [ ] Migrate `UEquipmentOperationService` → `USuspenseEquipmentOperationService`
  - [ ] Migrate `UEquipmentDataProviderService` → `USuspenseEquipmentDataService`
  - [ ] Migrate `UEquipmentValidationService` → `USuspenseEquipmentValidationService`
  - [ ] Test service integration

- [ ] **Week 2: Core Services (Part 2)**
  - [ ] Migrate `UEquipmentNetworkService` → `USuspenseEquipmentNetworkService`
  - [ ] Migrate `UEquipmentVisualService` → `USuspenseEquipmentVisualService`
  - [ ] Migrate `UEquipmentAbilityService` → `USuspenseEquipmentAbilityService`
  - [ ] Migrate service locator system
  - [ ] Integration testing

**Testing:**
- Service registration test
- Service discovery test
- Service communication test
- Performance test

**Acceptance Criteria:**
- ✅ All 7 services migrated
- ✅ Service locator working
- ✅ Service communication functional
- ✅ No initialization deadlocks

**Time Estimate:** 10 working days (2 weeks)

---

### Phase 3: Component Subsystems (2 weeks)

**Tasks:**
- [ ] **Week 1: Core Components**
  - [ ] Migrate Core/ subsystem (5 components)
    - EquipmentDataStore ✓ (done in Phase 1)
    - SystemCoordinator
    - EquipmentInventoryBridge
    - EquipmentOperationExecutor
    - WeaponStateManager
  - [ ] Test component lifecycle
  - [ ] Integration testing

- [ ] **Week 2: Specialized Components**
  - [ ] Migrate Network/ subsystem (3 components)
  - [ ] Migrate Presentation/ subsystem (3 components)
  - [ ] Migrate Integration/ subsystem (2 components)
  - [ ] Integration testing

**Testing:**
- Component initialization test
- Inter-component communication test
- Network replication test
- Visual presentation test

**Acceptance Criteria:**
- ✅ All component subsystems migrated
- ✅ Components communicating correctly
- ✅ No dependency issues

**Time Estimate:** 10 working days (2 weeks)

---

### Phase 4: Rules & Transaction Systems (2 weeks)

**Tasks:**
- [ ] **Week 1: Rules Engine**
  - [ ] Migrate all 6 rules components
    - RulesCoordinator
    - CompatibilityRulesEngine
    - RequirementRulesEngine
    - WeightRulesEngine
    - ConflictRulesEngine
    - EquipmentRulesEngine
  - [ ] Test validation pipeline
  - [ ] Performance profiling

- [ ] **Week 2: Transaction System**
  - [ ] Migrate `UMedComEquipmentTransactionProcessor` → `USuspenseEquipmentTransactionProcessor`
  - [ ] Update ACID compliance
  - [ ] Test rollback mechanisms
  - [ ] Test nested transactions
  - [ ] Integration testing

**Testing:**
- Validation pipeline test
- Transaction atomicity test
- Rollback test
- Conflict detection test

**Acceptance Criteria:**
- ✅ Rules engine working
- ✅ Transactions atomic
- ✅ Rollback functional
- ✅ Validation accurate

**Time Estimate:** 10 working days (2 weeks)

---

**Wave 4 Total:** 8 weeks (40 working days)

**Dependencies:** Wave 1, Wave 2, Wave 3
**Blocks:** Wave 5 (UI)
**Risk Factors:**
- ⚠️ Service initialization order
- ⚠️ Component dependency cycles
- ⚠️ Transaction deadlocks
- ⚠️ Network replication complexity
- ⚠️ Performance degradation

**Mitigation:**
- Phased subsystem migration
- Extensive integration testing
- Performance profiling at each phase
- Service initialization sequencing
- Lock ordering validation

---

## 7. Wave 5: UI Layer (MedComUI)

**Duration:** 4 weeks
**LOC:** 26,706
**Files:** 24 headers, 24 source
**Complexity:** MEDIUM-HIGH
**Risk:** MEDIUM (extensive Blueprint usage)

### Phase 1: UI Bridges & Managers (1 week)

**Tasks:**
- [ ] **Day 1-2: Inventory Bridge**
  - [ ] Migrate `UMedComInventoryUIBridge` → `USuspenseInventoryUIAdapter`
  - [ ] Update data conversion logic
  - [ ] Test batched updates
  - [ ] Test drag-drop integration

- [ ] **Day 3-4: Equipment Bridge & Managers**
  - [ ] Migrate `UMedComEquipmentUIBridge` → `USuspenseEquipmentUIAdapter`
  - [ ] Migrate `UMedComUIManager` → `USuspenseUIManager`
  - [ ] Migrate `UMedComDragDropHandler` → `USuspenseDragDropHandler`
  - [ ] Migrate `UMedComTooltipManager` → `USuspenseTooltipManager`

- [ ] **Day 5: Testing**
  - [ ] UI bridge integration test
  - [ ] Manager functionality test
  - [ ] Event flow test

**Testing:**
- Data conversion test
- Update batching test
- Manager lifecycle test
- Event broadcasting test

**Acceptance Criteria:**
- ✅ Bridges fully functional
- ✅ Managers working correctly
- ✅ No data conversion errors

**Time Estimate:** 5 days (1 week)

---

### Phase 2: Widget Base Classes (1 week)

**Tasks:**
- [ ] **Day 1-2: Base Widgets**
  - [ ] Migrate `UMedComBaseWidget` → `USuspenseBaseWidget`
  - [ ] Migrate `UMedComBaseContainerWidget` → `USuspenseBaseContainerWidget`
  - [ ] Migrate `UMedComBaseSlotWidget` → `USuspenseBaseSlotWidget`
  - [ ] Migrate `UMedComBaseLayoutWidget` → `USuspenseBaseLayoutWidget`

- [ ] **Day 3-4: Specialized Bases**
  - [ ] Update lifecycle methods
  - [ ] Update event integration
  - [ ] Test widget pooling
  - [ ] Test update batching

- [ ] **Day 5: Testing**
  - [ ] Widget lifecycle test
  - [ ] Pooling performance test
  - [ ] Event integration test

**Testing:**
- Widget creation test
- Lifecycle management test
- Pooling efficiency test
- Event binding test

**Acceptance Criteria:**
- ✅ All base widgets migrated
- ✅ Lifecycle working correctly
- ✅ Pooling functional
- ✅ Events firing

**Time Estimate:** 5 days (1 week)

---

### Phase 3: Specialized Widgets (1 week)

**Tasks:**
- [ ] **Day 1-2: Inventory Widgets**
  - [ ] Migrate `UMedComInventoryWidget` → `USuspenseInventoryWidget`
  - [ ] Migrate `UMedComInventorySlotWidget` → `USuspenseInventorySlotWidget`
  - [ ] Test grid rendering
  - [ ] Test drag-drop

- [ ] **Day 3-4: Equipment & HUD Widgets**
  - [ ] Migrate equipment container widgets (2 widgets)
  - [ ] Migrate equipment slot widgets (1 widget)
  - [ ] Migrate HUD widgets (4 widgets)
  - [ ] Test equipment UI flow

- [ ] **Day 5: Utility Widgets**
  - [ ] Migrate tooltip widgets
  - [ ] Migrate tab bar widgets
  - [ ] Migrate layout widgets
  - [ ] Integration testing

**Testing:**
- Widget rendering test
- Drag-drop functionality test
- Equipment UI integration test
- Tooltip display test

**Acceptance Criteria:**
- ✅ All specialized widgets migrated
- ✅ UI interactions working
- ✅ No visual glitches

**Time Estimate:** 5 days (1 week)

---

### Phase 4: Integration & Blueprint Migration (1 week)

**Tasks:**
- [ ] **Day 1-2: Blueprint Widget Migration**
  - [ ] Identify all Blueprint widgets (WBP_*)
  - [ ] Update parent class references
  - [ ] Update widget bindings
  - [ ] Test Blueprint compilation

- [ ] **Day 3-4: Full UI Integration**
  - [ ] Test complete UI flow
  - [ ] Test inventory → equipment → UI pipeline
  - [ ] Test multiplayer UI synchronization
  - [ ] Performance profiling

- [ ] **Day 5: Final Polish**
  - [ ] Fix visual issues
  - [ ] Optimize update frequency
  - [ ] Code review
  - [ ] Documentation

**Testing:**
- Blueprint widget test
- Complete UI flow test
- Multiplayer UI test
- Performance benchmark

**Acceptance Criteria:**
- ✅ All Blueprints updated
- ✅ UI fully functional
- ✅ Multiplayer UI working
- ✅ No performance issues

**Time Estimate:** 5 days (1 week)

---

**Wave 5 Total:** 4 weeks (20 working days)

**Dependencies:** All previous waves
**Risk Factors:**
- ⚠️ Blueprint breaking changes
- ⚠️ Widget binding issues
- ⚠️ Event flow disruption
- ⚠️ UI performance regression

**Mitigation:**
- Blueprint meta redirects
- Extensive UI testing
- Performance profiling
- Visual regression testing

---

## 8. Testing Strategy

### 8.1 Testing Levels

**Unit Tests (Per Class):**
- Individual class functionality
- Edge cases and error conditions
- Performance benchmarks
- Thread safety (where applicable)

**Integration Tests (Per Wave):**
- Module-to-module communication
- Cross-system functionality
- Network replication
- Save/load functionality

**System Tests (Per Wave):**
- End-to-end gameplay scenarios
- Multiplayer sessions
- Performance under load
- Memory leak detection

**Regression Tests (Continuous):**
- Automated test suite
- Performance monitoring
- Compatibility checks
- Blueprint validation

### 8.2 Test Coverage Goals

| Wave | Unit Test Coverage | Integration Test Coverage |
|------|-------------------|--------------------------|
| Wave 1 (Shared) | 80%+ | 90%+ (critical) |
| Wave 2 (Core) | 70%+ | 80%+ |
| Wave 3 (Inventory) | 75%+ | 85%+ |
| Wave 4 (Equipment) | 75%+ | 90%+ (complex) |
| Wave 5 (UI) | 60%+ (visual) | 80%+ |

### 8.3 Performance Benchmarks

**Baseline Measurements (Before Migration):**
- [ ] Inventory operation latency
- [ ] Equipment swap time
- [ ] UI update frequency
- [ ] Network bandwidth usage
- [ ] Memory footprint
- [ ] Load times

**Acceptance Thresholds:**
- ✅ No more than 5% performance regression
- ✅ No memory leaks
- ✅ Network bandwidth maintained or improved
- ✅ Load times maintained

---

## 9. Rollback Procedures

### 9.1 Version Control Strategy

**Branch Structure:**
```
main
├── migrate/wave1-shared-foundation
├── migrate/wave2-core-systems
├── migrate/wave3-inventory
├── migrate/wave4-equipment
└── migrate/wave5-ui
```

**Tagging:**
- `pre-migration-baseline` - Before any changes
- `wave1-complete` - After Wave 1
- `wave2-complete` - After Wave 2
- etc.

### 9.2 Rollback Triggers

**Automatic Rollback Conditions:**
- Critical test failures (>10% tests failing)
- Crash on startup
- Memory leaks detected
- Network replication broken
- Save file corruption

**Manual Rollback Decision:**
- Performance regression >10%
- Extensive Blueprint breakage
- Unforeseen architectural issues
- Team consensus

### 9.3 Rollback Procedure (Per Wave)

1. **Stop all development** on affected wave
2. **Revert to previous tag**
   ```bash
   git checkout <previous-wave-tag>
   ```
3. **Analyze failure** - Document what went wrong
4. **Create recovery plan** - Address root cause
5. **Retry migration** with fixes
6. **Additional testing** before re-attempting

---

## Summary

**Total Timeline:** 18-22 weeks (4.5-5.5 months)

| Wave | Duration | Complexity | Risk | Can Parallelize |
|------|----------|------------|------|-----------------|
| Wave 1 (Shared) | 4 weeks | HIGH | CRITICAL | No |
| Wave 2 (Core) | 3 weeks | MEDIUM | MEDIUM | Yes (3 modules) |
| Wave 3 (Inventory) | 4 weeks | MEDIUM-HIGH | MEDIUM-HIGH | No |
| Wave 4 (Equipment) | 8 weeks | VERY HIGH | CRITICAL | Partial (subsystems) |
| Wave 5 (UI) | 4 weeks | MEDIUM-HIGH | MEDIUM | Partial (widgets) |

**Success Factors:**
1. ✅ Disciplined phased approach
2. ✅ Comprehensive testing at each stage
3. ✅ Clear acceptance criteria
4. ✅ Rollback procedures in place
5. ✅ Team collaboration and code reviews
6. ✅ Continuous integration and monitoring

**Risk Management:**
- Foundation-first approach minimizes ripple effects
- Parallel work in Wave 2 optimizes timeline
- Extensive testing prevents regressions
- Clear rollback procedures provide safety net

---

**Document Status:** ✅ Complete
**Next Steps:** Begin Wave 1 preparation
**Approval Required:** Tech Lead, Project Manager
