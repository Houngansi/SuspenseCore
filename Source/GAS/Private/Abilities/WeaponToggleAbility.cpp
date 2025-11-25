// Copyright Suspense Team. All Rights Reserved.

#include "Abilities/WeaponToggleAbility.h"
#include "Interfaces/Equipment/IMedComEquipmentInterface.h"
#include "Interfaces/Weapon/IMedComWeaponAnimationInterface.h"
#include "Subsystems/WeaponAnimationSubsystem.h"
#include "Delegates/EventDelegateManager.h"
#include "Types/Inventory/InventoryTypes.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameplayTagsManager.h"

UWeaponToggleAbility::UWeaponToggleAbility()
{
    // Ability configuration
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
    
    // Initialize tags
    WeaponTogglingTag = FGameplayTag::RequestGameplayTag(TEXT("State.WeaponToggling"));
    ToggleBlockTag = FGameplayTag::RequestGameplayTag(TEXT("Block.WeaponToggle"));
    
    EquipmentDrawingTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Drawing"));
    EquipmentHolsteringTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Holstering"));
    EquipmentReadyTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Ready"));
    EquipmentHolsteredTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Holstered"));
    
    // Input tags
    InputSlot1Tag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input.WeaponSlot1"));
    InputSlot2Tag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input.WeaponSlot2"));
    InputSlot3Tag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input.WeaponSlot3"));
    InputSlot4Tag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input.WeaponSlot4"));
    InputSlot5Tag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input.WeaponSlot5"));
    
    // Set ability tags
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Weapon.Toggle")));
    SetAssetTags(AssetTags);
    
    // Set blocking tags
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Weapon.Fire")));
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Weapon.Switch")));
    
    // Set activation blocked tags
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Stunned")));
    ActivationBlockedTags.AddTag(ToggleBlockTag);
    
    // Default values
    CurrentToggleSlot = INDEX_NONE;
    bIsDrawing = false;
    CurrentMontage = nullptr;
    CurrentPredictionKey = 0;
    AnimationPlayRate = 1.0f;
}

bool UWeaponToggleAbility::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }
    
    // Check for blocking states
    if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
    {
        // Don't allow toggle if already toggling
        if (ASC->HasMatchingGameplayTag(WeaponTogglingTag))
        {
            LogToggleDebug(TEXT("Already toggling weapon"));
            return false;
        }
        
        // Check reload state
        if (!bAllowToggleDuringReload && ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Reloading"))))
        {
            LogToggleDebug(TEXT("Cannot toggle during reload"));
            return false;
        }
        
        // Check aiming state
        if (!bAllowToggleWhileAiming && ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Aiming"))))
        {
            LogToggleDebug(TEXT("Cannot toggle while aiming"));
            return false;
        }
    }
    
    return true;
}

void UWeaponToggleAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    LogToggleDebug(TEXT("ActivateAbility started"));
    
    CurrentSpecHandle = Handle;
    
    if (!HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    
    // Find equipment interface
    CachedEquipmentInterface = FindEquipmentInterface();
    if (!CachedEquipmentInterface.GetInterface())
    {
        LogToggleDebug(TEXT("Failed to find equipment interface"), true);
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    
    // Get animation interface
    CachedAnimationInterface = GetAnimationInterface();
    
    // Determine slot to toggle
    CurrentToggleSlot = DetermineToggleSlot(TriggerEventData);
    if (CurrentToggleSlot == INDEX_NONE)
    {
        // Get active weapon slot through interface
        CurrentToggleSlot = IMedComEquipmentInterface::Execute_GetActiveWeaponSlotIndex(CachedEquipmentInterface.GetObject());
        
        if (CurrentToggleSlot == INDEX_NONE)
        {
            LogToggleDebug(TEXT("No active weapon slot to toggle"));
            EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
            return;
        }
    }
    
    // Check if weapon is drawn
    bIsDrawing = !IsWeaponDrawn(CachedEquipmentInterface, CurrentToggleSlot);
    
    // Get weapon type
    CurrentWeaponType = GetWeaponTypeForSlot(CachedEquipmentInterface, CurrentToggleSlot);
    
    LogToggleDebug(FString::Printf(TEXT("Toggling slot %d: %s"), 
        CurrentToggleSlot, bIsDrawing ? TEXT("Drawing") : TEXT("Holstering")));
    
    // Apply tags
    ApplyToggleTags(true, bIsDrawing);
    
    // Send start event
    SendToggleEvent(true, CurrentToggleSlot, bIsDrawing);
    OnToggleStarted.Broadcast(CurrentToggleSlot, bIsDrawing);
    
    // Store prediction key
    CurrentPredictionKey = GetCurrentActivationInfo().GetActivationPredictionKey().Current;
    
    // Perform toggle
    if (bIsDrawing)
    {
        PerformDraw(CurrentToggleSlot);
    }
    else
    {
        PerformHolster(CurrentToggleSlot);
    }
}

void UWeaponToggleAbility::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    LogToggleDebug(FString::Printf(TEXT("EndAbility called. Cancelled: %s"), 
        bWasCancelled ? TEXT("Yes") : TEXT("No")));
    
    // Clear timer
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AnimationTimeoutHandle);
    }
    
    // Remove tags
    ApplyToggleTags(false, bIsDrawing);
    
    // Clear montage delegates
    if (CurrentMontage)
    {
        if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid())
        {
            if (ACharacter* Character = Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get()))
            {
                if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
                {
                    AnimInstance->OnMontageBlendingOut.RemoveDynamic(this, &UWeaponToggleAbility::OnMontageBlendingOut);
                    AnimInstance->OnMontageEnded.RemoveDynamic(this, &UWeaponToggleAbility::OnMontageEnded);
                }
            }
        }
    }
    
    // Clear state
    CurrentToggleSlot = INDEX_NONE;
    bIsDrawing = false;
    CurrentWeaponType = FGameplayTag();
    CurrentMontage = nullptr;
    CurrentPredictionKey = 0;
    CachedEquipmentInterface = nullptr;
    CachedAnimationInterface = nullptr;
    
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

