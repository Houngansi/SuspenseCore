// Copyright Suspense Team. All Rights Reserved.

#include "Components/SuspenseInteractionComponent.h"
#include "Interfaces/Interaction/ISuspenseInteract.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Utils/SuspenseInteractionSettings.h"
#include "Utils/SuspenseHelpers.h"
#include "Delegates/SuspenseEventManager.h"
#include "AbilitySystemGlobals.h"
#include "TimerManager.h"

USuspenseInteractionComponent::USuspenseInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // Optimization: only 10 ticks per second
    SetIsReplicatedByDefault(true);

    // Load settings from configuration
    const USuspenseInteractionSettings* Settings = GetDefault<USuspenseInteractionSettings>();

    // Default settings
    TraceDistance = Settings ? Settings->DefaultTraceDistance : 300.0f;
    TraceChannel = Settings ? Settings->DefaultTraceChannel : TEnumAsByte<ECollisionChannel>(ECC_Visibility);
    bEnableDebugTrace = Settings ? Settings->bEnableDebugDraw : false;

    // Initialize tags
    InteractAbilityTag = FGameplayTag::RequestGameplayTag(FName("Ability.Input.Interact"));
    InteractSuccessTag = FGameplayTag::RequestGameplayTag(FName("Ability.Interact.Success"));
    InteractFailedTag = FGameplayTag::RequestGameplayTag(FName("Ability.Interact.Failed"));

    // Initialize blocking tags
    BlockingTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
    BlockingTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));
    BlockingTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Disabled")));
}

void USuspenseInteractionComponent::BeginPlay()
{
    Super::BeginPlay();

    // Cache AbilitySystemComponent on initialization
    CachedASC = GetOwnerASC();

    // Cache delegate manager
    CachedDelegateManager = GetDelegateManager();

    // Subscribe to interaction events if ASC exists
    if (CachedASC.IsValid())
    {
        // Use methods with correct signature
        CachedASC->GenericGameplayEventCallbacks.FindOrAdd(InteractSuccessTag).AddUObject(
            this, &USuspenseInteractionComponent::HandleInteractionSuccessDelegate);

        CachedASC->GenericGameplayEventCallbacks.FindOrAdd(InteractFailedTag).AddUObject(
            this, &USuspenseInteractionComponent::HandleInteractionFailureDelegate);

        LogInteraction(TEXT("Subscribed to AbilitySystemComponent events"));
    }
    else
    {
        LogInteraction(TEXT("AbilitySystemComponent not found, interaction events won't work"), true);
    }
}

void USuspenseInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Unsubscribe from events when component ends
    if (CachedASC.IsValid())
    {
        CachedASC->GenericGameplayEventCallbacks.FindOrAdd(InteractSuccessTag).RemoveAll(this);
        CachedASC->GenericGameplayEventCallbacks.FindOrAdd(InteractFailedTag).RemoveAll(this);
    }

    // Clear cooldown timer if active
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(CooldownTimerHandle);
    }

    // Clear focus if we had one
    if (LastInteractableActor.IsValid())
    {
        UpdateInteractionFocus(nullptr);
    }

    Super::EndPlay(EndPlayReason);
}

void USuspenseInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update UI information in tick if needed
    // Optimization: trace only for UI, all interaction logic is in GAS abilities
    if (IsValid(GetOwner()) && GetOwner()->HasAuthority() == false)
    {
        AActor* InteractableActor = PerformUIInteractionTrace();

        // Update focus if changed
        if (InteractableActor != LastInteractableActor.Get())
        {
            UpdateInteractionFocus(InteractableActor);
        }
    }
}

void USuspenseInteractionComponent::StartInteraction()
{
    // Check if we can interact
    if (bInteractionOnCooldown)
    {
        LogInteraction(TEXT("Interaction on cooldown"), true);
        return;
    }

    if (!CanInteractNow())
    {
        LogInteraction(TEXT("Interaction blocked"), true);
        OnInteractionFailed.Broadcast(nullptr);
        BroadcastInteractionResult(nullptr, false);
        return;
    }

    // Set cooldown
    SetInteractionCooldown();

    // Get current target
    AActor* TargetActor = PerformUIInteractionTrace();

    // Broadcast interaction attempt
    BroadcastInteractionAttempt(TargetActor);

    // Get ASC and activate ability instead of direct call
    if (CachedASC.IsValid())
    {
        // Activate ability by tag
        CachedASC->TryActivateAbilitiesByTag(FGameplayTagContainer(InteractAbilityTag));
        LogInteraction(TEXT("Started interaction ability"));
        return;
    }
    else
    {
        // If cached ASC became invalid, update it
        CachedASC = GetOwnerASC();

        if (CachedASC.IsValid())
        {
            CachedASC->TryActivateAbilitiesByTag(FGameplayTagContainer(InteractAbilityTag));
            LogInteraction(TEXT("Started interaction ability (after cache update)"));
            return;
        }
    }

    // If no ASC, report error
    LogInteraction(TEXT("Failed to activate interaction ability - no AbilitySystemComponent"), true);
    OnInteractionFailed.Broadcast(nullptr);
    BroadcastInteractionResult(nullptr, false);
}

