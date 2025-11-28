// Copyright Suspense Team. All Rights Reserved.

#include "Abilities/WeaponSwitchAbility.h"
#include "Interfaces/Equipment/ISuspenseEquipment.h"
#include "Interfaces/Weapon/ISuspenseWeapon.h"
#include "Delegates/SuspenseEventManager.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "TimerManager.h"

UWeaponSwitchAbility::UWeaponSwitchAbility()
{
    // Ability configuration
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

    // Initialize tags
    WeaponSwitchingTag = FGameplayTag::RequestGameplayTag(TEXT("State.WeaponSwitching"));
    WeaponSwitchBlockTag = FGameplayTag::RequestGameplayTag(TEXT("Block.WeaponSwitch"));

    EquipmentDrawingTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Drawing"));
    EquipmentHolsteringTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Holstering"));
    EquipmentSwitchingTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Switching"));

    // Input tags
    InputNextWeaponTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input.NextWeapon"));
    InputPrevWeaponTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input.PrevWeapon"));
    InputQuickSwitchTag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input.QuickSwitch"));
    InputSlot1Tag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input.WeaponSlot1"));
    InputSlot2Tag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input.WeaponSlot2"));
    InputSlot3Tag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input.WeaponSlot3"));
    InputSlot4Tag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input.WeaponSlot4"));
    InputSlot5Tag = FGameplayTag::RequestGameplayTag(TEXT("Ability.Input.WeaponSlot5"));

    // Set ability tags using new API
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Weapon.Switch")));
    SetAssetTags(AssetTags);

    // Set blocking tags
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Weapon.Fire")));
    BlockAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Weapon.Reload")));

    // Set activation blocked tags
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Dead")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Stunned")));
    ActivationBlockedTags.AddTag(WeaponSwitchBlockTag);

    // Default values
    CurrentSwitchMode = EWeaponSwitchMode::Invalid;
    TargetSlotIndex = INDEX_NONE;
    SourceSlotIndex = INDEX_NONE;
    LastActiveWeaponSlot = INDEX_NONE;
}

bool UWeaponSwitchAbility::CanActivateAbility(
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

    // Check if we can switch
    if (!CanSwitchWeapons())
    {
        return false;
    }

    // Check for blocking states
    if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
    {
        // Don't allow switch if already switching
        if (ASC->HasMatchingGameplayTag(WeaponSwitchingTag))
        {
            LogSwitchDebug(TEXT("Already switching weapons"));
            return false;
        }

        // Check reload state
        if (!bAllowSwitchDuringReload && ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Reloading"))))
        {
            LogSwitchDebug(TEXT("Cannot switch during reload"));
            return false;
        }

        // Check firing state
        if (!bAllowSwitchWhileFiring && ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("Weapon.State.Firing"))))
        {
            LogSwitchDebug(TEXT("Cannot switch while firing"));
            return false;
        }
    }

    return true;
}

void UWeaponSwitchAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    LogSwitchDebug(TEXT("ActivateAbility started"));

    // Store current spec handle
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
        LogSwitchDebug(TEXT("Failed to find equipment interface"), true);
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Determine switch mode
    CurrentSwitchMode = DetermineSwitchMode(TriggerEventData);
    if (CurrentSwitchMode == EWeaponSwitchMode::Invalid)
    {
        LogSwitchDebug(TEXT("Invalid switch mode"), true);
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Get input slot if switching to specific slot
    int32 InputSlot = -1;
    if (CurrentSwitchMode == EWeaponSwitchMode::ToSlotIndex && TriggerEventData)
    {
        // Extract slot index from event tag
        if (TriggerEventData->EventTag == InputSlot1Tag) InputSlot = 0;
        else if (TriggerEventData->EventTag == InputSlot2Tag) InputSlot = 1;
        else if (TriggerEventData->EventTag == InputSlot3Tag) InputSlot = 2;
        else if (TriggerEventData->EventTag == InputSlot4Tag) InputSlot = 3;
        else if (TriggerEventData->EventTag == InputSlot5Tag) InputSlot = 4;
    }

    // Get target slot
    TargetSlotIndex = GetTargetSlot(CurrentSwitchMode, InputSlot);
    if (TargetSlotIndex == INDEX_NONE)
    {
        LogSwitchDebug(TEXT("No valid target slot found"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // Store source slot
    SourceSlotIndex = GetActiveWeaponSlot();

    // Check if we're already at target slot
    if (SourceSlotIndex == TargetSlotIndex)
    {
        LogSwitchDebug(TEXT("Already at target slot"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    // Store last active slot for quick switch
    if (SourceSlotIndex != INDEX_NONE)
    {
        LastActiveWeaponSlot = SourceSlotIndex;
    }

    LogSwitchDebug(FString::Printf(TEXT("Switching from slot %d to slot %d"), SourceSlotIndex, TargetSlotIndex));

    // Apply switching tags
    ApplySwitchTags(true);

    // Send start event
    SendWeaponSwitchEvent(true, SourceSlotIndex, TargetSlotIndex);

    // Store prediction key
    CurrentPredictionKey = GetCurrentActivationInfo().GetActivationPredictionKey().Current;

    // Start switch sequence
    if (bPlaySwitchAnimations && SourceSlotIndex != INDEX_NONE)
    {
        // Play holster animation first
        PlayHolsterAnimation();
    }
    else
    {
        // No animations, switch immediately
        PerformWeaponSwitch(TargetSlotIndex);
    }
}

void UWeaponSwitchAbility::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    LogSwitchDebug(FString::Printf(TEXT("EndAbility called. Cancelled: %s"), bWasCancelled ? TEXT("Yes") : TEXT("No")));

    // Clear timers
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(HolsterTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(DrawTimerHandle);
    }

    // Remove tags
    ApplySwitchTags(false);

    // Clear state
    CurrentSwitchMode = EWeaponSwitchMode::Invalid;
    TargetSlotIndex = INDEX_NONE;
    SourceSlotIndex = INDEX_NONE;
    CurrentPredictionKey = 0;
    CurrentHolsterMontage = nullptr;
    CurrentDrawMontage = nullptr;
    CachedEquipmentInterface = nullptr;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

EWeaponSwitchMode UWeaponSwitchAbility::DetermineSwitchMode(const FGameplayEventData* TriggerEventData) const
{
    // Если есть event data с тегом, используем его
    if (TriggerEventData && TriggerEventData->EventTag.IsValid())
    {
        const FGameplayTag& EventTag = TriggerEventData->EventTag;

        // Check for specific slot switches
        if (EventTag == InputSlot1Tag || EventTag == InputSlot2Tag ||
            EventTag == InputSlot3Tag || EventTag == InputSlot4Tag || EventTag == InputSlot5Tag)
        {
            return EWeaponSwitchMode::ToSlotIndex;
        }

        // Check for cycling
        if (EventTag == InputNextWeaponTag)
        {
            return EWeaponSwitchMode::NextWeapon;
        }

        if (EventTag == InputPrevWeaponTag)
        {
            return EWeaponSwitchMode::PreviousWeapon;
        }

        // Check for quick switch
        if (EventTag == InputQuickSwitchTag)
        {
            return EWeaponSwitchMode::QuickSwitch;
        }
    }

    // Если нет event data, определяем по InputID из текущего Spec
    if (CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
    {
        // Получаем текущий spec через handle
        FGameplayAbilitySpec* Spec = CurrentActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(CurrentSpecHandle);
        if (Spec)
        {
            // Теперь можем получить InputID из spec
            switch (Spec->InputID)
            {
                case static_cast<int32>(ESuspenseAbilityInputID::NextWeapon):
                    return EWeaponSwitchMode::NextWeapon;

                case static_cast<int32>(ESuspenseAbilityInputID::PrevWeapon):
                    return EWeaponSwitchMode::PreviousWeapon;

                case static_cast<int32>(ESuspenseAbilityInputID::QuickSwitch):
                    return EWeaponSwitchMode::QuickSwitch;

                case static_cast<int32>(ESuspenseAbilityInputID::WeaponSlot1):
                case static_cast<int32>(ESuspenseAbilityInputID::WeaponSlot2):
                case static_cast<int32>(ESuspenseAbilityInputID::WeaponSlot3):
                case static_cast<int32>(ESuspenseAbilityInputID::WeaponSlot4):
                case static_cast<int32>(ESuspenseAbilityInputID::WeaponSlot5):
                    return EWeaponSwitchMode::ToSlotIndex;

                default:
                    break;
            }
        }
    }

    return EWeaponSwitchMode::Invalid;
}

int32 UWeaponSwitchAbility::GetTargetSlot(EWeaponSwitchMode Mode, int32 InputSlot) const
{
    switch (Mode)
    {
    case EWeaponSwitchMode::ToSlotIndex:
        {
            // Проверяем что слот валидный и содержит оружие
            if (InputSlot >= 0 && IsWeaponSlot(InputSlot))
            {
                return InputSlot;
            }

            // Если InputSlot не задан, определяем по InputID
            if (InputSlot < 0 && CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
            {
                FGameplayAbilitySpec* Spec = CurrentActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(CurrentSpecHandle);
                if (Spec)
                {
                    // Map InputID to slot index
                    switch (Spec->InputID)
                    {
                        case static_cast<int32>(ESuspenseAbilityInputID::WeaponSlot1):
                            return IsWeaponSlot(0) ? 0 : INDEX_NONE;
                        case static_cast<int32>(ESuspenseAbilityInputID::WeaponSlot2):
                            return IsWeaponSlot(1) ? 1 : INDEX_NONE;
                        case static_cast<int32>(ESuspenseAbilityInputID::WeaponSlot3):
                            return IsWeaponSlot(2) ? 2 : INDEX_NONE;
                        case static_cast<int32>(ESuspenseAbilityInputID::WeaponSlot4):
                            return IsWeaponSlot(3) ? 3 : INDEX_NONE;
                        case static_cast<int32>(ESuspenseAbilityInputID::WeaponSlot5):
                            return IsWeaponSlot(4) ? 4 : INDEX_NONE;
                    }
                }
            }
            return INDEX_NONE;
        }

    case EWeaponSwitchMode::NextWeapon:
        {
            int32 CurrentSlot = GetActiveWeaponSlot();
            return GetNextWeaponSlot(CurrentSlot);
        }

    case EWeaponSwitchMode::PreviousWeapon:
        {
            int32 CurrentSlot = GetActiveWeaponSlot();
            return GetPreviousWeaponSlot(CurrentSlot);
        }

    case EWeaponSwitchMode::QuickSwitch:
        {
            // Используем новый метод интерфейса для получения предыдущего слота
            if (CachedEquipmentInterface.GetInterface())
            {
                int32 PrevSlot = ISuspenseEquipment::Execute_GetPreviousWeaponSlot(
                    CachedEquipmentInterface.GetObject()
                );

                if (PrevSlot != INDEX_NONE && IsWeaponSlot(PrevSlot))
                {
                    return PrevSlot;
                }
            }

            // Если нет предыдущего слота, переключаемся на первое доступное оружие
            TArray<int32> WeaponSlots = GetWeaponSlotIndices();
            if (WeaponSlots.Num() > 0)
            {
                int32 CurrentSlot = GetActiveWeaponSlot();
                for (int32 Slot : WeaponSlots)
                {
                    if (Slot != CurrentSlot)
                    {
                        return Slot;
                    }
                }
            }

            return INDEX_NONE;
        }

    default:
        return INDEX_NONE;
    }
}
void UWeaponSwitchAbility::PerformWeaponSwitch(int32 TargetSlot)
{
    if (!CachedEquipmentInterface.GetInterface())
    {
        LogSwitchDebug(TEXT("PerformWeaponSwitch: No equipment interface"), true);
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // Теперь используем новый метод интерфейса для переключения
    bool bSwitchSuccess = ISuspenseEquipment::Execute_SwitchToSlot(
        CachedEquipmentInterface.GetObject(),
        TargetSlot
    );

    if (!bSwitchSuccess)
    {
        LogSwitchDebug(TEXT("PerformWeaponSwitch: Failed to switch to slot"), true);
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    // Send RPC based on authority
    if (CurrentActivationInfo.ActivationMode == EGameplayAbilityActivationMode::Authority)
    {
        // Server: Update equipment state directly
        ISuspenseEquipment::Execute_SetEquipmentState(
            CachedEquipmentInterface.GetObject(),
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Ready")),
            true
        );

        // Play draw animation
        if (bPlaySwitchAnimations)
        {
            PlayDrawAnimation();
        }
        else
        {
            // Complete immediately
            SendWeaponSwitchEvent(false, SourceSlotIndex, TargetSlot);
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        }
    }
    else
    {
        // Client: send RPC
        ServerRequestWeaponSwitch(TargetSlot, CurrentPredictionKey);

        // Play draw animation optimistically
        if (bPlaySwitchAnimations)
        {
            PlayDrawAnimation();
        }
    }
}

void UWeaponSwitchAbility::PlayHolsterAnimation()
{
    if (!CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid())
    {
        OnHolsterAnimationComplete();
        return;
    }

    // For now, skip animation and proceed
    // In production, we'd get animation data through weapon interface
    OnHolsterAnimationComplete();
}

void UWeaponSwitchAbility::PlayDrawAnimation()
{
    if (!CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid())
    {
        OnDrawAnimationComplete();
        return;
    }

    // For now, skip animation and proceed
    // In production, we'd get animation data through weapon interface
    OnDrawAnimationComplete();
}

void UWeaponSwitchAbility::OnHolsterAnimationComplete()
{
    LogSwitchDebug(TEXT("Holster animation complete"));

    // Clear timer
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(HolsterTimerHandle);
    }

    // Now perform the actual switch
    PerformWeaponSwitch(TargetSlotIndex);
}

void UWeaponSwitchAbility::OnDrawAnimationComplete()
{
    LogSwitchDebug(TEXT("Draw animation complete"));

    // Clear timer
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(DrawTimerHandle);
    }

    // Update equipment state
    if (CachedEquipmentInterface.GetInterface())
    {
        ISuspenseEquipment::Execute_SetEquipmentState(
            CachedEquipmentInterface.GetObject(),
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Ready")),
            false
        );
    }

    // Send completion event
    SendWeaponSwitchEvent(false, SourceSlotIndex, TargetSlotIndex);

    // End ability
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UWeaponSwitchAbility::ServerRequestWeaponSwitch_Implementation(int32 TargetSlot, int32 PredictionKey)
{
    if (!CurrentActorInfo)
    {
        return;
    }

    // Validate and perform switch on server
    // For now, just confirm
    ClientConfirmWeaponSwitch(TargetSlot, true, PredictionKey);
}

void UWeaponSwitchAbility::ClientConfirmWeaponSwitch_Implementation(int32 NewActiveSlot, bool bSuccess, int32 PredictionKey)
{
    LogSwitchDebug(FString::Printf(TEXT("Client received switch confirmation: %s, Slot: %d"),
        bSuccess ? TEXT("Success") : TEXT("Failed"), NewActiveSlot));
}

TScriptInterface<ISuspenseEquipment> UWeaponSwitchAbility::FindEquipmentInterface() const
{
    TScriptInterface<ISuspenseEquipment> Result;

    if (!CurrentActorInfo || !CurrentActorInfo->AvatarActor.IsValid())
    {
        return Result;
    }

    // Check PlayerState for equipment component
    if (APawn* Pawn = Cast<APawn>(CurrentActorInfo->AvatarActor.Get()))
    {
        if (APlayerState* PS = Pawn->GetPlayerState())
        {
            // Find all components that implement the equipment interface
            TArray<UActorComponent*> Components = PS->GetComponents().Array();
            for (UActorComponent* Component : Components)
            {
                if (Component && Component->GetClass()->ImplementsInterface(USuspenseEquipment::StaticClass()))
                {
                    Result.SetObject(Component);
                    Result.SetInterface(Cast<ISuspenseEquipment>(Component));
                    return Result;
                }
            }
        }
    }

    return Result;
}

TArray<int32> UWeaponSwitchAbility::GetWeaponSlotIndices() const
{
    if (CachedEquipmentInterface.GetInterface())
    {
        return ISuspenseEquipment::Execute_GetWeaponSlotsByPriority(
            CachedEquipmentInterface.GetObject()
        );
    }

    return TArray<int32>();
}

int32 UWeaponSwitchAbility::GetActiveWeaponSlot() const
{
    if (CachedEquipmentInterface.GetInterface())
    {
        return ISuspenseEquipment::Execute_GetActiveWeaponSlotIndex(
            CachedEquipmentInterface.GetObject()
        );
    }

    return INDEX_NONE;
}


int32 UWeaponSwitchAbility::GetNextWeaponSlot(int32 CurrentSlot) const
{
    TArray<int32> WeaponSlots = GetWeaponSlotIndices();
    if (WeaponSlots.Num() == 0)
    {
        return INDEX_NONE;
    }

    // Find current slot in array
    int32 CurrentIndex = WeaponSlots.IndexOfByKey(CurrentSlot);
    if (CurrentIndex == INDEX_NONE)
    {
        // Start from first slot
        return WeaponSlots[0];
    }

    // Get next slot (with wrapping)
    int32 NextIndex = (CurrentIndex + 1) % WeaponSlots.Num();

    if (bSkipEmptySlots)
    {
        // Find next non-empty slot
        for (int32 i = 0; i < WeaponSlots.Num(); ++i)
        {
            int32 CheckIndex = (NextIndex + i) % WeaponSlots.Num();
            int32 SlotIndex = WeaponSlots[CheckIndex];

            // Check if slot has item (would need interface method)
            if (CachedEquipmentInterface.GetInterface())
            {
                FSuspenseInventoryItemInstance Item = ISuspenseEquipment::Execute_GetEquippedItemInstance(
                    CachedEquipmentInterface.GetObject()
                );

                if (Item.IsValid())
                {
                    return SlotIndex;
                }
            }
        }
    }

    return WeaponSlots[NextIndex];
}

int32 UWeaponSwitchAbility::GetPreviousWeaponSlot(int32 CurrentSlot) const
{
    TArray<int32> WeaponSlots = GetWeaponSlotIndices();
    if (WeaponSlots.Num() == 0)
    {
        return INDEX_NONE;
    }

    // Find current slot in array
    int32 CurrentIndex = WeaponSlots.IndexOfByKey(CurrentSlot);
    if (CurrentIndex == INDEX_NONE)
    {
        // Start from last slot
        return WeaponSlots.Last();
    }

    // Get previous slot (with wrapping)
    int32 PrevIndex = (CurrentIndex - 1 + WeaponSlots.Num()) % WeaponSlots.Num();

    if (bSkipEmptySlots)
    {
        // Find previous non-empty slot
        for (int32 i = 0; i < WeaponSlots.Num(); ++i)
        {
            int32 CheckIndex = (PrevIndex - i + WeaponSlots.Num()) % WeaponSlots.Num();
            int32 SlotIndex = WeaponSlots[CheckIndex];

            // Check if slot has item (would need interface method)
            if (CachedEquipmentInterface.GetInterface())
            {
                FSuspenseInventoryItemInstance Item = ISuspenseEquipment::Execute_GetEquippedItemInstance(
                    CachedEquipmentInterface.GetObject()
                );

                if (Item.IsValid())
                {
                    return SlotIndex;
                }
            }
        }
    }

    return WeaponSlots[PrevIndex];
}

bool UWeaponSwitchAbility::IsWeaponSlot(int32 SlotIndex) const
{
    if (CachedEquipmentInterface.GetInterface())
    {
        return ISuspenseEquipment::Execute_IsSlotWeapon(
            CachedEquipmentInterface.GetObject(),
            SlotIndex
        );
    }

    return false;
}

USuspenseEventManager* UWeaponSwitchAbility::GetDelegateManager() const
{
    if (CurrentActorInfo && CurrentActorInfo->AvatarActor.IsValid())
    {
        return USuspenseEventManager::Get(CurrentActorInfo->AvatarActor.Get());
    }
    return nullptr;
}

void UWeaponSwitchAbility::SendWeaponSwitchEvent(bool bStarted, int32 FromSlot, int32 ToSlot) const
{
    if (USuspenseEventManager* Manager = GetDelegateManager())
    {
        if (bStarted)
        {
            Manager->OnWeaponSwitchStarted.Broadcast(FromSlot, ToSlot);
        }
        else
        {
            Manager->OnWeaponSwitchCompleted.Broadcast(FromSlot, ToSlot);
        }
    }
}

bool UWeaponSwitchAbility::CanSwitchWeapons() const
{
    // Find equipment interface
    TScriptInterface<IMedComEquipmentInterface> EquipInterface = FindEquipmentInterface();
    if (!EquipInterface.GetInterface())
    {
        LogSwitchDebug(TEXT("No equipment interface found"));
        return false;
    }

    // Check if any weapons are available
    TArray<int32> WeaponSlots = GetWeaponSlotIndices();
    if (WeaponSlots.Num() == 0)
    {
        LogSwitchDebug(TEXT("No weapon slots available"));
        return false;
    }

    return true;
}

void UWeaponSwitchAbility::ApplySwitchTags(bool bApply)
{
    if (!CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid())
    {
        return;
    }

    UAbilitySystemComponent* ASC = CurrentActorInfo->AbilitySystemComponent.Get();

    if (bApply)
    {
        ASC->AddLooseGameplayTag(WeaponSwitchingTag);
        ASC->AddLooseGameplayTag(EquipmentSwitchingTag);
    }
    else
    {
        ASC->RemoveLooseGameplayTag(WeaponSwitchingTag);
        ASC->RemoveLooseGameplayTag(EquipmentSwitchingTag);
        ASC->RemoveLooseGameplayTag(EquipmentHolsteringTag);
        ASC->RemoveLooseGameplayTag(EquipmentDrawingTag);
    }
}

void UWeaponSwitchAbility::LogSwitchDebug(const FString& Message, bool bError) const
{
#if !UE_BUILD_SHIPPING
    if (bShowDebugInfo)
    {
        const FString Prefix = TEXT("[WeaponSwitchAbility] ");
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