int32 UWeaponToggleAbility::DetermineToggleSlot(const FGameplayEventData* TriggerEventData) const
{
    // Check event data for slot info
    if (TriggerEventData && TriggerEventData->EventTag.IsValid())
    {
        const FGameplayTag& EventTag = TriggerEventData->EventTag;
        
        if (EventTag == InputSlot1Tag) return 0;
        if (EventTag == InputSlot2Tag) return 1;
        if (EventTag == InputSlot3Tag) return 2;
        if (EventTag == InputSlot4Tag) return 3;
        if (EventTag == InputSlot5Tag) return 4;
    }
    
    // Check current ability spec input ID
    if (CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
    {
        FGameplayAbilitySpec* Spec = CurrentActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(CurrentSpecHandle);
        if (Spec)
        {
            switch (Spec->InputID)
            {
                case static_cast<int32>(EMCAbilityInputID::WeaponSlot1): return 0;
                case static_cast<int32>(EMCAbilityInputID::WeaponSlot2): return 1;
                case static_cast<int32>(EMCAbilityInputID::WeaponSlot3): return 2;
                case static_cast<int32>(EMCAbilityInputID::WeaponSlot4): return 3;
                case static_cast<int32>(EMCAbilityInputID::WeaponSlot5): return 4;
            }
        }
    }
    
    return INDEX_NONE;
}

bool UWeaponToggleAbility::IsWeaponDrawn(const TScriptInterface<IMedComEquipmentInterface>& EquipmentInterface, int32 SlotIndex) const
{
    if (!EquipmentInterface.GetInterface())
    {
        return false;
    }
    
    // Get current equipment state through interface
    FGameplayTag CurrentState = IMedComEquipmentInterface::Execute_GetCurrentEquipmentState(EquipmentInterface.GetObject());
    
    // Check if in ready state (weapon drawn)
    return CurrentState == EquipmentReadyTag;
}

FGameplayTag UWeaponToggleAbility::GetCurrentEquipmentState(const TScriptInterface<IMedComEquipmentInterface>& EquipmentInterface) const
{
    if (!EquipmentInterface.GetInterface())
    {
        return FGameplayTag();
    }
    
    return IMedComEquipmentInterface::Execute_GetCurrentEquipmentState(EquipmentInterface.GetObject());
}

void UWeaponToggleAbility::SetEquipmentState(const TScriptInterface<IMedComEquipmentInterface>& EquipmentInterface, const FGameplayTag& NewState)
{
    if (!EquipmentInterface.GetInterface())
    {
        return;
    }
    
    IMedComEquipmentInterface::Execute_SetEquipmentState(EquipmentInterface.GetObject(), NewState, false);
}

void UWeaponToggleAbility::PerformDraw(int32 SlotIndex)
{
    // Update equipment state through interface
    SetEquipmentState(CachedEquipmentInterface, EquipmentDrawingTag);
    
    // Play animation
    if (bPlayToggleAnimations)
    {
        // Check if this is first draw
        bool bFirstDraw = false; // TODO: Track first draw per weapon through equipment interface
        PlayDrawAnimation(CurrentWeaponType, bFirstDraw);
    }
    else
    {
        // No animation, complete immediately
        OnDrawAnimationComplete();
    }
    
    // Send server RPC
    if (CurrentActivationInfo.ActivationMode != EGameplayAbilityActivationMode::Authority)
    {
        ServerRequestToggle(SlotIndex, true, CurrentPredictionKey);
    }
}

void UWeaponToggleAbility::PerformHolster(int32 SlotIndex)
{
    // Update equipment state through interface
    SetEquipmentState(CachedEquipmentInterface, EquipmentHolsteringTag);
    
    // Play animation
    if (bPlayToggleAnimations)
    {
        PlayHolsterAnimation(CurrentWeaponType);
    }
    else
    {
        // No animation, complete immediately
        OnHolsterAnimationComplete();
    }
    
    // Send server RPC
    if (CurrentActivationInfo.ActivationMode != EGameplayAbilityActivationMode::Authority)
    {
        ServerRequestToggle(SlotIndex, false, CurrentPredictionKey);
    }
}

void UWeaponToggleAbility::PlayDrawAnimation(const FGameplayTag& WeaponType, bool bFirstDraw)
{
    if (!CachedAnimationInterface.GetInterface())
    {
        OnDrawAnimationComplete();
        return;
    }
    
    // Get draw montage through interface
    UAnimMontage* DrawMontage = IMedComWeaponAnimationInterface::Execute_GetDrawMontage(
        CachedAnimationInterface.GetObject(), WeaponType, bFirstDraw);
    
    if (!DrawMontage)
    {
        LogToggleDebug(TEXT("No draw montage found"), true);
        OnDrawAnimationComplete();
        return;
    }
    
    // Play montage
    if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid())
    {
        if (ACharacter* Character = Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get()))
        {
            UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
            if (AnimInstance)
            {
                // Bind delegates
                AnimInstance->OnMontageBlendingOut.AddDynamic(this, &UWeaponToggleAbility::OnMontageBlendingOut);
                AnimInstance->OnMontageEnded.AddDynamic(this, &UWeaponToggleAbility::OnMontageEnded);
                
                // Play montage
                float Duration = AnimInstance->Montage_Play(DrawMontage, AnimationPlayRate);
                CurrentMontage = DrawMontage;
                
                // Set timeout
                if (Duration > 0.0f)
                {
                    GetWorld()->GetTimerManager().SetTimer(
                        AnimationTimeoutHandle,
                        this,
                        &UWeaponToggleAbility::OnDrawAnimationComplete,
                        Duration / AnimationPlayRate,
                        false
                    );
                }
                
                LogToggleDebug(FString::Printf(TEXT("Playing draw animation for %.2f seconds"), Duration));
            }
        }
    }
}

