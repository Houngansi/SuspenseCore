// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Types/Animation/SuspenseAnimationState.h"
#include "ISuspenseWeaponAnimation.generated.h"

// Forward declarations
class UAnimMontage;
class UBlendSpace;
class UAnimSequence;
struct FSuspenseInventoryItemInstance;

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseWeaponAnimation : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for weapon animation data access
 * Provides unified access to animation assets for weapon states
 * 
 * ARCHITECTURE PHILOSOPHY:
 * This interface serves as a contract between animation consumers and providers.
 * It deliberately separates high-performance C++ access (through virtual methods)
 * from safe Blueprint access (through UFUNCTIONs with value semantics).
 * 
 * PERFORMANCE CONSIDERATIONS:
 * - C++ code should use GetAnimationStateDataPtr() for zero-copy access
 * - Blueprint code uses GetAnimationStateData() which copies data for safety
 * - Direct asset getters (GetDrawMontage, etc.) return pointers safely as UObject* is managed
 * 
 * THREAD SAFETY:
 * Implementations must ensure thread-safe access to animation data,
 * especially when caching is involved.
 */
class BRIDGESYSTEM_API ISuspenseWeaponAnimation
{
    GENERATED_BODY()

public:
    //==================================================================
    // C++ High-Performance Interface (Virtual Methods)
    //==================================================================
    
    /**
     * Get animation state data pointer for maximum performance (C++ only)
     * 
     * This method provides direct const pointer access to avoid copying.
     * The returned pointer remains valid for the lifetime of the data provider.
     * Callers must not store this pointer beyond immediate use.
     * 
     * @param WeaponType Gameplay tag identifying the weapon archetype
     * @return Const pointer to animation data, or nullptr if not found
     */
    virtual const FAnimationStateData* GetAnimationStateDataPtr(const FGameplayTag& WeaponType) const { return nullptr; }
    
    /**
     * Batch preload animation data for multiple weapon types (C++ optimization)
     * 
     * This allows implementations to optimize loading and caching strategies
     * when multiple weapon types are known to be needed soon.
     * 
     * @param WeaponTypes Array of weapon type tags to preload
     */
    virtual void PreloadAnimationDataBatch(const TArray<FGameplayTag>& WeaponTypes) {}
    
    /**
     * Get cache statistics for performance monitoring (C++ only)
     * 
     * @param OutHitRate Cache hit rate percentage (0-100)
     * @param OutMemoryUsageBytes Approximate memory usage in bytes
     * @param OutCacheEntries Number of cached entries
     */
    virtual void GetCacheMetrics(float& OutHitRate, int32& OutMemoryUsageBytes, int32& OutCacheEntries) const {}

    //==================================================================
    // Blueprint-Compatible Interface (UFUNCTIONs)
    //==================================================================
    
