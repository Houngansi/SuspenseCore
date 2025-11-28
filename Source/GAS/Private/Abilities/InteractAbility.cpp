// Copyright Suspense Team. All Rights Reserved.

#include "Abilities/InteractAbility.h"
#include "Interfaces/Interaction/ISuspenseInteract.h"
#include "Delegates/SuspenseEventManager.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UInteractAbility::UInteractAbility()
{
    // Instance per actor for state tracking
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

    // Local predicted for responsive interaction
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // Set input ID
    AbilityInputID = ESuspenseAbilityInputID::Interact;

    // Enable replication
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

    // Initialize tags
    InteractInputTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input.Interact"));
    InteractSuccessTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Interact.Success"));
    InteractFailedTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Interact.Failed"));
    InteractCooldownTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Interact.Cooldown"));
    InteractingTag = FGameplayTag::RequestGameplayTag(TEXT("State.Interacting"));

    // Setup ability tags - renamed to avoid shadowing
    FGameplayTagContainer InteractAbilityTagContainer;
    InteractAbilityTagContainer.AddTag(InteractInputTag);
    SetAssetTags(InteractAbilityTagContainer);

    // Setup blocking tags
    BlockTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead")));
    BlockTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Stunned")));
    BlockTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Disabled")));
    ActivationBlockedTags.AppendTags(BlockTags);

    // Setup cooldown tags
    CooldownTags.AddTag(InteractCooldownTag);

    // Default values
    InteractDistance = 500.0f;
    CooldownDuration = 0.5f;
    TraceChannel = ECC_Visibility;

    // Additional channels for better compatibility
    AdditionalTraceChannels.Add(ECC_WorldDynamic);
    AdditionalTraceChannels.Add(ECC_Pawn);

    // Debug settings
    bShowDebugTrace = false;
    DebugTraceDuration = 2.0f;
}

bool UInteractAbility::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    // Base checks
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        LogInteractionDebugInfo(TEXT("Base ability checks failed"));
        return false;
    }

    // Check for valid actor info
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        LogInteractionDebugInfo(TEXT("Invalid ActorInfo or AvatarActor"), true);
        return false;
    }

    // Check for blocking tags
    if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
    {
        if (ASC->HasAnyMatchingGameplayTags(BlockTags))
        {
            LogInteractionDebugInfo(TEXT("Blocked by gameplay tags"));
            return false;
        }
    }

    return true;
}

void UInteractAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    LogInteractionDebugInfo(TEXT("ActivateAbility started"));

    // Check authority or prediction
    if (!HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
    {
        LogInteractionDebugInfo(TEXT("No authority or prediction key - ending ability"), true);
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Store prediction key for networking
    CurrentPredictionKey = GetCurrentActivationInfo().GetActivationPredictionKey();

    // Commit ability
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        LogInteractionDebugInfo(TEXT("CommitAbility failed - ending ability"), true);
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Add interacting tag
    if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
    {
        ASC->AddLooseGameplayTag(InteractingTag);
    }

    // Perform trace to find interaction target
    AActor* TargetActor = PerformInteractionTrace(ActorInfo);

    if (TargetActor)
    {
        // Check if target implements interaction interface
        if (TargetActor->Implements<USuspenseInteract>())
        {
            APlayerController* PC = Cast<APlayerController>(ActorInfo->PlayerController.Get());

            // Check if we can interact
            bool bCanInteract = ISuspenseInteract::Execute_CanInteract(TargetActor, PC);

            if (bCanInteract)
            {
                if (ActivationInfo.ActivationMode == EGameplayAbilityActivationMode::Authority)
                {
                    // Server: perform interaction directly
                    bool bSuccess = ISuspenseInteract::Execute_Interact(TargetActor, PC);

                    // Send result to client
                    if (PC && !PC->IsLocalController())
                    {
                        ClientInteractionResult(bSuccess, TargetActor);
                    }

                    // Send events and notifications
                    SendInteractionEvent(ActorInfo, bSuccess, TargetActor);
                    NotifyInteraction(bSuccess, TargetActor);
                }
                else
                {
                    // Client: send RPC to server
                    ServerPerformInteraction(TargetActor);

                    // Optimistically show success for responsiveness
                    SendInteractionEvent(ActorInfo, true, TargetActor);
                }

                LogInteractionDebugInfo(FString::Printf(TEXT("Interaction initiated with %s"),
                    *TargetActor->GetName()));
            }
            else
            {
                LogInteractionDebugInfo(FString::Printf(TEXT("Cannot interact with %s"),
                    *TargetActor->GetName()));

                SendInteractionEvent(ActorInfo, false, TargetActor);
                NotifyInteraction(false, TargetActor);
            }
        }
        else
        {
            LogInteractionDebugInfo(FString::Printf(TEXT("Target %s doesn't implement interaction interface"),
                *TargetActor->GetName()));

            SendInteractionEvent(ActorInfo, false, TargetActor);
            NotifyInteraction(false, TargetActor);
        }
    }
    else
    {
        LogInteractionDebugInfo(TEXT("No valid interaction target found"));
        SendInteractionEvent(ActorInfo, false, nullptr);
        NotifyInteraction(false, nullptr);
    }

    // Apply cooldown on authority
    if (ActivationInfo.ActivationMode == EGameplayAbilityActivationMode::Authority)
    {
        ApplyCooldown(Handle, ActorInfo, ActivationInfo);
    }

    // End ability
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void UInteractAbility::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    LogInteractionDebugInfo(FString::Printf(TEXT("EndAbility called. Cancelled: %s"),
        bWasCancelled ? TEXT("Yes") : TEXT("No")));

    // Remove tags
    if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
    {
        ASC->RemoveLooseGameplayTag(InteractingTag);
        ASC->RemoveLooseGameplayTag(InteractSuccessTag);
        ASC->RemoveLooseGameplayTag(InteractFailedTag);
    }

    // Clear prediction key
    CurrentPredictionKey = FPredictionKey();

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UInteractAbility::ServerPerformInteraction_Implementation(AActor* TargetActor)
{
    if (!CurrentActorInfo || !TargetActor)
    {
        return;
    }

    // Validate target is still valid and in range
    AActor* ValidatedTarget = PerformInteractionTrace(CurrentActorInfo);
    if (ValidatedTarget != TargetActor)
    {
        LogInteractionDebugInfo(TEXT("Server validation failed - target mismatch"), true);
        ClientInteractionResult(false, nullptr);
        return;
    }

    // Perform interaction
    APlayerController* PC = Cast<APlayerController>(CurrentActorInfo->PlayerController.Get());
    bool bSuccess = false;

    if (TargetActor->Implements<USuspenseInteract>())
    {
        bool bCanInteract = ISuspenseInteract::Execute_CanInteract(TargetActor, PC);
        if (bCanInteract)
        {
            bSuccess = ISuspenseInteract::Execute_Interact(TargetActor, PC);
        }
    }

    // Send result back to client
    ClientInteractionResult(bSuccess, TargetActor);

    // Send events and notifications
    SendInteractionEvent(CurrentActorInfo, bSuccess, TargetActor);
    NotifyInteraction(bSuccess, TargetActor);
}

void UInteractAbility::ClientInteractionResult_Implementation(bool bSuccess, AActor* TargetActor)
{
    if (!CurrentActorInfo)
    {
        return;
    }

    // Update client state based on server result
    SendInteractionEvent(CurrentActorInfo, bSuccess, TargetActor);
    NotifyInteraction(bSuccess, TargetActor);

    LogInteractionDebugInfo(FString::Printf(TEXT("Client received interaction result: %s"),
        bSuccess ? TEXT("Success") : TEXT("Failed")));
}

AActor* UInteractAbility::PerformInteractionTrace(const FGameplayAbilityActorInfo* ActorInfo) const
{
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        LogInteractionDebugInfo(TEXT("PerformInteractionTrace: Invalid AvatarActor"), true);
        return nullptr;
    }

    // Get trace start and direction
    FVector TraceStart;
    FRotator TraceRotation;

    APlayerController* PC = Cast<APlayerController>(ActorInfo->PlayerController.Get());
    ACharacter* CharacterPawn = Cast<ACharacter>(ActorInfo->AvatarActor.Get());

    if (PC)
    {
        PC->GetPlayerViewPoint(TraceStart, TraceRotation);
    }
    else if (CharacterPawn)
    {
        UCameraComponent* Camera = CharacterPawn->FindComponentByClass<UCameraComponent>();
        if (Camera)
        {
            TraceStart = Camera->GetComponentLocation();
            TraceRotation = Camera->GetComponentRotation();
        }
        else
        {
            TraceStart = CharacterPawn->GetActorLocation() + FVector(0, 0, CharacterPawn->BaseEyeHeight);
            TraceRotation = CharacterPawn->GetControlRotation();
        }
    }
    else
    {
        AActor* Avatar = ActorInfo->AvatarActor.Get();
        TraceStart = Avatar->GetActorLocation();
        TraceRotation = Avatar->GetActorRotation();
    }

    // Calculate trace end
    float Distance = InteractDistance.GetValueAtLevel(GetAbilityLevel());
    FVector TraceEnd = TraceStart + TraceRotation.Vector() * Distance;

    // Setup collision params
    FCollisionQueryParams Params(TEXT("InteractTrace"), true, ActorInfo->AvatarActor.Get());
    Params.bReturnPhysicalMaterial = false;

    // Ignore character's capsule
    if (CharacterPawn)
    {
        Params.AddIgnoredComponent(CharacterPawn->GetCapsuleComponent());
    }

    // Perform multi-channel trace
    TArray<FHitResult> AllHits;

    // Primary channel
    TArray<FHitResult> PrimaryHits;
    GetWorld()->LineTraceMultiByChannel(PrimaryHits, TraceStart, TraceEnd, TraceChannel, Params);
    AllHits.Append(PrimaryHits);

    // Additional channels
    for (ECollisionChannel Channel : AdditionalTraceChannels)
    {
        TArray<FHitResult> ChannelHits;
        if (GetWorld()->LineTraceMultiByChannel(ChannelHits, TraceStart, TraceEnd, Channel, Params))
        {
            AllHits.Append(ChannelHits);
        }
    }

    // Sort by distance
    AllHits.Sort([](const FHitResult& A, const FHitResult& B) {
        return A.Distance < B.Distance;
    });

    // Debug visualization
    if (bShowDebugTrace)
    {
        DrawDebugInteraction(TraceStart, TraceEnd, AllHits.Num() > 0, AllHits);
    }

    // Find best interactable target
    AActor* BestTarget = nullptr;
    for (const FHitResult& Hit : AllHits)
    {
        AActor* HitActor = Hit.GetActor();
        if (HitActor && HitActor->Implements<USuspenseInteract>())
        {
            BestTarget = HitActor;
            break;
        }
    }

    // If no interactable found, return closest hit
    if (!BestTarget && AllHits.Num() > 0)
    {
        BestTarget = AllHits[0].GetActor();
    }

    return BestTarget;
}

void UInteractAbility::ApplyCooldown(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo) const
{
    UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
    if (!CooldownGE)
    {
        return;
    }

    TSubclassOf<UGameplayEffect> CooldownClass = CooldownGE->GetClass();
    int32 AbilityLevel = GetAbilityLevel(Handle, ActorInfo);
    FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CooldownClass, AbilityLevel);

    if (SpecHandle.IsValid())
    {
        FGameplayTag DurationTag = FGameplayTag::RequestGameplayTag(TEXT("Data.Cooldown.Duration"));
        SpecHandle.Data->SetSetByCallerMagnitude(DurationTag, CooldownDuration.GetValueAtLevel(AbilityLevel));

        FActiveGameplayEffectHandle ActiveHandle = ApplyGameplayEffectSpecToOwner(
            Handle, ActorInfo, ActivationInfo, SpecHandle);

        if (ActiveHandle.IsValid())
        {
            LogInteractionDebugInfo(FString::Printf(TEXT("Applied cooldown: %.2f seconds"),
                CooldownDuration.GetValueAtLevel(AbilityLevel)));
        }
    }
}

