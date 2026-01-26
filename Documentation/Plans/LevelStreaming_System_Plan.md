# Level Streaming System Implementation Plan

> **Version:** 1.0
> **Date:** 2026-01-26
> **Author:** Claude Code (Technical Lead)
> **Status:** PROPOSAL
> **Scope:** Level Streaming, Map Management, Lobby System, MMO FPS Architecture

---

## Executive Summary

This document outlines the implementation plan for a robust Level Streaming system for SuspenseCore - an MMO FPS in the Tarkov style. The system will use **Level Instances** (not World Partition) to achieve seamless loading, instanced raid maps, and optimized memory management.

---

## Table of Contents

1. [Current Architecture Analysis](#1-current-architecture-analysis)
2. [Level Streaming Approaches for MMO FPS](#2-level-streaming-approaches-for-mmo-fps)
3. [Recommended Architecture](#3-recommended-architecture)
4. [Implementation Plan](#4-implementation-plan)
5. [File Changes](#5-file-changes)
6. [Technical Specifications](#6-technical-specifications)

---

## 1. Current Architecture Analysis

### 1.1 Existing Files (31 files found)

#### Core Subsystems (BridgeSystem)
| File | Purpose |
|------|---------|
| `SuspenseCoreMapTransitionSubsystem.h/cpp` | GameInstance subsystem for map transitions |
| `SuspenseCoreCharacterSelectionSubsystem.h/cpp` | Persists character selection between maps |

#### Game Modes (PlayerCore)
| File | Purpose |
|------|---------|
| `SuspenseCoreMenuGameMode.h/cpp` | Main menu game mode (no pawn spawn) |
| `SuspenseCoreGameGameMode.h/cpp` | Gameplay game mode with save system |

#### Controllers (PlayerCore)
| File | Purpose |
|------|---------|
| `SuspenseCoreMenuPlayerController.h/cpp` | Menu-only controller (UI input) |
| `SuspenseCorePlayerController.h/cpp` | Game controller (abilities, input) |

#### Player State (PlayerCore)
| File | Purpose |
|------|---------|
| `SuspenseCorePlayerState.h/cpp` | ASC owner, inventory, equipment |

#### UI Widgets (UISystem)
| File | Purpose |
|------|---------|
| `SuspenseCoreMainMenuWidget.h/cpp` | Main menu with screen switching |
| `SuspenseCoreCharacterSelectWidget.h/cpp` | Character list and selection |
| `SuspenseCoreRegistrationWidget.h/cpp` | New character creation |
| `SuspenseCorePauseMenuWidget.h/cpp` | In-game pause with save/load |
| `SuspenseCoreSaveLoadMenuWidget.h/cpp` | Save slot management |

### 1.2 Current Map Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                    CURRENT ARCHITECTURE                          │
│                   (No Level Streaming)                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  MainMenuMap ─────────────────────────► GameMap                  │
│     │           OpenLevel()                │                     │
│     │        (Hard Level Load)             │                     │
│     │                                      │                     │
│     │  • Full map unload                   │  • Full map load    │
│     │  • Loading screen required           │  • All assets loaded│
│     │  • GameMode switch via URL           │  • No streaming     │
│     │                                      │                     │
│     └──────────────────────────────────────┘                     │
│                                                                  │
│  PROBLEMS:                                                       │
│  • Long loading times for large maps                             │
│  • No instanced raids (Tarkov-style)                             │
│  • Cannot stream sub-levels dynamically                          │
│  • All players share same map (no private instances)             │
│  • Memory inefficient for MMO scale                              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. Level Streaming Approaches for MMO FPS

### 2.1 Approach Comparison Matrix

| Approach | Pros | Cons | Use Case |
|----------|------|------|----------|
| **Level Streaming Volumes** | Simple setup, proximity-based | No instancing, hard to control | Open world areas |
| **Blueprint/C++ Streaming** | Full control, async loading | More code, manual management | Tarkov-style raids |
| **Level Instances (LevelInstance Actor)** | True instancing, transforms | UE5.1+, newer API | Instanced dungeons/raids |
| **Packed Level Actors** | Actor instancing, lightweight | Not full levels | Repeated structures |
| **Data Layers** | Networked streaming control | Complex setup | MMO zones |
| **Custom Subsystem** | Full control, SSOT | Most code | AAA production |

### 2.2 Detailed Analysis

#### Approach 1: Level Streaming Volumes (NOT RECOMMENDED)

```cpp
// Built-in ALevelStreamingVolume
// Streams levels when player enters volume
// NO INSTANCING - same level for all players
```

**Why Not:**
- Cannot create multiple instances of same level
- All players see same level state
- No Tarkov-style private raids

---

#### Approach 2: Blueprint/C++ Level Streaming (VIABLE)

```cpp
// UGameplayStatics::LoadStreamingLevel / UnloadStreamingLevel
// Manual control over streaming

FLatentActionInfo LatentInfo;
LatentInfo.CallbackTarget = this;
LatentInfo.ExecutionFunction = "OnLevelLoaded";
LatentInfo.UUID = GetUniqueID();
LatentInfo.Linkage = 0;

UGameplayStatics::LoadStreamingLevel(
    this,
    LevelName,
    bMakeVisibleAfterLoad,
    bShouldBlockOnLoad,
    LatentInfo
);
```

**Pros:**
- Full async control
- Can load/unload on demand
- Works with existing systems

**Cons:**
- No true instancing (same level for all)
- Manual state management
- Memory not isolated per instance

---

#### Approach 3: Level Instances (ULevelInstanceSubsystem) - RECOMMENDED

```cpp
// UE5.1+ Level Instance system
// TRUE INSTANCING with unique transforms and state

// Get subsystem
ULevelInstanceSubsystem* LevelInstanceSubsystem =
    GetWorld()->GetSubsystem<ULevelInstanceSubsystem>();

// Create instance
ALevelInstance* LevelInstance = LevelInstanceSubsystem->LoadLevelInstance(
    LevelAsset,
    InstanceTransform,
    bInitiallyVisible
);

// Each instance is independent!
// Perfect for Tarkov-style instanced raids
```

**Pros:**
- True instancing (multiple copies of same level)
- Each instance has own transform and state
- Perfect for instanced raids/dungeons
- Memory efficient (shared assets, unique state)

**Cons:**
- UE5.1+ required
- Newer API, less documentation
- Requires custom management system

---

#### Approach 4: Custom Level Streaming Subsystem (BEST FOR AAA)

Combines Level Instances with custom management:

```cpp
UCLASS()
class USuspenseCoreLevelStreamingSubsystem : public UWorldSubsystem
{
    // Manages all level instances
    // Provides SSOT for streaming data
    // Handles instanced raids
    // Integrates with save system
};
```

---

### 2.3 Tarkov-Style Raid System Requirements

| Requirement | Solution |
|-------------|----------|
| **Private raid instances** | Level Instances with unique IDs |
| **Multiple players in same instance** | Instance UUID shared via matchmaking |
| **Persist raid state** | Save instance state to repository |
| **Extract/Death ends raid** | Unload instance, save loot |
| **Spawn points per instance** | PlayerStart actors in instance |
| **AI per instance** | AI spawned within instance |
| **Loot per instance** | Loot tables applied per instance |

---

## 3. Recommended Architecture

### 3.1 High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    PROPOSED LEVEL STREAMING ARCHITECTURE                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────────────────────────────────────────────────────────────┐   │
│  │                         PERSISTENT LEVEL                              │   │
│  │                     (MainMenuMap / HubMap)                            │   │
│  │  ┌─────────────────────────────────────────────────────────────────┐ │   │
│  │  │ GameMode: SuspenseCoreHubGameMode                                │ │   │
│  │  │ - Always loaded                                                  │ │   │
│  │  │ - Contains PlayerController                                      │ │   │
│  │  │ - Manages Level Instance spawning                                │ │   │
│  │  │ - No gameplay actors (safe zone)                                 │ │   │
│  │  └─────────────────────────────────────────────────────────────────┘ │   │
│  │                              │                                        │   │
│  │          ┌───────────────────┼───────────────────┐                   │   │
│  │          │                   │                   │                   │   │
│  │          ▼                   ▼                   ▼                   │   │
│  │  ┌───────────────┐   ┌───────────────┐   ┌───────────────┐          │   │
│  │  │ Level Instance│   │ Level Instance│   │ Level Instance│          │   │
│  │  │ "Factory_01"  │   │ "Factory_02"  │   │ "Woods_01"    │          │   │
│  │  │ (Raid #1234)  │   │ (Raid #5678)  │   │ (Raid #9012)  │          │   │
│  │  │               │   │               │   │               │          │   │
│  │  │ - 5 players   │   │ - 3 players   │   │ - 1 player    │          │   │
│  │  │ - Own AI      │   │ - Own AI      │   │ - Own AI      │          │   │
│  │  │ - Own loot    │   │ - Own loot    │   │ - Own loot    │          │   │
│  │  │ - Own state   │   │ - Own state   │   │ - Own state   │          │   │
│  │  └───────────────┘   └───────────────┘   └───────────────┘          │   │
│  │                                                                       │   │
│  └──────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│  KEY FEATURES:                                                               │
│  • Single persistent level (no hard map transitions)                         │
│  • Instanced raids via Level Instances                                       │
│  • Each raid is isolated (own AI, loot, state)                               │
│  • Players teleported into instances                                         │
│  • Extract returns to Hub                                                    │
│  • Memory efficient (shared level assets)                                    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Map Structure

```
Content/Maps/
├── Persistent/
│   ├── MainMenu.umap              ← Menu-only (no streaming)
│   └── Hub.umap                   ← Persistent gameplay level (hideout)
│
├── Raids/
│   ├── Factory/
│   │   ├── Factory_Main.umap      ← Full raid map (Level Instance source)
│   │   ├── Factory_Underground.umap ← Sub-level (streamed within instance)
│   │   └── Factory_Rooftop.umap   ← Sub-level
│   │
│   ├── Customs/
│   │   ├── Customs_Main.umap
│   │   ├── Customs_Dorms.umap
│   │   └── Customs_Construction.umap
│   │
│   └── Woods/
│       ├── Woods_Main.umap
│       └── Woods_Sawmill.umap
│
└── Shared/
    └── CommonActors.umap          ← Shared actors (UI, etc.)
```

### 3.3 Core Classes

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           NEW CLASS HIERARCHY                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  BridgeSystem/                                                               │
│  ├── Subsystems/                                                             │
│  │   ├── SuspenseCoreLevelStreamingSubsystem.h     ← NEW: Core streaming    │
│  │   ├── SuspenseCoreRaidInstanceSubsystem.h       ← NEW: Raid management   │
│  │   └── SuspenseCoreMapTransitionSubsystem.h      ← EXISTING: Transitions  │
│  │                                                                           │
│  ├── Data/                                                                   │
│  │   ├── SuspenseCoreRaidData.h                    ← NEW: Raid Data Asset   │
│  │   ├── SuspenseCoreZoneData.h                    ← NEW: Zone definitions  │
│  │   └── SuspenseCoreSpawnPointData.h              ← NEW: Spawn configs     │
│  │                                                                           │
│  └── Interfaces/                                                             │
│      ├── ISuspenseCoreRaidParticipant.h            ← NEW: Player in raid    │
│      └── ISuspenseCoreStreamingZone.h              ← NEW: Zone interface    │
│                                                                              │
│  PlayerCore/                                                                 │
│  ├── GameModes/                                                              │
│  │   ├── SuspenseCoreHubGameMode.h                 ← NEW: Hub/hideout mode  │
│  │   ├── SuspenseCoreRaidGameMode.h                ← NEW: Raid instance mode│
│  │   └── SuspenseCoreMenuGameMode.h                ← EXISTING: Menu mode    │
│  │                                                                           │
│  └── Actors/                                                                 │
│      ├── SuspenseCoreRaidTrigger.h                 ← NEW: Enter raid        │
│      ├── SuspenseCoreExtractionPoint.h             ← NEW: Exit raid         │
│      └── SuspenseCoreSpawnPoint.h                  ← NEW: Raid spawn        │
│                                                                              │
│  GAS/                                                                        │
│  └── Subsystems/                                                             │
│      └── SuspenseCoreCharacterClassSubsystem.h     ← EXISTING: Class system │
│                                                                              │
│  UISystem/                                                                   │
│  └── Widgets/                                                                │
│      ├── SuspenseCoreRaidSelectWidget.h            ← NEW: Raid selection    │
│      ├── SuspenseCoreLoadingWidget.h               ← NEW: Loading screen    │
│      ├── SuspenseCoreExtractionWidget.h            ← NEW: Extract countdown │
│      └── SuspenseCoreMainMenuWidget.h              ← EXISTING: Main menu    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Implementation Plan

### Phase 1: Core Level Streaming Subsystem

#### 4.1.1 SuspenseCoreLevelStreamingSubsystem

```cpp
// SuspenseCoreLevelStreamingSubsystem.h
// Core level streaming management

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/LevelStreamingDynamic.h"
#include "SuspenseCoreLevelStreamingSubsystem.generated.h"

class ALevelInstance;

UENUM(BlueprintType)
enum class ESuspenseCoreStreamingState : uint8
{
    Unloaded,
    Loading,
    Loaded,
    Visible,
    Unloading
};

USTRUCT(BlueprintType)
struct FSuspenseCoreStreamingLevelInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName LevelID;

    UPROPERTY(BlueprintReadOnly)
    TSoftObjectPtr<UWorld> LevelAsset;

    UPROPERTY(BlueprintReadOnly)
    ESuspenseCoreStreamingState State = ESuspenseCoreStreamingState::Unloaded;

    UPROPERTY(BlueprintReadOnly)
    FTransform InstanceTransform;

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<ALevelInstance> LevelInstance;

    UPROPERTY(BlueprintReadOnly)
    FGuid InstanceGuid;

    UPROPERTY(BlueprintReadOnly)
    float LoadProgress = 0.0f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLevelStreamingStateChanged, FName, LevelID, ESuspenseCoreStreamingState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLevelLoadProgress, FName, LevelID, float, Progress);

/**
 * USuspenseCoreLevelStreamingSubsystem
 *
 * World subsystem managing level streaming and level instances.
 * Provides centralized control over all streamed levels.
 *
 * Features:
 * - Async level loading/unloading
 * - Level Instance management (true instancing)
 * - Progress tracking with delegates
 * - State machine per level
 * - SSOT for streaming configuration
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreLevelStreamingSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    // ═══════════════════════════════════════════════════════════════════════════
    // LIFECYCLE
    // ═══════════════════════════════════════════════════════════════════════════

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    static USuspenseCoreLevelStreamingSubsystem* Get(const UObject* WorldContext);

    // ═══════════════════════════════════════════════════════════════════════════
    // LEVEL STREAMING (Traditional)
    // ═══════════════════════════════════════════════════════════════════════════

    /** Load a streaming level by name */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Streaming")
    bool LoadStreamingLevel(FName LevelName, bool bMakeVisibleAfterLoad = true, bool bBlockOnLoad = false);

    /** Unload a streaming level */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Streaming")
    bool UnloadStreamingLevel(FName LevelName);

    /** Set level visibility */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Streaming")
    bool SetLevelVisibility(FName LevelName, bool bVisible);

    // ═══════════════════════════════════════════════════════════════════════════
    // LEVEL INSTANCES (UE5.1+)
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * Create a new level instance from a level asset.
     * Each instance is independent with its own state.
     *
     * @param LevelAsset The level to instance
     * @param InstanceTransform World transform for the instance
     * @param OutInstanceGuid Unique ID for this instance
     * @return The created ALevelInstance actor
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Streaming|Instance")
    ALevelInstance* CreateLevelInstance(
        TSoftObjectPtr<UWorld> LevelAsset,
        FTransform InstanceTransform,
        FGuid& OutInstanceGuid
    );

    /** Destroy a level instance by GUID */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Streaming|Instance")
    bool DestroyLevelInstance(const FGuid& InstanceGuid);

    /** Get level instance by GUID */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Streaming|Instance")
    ALevelInstance* GetLevelInstance(const FGuid& InstanceGuid) const;

    /** Get all active level instances */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Streaming|Instance")
    TArray<ALevelInstance*> GetAllLevelInstances() const;

    // ═══════════════════════════════════════════════════════════════════════════
    // STATE QUERIES
    // ═══════════════════════════════════════════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Streaming")
    ESuspenseCoreStreamingState GetLevelState(FName LevelID) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Streaming")
    float GetLoadProgress(FName LevelID) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Streaming")
    bool IsLevelLoaded(FName LevelID) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Streaming")
    bool IsLevelVisible(FName LevelID) const;

    // ═══════════════════════════════════════════════════════════════════════════
    // DELEGATES
    // ═══════════════════════════════════════════════════════════════════════════

    UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Streaming")
    FOnLevelStreamingStateChanged OnLevelStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Streaming")
    FOnLevelLoadProgress OnLoadProgress;

private:
    UPROPERTY()
    TMap<FName, FSuspenseCoreStreamingLevelInfo> StreamingLevels;

    UPROPERTY()
    TMap<FGuid, TWeakObjectPtr<ALevelInstance>> LevelInstances;

    void OnLevelLoadedCallback(FName LevelName);
    void OnLevelUnloadedCallback(FName LevelName);
    void UpdateLoadProgress(FName LevelName);
    void SetLevelState(FName LevelID, ESuspenseCoreStreamingState NewState);
};
```

### Phase 2: Raid Instance System

#### 4.2.1 SuspenseCoreRaidInstanceSubsystem

```cpp
// SuspenseCoreRaidInstanceSubsystem.h
// Tarkov-style raid instance management

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SuspenseCoreRaidInstanceSubsystem.generated.h"

class USuspenseCoreRaidData;
class ALevelInstance;

UENUM(BlueprintType)
enum class ESuspenseCoreRaidState : uint8
{
    None,
    Matchmaking,      // Finding/creating instance
    Loading,          // Level loading
    Deploying,        // Players spawning
    InProgress,       // Raid active
    Extracting,       // Extraction countdown
    Completed,        // Raid ended successfully
    Failed            // Player died / MIA
};

USTRUCT(BlueprintType)
struct FSuspenseCoreRaidSession
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid RaidGuid;

    UPROPERTY(BlueprintReadOnly)
    FName RaidMapID;

    UPROPERTY(BlueprintReadOnly)
    ESuspenseCoreRaidState State = ESuspenseCoreRaidState::None;

    UPROPERTY(BlueprintReadOnly)
    TArray<FGuid> ParticipantPlayerIds;

    UPROPERTY(BlueprintReadOnly)
    float RaidTimeRemaining = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    float RaidStartTime = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<ALevelInstance> LevelInstance;

    UPROPERTY(BlueprintReadOnly)
    FTransform SpawnTransform;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRaidStateChanged, FGuid, RaidGuid, ESuspenseCoreRaidState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRaidLoadProgress, float, Progress);

/**
 * USuspenseCoreRaidInstanceSubsystem
 *
 * GameInstance subsystem managing Tarkov-style raid instances.
 * Persists across level transitions for seamless raid flow.
 *
 * Flow:
 * 1. Player selects raid from UI
 * 2. Subsystem creates level instance
 * 3. Player teleported into instance
 * 4. Raid timer starts
 * 5. Extract or Die ends raid
 * 6. Return to Hub with loot (or lose everything)
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreRaidInstanceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ═══════════════════════════════════════════════════════════════════════════
    // LIFECYCLE
    // ═══════════════════════════════════════════════════════════════════════════

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    static USuspenseCoreRaidInstanceSubsystem* Get(const UObject* WorldContext);

    // ═══════════════════════════════════════════════════════════════════════════
    // RAID MANAGEMENT
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * Start a new raid instance.
     * Creates level instance and prepares for player deployment.
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Raid")
    bool StartRaid(const USuspenseCoreRaidData* RaidData, FGuid& OutRaidGuid);

    /** Join an existing raid instance (for multiplayer) */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Raid")
    bool JoinRaid(const FGuid& RaidGuid, const FGuid& PlayerGuid);

    /** Deploy player into raid (after loading complete) */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Raid")
    bool DeployPlayer(const FGuid& RaidGuid, const FGuid& PlayerGuid);

    /** Extract from raid (successful exit) */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Raid")
    bool ExtractFromRaid(const FGuid& RaidGuid, const FGuid& PlayerGuid);

    /** Player died in raid */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Raid")
    void PlayerDiedInRaid(const FGuid& RaidGuid, const FGuid& PlayerGuid);

    /** End raid instance (cleanup) */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Raid")
    void EndRaid(const FGuid& RaidGuid);

    // ═══════════════════════════════════════════════════════════════════════════
    // QUERIES
    // ═══════════════════════════════════════════════════════════════════════════

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Raid")
    ESuspenseCoreRaidState GetRaidState(const FGuid& RaidGuid) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Raid")
    float GetRaidTimeRemaining(const FGuid& RaidGuid) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Raid")
    bool IsPlayerInRaid(const FGuid& PlayerGuid) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Raid")
    FGuid GetPlayerCurrentRaid(const FGuid& PlayerGuid) const;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Raid")
    FSuspenseCoreRaidSession GetRaidSession(const FGuid& RaidGuid) const;

    // ═══════════════════════════════════════════════════════════════════════════
    // DELEGATES
    // ═══════════════════════════════════════════════════════════════════════════

    UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Raid")
    FOnRaidStateChanged OnRaidStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Raid")
    FOnRaidLoadProgress OnRaidLoadProgress;

private:
    UPROPERTY()
    TMap<FGuid, FSuspenseCoreRaidSession> ActiveRaids;

    UPROPERTY()
    TMap<FGuid, FGuid> PlayerToRaidMap; // PlayerGuid → RaidGuid

    void SetRaidState(const FGuid& RaidGuid, ESuspenseCoreRaidState NewState);
    void TickRaidTimers(float DeltaTime);
    void CleanupRaid(const FGuid& RaidGuid);

    FTimerHandle RaidTimerHandle;
};
```

### Phase 3: Raid Data Asset (SSOT)

#### 4.3.1 SuspenseCoreRaidData

```cpp
// SuspenseCoreRaidData.h
// SSOT for raid configuration

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreRaidData.generated.h"

UENUM(BlueprintType)
enum class ESuspenseCoreRaidDifficulty : uint8
{
    Scav,       // Easy - shorter timer, less AI
    PMC,        // Normal
    Boss,       // Hard - boss spawns
    Labs        // Extreme - requires keycard
};

USTRUCT(BlueprintType)
struct FSuspenseCoreExtractionPoint
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FName ExtractionID;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FVector Location;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float ExtractionTime = 7.0f; // Seconds to extract

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bAlwaysAvailable = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float AvailabilityChance = 1.0f; // Random availability

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    bool bRequiresPayment = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 PaymentAmount = 0;
};

USTRUCT(BlueprintType)
struct FSuspenseCoreSpawnConfiguration
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FName SpawnGroupID;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TArray<FTransform> SpawnPoints;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 MaxPlayersPerGroup = 4;
};

/**
 * USuspenseCoreRaidData
 *
 * Primary Data Asset defining a raid/map.
 * SSOT for all raid configuration.
 */
UCLASS(BlueprintType, Blueprintable)
class BRIDGESYSTEM_API USuspenseCoreRaidData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // ═══════════════════════════════════════════════════════════════════════════
    // IDENTITY
    // ═══════════════════════════════════════════════════════════════════════════

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FName RaidID;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity", meta = (MultiLine = true))
    FText Description;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FGameplayTag RaidTag;

    // ═══════════════════════════════════════════════════════════════════════════
    // MAP CONFIGURATION
    // ═══════════════════════════════════════════════════════════════════════════

    /** Main level asset for this raid */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
    TSoftObjectPtr<UWorld> MainLevelAsset;

    /** Sub-levels to stream within the raid */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Map")
    TArray<TSoftObjectPtr<UWorld>> SubLevels;

    // ═══════════════════════════════════════════════════════════════════════════
    // TIMING
    // ═══════════════════════════════════════════════════════════════════════════

    /** Raid duration in minutes */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "5", ClampMax = "60"))
    float RaidDurationMinutes = 35.0f;

    /** Deploy phase duration */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timing")
    float DeployPhaseDuration = 120.0f; // 2 minutes to spawn

    // ═══════════════════════════════════════════════════════════════════════════
    // DIFFICULTY
    // ═══════════════════════════════════════════════════════════════════════════

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Difficulty")
    ESuspenseCoreRaidDifficulty Difficulty = ESuspenseCoreRaidDifficulty::PMC;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Difficulty")
    int32 RecommendedLevel = 1;

    // ═══════════════════════════════════════════════════════════════════════════
    // SPAWNS
    // ═══════════════════════════════════════════════════════════════════════════

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Spawns")
    TArray<FSuspenseCoreSpawnConfiguration> SpawnConfigurations;

    // ═══════════════════════════════════════════════════════════════════════════
    // EXTRACTIONS
    // ═══════════════════════════════════════════════════════════════════════════

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extractions")
    TArray<FSuspenseCoreExtractionPoint> ExtractionPoints;

    // ═══════════════════════════════════════════════════════════════════════════
    // VISUALS
    // ═══════════════════════════════════════════════════════════════════════════

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
    TSoftObjectPtr<UTexture2D> MapPreviewImage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
    TSoftObjectPtr<UTexture2D> MapIcon;

    // ═══════════════════════════════════════════════════════════════════════════
    // REQUIREMENTS
    // ═══════════════════════════════════════════════════════════════════════════

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Requirements")
    bool bRequiresKeycard = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Requirements")
    FName RequiredKeycardID;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Requirements")
    int32 MinimumPlayerLevel = 0;

    // ═══════════════════════════════════════════════════════════════════════════
    // UTILITY
    // ═══════════════════════════════════════════════════════════════════════════

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Raid")
    TArray<FSuspenseCoreExtractionPoint> GetAvailableExtractions() const;
};
```

---

## 5. File Changes

### 5.1 New Files

| File | Module | Purpose |
|------|--------|---------|
| `SuspenseCoreLevelStreamingSubsystem.h/cpp` | BridgeSystem | Core streaming management |
| `SuspenseCoreRaidInstanceSubsystem.h/cpp` | BridgeSystem | Raid instance lifecycle |
| `SuspenseCoreRaidData.h/cpp` | BridgeSystem | Raid SSOT Data Asset |
| `SuspenseCoreZoneData.h/cpp` | BridgeSystem | Zone definitions |
| `SuspenseCoreHubGameMode.h/cpp` | PlayerCore | Hub/hideout game mode |
| `SuspenseCoreRaidGameMode.h/cpp` | PlayerCore | Raid instance game mode |
| `SuspenseCoreRaidTrigger.h/cpp` | PlayerCore | Actor to enter raid |
| `SuspenseCoreExtractionPoint.h/cpp` | PlayerCore | Extraction actor |
| `SuspenseCoreSpawnPoint.h/cpp` | PlayerCore | Raid spawn point |
| `SuspenseCoreRaidSelectWidget.h/cpp` | UISystem | Raid selection UI |
| `SuspenseCoreLoadingWidget.h/cpp` | UISystem | Loading screen |
| `SuspenseCoreExtractionWidget.h/cpp` | UISystem | Extraction countdown |

### 5.2 Modified Files

| File | Changes |
|------|---------|
| `SuspenseCoreMapTransitionSubsystem.h/cpp` | Add raid transition support |
| `SuspenseCoreMainMenuWidget.h/cpp` | Add raid selection panel |
| `SuspenseCorePlayerController.h/cpp` | Add raid state handling |
| `SuspenseCorePlayerState.h/cpp` | Add raid participant interface |
| `SuspenseCoreProjectSettings.h` | Add raid/streaming settings |

### 5.3 Data Assets to Create

```
Content/Data/Raids/
├── DA_Raid_Factory.uasset
├── DA_Raid_Customs.uasset
├── DA_Raid_Woods.uasset
├── DA_Raid_Interchange.uasset
└── DA_Raid_Labs.uasset
```

---

## 6. Technical Specifications

### 6.1 Level Instance Implementation Details

```cpp
// Using UE5.1+ Level Instance API
// Key classes:
// - ULevelInstanceSubsystem (World Subsystem)
// - ALevelInstance (Actor representing instance)
// - ULevelInstanceComponent (Component for spawned instances)

// Creating instance:
ULevelInstanceSubsystem* LIS = GetWorld()->GetSubsystem<ULevelInstanceSubsystem>();
if (LIS)
{
    FLevelInstanceID InstanceID;
    ALevelInstance* Instance = LIS->LoadLevelInstance(
        LevelPackage,
        InstanceTransform,
        /* bInitiallyVisible */ true,
        &InstanceID
    );
}
```

### 6.2 Memory Management

| Strategy | Implementation |
|----------|---------------|
| **Shared Assets** | Level Instance shares textures, meshes, materials |
| **Unique State** | Each instance has own actor state, AI state |
| **Garbage Collection** | Instances unloaded when no players remain |
| **Preloading** | Common assets preloaded in Hub level |

### 6.3 Network Considerations

| Feature | Implementation |
|---------|---------------|
| **Instance ID Sync** | Server generates, replicates to clients |
| **Player Assignment** | Server assigns players to instances |
| **State Replication** | Instance state replicated within instance |
| **Cross-Instance** | No direct communication between instances |

---

## 7. Implementation Checklist

### Phase 1: Core Infrastructure
- [ ] Create `SuspenseCoreLevelStreamingSubsystem`
- [ ] Implement async level loading
- [ ] Implement Level Instance creation
- [ ] Add streaming state machine
- [ ] Add progress tracking delegates

### Phase 2: Raid System
- [ ] Create `SuspenseCoreRaidInstanceSubsystem`
- [ ] Create `SuspenseCoreRaidData` Data Asset
- [ ] Implement raid lifecycle (start → deploy → in-progress → extract)
- [ ] Create `SuspenseCoreRaidGameMode`
- [ ] Implement raid timer system

### Phase 3: Actors
- [ ] Create `SuspenseCoreRaidTrigger`
- [ ] Create `SuspenseCoreExtractionPoint`
- [ ] Create `SuspenseCoreSpawnPoint`
- [ ] Integrate with existing spawn system

### Phase 4: UI
- [ ] Create `SuspenseCoreRaidSelectWidget`
- [ ] Create `SuspenseCoreLoadingWidget`
- [ ] Create `SuspenseCoreExtractionWidget`
- [ ] Update `SuspenseCoreMainMenuWidget`

### Phase 5: Integration
- [ ] Update `SuspenseCoreMapTransitionSubsystem`
- [ ] Update `SuspenseCorePlayerController`
- [ ] Update `SuspenseCorePlayerState`
- [ ] Create Data Assets for all raids
- [ ] Create test maps

### Phase 6: Testing
- [ ] Test single-player raid flow
- [ ] Test multiplayer raid instance isolation
- [ ] Test extraction mechanics
- [ ] Test level streaming performance
- [ ] Memory profiling

---

## 8. Summary

This plan provides a complete Level Streaming solution for SuspenseCore MMO FPS:

| Feature | Implementation |
|---------|---------------|
| **No World Partition** | Uses Level Instances instead |
| **True Instancing** | Each raid is isolated |
| **Tarkov-Style Raids** | Timer, extractions, death penalty |
| **SSOT** | `SuspenseCoreRaidData` Data Assets |
| **Async Loading** | No blocking loads |
| **Memory Efficient** | Shared assets, unique state |
| **Multiplayer Ready** | Instance ID synchronization |

---

**Document End**

*Awaiting approval before implementation.*