bool USuspenseInteractionComponent::CanInteractNow() const
{
    // Check for blocking tags
    if (HasBlockingTags())
    {
        LogInteraction(TEXT("Interaction blocked by state tags"), true);
        return false;
    }

    // Check AbilitySystemComponent availability
    if (!CachedASC.IsValid())
    {
        UAbilitySystemComponent* CurrentASC = GetOwnerASC();
        if (!CurrentASC)
        {
            LogInteraction(TEXT("No AbilitySystemComponent"), true);
            return false;
        }

        // Update cache if ASC found
        const_cast<USuspenseInteractionComponent*>(this)->CachedASC = CurrentASC;
    }

    // Check interaction ability availability
    if (!CachedASC->HasMatchingGameplayTag(InteractAbilityTag))
    {
        LogInteraction(TEXT("No interaction ability with tag ") + InteractAbilityTag.ToString(), true);
        return false;
    }

    // Check for interactable object
    AActor* InteractableActor = PerformUIInteractionTrace();
    if (!InteractableActor)
    {
        LogInteraction(TEXT("No interactable object within reach"), true);
        return false;
    }

    // Check if object supports interaction interface
    if (!InteractableActor->GetClass()->ImplementsInterface(UMedComInteractInterface::StaticClass()))
    {
        LogInteraction(TEXT("Object doesn't support interaction interface"), true);
        return false;
    }

    // Check interaction possibility through interface
    APlayerController* PC = GetOwner()->GetInstigatorController<APlayerController>();
    if (!PC)
    {
        LogInteraction(TEXT("No PlayerController for interaction"), true);
        return false;
    }

    if (!IMedComInteractInterface::Execute_CanInteract(InteractableActor, PC))
    {
        LogInteraction(TEXT("Object doesn't allow interaction at this moment"), true);
        return false;
    }

    return true;
}

AActor* USuspenseInteractionComponent::PerformUIInteractionTrace() const
{
    // Get current camera position and direction
    FVector CameraLocation;
    FRotator CameraRotation;

    // Get view point for UI
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor) return nullptr;

    // Get view point, trying to get most accurate data
    if (APlayerController* PC = Cast<APlayerController>(OwnerActor->GetInstigatorController()))
    {
        PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
    }
    else if (UCameraComponent* FoundCamera = OwnerActor->FindComponentByClass<UCameraComponent>())
    {
        CameraLocation = FoundCamera->GetComponentLocation();
        CameraRotation = FoundCamera->GetComponentRotation();
    }
    else if (ACharacter* Character = Cast<ACharacter>(OwnerActor))
    {
        CameraLocation = Character->GetActorLocation() + FVector(0, 0, Character->BaseEyeHeight);
        CameraRotation = Character->GetControlRotation();
    }
    else
    {
        CameraLocation = OwnerActor->GetActorLocation() + FVector(0, 0, 50);
        CameraRotation = OwnerActor->GetActorRotation();
    }

    // Calculate trace
    FVector TraceStart = CameraLocation;
    FVector TraceEnd = TraceStart + CameraRotation.Vector() * TraceDistance;

    FHitResult HitResult;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(OwnerActor);

    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, TraceChannel, Params);

    // Debug line drawing only if enabled
    if (bEnableDebugTrace)
    {
        DrawDebugLine(GetWorld(), TraceStart, TraceEnd, bHit ? FColor::Green : FColor::Red, false, 0.1f);
        if (bHit)
        {
            DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, 10.f, 8, FColor::Yellow, false, 0.1f);
        }
    }

    if (bHit && HitResult.GetActor())
    {
        AActor* HitActor = HitResult.GetActor();

        // Check if actor implements interaction interface
        if (HitActor->GetClass()->ImplementsInterface(UMedComInteractInterface::StaticClass()))
        {
            return HitActor;
        }
    }

    // Nothing found or actor doesn't implement interface
    return nullptr;
}

