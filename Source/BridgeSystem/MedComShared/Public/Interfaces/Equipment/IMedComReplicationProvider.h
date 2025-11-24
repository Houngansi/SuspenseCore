// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Net/UnrealNetwork.h"
#include "IMedComReplicationProvider.generated.h"

/**
 * Replication policy
 */
UENUM(BlueprintType)
enum class EEquipmentReplicationPolicy : uint8
{
    Always = 0,
    OnlyToOwner,
    OnlyToRelevant,
    SkipOwner,
    Custom
};

/**
 * Replicated equipment data
 */
USTRUCT(BlueprintType)
struct FReplicatedEquipmentData
{
    GENERATED_BODY()
    
    UPROPERTY()
    TArray<FInventoryItemInstance> SlotInstances;
    
    UPROPERTY()
    int32 ActiveWeaponSlot = INDEX_NONE;
    
    UPROPERTY()
    FGameplayTag CurrentState;
    
    UPROPERTY()
    uint32 ReplicationVersion = 0;
    
    UPROPERTY()
    float LastUpdateTime = 0.0f;
};

UINTERFACE(MinimalAPI, Blueprintable)
class UMedComReplicationProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for equipment replication management
 * 
 * Philosophy: Manages state synchronization across network.
 * Optimizes bandwidth and ensures consistency.
 */
class MEDCOMSHARED_API IMedComReplicationProvider
{
    GENERATED_BODY()

public:
    /**
     * Mark data for replication
     * @param SlotIndex Slot that changed
     * @param bForceUpdate Force immediate replication
     */
    virtual void MarkForReplication(int32 SlotIndex, bool bForceUpdate = false) = 0;
    
    /**
     * Get replicated data
     * @return Current replicated state
     */
    virtual FReplicatedEquipmentData GetReplicatedData() const = 0;
    
    /**
     * Apply replicated data
     * @param Data Data to apply
     * @param bIsInitialReplication First replication
     */
    virtual void ApplyReplicatedData(const FReplicatedEquipmentData& Data, bool bIsInitialReplication = false) = 0;
    
    /**
     * Set replication policy
     * @param Policy New policy
     */
    virtual void SetReplicationPolicy(EEquipmentReplicationPolicy Policy) = 0;
    
    /**
     * Force full state replication
     */
    virtual void ForceFullReplication() = 0;
    
    /**
     * Check if needs replication
     * @param ViewTarget Viewing player
     * @return True if should replicate
     */
    virtual bool ShouldReplicateTo(APlayerController* ViewTarget) const = 0;
    
    /**
     * Get replication priority
     * @param ViewTarget Viewing player
     * @param OutPriority Priority value
     * @return True if has priority
     */
    virtual bool GetReplicationPriority(APlayerController* ViewTarget, float& OutPriority) const = 0;
    
    /**
     * Optimize replication data
     * @param Data Data to optimize
     * @return Optimized data
     */
    virtual FReplicatedEquipmentData OptimizeReplicationData(const FReplicatedEquipmentData& Data) const = 0;
    
    /**
     * Get delta since last replication
     * @param LastVersion Last replicated version
     * @return Delta data
     */
    virtual FReplicatedEquipmentData GetReplicationDelta(uint32 LastVersion) const = 0;
    
    /**
     * Handle replication callback
     * @param PropertyName Property that replicated
     */
    virtual void OnReplicationCallback(const FName& PropertyName) = 0;
};