void UWeaponToggleAbility::PlayHolsterAnimation(const FGameplayTag& WeaponType)
{
    if (!CachedAnimationInterface.GetInterface())
    {
        OnHolsterAnimationComplete();
        return;
    }
    
    // Get holster montage through interface
    UAnimMontage* HolsterMontage = IMedComWeaponAnimationInterface::Execute_GetHolsterMontage(
        CachedAnimationInterface.GetObject(), WeaponType);
    
    if (!HolsterMontage)
    {
        LogToggleDebug(TEXT("No holster montage found"), true);
        OnHolsterAnimationComplete();
        return;
    }
    
    // Play montage
    if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid())
    {
        if (ACharacter* Character = Cast<ACharacter>(CurrentActorInfo->AvatarActor.Get()))
        {
            UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
            if (AnimInstance)
            {
                // Bind delegates
                AnimInstance->OnMontageBlendingOut.AddDynamic(this, &UWeaponToggleAbility::OnMontageBlendingOut);
                AnimInstance->OnMontageEnded.AddDynamic(this, &UWeaponToggleAbility::OnMontageEnded);
                
                // Play montage
                float Duration = AnimInstance->Montage_Play(HolsterMontage, AnimationPlayRate);
                CurrentMontage = HolsterMontage;
                
                // Set timeout
                if (Duration > 0.0f)
                {
                    GetWorld()->GetTimerManager().SetTimer(
                        AnimationTimeoutHandle,
                        this,
                        &UWeaponToggleAbility::OnHolsterAnimationComplete,
                        Duration / AnimationPlayRate,
                        false
                    );
                }
                
                LogToggleDebug(FString::Printf(TEXT("Playing holster animation for %.2f seconds"), Duration));
            }
        }
    }
}