void USuspenseInteractionComponent::HandleInteractionSuccessDelegate(const FGameplayEventData* Payload)
{
    if (Payload)
    {
        HandleInteractionSuccess(*Payload);
    }
}

void USuspenseInteractionComponent::HandleInteractionFailureDelegate(const FGameplayEventData* Payload)
{
    if (Payload)
    {
        HandleInteractionFailure(*Payload);
    }
}

void USuspenseInteractionComponent::HandleInteractionSuccess(const FGameplayEventData& Payload)
{
    const AActor* TargetActor = nullptr;

    // Get TargetActor from Payload
    if (Payload.Target)
    {
        TargetActor = Cast<AActor>(Payload.Target.Get());
    }

    if (!TargetActor)
    {
        LogInteraction(TEXT("HandleInteractionSuccess: No target actor in Payload"), true);
        return;
    }

    // Call successful interaction delegate
    OnInteractionSucceeded.Broadcast(const_cast<AActor*>(TargetActor));

    // Broadcast success through delegate manager
    BroadcastInteractionResult(const_cast<AActor*>(TargetActor), true);

    // Get interaction type for UI
    if (TargetActor->GetClass()->ImplementsInterface(UMedComInteractInterface::StaticClass()))
    {
        FGameplayTag InteractionType = IMedComInteractInterface::Execute_GetInteractionType(TargetActor);
        OnInteractionTypeChanged.Broadcast(const_cast<AActor*>(TargetActor), InteractionType);
    }

    LogInteraction(FString::Printf(TEXT("Successful interaction with %s"), *TargetActor->GetName()));
}

void USuspenseInteractionComponent::HandleInteractionFailure(const FGameplayEventData& Payload)
{
    const AActor* TargetActor = nullptr;

    // Get TargetActor from Payload
    if (Payload.Target)
    {
        TargetActor = Cast<AActor>(Payload.Target.Get());
    }

    // Call failed interaction delegate
    OnInteractionFailed.Broadcast(const_cast<AActor*>(TargetActor));

    // Broadcast failure through delegate manager
    BroadcastInteractionResult(const_cast<AActor*>(TargetActor), false);

    if (TargetActor)
    {
        LogInteraction(FString::Printf(TEXT("Failed interaction with %s"), *TargetActor->GetName()), true);
    }
    else
    {
        LogInteraction(TEXT("Failed interaction, target not found"), true);
    }
}

bool USuspenseInteractionComponent::HasBlockingTags() const
{
    // Check blocking tags through ASC
    if (CachedASC.IsValid())
    {
        return CachedASC->HasAnyMatchingGameplayTags(BlockingTags);
    }

    // If no cached ASC, try to get it
    UAbilitySystemComponent* CurrentASC = GetOwnerASC();
    if (CurrentASC)
    {
        return CurrentASC->HasAnyMatchingGameplayTags(BlockingTags);
    }

    // If no ASC, assume no blocking tags
    return false;
}

void USuspenseInteractionComponent::LogInteraction(const FString& Message, bool bError) const
{
    const FString Prefix = FString::Printf(TEXT("[%s] "), *GetNameSafe(GetOwner()));

    if (bError)
    {
        UE_LOG(LogSuspenseInteraction, Warning, TEXT("%s%s"), *Prefix, *Message);
    }
    else
    {
        UE_LOG(LogSuspenseInteraction, Log, TEXT("%s%s"), *Prefix, *Message);
    }
}

UAbilitySystemComponent* USuspenseInteractionComponent::GetOwnerASC() const
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        return nullptr;
    }

    // Check owner first
    if (OwnerActor->Implements<UAbilitySystemInterface>())
    {
        IAbilitySystemInterface* ASInterface = Cast<IAbilitySystemInterface>(OwnerActor);
        if (ASInterface)
        {
            UAbilitySystemComponent* ASC = ASInterface->GetAbilitySystemComponent();
            if (ASC) return ASC;
        }
    }

    // Check controller or pawn
    if (APlayerController* PC = Cast<APlayerController>(OwnerActor->GetInstigatorController()))
    {
        // Check controller
        if (PC->Implements<UAbilitySystemInterface>())
        {
            IAbilitySystemInterface* ASInterface = Cast<IAbilitySystemInterface>(PC);
            if (ASInterface)
            {
                UAbilitySystemComponent* ASC = ASInterface->GetAbilitySystemComponent();
                if (ASC) return ASC;
            }
        }

        // Check controller's pawn
        if (APawn* Pawn = PC->GetPawn())
        {
            if (Pawn->Implements<UAbilitySystemInterface>())
            {
                IAbilitySystemInterface* ASInterface = Cast<IAbilitySystemInterface>(Pawn);
                if (ASInterface)
                {
                    UAbilitySystemComponent* ASC = ASInterface->GetAbilitySystemComponent();
                    if (ASC) return ASC;
                }
            }
        }
    }

    // Nothing found
    return nullptr;
}

