// Copyright Suspense Team. All Rights Reserved.

#include "Characters/SuspenseCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/SuspenseBaseAttributeSet.h"
#include "Interfaces/Core/ISuspenseMovement.h"

USuspenseCharacterMovementComponent::USuspenseCharacterMovementComponent()
{
    // Do NOT set MaxWalkSpeed here! It will be taken from AttributeSet
    // Set only physical movement parameters
    GroundFriction = 8.0f;
    BrakingFriction = 2.0f;
    BrakingDecelerationWalking = 2048.0f;

    // Initialize tags
    SprintingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Sprinting"));
    CrouchingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Crouching"));

    // Initialize states
    bIsSprintingGAS = false;
    bIsCrouchingGAS = false;
    bIsJumping = false;
    bIsSliding = false;

    SyncLogCounter = 0;
}

void USuspenseCharacterMovementComponent::BeginPlay()
{
    Super::BeginPlay();

    // Perform initial speed sync
    // Give a small delay to ensure AttributeSet is initialized
    if (CharacterOwner && CharacterOwner->GetWorld())
    {
        FTimerHandle InitTimer;
        CharacterOwner->GetWorld()->GetTimerManager().SetTimer(InitTimer, [this]()
        {
            SyncMovementSpeedFromAttributes();
            UE_LOG(LogTemp, Log, TEXT("[MovementComponent] Initial speed sync completed"));
        }, 0.1f, false);
    }
}

void USuspenseCharacterMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Sync speed with AttributeSet every tick
    // This ensures any changes through GameplayEffects are applied immediately
    SyncMovementSpeedFromAttributes();

    // Update movement states based on GAS tags
    UpdateMovementStateFromTags();

    // Update sliding if active
    if (bIsSliding)
    {
        UpdateSliding(DeltaTime);
    }

    // Update jump state
    if (IsFalling())
    {
        bIsJumping = Velocity.Z > 0.0f;
    }
    else
    {
        bIsJumping = false;
    }
}

void USuspenseCharacterMovementComponent::SyncMovementSpeedFromAttributes()
{
    // Get ASC
    UAbilitySystemComponent* ASC = GetOwnerASC();
    if (!ASC)
    {
        // Log only once per second to avoid spam
        if (++SyncLogCounter % 60 == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[MovementSync] No ASC found for speed sync"));
        }
        return;
    }

    // Get AttributeSet
    const USuspenseBaseAttributeSet* AttributeSet = GetOwnerAttributeSet();
    if (!AttributeSet)
    {
        if (++SyncLogCounter % 60 == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[MovementSync] No AttributeSet found for speed sync"));
        }
        return;
    }

    // CRITICAL: Get CURRENT attribute value with ALL modifiers
    // This includes base value + all active GameplayEffects
    float CurrentAttributeSpeed = ASC->GetNumericAttribute(AttributeSet->GetMovementSpeedAttribute());

    // Sync only if there's a discrepancy
    if (!FMath::IsNearlyEqual(MaxWalkSpeed, CurrentAttributeSpeed, 0.1f))
    {
        float OldSpeed = MaxWalkSpeed;
        MaxWalkSpeed = CurrentAttributeSpeed;

        // Log speed changes
        UE_LOG(LogTemp, Log, TEXT("[MovementSync] Speed updated: %.1f -> %.1f"),
            OldSpeed, CurrentAttributeSpeed);

        // Notify about speed change using GAS-synchronized value
        if (CharacterOwner && CharacterOwner->GetClass()->ImplementsInterface(USuspenseMovement::StaticClass()))
        {
            ISuspenseMovement::NotifyMovementSpeedChanged(
                CharacterOwner, OldSpeed, CurrentAttributeSpeed, bIsSprintingGAS);
        }
    }
}

void USuspenseCharacterMovementComponent::UpdateMovementStateFromTags()
{
    UAbilitySystemComponent* ASC = GetOwnerASC();
    if (!ASC)
    {
        return;
    }

    // Sync sprint state with tag
    bool bHasSprintTag = ASC->HasMatchingGameplayTag(SprintingTag);
    if (bIsSprintingGAS != bHasSprintTag)
    {
        bIsSprintingGAS = bHasSprintTag;
        UE_LOG(LogTemp, Log, TEXT("[MovementSync] Sprint state updated from tags: %s"),
            bIsSprintingGAS ? TEXT("ON") : TEXT("OFF"));
    }

    // Sync crouch state with tag
    bool bHasCrouchTag = ASC->HasMatchingGameplayTag(CrouchingTag);
    if (bIsCrouchingGAS != bHasCrouchTag)
    {
        bIsCrouchingGAS = bHasCrouchTag;
        UE_LOG(LogTemp, Log, TEXT("[MovementSync] Crouch state updated from tags: %s"),
            bIsCrouchingGAS ? TEXT("ON") : TEXT("OFF"));
    }
}

UAbilitySystemComponent* USuspenseCharacterMovementComponent::GetOwnerASC() const
{
    if (!CharacterOwner)
    {
        return nullptr;
    }

    // First try to get ASC directly from character
    UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CharacterOwner);

    // If not found, check PlayerState
    if (!ASC)
    {
        if (APlayerState* PS = CharacterOwner->GetPlayerState())
        {
            ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS);
        }
    }

    return ASC;
}