void UWeaponToggleAbility::OnDrawAnimationComplete()
{
    LogToggleDebug(TEXT("Draw animation complete"));
    
    // Clear timer
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AnimationTimeoutHandle);
    }
    
    // Update equipment state through interface
    SetEquipmentState(CachedEquipmentInterface, EquipmentReadyTag);
    
    // Send completion event
    SendToggleEvent(false, CurrentToggleSlot, true);
    OnToggleCompleted.Broadcast(CurrentToggleSlot, true);
    
    // End ability
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UWeaponToggleAbility::OnHolsterAnimationComplete()
{
    LogToggleDebug(TEXT("Holster animation complete"));
    
    // Clear timer
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AnimationTimeoutHandle);
    }
    
    // Update equipment state through interface
    SetEquipmentState(CachedEquipmentInterface, EquipmentHolsteredTag);
    
    // Send completion event
    SendToggleEvent(false, CurrentToggleSlot, false);
    OnToggleCompleted.Broadcast(CurrentToggleSlot, false);
    
    // End ability
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UWeaponToggleAbility::OnMontageBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage == CurrentMontage)
    {
        LogToggleDebug(FString::Printf(TEXT("Montage blending out. Interrupted: %s"), 
            bInterrupted ? TEXT("Yes") : TEXT("No")));
    }
}

void UWeaponToggleAbility::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage == CurrentMontage)
    {
        LogToggleDebug(FString::Printf(TEXT("Montage ended. Interrupted: %s"), 
            bInterrupted ? TEXT("Yes") : TEXT("No")));
        
        if (!bInterrupted)
        {
            // Animation completed normally
            if (bIsDrawing)
            {
                OnDrawAnimationComplete();
            }
            else
            {
                OnHolsterAnimationComplete();
            }
        }
    }
}

TScriptInterface<IMedComEquipmentInterface> UWeaponToggleAbility::FindEquipmentInterface() const
{
    TScriptInterface<IMedComEquipmentInterface> Result;
    
    if (!CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid())
    {
        return Result;
    }
    
    // First check the pawn itself for equipment interface
    if (CurrentActorInfo->AvatarActor->GetClass()->ImplementsInterface(UMedComEquipmentInterface::StaticClass()))
    {
        Result.SetObject(CurrentActorInfo->AvatarActor.Get());
        Result.SetInterface(Cast<IMedComEquipmentInterface>(CurrentActorInfo->AvatarActor.Get()));
        return Result;
    }
    
    // Check PlayerState for equipment component that implements interface
    if (APawn* Pawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get()))
    {
        if (APlayerState* PS = Pawn->GetPlayerState())
        {
            // Find component that implements equipment interface
            TArray<UActorComponent*> Components = PS->GetComponents().Array();
            for (UActorComponent* Component : Components)
            {
                if (Component && Component->GetClass()->ImplementsInterface(UMedComEquipmentInterface::StaticClass()))
                {
                    Result.SetObject(Component);
                    Result.SetInterface(Cast<IMedComEquipmentInterface>(Component));
                    return Result;
                }
            }
        }
    }
    
    return Result;
}

