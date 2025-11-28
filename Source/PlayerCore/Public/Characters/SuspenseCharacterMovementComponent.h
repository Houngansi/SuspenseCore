// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCharacterMovementComponent.generated.h"

UENUM(BlueprintType)
enum class ESuspenseMovementMode : uint8
{
    None        UMETA(DisplayName = "None"),
    Walking     UMETA(DisplayName = "Walking"),
    Sprinting   UMETA(DisplayName = "Sprinting"),
    Crouching   UMETA(DisplayName = "Crouching"),
    Jumping     UMETA(DisplayName = "Jumping"),
    Falling     UMETA(DisplayName = "Falling"),
    Sliding     UMETA(DisplayName = "Sliding"),
    Swimming    UMETA(DisplayName = "Swimming"),
    Flying      UMETA(DisplayName = "Flying")
};

/**
 * Custom movement component for Suspense characters
 * Synchronizes movement speed with GAS AttributeSet
 * Manages movement states through GameplayAbility System tags
 */
UCLASS()
class PLAYERCORE_API USuspenseCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    USuspenseCharacterMovementComponent();

    // BEGIN UActorComponent Interface
    virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void BeginPlay() override;
    // END UActorComponent Interface

    // BEGIN UCharacterMovementComponent Interface
    virtual bool DoJump(bool bReplayingMoves) override;
    virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
    virtual void Crouch(bool bClientSimulation = false) override;
    virtual void UnCrouch(bool bClientSimulation = false) override;
    // END UCharacterMovementComponent Interface

    /** CRITICAL: Synchronizes movement speed with AttributeSet - the only way to update speed */
    UFUNCTION(BlueprintCallable, Category = "Suspense|Movement")
    void SyncMovementSpeedFromAttributes();

    /** Get current movement mode */
    UFUNCTION(BlueprintPure, Category = "Suspense|Movement")
    ESuspenseMovementMode GetCurrentMovementMode() const;

    // Movement states - read only, managed through GAS tags
    // Using unique names to avoid conflicts with parent class

    /** Check if character is in sprint state (synchronized with GAS) */
    UFUNCTION(BlueprintPure, Category = "Suspense|Movement", DisplayName = "Is Sprinting (GAS)")
    bool IsSprintingFromGAS() const { return bIsSprintingGAS; }

    /** Check if character is in crouch state (synchronized with GAS) */
    UFUNCTION(BlueprintPure, Category = "Suspense|Movement", DisplayName = "Is Crouching (GAS)")
    bool IsCrouchingFromGAS() const { return bIsCrouchingGAS; }

    /** Check if character is jumping */
    UFUNCTION(BlueprintPure, Category = "Suspense|Movement")
    bool IsJumping() const { return bIsJumping; }

    /** Check if character is in air */
    UFUNCTION(BlueprintPure, Category = "Suspense|Movement")
    bool IsInAir() const { return IsFalling(); }

    /** Check if character is sliding */
    UFUNCTION(BlueprintPure, Category = "Suspense|Movement")
    bool IsSliding() const { return bIsSliding; }

    // Backward compatibility methods - use GAS-synchronized values
    bool IsSprinting() const { return bIsSprintingGAS; }
    bool IsCrouching() const override { return bIsCrouchingGAS; }

    // Sliding mechanics
    void StartSliding();
    void StopSliding();
    bool CanSlide() const;

protected:
    /** Updates internal state flags based on GAS tags */
    void UpdateMovementStateFromTags();

    /** Get owner's ASC (supports ASC on Character or PlayerState) */
    class UAbilitySystemComponent* GetOwnerASC() const;

    /** Get owner's AttributeSet */
    const class UDefaultAttributeSet* GetOwnerAttributeSet() const;

    // Internal state flags - synchronized with GAS tags
    // Renamed for clarity that these are GAS-synchronized values
    bool bIsSprintingGAS;
    bool bIsCrouchingGAS;
    bool bIsJumping;
    bool bIsSliding;

    // Sliding parameters
    UPROPERTY(EditDefaultsOnly, Category = "Suspense|Movement|Sliding")
    float MinSlideSpeed = 400.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Suspense|Movement|Sliding")
    float SlideSpeed = 600.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Suspense|Movement|Sliding")
    float SlideFriction = 0.1f;

    UPROPERTY(EditDefaultsOnly, Category = "Suspense|Movement|Sliding")
    float SlideDuration = 1.5f;

private:
    /** Cached tags for optimization */
    FGameplayTag SprintingTag;
    FGameplayTag CrouchingTag;

    /** Sliding variables */
    float SlideTimer;
    FVector SlideStartVelocity;

    /** Sliding logic update */
    void UpdateSliding(float DeltaTime);

    /** Counter for log rate limiting */
    int32 SyncLogCounter;
};
