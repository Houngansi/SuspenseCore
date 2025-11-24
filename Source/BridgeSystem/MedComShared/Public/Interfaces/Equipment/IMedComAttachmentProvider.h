// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayEffectTypes.h"
#include "IMedComAttachmentProvider.generated.h"

/**
 * Attachment configuration
 */
USTRUCT(BlueprintType)
struct FEquipmentAttachmentConfig
{
    GENERATED_BODY()
    
    UPROPERTY()
    FName SocketName;
    
    UPROPERTY()
    FTransform RelativeTransform;
    
    UPROPERTY()
    EAttachmentRule LocationRule = EAttachmentRule::SnapToTarget;
    
    UPROPERTY()
    EAttachmentRule RotationRule = EAttachmentRule::SnapToTarget;
    
    UPROPERTY()
    EAttachmentRule ScaleRule = EAttachmentRule::KeepRelative;
    
    UPROPERTY()
    bool bWeldSimulatedBodies = true;
};

/**
 * Attachment state
 */
USTRUCT(BlueprintType)
struct FEquipmentAttachmentState
{
    GENERATED_BODY()
    
    UPROPERTY()
    bool bIsAttached = false;
    
    UPROPERTY()
    USceneComponent* AttachedTo = nullptr;
    
    UPROPERTY()
    FName CurrentSocket;
    
    UPROPERTY()
    FTransform CurrentOffset;
    
    UPROPERTY()
    bool bIsActive = false;
};

UINTERFACE(MinimalAPI, Blueprintable)
class UMedComAttachmentProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for equipment attachment management
 * 
 * Philosophy: Manages physical attachment of equipment to characters.
 * Handles socket management and position updates.
 */
class MEDCOMSHARED_API IMedComAttachmentProvider
{
    GENERATED_BODY()

public:
    /**
     * Attach equipment to character
     * @param Equipment Equipment actor
     * @param Target Target component
     * @param Config Attachment configuration
     * @return True if attached
     */
    virtual bool AttachEquipment(
        AActor* Equipment,
        USceneComponent* Target,
        const FEquipmentAttachmentConfig& Config) = 0;
    
    /**
     * Detach equipment
     * @param Equipment Equipment to detach
     * @param bMaintainWorldTransform Keep world position
     * @return True if detached
     */
    virtual bool DetachEquipment(AActor* Equipment, bool bMaintainWorldTransform = false) = 0;
    
    /**
     * Update attachment position
     * @param Equipment Equipment actor
     * @param NewConfig New configuration
     * @param bSmooth Smooth transition
     * @return True if updated
     */
    virtual bool UpdateAttachment(
        AActor* Equipment,
        const FEquipmentAttachmentConfig& NewConfig,
        bool bSmooth = false) = 0;
    
    /**
     * Get attachment state
     * @param Equipment Equipment actor
     * @return Current attachment state
     */
    virtual FEquipmentAttachmentState GetAttachmentState(AActor* Equipment) const = 0;
    
    /**
     * Find best socket for item
     * @param Target Target mesh
     * @param ItemType Type of item
     * @param bIsActive Active or holstered position
     * @return Socket name
     */
    virtual FName FindBestSocket(
        USkeletalMeshComponent* Target,
        const FGameplayTag& ItemType,
        bool bIsActive) const = 0;
    
    /**
     * Switch between active and holstered positions
     * @param Equipment Equipment to switch
     * @param bMakeActive New state
     * @param Duration Transition duration
     * @return True if switched
     */
    virtual bool SwitchAttachmentState(
        AActor* Equipment,
        bool bMakeActive,
        float Duration = 0.0f) = 0;
    
    /**
     * Get attachment config for slot
     * @param SlotIndex Equipment slot
     * @param bIsActive Active or holstered
     * @return Attachment configuration
     */
    virtual FEquipmentAttachmentConfig GetSlotAttachmentConfig(
        int32 SlotIndex,
        bool bIsActive) const = 0;
    
    /**
     * Validate attachment socket
     * @param Target Target component
     * @param SocketName Socket to validate
     * @return True if valid
     */
    virtual bool ValidateSocket(
        USceneComponent* Target,
        const FName& SocketName) const = 0;
};