    /**
     * Get complete animation state data for a weapon type (Blueprint-safe)
     * 
     * This method uses value semantics for Blueprint safety. The data is copied
     * to the output parameter, which has performance implications but ensures
     * memory safety in Blueprint graphs.
     * 
     * @param WeaponType Gameplay tag identifying weapon type
     * @param OutAnimationData Copy of animation state data (output parameter)
     * @return True if data was found and successfully copied
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Data", 
        meta = (DisplayName = "Get Animation State Data"))
    bool GetAnimationStateData(const FGameplayTag& WeaponType, FAnimationStateData& OutAnimationData) const;
    
    /**
     * Get draw animation montage for weapon
     * 
     * Returns the montage used when drawing (equipping) the weapon.
     * Different animations can be provided for first draw vs subsequent draws.
     * 
     * @param WeaponType Weapon archetype tag (e.g., Weapon.Type.Rifle.AssaultRifle)
     * @param bFirstDraw True if this is the first time drawing this weapon in current session
     * @return Draw animation montage, or nullptr if not configured
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Montages",
        meta = (DisplayName = "Get Draw Montage"))
    UAnimMontage* GetDrawMontage(const FGameplayTag& WeaponType, bool bFirstDraw = false) const;
    
    /**
     * Get holster animation montage for weapon
     * 
     * Returns the montage used when holstering (unequipping) the weapon.
     * 
     * @param WeaponType Weapon archetype tag
     * @return Holster animation montage, or nullptr if not configured
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Montages",
        meta = (DisplayName = "Get Holster Montage"))
    UAnimMontage* GetHolsterMontage(const FGameplayTag& WeaponType) const;
    
    /**
     * Get stance blend space for weapon movement
     * 
     * Returns the blend space used for locomotion while this weapon is equipped.
     * Typically blends between idle, walk, and run animations based on speed.
     * 
     * @param WeaponType Weapon archetype tag
     * @return Stance blend space, or nullptr if not configured
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|BlendSpaces",
        meta = (DisplayName = "Get Stance Blend Space"))
    UBlendSpace* GetStanceBlendSpace(const FGameplayTag& WeaponType) const;
    
    /**
     * Get idle animation sequence for weapon
     * 
     * Returns the base idle animation when standing still with this weapon.
     * 
     * @param WeaponType Weapon archetype tag
     * @return Idle animation sequence, or nullptr if not configured
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Sequences",
        meta = (DisplayName = "Get Idle Animation"))
    UAnimSequence* GetIdleAnimation(const FGameplayTag& WeaponType) const;
    
    /**
     * Get switch animation montage for transitioning between weapons
     * 
     * Returns specialized transition animation between specific weapon types.
     * If no specific transition exists, implementations should return nullptr
     * and the system will use holster + draw sequence instead.
     * 
     * @param FromWeaponType Source weapon being holstered
     * @param ToWeaponType Target weapon being drawn
     * @return Switch animation montage, or nullptr for default behavior
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Montages",
        meta = (DisplayName = "Get Switch Montage"))
    UAnimMontage* GetSwitchMontage(const FGameplayTag& FromWeaponType, const FGameplayTag& ToWeaponType) const;
    
    /**
     * Get reload animation montage
     * 
     * Returns reload animation, with different variants for tactical vs empty reload.
     * 
     * @param WeaponType Weapon archetype tag
     * @param bIsEmpty True for empty reload (chamber empty), false for tactical reload
     * @return Reload animation montage, or nullptr if not configured
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Montages",
        meta = (DisplayName = "Get Reload Montage"))
    UAnimMontage* GetReloadMontage(const FGameplayTag& WeaponType, bool bIsEmpty = false) const;
    
    //==================================================================
    // Transform Data Access
    //==================================================================
    
    /**
     * Get weapon attachment transform offset
     * 
     * Returns the transform offset for attaching weapon mesh to character.
     * This is relative to the attachment socket.
     * 
     * @param WeaponType Weapon archetype tag
     * @return Weapon transform offset in socket space
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Transforms",
        meta = (DisplayName = "Get Weapon Transform"))
    FTransform GetWeaponTransform(const FGameplayTag& WeaponType) const;
    
    /**
     * Get left hand IK transform for two-handed weapon grip
     * 
     * Returns transform for left hand IK target relative to weapon.
     * Multiple grip indices support different poses (relaxed, aiming, etc.)
     * 
     * @param WeaponType Weapon archetype tag
     * @param GripIndex Grip variation (0 = default, 1 = aiming, 2 = sprinting, etc.)
     * @return Left hand transform in weapon space
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Transforms",
        meta = (DisplayName = "Get Left Hand Grip Transform"))
    FTransform GetLeftHandGripTransform(const FGameplayTag& WeaponType, int32 GripIndex = 0) const;
    
    /**
     * Get right hand transform override for weapon
     * 
     * Returns transform override for right hand when holding this weapon.
     * Usually Identity, but some weapons may need adjustments.
     * 
     * @param WeaponType Weapon archetype tag
     * @return Right hand transform adjustment
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Transforms",
        meta = (DisplayName = "Get Right Hand Transform"))
    FTransform GetRightHandTransform(const FGameplayTag& WeaponType) const;
    
    //==================================================================
    // Animation Timing Queries
    //==================================================================
    
    /**
     * Get duration of draw animation in seconds
     * 
     * Used for timing state transitions and ability cooldowns.
     * 
     * @param WeaponType Weapon archetype tag
     * @param bFirstDraw True if querying first draw duration
     * @return Animation duration in seconds, or default if not found
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Duration",
        meta = (DisplayName = "Get Draw Duration"))
    float GetDrawDuration(const FGameplayTag& WeaponType, bool bFirstDraw = false) const;
    
    /**
     * Get duration of holster animation in seconds
     * 
     * @param WeaponType Weapon archetype tag
     * @return Animation duration in seconds, or default if not found
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Duration",
        meta = (DisplayName = "Get Holster Duration"))
    float GetHolsterDuration(const FGameplayTag& WeaponType) const;
    
    /**
     * Get total duration for switching between weapons
     * 
     * Calculates total time including holster and draw animations.
     * May account for specialized transition animations.
     * 
     * @param FromWeaponType Source weapon type
     * @param ToWeaponType Target weapon type
     * @return Total switch duration in seconds
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Duration",
        meta = (DisplayName = "Get Switch Duration"))
    float GetSwitchDuration(const FGameplayTag& FromWeaponType, const FGameplayTag& ToWeaponType) const;
    
    /**
     * Get reload animation duration
     * 
     * @param WeaponType Weapon archetype tag
     * @param bIsEmpty True for empty reload duration
     * @return Reload duration in seconds
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Duration",
        meta = (DisplayName = "Get Reload Duration"))
    float GetReloadDuration(const FGameplayTag& WeaponType, bool bIsEmpty = false) const;
    
    //==================================================================
    // Validation and Queries
    //==================================================================
    
    /**
     * Check if animation data exists for weapon type
     * 
     * Quick check without loading full data structure.
     * 
     * @param WeaponType Weapon archetype tag to check
     * @return True if animation data is configured for this weapon
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Validation",
        meta = (DisplayName = "Has Animation Data"))
    bool HasAnimationData(const FGameplayTag& WeaponType) const;
    
    /**
     * Validate animation data completeness and integrity
     * 
     * Performs comprehensive validation of all animation assets.
     * Useful for content validation and debugging missing animations.
     * 
     * @param WeaponType Weapon archetype tag to validate
     * @param OutErrors Array populated with validation error messages
     * @return True if all required animations are present and valid
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Validation",
        meta = (DisplayName = "Validate Animation Data"))
    bool ValidateAnimationData(const FGameplayTag& WeaponType, TArray<FString>& OutErrors) const;
    
    /**
     * Get all configured weapon types
     * 
     * Returns list of all weapon types that have animation data configured.
     * Useful for UI and debugging.
     * 
     * @return Array of weapon type tags with available animations
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Queries",
        meta = (DisplayName = "Get Available Weapon Types"))
    TArray<FGameplayTag> GetAvailableWeaponTypes() const;
    
    /**
     * Check if specialized switch animation exists
     * 
     * Quick check for transition animation without loading the asset.
     * 
     * @param FromWeaponType Source weapon type
     * @param ToWeaponType Target weapon type
     * @return True if specialized transition exists
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "WeaponAnimation|Queries",
        meta = (DisplayName = "Has Switch Animation"))
    bool HasSwitchAnimation(const FGameplayTag& FromWeaponType, const FGameplayTag& ToWeaponType) const;
};