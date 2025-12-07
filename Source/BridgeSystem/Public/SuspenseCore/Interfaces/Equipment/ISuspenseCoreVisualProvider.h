// Copyright SuspenseCore Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Interfaces/Equipment/ISuspenseVisualProvider.h"
#include "ISuspenseCoreVisualProvider.generated.h"

/**
 * SuspenseCore Visual effect configuration
 */
USTRUCT(BlueprintType)
struct FSuspenseCoreVisualEffect
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    FGameplayTag EffectType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    class UNiagaraSystem* NiagaraEffect = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    class UParticleSystem* CascadeEffect = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    FName AttachSocket;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    FTransform RelativeTransform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    float Duration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
    bool bLooping = false;

    /** Convert to legacy format */
    FEquipmentVisualEffect ToLegacy() const
    {
        FEquipmentVisualEffect Legacy;
        Legacy.EffectType = EffectType;
        Legacy.NiagaraEffect = NiagaraEffect;
        Legacy.CascadeEffect = CascadeEffect;
        Legacy.AttachSocket = AttachSocket;
        Legacy.RelativeTransform = RelativeTransform;
        Legacy.Duration = Duration;
        Legacy.bLooping = bLooping;
        return Legacy;
    }

    /** Create from legacy format */
    static FSuspenseCoreVisualEffect FromLegacy(const FEquipmentVisualEffect& Legacy)
    {
        FSuspenseCoreVisualEffect Result;
        Result.EffectType = Legacy.EffectType;
        Result.NiagaraEffect = Legacy.NiagaraEffect;
        Result.CascadeEffect = Legacy.CascadeEffect;
        Result.AttachSocket = Legacy.AttachSocket;
        Result.RelativeTransform = Legacy.RelativeTransform;
        Result.Duration = Legacy.Duration;
        Result.bLooping = Legacy.bLooping;
        return Result;
    }
};

/**
 * SuspenseCore Material override configuration
 */
USTRUCT(BlueprintType)
struct FSuspenseCoreMaterialOverride
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    int32 MaterialSlot = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    class UMaterialInterface* OverrideMaterial = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    TMap<FName, float> ScalarParameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    TMap<FName, FLinearColor> VectorParameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
    TMap<FName, class UTexture*> TextureParameters;

    /** Convert to legacy format */
    FEquipmentMaterialOverride ToLegacy() const
    {
        FEquipmentMaterialOverride Legacy;
        Legacy.MaterialSlot = MaterialSlot;
        Legacy.OverrideMaterial = OverrideMaterial;
        Legacy.ScalarParameters = ScalarParameters;
        Legacy.VectorParameters = VectorParameters;
        Legacy.TextureParameters = TextureParameters;
        return Legacy;
    }

    /** Create from legacy format */
    static FSuspenseCoreMaterialOverride FromLegacy(const FEquipmentMaterialOverride& Legacy)
    {
        FSuspenseCoreMaterialOverride Result;
        Result.MaterialSlot = Legacy.MaterialSlot;
        Result.OverrideMaterial = Legacy.OverrideMaterial;
        Result.ScalarParameters = Legacy.ScalarParameters;
        Result.VectorParameters = Legacy.VectorParameters;
        Result.TextureParameters = Legacy.TextureParameters;
        return Result;
    }
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreVisualProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * SuspenseCore Interface for equipment visual effects management
 *
 * Philosophy: Manages all visual aspects of equipment.
 * Handles materials, effects, and visual state changes.
 *
 * This is the SuspenseCore version of ISuspenseVisualProvider with
 * extended functionality and proper type naming conventions.
 */
class BRIDGESYSTEM_API ISuspenseCoreVisualProvider
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

// Backward compatibility alias
using IVisualProvider = ISuspenseCoreVisualProvider;
