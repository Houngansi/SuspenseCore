// SuspenseCoreQuickSlotAbility.cpp
// QuickSlot ability for fast magazine/item access
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Weapon/SuspenseCoreQuickSlotAbility.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreQuickSlotProvider.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreQuickSlotAbility, Log, All);

//==================================================================
// Constructor
//==================================================================

USuspenseCoreQuickSlotAbility::USuspenseCoreQuickSlotAbility()
{
    SlotIndex = 0;

    // Instant activation
    InstancingPolicy = EGameplayAbilityInstancingPolicy::NonInstanced;

    // Blocking tags - use native tags per project architecture
    // @see SuspenseCoreGameplayTags.h
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Disabled);

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

    ISuspenseCoreQuickSlotProvider* Provider =
        const_cast<USuspenseCoreQuickSlotAbility*>(this)->GetQuickSlotProvider();

    if (!Provider)
    {
        return false;
    }

    // Use interface Execute_ pattern for BlueprintNativeEvent
    return ISuspenseCoreQuickSlotProvider::Execute_IsSlotReady(
        Cast<UObject>(Provider), SlotIndex);
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

    ISuspenseCoreQuickSlotProvider* Provider = GetQuickSlotProvider();
    if (Provider)
    {
        UObject* ProviderObject = Cast<UObject>(Provider);
        bool bSuccess = ISuspenseCoreQuickSlotProvider::Execute_UseQuickSlot(
            ProviderObject, SlotIndex);

        UE_LOG(LogSuspenseCoreQuickSlotAbility, Log,
            TEXT("QuickSlot %d activated: %s"),
            SlotIndex + 1,
            bSuccess ? TEXT("Success") : TEXT("Failed"));
    }

    // Instant ability - end immediately
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

//==================================================================
// Internal Methods (Interface-based)
//==================================================================

ISuspenseCoreQuickSlotProvider* USuspenseCoreQuickSlotAbility::GetQuickSlotProvider() const
{
    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
        return nullptr;
    }

    // Check actor itself
    if (AvatarActor->GetClass()->ImplementsInterface(USuspenseCoreQuickSlotProvider::StaticClass()))
    {
        return Cast<ISuspenseCoreQuickSlotProvider>(AvatarActor);
    }

    // Check components for interface implementation
    TArray<UActorComponent*> Components;
    AvatarActor->GetComponents(Components);

    for (UActorComponent* Comp : Components)
    {
        if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreQuickSlotProvider::StaticClass()))
        {
            return Cast<ISuspenseCoreQuickSlotProvider>(Comp);
        }
    }

    return nullptr;
}
