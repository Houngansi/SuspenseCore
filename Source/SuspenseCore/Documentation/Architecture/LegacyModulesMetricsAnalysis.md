# Legacy Modules - Metrics and Analysis

## Quick Reference Metrics

### By Size (Lines of Code)

```
1. MedComEquipment    54,213 LOC ████████████████████████████████░░░░░░░ 35%
2. MedComInventory    27,862 LOC █████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░ 18%
3. MedComShared       26,680 LOC █████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░ 17%
4. MedComUI           26,706 LOC █████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░ 17%
5. MedComCore          8,638 LOC ████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 6%
6. MedComGAS           8,003 LOC ███░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 5%
7. MedComInteraction   3,486 LOC █░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 2%
                       ─────────
TOTAL               155,588 LOC
```

### By Class Count (UCLASS)

```
1. MedComEquipment    38 classes ███████████████████░░░░░░░░░░░░░░░░░░░░░ 32%
2. MedComUI          23 classes ████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 19%
3. MedComGAS         22 classes ███████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 19%
4. MedComInventory   16 classes █████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 14%
5. MedComCore         7 classes ████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 6%
6. MedComShared       7 classes ████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 6%
7. MedComInteraction  5 classes ███░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 4%
                        ─────
TOTAL              118 classes
```

### By Struct Count (USTRUCT)

```
1. MedComShared      85 structs ██████████████████████████████████████░░░░░░░░░░ 43%
2. MedComEquipment   77 structs ███████████████████████████████████░░░░░░░░░░░░ 39%
3. MedComInventory   16 structs ████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 8%
4. MedComUI          15 structs ███████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 8%
5. MedComGAS          1 struct  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 1%
6. MedComCore         1 struct  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 1%
7. MedComInteraction  1 struct  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░ 1%
                       ─────
TOTAL             196 structs
```

### By Interface Count (UINTERFACE)

```
1. MedComShared     60 interfaces ██████████████████████████████████████░░░░░░░░░░░░░░░░░░ 100%
2. All Others        0 interfaces (Use direct class inheritance)
                      ─────
TOTAL              60 interfaces
```

---

## Detailed Metrics by Module

### MedComShared - Shared Contracts & Types

| Metric | Value | Notes |
|--------|-------|-------|
| Header Files | 86 | Highest file count |
| Source Files | 37 | More headers than sources (interface-heavy) |
| Total LOC | 26,680 | Third largest by LOC |
| UCLASS | 7 | Minimal classes, contract-focused |
| USTRUCT | 85 | Data-structure heavy (tied 1st) |
| UINTERFACE | 60 | **100% of all interfaces in legacy code** |
| Avg. File Size | 220 LOC | Relatively small files |
| Complexity | HIGH | Provides contracts for all systems |

**Key Insights:**
- Acts as the **foundational interface layer** for all other modules
- High ratio of headers to sources indicates heavy use of inline definitions
- All 60 legacy interfaces are defined here (no interfaces in other modules)
- 85 structs serve as data contracts between systems

---

### MedComEquipment - Complex Multi-Layer System

| Metric | Value | Notes |
|--------|-------|-------|
| Header Files | 40 | Balanced count |
| Source Files | 40 | 1:1 ratio (well-documented) |
| Total LOC | 54,213 | **Largest module by far** |
| UCLASS | 38 | **Most classes** (32% of total) |
| USTRUCT | 77 | Second highest (data-heavy) |
| UINTERFACE | 0 | Uses direct inheritance |
| Avg. File Size | 1,353 LOC | Largest average file size |
| Complexity | VERY HIGH | Most complex system |

**Subdirectory Complexity:**
- Components/ (7 main areas): Coordination, Core, Integration, Network, Presentation, Rules, Transaction, Validation
- Services/ (7 service classes)
- Base/ (2 base classes)
- Subsystems/ (1 subsystem)

**Key Insights:**
- **The heavyweight system** - requires the most attention during refactoring
- Multi-layered architecture (10+ distinct concerns)
- High class count suggests many specialized components
- Good documentation ratio (1:1 header:source)
- Likely candidate for breaking into smaller, focused modules