void UInteractAbility::SendInteractionEvent(
    const FGameplayAbilityActorInfo* ActorInfo,
    bool bSuccess,
    AActor* TargetActor) const
{
    if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
    {
        return;
    }

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

    // Add result tag
    FGameplayTag ResultTag = bSuccess ? InteractSuccessTag : InteractFailedTag;
    ASC->AddLooseGameplayTag(ResultTag);

    // Create and send event
    FGameplayEventData Payload;
    Payload.EventTag = ResultTag;
    Payload.Instigator = ActorInfo->AvatarActor.Get();
    Payload.Target = TargetActor;

    ASC->HandleGameplayEvent(ResultTag, &Payload);
}

void UInteractAbility::NotifyInteraction(bool bSuccess, AActor* TargetActor) const
{
    if (USuspenseEventManager* Manager = GetDelegateManager())
    {
        // Send interaction event
        FGameplayTag EventTag = bSuccess ? InteractSuccessTag : InteractFailedTag;
        FString EventData = FString::Printf(TEXT("Target: %s"),
            TargetActor ? *TargetActor->GetName() : TEXT("None"));

        Manager->NotifyEquipmentEvent(
            const_cast<UInteractAbility*>(this),
            EventTag,
            EventData);
    }
}

USuspenseEventManager* UInteractAbility::GetDelegateManager() const
{
    if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid())
    {
        return USuspenseEventManager::Get(CurrentActorInfo->AvatarActor.Get());
    }
    return nullptr;
}

void UInteractAbility::LogInteractionDebugInfo(const FString& Message, bool bError) const
{
#if !UE_BUILD_SHIPPING
    const FString Prefix = TEXT("[InteractAbility] ");
    if (bError)
    {
        UE_LOG(LogTemp, Warning, TEXT("%s%s"), *Prefix, *Message);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("%s%s"), *Prefix, *Message);
    }
#endif
}

void UInteractAbility::DrawDebugInteraction(
    const FVector& Start,
    const FVector& End,
    bool bHit,
    const TArray<FHitResult>& Hits) const
{
#if ENABLE_DRAW_DEBUG
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // Draw trace line
    DrawDebugLine(World, Start, End, bHit ? FColor::Green : FColor::Red, false, DebugTraceDuration, 0, 2.0f);

    // Draw hit points
    for (const FHitResult& Hit : Hits)
    {
        FColor Color = FColor::Yellow;

        if (Hit.GetActor() && Hit.GetActor()->Implements<USuspenseInteract>())
        {
            Color = FColor::Green;
        }

        DrawDebugSphere(World, Hit.ImpactPoint, 10.0f, 8, Color, false, DebugTraceDuration);

        if (Hit.GetActor())
        {
            DrawDebugString(World, Hit.ImpactPoint + FVector(0, 0, 20),
                Hit.GetActor()->GetName(),
                nullptr, Color, DebugTraceDuration);
        }
    }
#endif
}