const USuspenseBaseAttributeSet* USuspenseCharacterMovementComponent::GetOwnerAttributeSet() const
{
    if (UAbilitySystemComponent* ASC = GetOwnerASC())
    {
        return ASC->GetSet<USuspenseBaseAttributeSet>();
    }
    return nullptr;
}

ESuspenseMovementMode USuspenseCharacterMovementComponent::GetCurrentMovementMode() const
{
    if (IsFlying())
        return ESuspenseMovementMode::Flying;

    if (IsSwimming())
        return ESuspenseMovementMode::Swimming;

    if (IsFalling())
    {
        if (bIsJumping && Velocity.Z > 0.0f)
            return ESuspenseMovementMode::Jumping;
        else
            return ESuspenseMovementMode::Falling;
    }

    if (bIsSliding)
        return ESuspenseMovementMode::Sliding;

    // Check if crouching (GAS-synchronized)
    if (bIsCrouchingGAS)
        return ESuspenseMovementMode::Crouching;

    // Check if sprinting (GAS-synchronized)
    if (bIsSprintingGAS)
    {
        // IMPORTANT: Only return Sprinting if actually moving!
        float Speed2D = Velocity.Size2D();
        if (Speed2D > 10.0f)
        {
            return ESuspenseMovementMode::Sprinting;
        }
        // If sprinting but not moving, fall through to idle check
    }

    // CRITICAL: Check actual velocity to determine idle vs walking
    float Speed2D = Velocity.Size2D();

    // Use a small threshold to account for floating point precision
    const float IdleThreshold = 10.0f;

    if (Speed2D < IdleThreshold)
    {
        // Character is stationary
        return ESuspenseMovementMode::None; // This represents idle state
    }

    // Character is moving on ground
    if (IsMovingOnGround())
    {
        return ESuspenseMovementMode::Walking;
    }

    return ESuspenseMovementMode::None;
}

void USuspenseCharacterMovementComponent::StartSliding()
{
    if (CanSlide())
    {
        bIsSliding = true;
        SlideTimer = SlideDuration;
        SlideStartVelocity = Velocity;

        // Reduce friction for sliding
        GroundFriction = SlideFriction;
        BrakingFriction = 0.0f;

        // Force crouch during slide
        if (CharacterOwner)
        {
            CharacterOwner->Crouch();
        }

        UE_LOG(LogTemp, Log, TEXT("[Movement] Slide started"));
    }
}

void USuspenseCharacterMovementComponent::StopSliding()
{
    if (bIsSliding)
    {
        bIsSliding = false;
        SlideTimer = 0.0f;

        // Restore normal friction
        GroundFriction = 8.0f;
        BrakingFriction = 2.0f;

        // Try to stand up
        if (CharacterOwner)
        {
            CharacterOwner->UnCrouch();
        }

        UE_LOG(LogTemp, Log, TEXT("[Movement] Slide stopped"));
    }
}

bool USuspenseCharacterMovementComponent::CanSlide() const
{
    return IsMovingOnGround() &&
           !bIsSliding &&
           !IsFalling() &&
           Velocity.Size() >= MinSlideSpeed;
}

void USuspenseCharacterMovementComponent::UpdateSliding(float DeltaTime)
{
    if (!bIsSliding)
        return;

    SlideTimer -= DeltaTime;

    // End slide when timer expires or speed is too low
    if (SlideTimer <= 0.0f || Velocity.Size() < MinSlideSpeed * 0.5f)
    {
        StopSliding();
        return;
    }

    // Apply sliding physics
    FVector ForwardDir = Velocity.GetSafeNormal();
    if (ForwardDir.IsZero())
    {
        ForwardDir = CharacterOwner->GetActorForwardVector();
    }

    // Add small forward impulse to maintain slide
    AddForce(ForwardDir * SlideSpeed * 2.0f);
}

bool USuspenseCharacterMovementComponent::DoJump(bool bReplayingMoves)
{
    // Stop sliding when jumping
    if (bIsSliding)
    {
        StopSliding();
    }

    bool bJumpSuccess = Super::DoJump(bReplayingMoves);

    if (bJumpSuccess)
    {
        bIsJumping = true;
        UE_LOG(LogTemp, Log, TEXT("[Movement] Jump started"));
    }

    return bJumpSuccess;
}

void USuspenseCharacterMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
    Super::ProcessLanded(Hit, remainingTime, Iterations);

    // Reset jump flag on landing
    bIsJumping = false;

    // Notify about landing
    if (CharacterOwner && CharacterOwner->GetClass()->ImplementsInterface(USuspenseMovement::StaticClass()))
    {
        float ImpactVelocity = Velocity.Z;
        ISuspenseMovement::NotifyLanded(CharacterOwner, ImpactVelocity);
    }

    UE_LOG(LogTemp, Log, TEXT("[Movement] Landed"));
}

void USuspenseCharacterMovementComponent::Crouch(bool bClientSimulation)
{
    // Don't allow normal crouch during slide
    if (!bIsSliding)
    {
        Super::Crouch(bClientSimulation);
        UE_LOG(LogTemp, Log, TEXT("[Movement] Crouch called"));
    }
}

void USuspenseCharacterMovementComponent::UnCrouch(bool bClientSimulation)
{
    // Don't allow standing up during slide
    if (!bIsSliding)
    {
        Super::UnCrouch(bClientSimulation);
        UE_LOG(LogTemp, Log, TEXT("[Movement] UnCrouch called"));
    }
}