---

### MedComInventory - Operation-Driven System

| Metric | Value | Notes |
|--------|-------|-------|
| Header Files | 22 | Moderate count |
| Source Files | 21 | Nearly 1:1 ratio |
| Total LOC | 27,862 | Second largest |
| UCLASS | 16 | Mid-range complexity |
| USTRUCT | 16 | Moderate data structures |
| UINTERFACE | 0 | Uses direct inheritance |
| Avg. File Size | 1,268 LOC | Large files |
| Complexity | HIGH | Complex but focused |

**Key Operations:**
- 5-6 operation classes
- 5 base classes
- Supporting: Debug, Events, Network, Serialization, Storage, Templates, UI, Utility, Validation

**Key Insights:**
- **Operation-driven architecture** - well-defined command pattern
- Comprehensive support systems (validation, serialization, network)
- Good separation of concerns (12+ subdirectories)
- Strong focus on data integrity (validation, serialization)

---

### MedComUI - Widget-Heavy UI System

| Metric | Value | Notes |
|--------|-------|-------|
| Header Files | 24 | Moderate count |
| Source Files | 24 | Perfect 1:1 ratio |
| Total LOC | 26,706 | Nearly equal to Inventory |
| UCLASS | 23 | Second most classes |
| USTRUCT | 15 | Moderate structures |
| UINTERFACE | 0 | Uses direct inheritance |
| Avg. File Size | 1,113 LOC | Moderate file size |
| Complexity | MEDIUM-HIGH | Widget-specialized |

**Widget Hierarchy:**
- 9 distinct widget categories: Base, DragDrop, Equipment, HUD, Inventory, Layout, Screens, Tabs, Tooltip
- Supporting: Components, DragDrop system, Tooltip system

**Key Insights:**
- **Rich UI hierarchy** - organized by functional areas
- Clean separation of widget types
- Good documentation (1:1 ratio)
- High reusability potential for UI components

---

### MedComCore - Lightweight Player Core

| Metric | Value | Notes |
|--------|-------|-------|
| Header Files | 8 | Minimal |
| Source Files | 8 | Perfect 1:1 ratio |
| Total LOC | 8,638 | Mid-range |
| UCLASS | 7 | Minimal classes |
| USTRUCT | 1 | Minimal data structures |
| UINTERFACE | 0 | Uses direct inheritance |
| Avg. File Size | 1,080 LOC | Moderate file size |
| Complexity | LOW | Well-focused |

**Structure:**
- Characters/ (2 files) - Character definitions
- Core/ (5 files) - Core systems

**Key Insights:**
- **Simple and focused** - good foundation
- Relies heavily on MedComShared interfaces
- Minimal data structures
- Best practices for size and clarity

---

### MedComGAS - Ability System

| Metric | Value | Notes |
|--------|-------|-------|
| Header Files | 23 | Balanced count |
| Source Files | 23 | Perfect 1:1 ratio |
| Total LOC | 8,003 | Moderate size |
| UCLASS | 22 | High class count (relative to size) |
| USTRUCT | 1 | Minimal data structures |
| UINTERFACE | 0 | Uses direct inheritance |
| Avg. File Size | 348 LOC | Smallest average file size |
| Complexity | MEDIUM | Specialized system |

**Key Systems:**
- Abilities (7 classes)
- Attributes (5 classes)
- Effects (8 classes)
- Components, Subsystems (support)

**Key Insights:**
- **Well-modularized** - each file is focused
- High class-to-LOC ratio shows specialized classes
- Symmetric structure (equal public/private)
- Good candidate for modular design pattern

---

### MedComInteraction - Minimal Interaction System

| Metric | Value | Notes |
|--------|-------|-------|
| Header Files | 6 | Minimal |
| Source Files | 5 | Nearly 1:1 |
| Total LOC | 3,486 | **Smallest module** |
| UCLASS | 5 | Minimal classes |
| USTRUCT | 1 | Minimal structures |
| UINTERFACE | 0 | Uses direct inheritance |
| Avg. File Size | 581 LOC | Moderate file size |
| Complexity | LOW | Very focused |

