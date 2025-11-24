// EquipmentVisualizationTypes.h
// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/Inventory/InventoryTypes.h"
#include "EquipmentVisualizationTypes.generated.h"

/**
 * Visual performance metrics for monitoring
 */
USTRUCT(BlueprintType)
struct MEDCOMSHARED_API FVisualPerformanceMetrics
{
    GENERATED_BODY()
    
    /** Average spawn time in milliseconds */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float AverageSpawnTime = 0.0f;
    
    /** Peak spawn time in milliseconds */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float PeakSpawnTime = 0.0f;
    
    /** Pool hit rate percentage */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float PoolHitRate = 0.0f;
    
    /** Active visual actor count */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int32 ActiveVisualCount = 0;
    
    /** Total memory usage in MB */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float MemoryUsageMB = 0.0f;
    
    /** Draw calls count */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int32 DrawCalls = 0;
    
    /** Triangle count */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int32 TriangleCount = 0;
};

/**
 * Visual equipment state snapshot for visual persistence
 * Переименовано чтобы не конфликтовать с FEquipmentStateSnapshot из IMedComEquipmentDataProvider
 */
USTRUCT(BlueprintType)
struct MEDCOMSHARED_API FEquipmentVisualSnapshot
{
    GENERATED_BODY()
    
    /** Snapshot ID */
    UPROPERTY(BlueprintReadOnly)
    FGuid SnapshotId;
    
    /** Creation timestamp */
    UPROPERTY(BlueprintReadOnly)
    FDateTime Timestamp;
    
    /** Visual actors by slot */
    //UPROPERTY(BlueprintReadOnly)
    TMap<int32, TWeakObjectPtr<AActor>> SlotVisualActors;
    
    /** Active visual effects */
    UPROPERTY(BlueprintReadOnly)
    TMap<FGuid, FGameplayTag> ActiveEffects;
    
    /** Material states for actors */
    UPROPERTY(BlueprintReadOnly)
    TMap<AActor*, int32> MaterialStates;
    
    /** Static factory method to create snapshot with generated ID */
    static FEquipmentVisualSnapshot Create()
    {
        FEquipmentVisualSnapshot Snapshot;
        Snapshot.SnapshotId = FGuid::NewGuid();
        Snapshot.Timestamp = FDateTime::Now();
        return Snapshot;
    }
    
    /** Static factory method to create snapshot with specific ID (for replication) */
    static FEquipmentVisualSnapshot CreateWithID(const FGuid& InSnapshotId)
    {
        FEquipmentVisualSnapshot Snapshot;
        Snapshot.SnapshotId = InSnapshotId;
        Snapshot.Timestamp = FDateTime::Now();
        return Snapshot;
    }
};

/**
 * Equipment visual event data for visual system events
 */
USTRUCT(BlueprintType)
struct MEDCOMSHARED_API FEquipmentVisualEventData
{
    GENERATED_BODY()
    
    /** Event type tag */
    UPROPERTY(BlueprintReadOnly)
    FGameplayTag EventType;
    
    /** Affected slot index */
    UPROPERTY(BlueprintReadOnly)
    int32 SlotIndex = INDEX_NONE;
    
    /** Visual actor involved */
    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> VisualActor;
    
    /** Event timestamp */
    UPROPERTY(BlueprintReadOnly)
    float Timestamp = 0.0f;
    
    /** Effect or material data */
    UPROPERTY(BlueprintReadOnly)
    FString VisualPayload;
};