USuspenseEventManager* USuspenseInteractionComponent::GetDelegateManager() const
{
    // Check cached manager first
    if (CachedDelegateManager.IsValid())
    {
        return CachedDelegateManager.Get();
    }

    // Get manager through static method
    USuspenseEventManager* Manager = ISuspenseInteract::GetDelegateManagerStatic(this);
    if (Manager)
    {
        const_cast<USuspenseInteractionComponent*>(this)->CachedDelegateManager = Manager;
    }

    return Manager;
}

void USuspenseInteractionComponent::SetInteractionCooldown()
{
    if (InteractionCooldown > 0.0f)
    {
        bInteractionOnCooldown = true;
        GetWorld()->GetTimerManager().SetTimer(
            CooldownTimerHandle,
            this,
            &USuspenseInteractionComponent::ResetInteractionCooldown,
            InteractionCooldown,
            false);
    }
}

void USuspenseInteractionComponent::ResetInteractionCooldown()
{
    bInteractionOnCooldown = false;
}

void USuspenseInteractionComponent::UpdateInteractionFocus(AActor* NewFocusActor)
{
    // Get player controller for focus events
    APlayerController* PC = GetOwner() ? GetOwner()->GetInstigatorController<APlayerController>() : nullptr;

    // Handle previous focus loss
    if (LastInteractableActor.IsValid() && LastInteractableActor.Get() != NewFocusActor)
    {
        if (LastInteractableActor->GetClass()->ImplementsInterface(UMedComInteractInterface::StaticClass()))
        {
            if (PC)
            {
                IMedComInteractInterface::Execute_OnInteractionFocusLost(LastInteractableActor.Get(), PC);
            }

            // Notify UI
            OnInteractionTypeChanged.Broadcast(nullptr, FGameplayTag::EmptyTag);
        }
    }

    // Update focus
    LastInteractableActor = NewFocusActor;

    // Handle new focus gain
    if (NewFocusActor && NewFocusActor->GetClass()->ImplementsInterface(UMedComInteractInterface::StaticClass()))
    {
        if (PC)
        {
            IMedComInteractInterface::Execute_OnInteractionFocusGained(NewFocusActor, PC);
        }

        // Notify UI
        FGameplayTag InteractionType = IMedComInteractInterface::Execute_GetInteractionType(NewFocusActor);
        OnInteractionTypeChanged.Broadcast(NewFocusActor, InteractionType);
    }
}

void USuspenseInteractionComponent::BroadcastInteractionAttempt(AActor* TargetActor)
{
    USuspenseEventManager* Manager = GetDelegateManager();
    if (Manager)
    {
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Interaction.Event.Attempt"));
        FString EventData = TargetActor
            ? FString::Printf(TEXT("Target:%s"), *TargetActor->GetName())
            : TEXT("Target:None");

        Manager->NotifyEquipmentEvent(this, EventTag, EventData);
    }
}

void USuspenseInteractionComponent::BroadcastInteractionResult(AActor* TargetActor, bool bSuccess)
{
    USuspenseEventManager* Manager = GetDelegateManager();
    if (Manager)
    {
        FGameplayTag EventTag = bSuccess
            ? FGameplayTag::RequestGameplayTag(TEXT("Interaction.Event.Success"))
            : FGameplayTag::RequestGameplayTag(TEXT("Interaction.Event.Failed"));

        FString EventData = TargetActor
            ? FString::Printf(TEXT("Target:%s,Result:%s"), *TargetActor->GetName(), bSuccess ? TEXT("Success") : TEXT("Failed"))
            : FString::Printf(TEXT("Target:None,Result:%s"), bSuccess ? TEXT("Success") : TEXT("Failed"));

        Manager->NotifyEquipmentEvent(this, EventTag, EventData);
    }
}
