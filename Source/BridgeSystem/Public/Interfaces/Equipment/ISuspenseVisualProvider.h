// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseVisualProvider.generated.h"

/**
 * Visual effect configuration
 */
USTRUCT(BlueprintType)
struct FEquipmentVisualEffect
{
    GENERATED_BODY()
    
    UPROPERTY()
    FGameplayTag EffectType;
    
    UPROPERTY()
    class UNiagaraSystem* NiagaraEffect = nullptr;
    
    UPROPERTY()
    class UParticleSystem* CascadeEffect = nullptr;
    
    UPROPERTY()
    FName AttachSocket;
    
    UPROPERTY()
    FTransform RelativeTransform;
    
    UPROPERTY()
    float Duration = 0.0f;
    
    UPROPERTY()
    bool bLooping = false;
};

/**
 * Material override configuration
 */
USTRUCT(BlueprintType)
struct FEquipmentMaterialOverride
{
    GENERATED_BODY()
    
    UPROPERTY()
    int32 MaterialSlot = 0;
    
    UPROPERTY()
    class UMaterialInterface* OverrideMaterial = nullptr;
    
    UPROPERTY()
    TMap<FName, float> ScalarParameters;
    
    UPROPERTY()
    TMap<FName, FLinearColor> VectorParameters;
    
    UPROPERTY()
    TMap<FName, class UTexture*> TextureParameters;
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseVisualProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for equipment visual effects management
 * 
 * Philosophy: Manages all visual aspects of equipment.
 * Handles materials, effects, and visual state changes.
 */
class BRIDGESYSTEM_API ISuspenseVisualProvider
{
    GENERATED_BODY()

public:
    /**
     * Apply visual effect to equipment
     * @param Equipment Target equipment
     * @param Effect Effect configuration
     * @return Effect instance ID
     */
    virtual FGuid ApplyVisualEffect(AActor* Equipment, const FEquipmentVisualEffect& Effect) = 0;
    
    /**
     * Remove visual effect
     * @param EffectId Effect to remove
     * @return True if removed
     */
    virtual bool RemoveVisualEffect(const FGuid& EffectId) = 0;
    
    /**
     * Apply material override
     * @param Equipment Target equipment
     * @param Override Material configuration
     * @return True if applied
     */
    virtual bool ApplyMaterialOverride(AActor* Equipment, const FEquipmentMaterialOverride& Override) = 0;
    
    /**
     * Reset materials to default
     * @param Equipment Target equipment
     */
    virtual void ResetMaterials(AActor* Equipment) = 0;
    
    /**
     * Update visual wear state
     * @param Equipment Target equipment
     * @param WearPercent Wear from 0 to 1
     */
    virtual void UpdateWearState(AActor* Equipment, float WearPercent) = 0;
    
    /**
     * Set highlight state
     * @param Equipment Target equipment
     * @param bHighlighted Highlight state
     * @param HighlightColor Color for highlight
     */
    virtual void SetHighlighted(AActor* Equipment, bool bHighlighted, const FLinearColor& HighlightColor = FLinearColor::White) = 0;
    
    /**
     * Play equipment animation
     * @param Equipment Target equipment
     * @param AnimationTag Animation type
     * @return True if played
     */
    virtual bool PlayEquipmentAnimation(AActor* Equipment, const FGameplayTag& AnimationTag) = 0;
};