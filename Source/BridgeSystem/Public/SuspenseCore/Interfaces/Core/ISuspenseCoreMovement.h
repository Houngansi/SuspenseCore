// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseCoreMovement.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreMovement : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for movement-related functionality
 * Provides unified API for all movement mechanics: walking, sprinting, jumping, crouching
 * Uses EventDelegateManager for all event notifications
 */
class BRIDGESYSTEM_API ISuspenseCoreMovement
{
    GENERATED_BODY()

public:
    // ========================================
    // Speed Management
    // ========================================
    
    /** Get current movement speed */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Speed")
    float GetCurrentMovementSpeed() const;

    /** Set movement speed with optional notification */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Speed")
    void SetMovementSpeed(float NewSpeed);

    /** Get default movement speed (base speed without modifiers) */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Speed")
    float GetDefaultMovementSpeed() const;

    /** Get max walk speed for character */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Speed")
    float GetMaxWalkSpeed() const;

    // ========================================
    // Sprint Management
    // ========================================
    
    /** Check if character can sprint */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Sprint")
    bool CanSprint() const;

    /** Check if character is currently sprinting */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Sprint")
    bool IsSprinting() const;

    /** Start sprinting */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Sprint")
    void StartSprinting();

    /** Stop sprinting */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Sprint")
    void StopSprinting();

    // ========================================
    // Jump Management
    // ========================================
    
    /** Perform jump action */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Jump")
    void Jump();

    /** Stop jumping (for controlling jump height) */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Jump")
    void StopJumping();

    /** Check if character can jump */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Jump")
    bool CanJump() const;

    /** Check if character is on ground */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Jump")
    bool IsGrounded() const;

    /** Check if character is falling */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Jump")
    bool IsFalling() const;

    /** Get jump Z velocity */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Jump")
    float GetJumpZVelocity() const;

    /** Set jump Z velocity (for ability modifications) */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Jump")
    void SetJumpZVelocity(float NewJumpZVelocity);

    // ========================================
    // Crouch Management
    // ========================================
    
    /** Start crouching */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Crouch")
    void Crouch();

    /** Stop crouching */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Crouch")
    void UnCrouch();

    /** Check if character can crouch */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Crouch")
    bool CanCrouch() const;

    /** Check if character is currently crouching */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Crouch")
    bool IsCrouching() const;

    /** Get crouch height scale */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Crouch")
    float GetCrouchedHalfHeight() const;

    // ========================================
    // State Management
    // ========================================
    
    /** Get current movement state as gameplay tag */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|State")
    FGameplayTag GetMovementState() const;

    /** Set movement state (for ability system integration) */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|State")
    void SetMovementState(FGameplayTag NewState);

    /** Get all active movement tags */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|State")
    FGameplayTagContainer GetActiveMovementTags() const;

    /** Check if has specific movement tag */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|State")
    bool HasMovementTag(FGameplayTag Tag) const;

    // ========================================
    // Physics & Environment
    // ========================================
    
    /** Check if character is in water */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Environment")
    bool IsSwimming() const;

    /** Check if character is flying */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Environment")
    bool IsFlying() const;

    /** Get current velocity */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Physics")
    FVector GetVelocity() const;

    /** Get ground normal if grounded */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Movement|Physics")
    FVector GetGroundNormal() const;

    // ========================================
    // Static Event Notifications (Use EventDelegateManager)
    // ========================================
    
    /** Notify about movement speed change through event system */
    static void NotifyMovementSpeedChanged(const UObject* Source, float OldSpeed, float NewSpeed, bool bIsSprinting);
    
    /** Notify about movement state change through event system */
    static void NotifyMovementStateChanged(const UObject* Source, FGameplayTag NewState, bool bIsTransitioning);
    
    /** Notify about jump state change */
    static void NotifyJumpStateChanged(const UObject* Source, bool bIsJumping);
    
    /** Notify about crouch state change */
    static void NotifyCrouchStateChanged(const UObject* Source, bool bIsCrouching);
    
    /** Notify about landing after jump/fall */
    static void NotifyLanded(const UObject* Source, float ImpactVelocity);

    /** Get delegate manager for movement events */
    static class USuspenseCoreEventManager* GetDelegateManagerStatic(const UObject* WorldContextObject);
};