**Structure:**
- Components (1 file)
- Pickup (1 file)
- Utils (2-3 files)

**Key Insights:**
- **Exemplar of focused design** - does one thing well
- Smallest and simplest system
- Good model for minimal modules
- Could potentially be integrated into another system if needed

---

## Refactoring Priority Analysis

### By Complexity (Highest Priority First)

1. **MedComEquipment** - CRITICAL
   - Largest (54K LOC), most complex (10+ subdirectories)
   - Highest risk for refactoring
   - Recommend: Break into 3-4 focused modules
   - Estimated effort: High

2. **MedComInventory** - HIGH
   - Large (28K LOC), well-structured but dense
   - Good candidate for modularization
   - Recommend: Extract operations into separate module
   - Estimated effort: Medium-High

3. **MedComShared** - MEDIUM
   - Interface-heavy (60 interfaces)
   - Already modular but large (87 files)
   - Recommend: Extract interfaces by domain
   - Estimated effort: Medium

4. **MedComUI** - MEDIUM
   - Large (27K LOC), well-organized by widget type
   - Good separation of concerns
   - Recommend: Modularize by widget category
   - Estimated effort: Medium

5. **MedComCore** - LOW
   - Well-focused (9K LOC)
   - Simple structure
   - Recommend: Minimal changes needed
   - Estimated effort: Low

6. **MedComGAS** - LOW
   - Well-modularized (8K LOC)
   - Each class has clear purpose
   - Recommend: Minimal changes needed
   - Estimated effort: Low

7. **MedComInteraction** - LOW
   - Excellent focus (3.5K LOC)
   - Clear responsibility boundaries
   - Recommend: Use as model
   - Estimated effort: Minimal

---

## Dependency Analysis

### Interface Consumers
All modules depend on **MedComShared**:
- MedComGAS → MedComShared (interfaces)
- MedComCore → MedComShared (interfaces)
- MedComEquipment → MedComShared (interfaces)
- MedComInventory → MedComShared (interfaces)
- MedComInteraction → MedComShared (interfaces)
- MedComUI → MedComShared (interfaces)

### Cross-Module Dependencies (Estimated)
- **MedComEquipment** → MedComUI (presentation)
- **MedComInventory** → MedComUI (UI)
- **MedComUI** → MedComInventory (data access)
- **MedComEquipment** → MedComInventory (integration)

### Critical Dependencies to Maintain
1. MedComShared interfaces - used by all modules
2. MedComCore player definitions - foundation for player systems
3. MedComGAS abilities - used by equipment system
4. MedComUI widgets - used by equipment and inventory

---

## Migration Strategy Recommendations

### Phase 1: Prepare (Low Risk)
- Document all 7 modules (This document)
- Analyze dependencies between modules
- Create test coverage for each module

### Phase 2: Extract (Medium Risk)
- Start with **MedComInteraction** (smallest, best practices)
- Then **MedComGAS** (well-modularized)
- Then **MedComCore** (lightweight)

### Phase 3: Refactor (High Risk)
- Break **MedComEquipment** into focused modules
- Refactor **MedComInventory** to extract operations
- Improve **MedComUI** widget organization

### Phase 4: Consolidate (Medium Risk)
- Extract interfaces from **MedComShared**
- Consolidate and remove duplicates
- Optimize interdependencies

---

## Quality Metrics Summary

| Metric | Rating | Notes |
|--------|--------|-------|
| Modularization | Good | Most modules are focused |
| Documentation | Good | Good header:source ratio |
| Size Distribution | Fair | Equipment module is too large |
| Interface Usage | Excellent | 60 interfaces provide contracts |
| Code Reusability | Good | Shared contracts and types |
| Complexity | High | 4 modules are complex |
| Maintainability | Fair | Large modules difficult to maintain |

