// SuspenseCoreQuickSlotAbility.cpp
// QuickSlot ability for fast magazine/item access
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Weapon/SuspenseCoreQuickSlotAbility.h"
#include "SuspenseCore/Components/SuspenseCoreQuickSlotComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreQuickSlotAbility, Log, All);

//==================================================================
// Constructor
//==================================================================

USuspenseCoreQuickSlotAbility::USuspenseCoreQuickSlotAbility()
{
    SlotIndex = 0;

    // Instant activation
    InstancingPolicy = EGameplayAbilityInstancingPolicy::NonInstanced;

    // Blocking tags
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Disabled")));

    // EventBus
    bPublishAbilityEvents = true;
}

//==================================================================
// GameplayAbility Interface
//==================================================================

bool USuspenseCoreQuickSlotAbility::CanActivateAbility(
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

    USuspenseCoreQuickSlotComponent* QuickSlotComp =
        const_cast<USuspenseCoreQuickSlotAbility*>(this)->GetQuickSlotComponent();

    if (!QuickSlotComp)
    {
        return false;
    }

    return QuickSlotComp->IsSlotReady(SlotIndex);
}

void USuspenseCoreQuickSlotAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    USuspenseCoreQuickSlotComponent* QuickSlotComp = GetQuickSlotComponent();
    if (QuickSlotComp)
    {
        bool bSuccess = QuickSlotComp->UseQuickSlot(SlotIndex);

        UE_LOG(LogSuspenseCoreQuickSlotAbility, Log,
            TEXT("QuickSlot %d activated: %s"),
            SlotIndex + 1,
            bSuccess ? TEXT("Success") : TEXT("Failed"));
    }

    // Instant ability - end immediately
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

//==================================================================
// Internal Methods
//==================================================================

USuspenseCoreQuickSlotComponent* USuspenseCoreQuickSlotAbility::GetQuickSlotComponent() const
{
    AActor* OwnerActor = GetOwningActorFromActorInfo();
    if (!OwnerActor)
    {
        return nullptr;
    }

    return OwnerActor->FindComponentByClass<USuspenseCoreQuickSlotComponent>();
}