TScriptInterface<IMedComWeaponAnimationInterface> UWeaponToggleAbility::GetAnimationInterface() const
{
    TScriptInterface<IMedComWeaponAnimationInterface> Result;
    
    // Get from animation subsystem
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GameInstance = World->GetGameInstance())
        {
            if (UWeaponAnimationSubsystem* AnimSubsystem = GameInstance->GetSubsystem<UWeaponAnimationSubsystem>())
            {
                Result.SetObject(AnimSubsystem);
                Result.SetInterface(AnimSubsystem);
                return Result;
            }
        }
    }
    
    return Result;
}

FGameplayTag UWeaponToggleAbility::GetWeaponTypeForSlot(const TScriptInterface<IMedComEquipmentInterface>& EquipmentInterface, int32 SlotIndex) const
{
    if (!EquipmentInterface.GetInterface())
    {
        return FGameplayTag();
    }
    
    // Get weapon archetype through equipment interface
    return IMedComEquipmentInterface::Execute_GetWeaponArchetype(EquipmentInterface.GetObject());
}

void UWeaponToggleAbility::ApplyToggleTags(bool bApply, bool bIsDrawingWeapon)
{
    if (!CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid())
    {
        return;
    }
    
    UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
    
    if (bApply)
    {
        ASC->AddLooseGameplayTag(WeaponTogglingTag);
        
        if (bIsDrawingWeapon)
        {
            ASC->AddLooseGameplayTag(EquipmentDrawingTag);
        }
        else
        {
            ASC->AddLooseGameplayTag(EquipmentHolsteringTag);
        }
    }
    else
    {
        ASC->RemoveLooseGameplayTag(WeaponTogglingTag);
        ASC->RemoveLooseGameplayTag(EquipmentDrawingTag);
        ASC->RemoveLooseGameplayTag(EquipmentHolsteringTag);
    }
}

void UWeaponToggleAbility::SendToggleEvent(bool bStarted, int32 SlotIndex, bool bIsDrawingWeapon) const
{
    // Get delegate manager through equipment interface
    if (CachedEquipmentInterface.GetInterface())
    {
        if (UEventDelegateManager* DelegateManager = IMedComEquipmentInterface::Execute_GetDelegateManager(CachedEquipmentInterface.GetObject()))
        {
            FGameplayTag EventTag;
            if (bStarted)
            {
                EventTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Toggle.Started"));
            }
            else
            {
                EventTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Toggle.Completed"));
            }
            
            FString EventData = FString::Printf(TEXT("Slot:%d,Drawing:%s"), 
                SlotIndex, bIsDrawingWeapon ? TEXT("true") : TEXT("false"));
            
            DelegateManager->BroadcastGenericEvent(
                this,
                EventTag,
                EventData
            );
        }
    }
}

void UWeaponToggleAbility::LogToggleDebug(const FString& Message, bool bError) const
{
#if !UE_BUILD_SHIPPING
    if (bShowDebugInfo)
    {
        const FString Prefix = TEXT("[WeaponToggleAbility] ");
        if (bError)
        {
            UE_LOG(LogTemp, Warning, TEXT("%s%s"), *Prefix, *Message);
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("%s%s"), *Prefix, *Message);
        }
    }
#endif
}

void UWeaponToggleAbility::ServerRequestToggle_Implementation(int32 SlotIndex, bool bDraw, int32 PredictionKey)
{
    // Validate and perform toggle on server
    // For now, just confirm
    ClientConfirmToggle(SlotIndex, true, PredictionKey);
}

void UWeaponToggleAbility::ClientConfirmToggle_Implementation(int32 SlotIndex, bool bSuccess, int32 PredictionKey)
{
    LogToggleDebug(FString::Printf(TEXT("Client received toggle confirmation: %s, Slot: %d"), 
        bSuccess ? TEXT("Success") : TEXT("Failed"), SlotIndex));
}