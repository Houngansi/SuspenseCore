// SuspenseCoreReloadAbility.cpp
// Tarkov-style reload ability with magazine management
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Weapon/SuspenseCoreReloadAbility.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreMagazineProvider.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreQuickSlotProvider.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreReload, Log, All);

#define RELOAD_LOG(Verbosity, Format, ...) \
    UE_LOG(LogSuspenseCoreReload, Verbosity, TEXT("[%s] " Format), *GetNameSafe(GetOwningActorFromActorInfo()), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

USuspenseCoreReloadAbility::USuspenseCoreReloadAbility()
{
    // Input binding
    AbilityInputID = ESuspenseCoreAbilityInputID::Reload;

    // Default timing
    BaseTacticalReloadTime = 2.0f;
    BaseEmptyReloadTime = 2.5f;
    EmergencyReloadTimeMultiplier = 0.8f;
    ChamberOnlyTime = 0.8f;

    // Initialize runtime state
    CurrentReloadType = ESuspenseCoreReloadType::None;
    ReloadDuration = 0.0f;
    ReloadStartTime = 0.0f;
    bIsReloading = false;
    NewMagazineQuickSlotIndex = -1;

    // Ability configuration
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    bRetriggerInstancedAbility = false;

    // Blocking tags - can't reload while doing these
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Sprinting")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Firing")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Dead")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Stunned")));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Disabled")));

    // Tags applied while reloading
    ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Reloading")));

    // Cancel these abilities when reloading
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Ability.Sprint")));
    CancelAbilitiesWithTag.AddTag(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Ability.Aim")));

    // EventBus configuration
    bPublishAbilityEvents = true;
}

//==================================================================
// Runtime Accessors
//==================================================================

float USuspenseCoreReloadAbility::GetReloadProgress() const
{
    if (!bIsReloading || ReloadDuration <= 0.0f)
    {
        return 0.0f;
    }

    float ElapsedTime = GetWorld()->GetTimeSeconds() - ReloadStartTime;
    return FMath::Clamp(ElapsedTime / ReloadDuration, 0.0f, 1.0f);
}

//==================================================================
// GameplayAbility Interface
//==================================================================

bool USuspenseCoreReloadAbility::CanActivateAbility(
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

    // Check if already reloading
    if (bIsReloading)
    {
        return false;
    }

    // Get magazine provider interface
    ISuspenseCoreMagazineProvider* MagProvider = const_cast<USuspenseCoreReloadAbility*>(this)->GetMagazineProvider();
    if (!MagProvider)
    {
        RELOAD_LOG(Verbose, TEXT("CanActivateAbility: No MagazineProvider found"));
        return false;
    }

    // Determine what kind of reload is possible
    ESuspenseCoreReloadType PossibleReload = DetermineReloadType();
    if (PossibleReload == ESuspenseCoreReloadType::None)
    {
        RELOAD_LOG(Verbose, TEXT("CanActivateAbility: No valid reload type available"));
        return false;
    }

    return true;
}

void USuspenseCoreReloadAbility::ActivateAbility(
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

    // Cache for later use
    CachedActorInfo = ActorInfo;
    CachedSpecHandle = Handle;
    CachedActivationInfo = ActivationInfo;

    // Determine reload type
    CurrentReloadType = DetermineReloadType();
    if (CurrentReloadType == ESuspenseCoreReloadType::None)
    {
        RELOAD_LOG(Warning, TEXT("ActivateAbility: No valid reload type"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Find magazine to use
    if (CurrentReloadType != ESuspenseCoreReloadType::ChamberOnly)
    {
        if (!FindBestMagazine(NewMagazineQuickSlotIndex, NewMagazine))
        {
            RELOAD_LOG(Warning, TEXT("ActivateAbility: No magazine available for reload"));
            EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
            return;
        }
    }

    // Calculate reload duration
    ReloadDuration = CalculateReloadDuration(CurrentReloadType);
    ReloadStartTime = GetWorld()->GetTimeSeconds();
    bIsReloading = true;

    // Apply effects
    ApplyReloadEffects(ActorInfo);

    // Play montage
    if (!PlayReloadMontage())
    {
        RELOAD_LOG(Warning, TEXT("ActivateAbility: Failed to play reload montage"));
        RemoveReloadEffects(ActorInfo);
        bIsReloading = false;
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Broadcast reload started
    BroadcastReloadStarted();

    RELOAD_LOG(Log, TEXT("Reload started: Type=%d, Duration=%.2f"),
        static_cast<int32>(CurrentReloadType), ReloadDuration);

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void USuspenseCoreReloadAbility::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    // Clean up
    RemoveReloadEffects(ActorInfo);
    StopReloadMontage();

    if (bWasCancelled && bIsReloading)
    {
        BroadcastReloadCancelled();
        RELOAD_LOG(Log, TEXT("Reload cancelled"));
    }

    bIsReloading = false;
    CurrentReloadType = ESuspenseCoreReloadType::None;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

//==================================================================
// Reload Logic
//==================================================================

ESuspenseCoreReloadType USuspenseCoreReloadAbility::DetermineReloadType() const
{
    ISuspenseCoreMagazineProvider* MagProvider = const_cast<USuspenseCoreReloadAbility*>(this)->GetMagazineProvider();
    if (!MagProvider)
    {
        return ESuspenseCoreReloadType::None;
    }

    // Get current weapon state via interface
    FSuspenseCoreWeaponAmmoState AmmoState = ISuspenseCoreMagazineProvider::Execute_GetAmmoState(Cast<UObject>(MagProvider));

    // Check if we need to chamber only
    if (!AmmoState.IsReadyToFire() && AmmoState.bHasMagazine && !AmmoState.IsMagazineEmpty())
    {
        return ESuspenseCoreReloadType::ChamberOnly;
    }

    // Check if we need to reload at all
    if (AmmoState.bHasMagazine && !AmmoState.IsMagazineEmpty())
    {
        // Magazine has ammo, might want tactical reload
        // Only allow if magazine is not full
        if (AmmoState.InsertedMagazine.IsFull())
        {
            return ESuspenseCoreReloadType::None;
        }

        return ESuspenseCoreReloadType::Tactical;
    }

    // Empty magazine or no magazine
    if (AmmoState.IsReadyToFire())
    {
        // Round in chamber, tactical reload
        return ESuspenseCoreReloadType::Tactical;
    }
    else
    {
        // No round in chamber, full reload
        return ESuspenseCoreReloadType::Empty;
    }
}

float USuspenseCoreReloadAbility::CalculateReloadDuration(ESuspenseCoreReloadType ReloadType) const
{
    float BaseTime = 0.0f;

    switch (ReloadType)
    {
        case ESuspenseCoreReloadType::Tactical:
            BaseTime = BaseTacticalReloadTime;
            break;

        case ESuspenseCoreReloadType::Empty:
            BaseTime = BaseEmptyReloadTime;
            break;

        case ESuspenseCoreReloadType::Emergency:
            BaseTime = BaseTacticalReloadTime * EmergencyReloadTimeMultiplier;
            break;

        case ESuspenseCoreReloadType::ChamberOnly:
            BaseTime = ChamberOnlyTime;
            break;

        default:
            return 0.0f;
    }

    // TODO: Apply modifiers via MagazineProvider interface
    // ISuspenseCoreMagazineProvider::Execute_CalculateReloadDuration(...)

    return BaseTime;
}

UAnimMontage* USuspenseCoreReloadAbility::GetMontageForReloadType(ESuspenseCoreReloadType ReloadType) const
{
    switch (ReloadType)
    {
        case ESuspenseCoreReloadType::Tactical:
            return TacticalReloadMontage.Get();

        case ESuspenseCoreReloadType::Empty:
            return EmptyReloadMontage.Get();

        case ESuspenseCoreReloadType::Emergency:
            return EmergencyReloadMontage.Get();

        case ESuspenseCoreReloadType::ChamberOnly:
            return ChamberOnlyMontage.Get();

        default:
            return nullptr;
    }
}

bool USuspenseCoreReloadAbility::FindBestMagazine(int32& OutQuickSlotIndex, FSuspenseCoreMagazineInstance& OutMagazine) const
{
    // First, try QuickSlots via interface
    ISuspenseCoreQuickSlotProvider* QuickSlotProvider = const_cast<USuspenseCoreReloadAbility*>(this)->GetQuickSlotProvider();
    if (QuickSlotProvider)
    {
        UObject* ProviderObj = Cast<UObject>(QuickSlotProvider);

        // Look for first available magazine with ammo
        for (int32 i = 0; i < 4; ++i)
        {
            FSuspenseCoreMagazineInstance Mag;
            if (ISuspenseCoreQuickSlotProvider::Execute_GetMagazineFromSlot(ProviderObj, i, Mag) && Mag.HasAmmo())
            {
                OutQuickSlotIndex = i;
                OutMagazine = Mag;
                RELOAD_LOG(Verbose, TEXT("Found magazine in QuickSlot %d: %d rounds"), i, Mag.CurrentRoundCount);
                return true;
            }
        }
    }

    // TODO: Fall back to inventory search
    OutQuickSlotIndex = -1;

    RELOAD_LOG(Verbose, TEXT("No suitable magazine found"));
    return false;
}

//==================================================================
// Animation Notify Handlers
//==================================================================

void USuspenseCoreReloadAbility::OnMagOutNotify()
{
    RELOAD_LOG(Verbose, TEXT("MagOut notify fired"));

    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (!MagProvider)
    {
        return;
    }

    UObject* ProviderObj = Cast<UObject>(MagProvider);

    // Eject current magazine
    bool bDropToGround = (CurrentReloadType == ESuspenseCoreReloadType::Emergency);
    EjectedMagazine = ISuspenseCoreMagazineProvider::Execute_EjectMagazine(ProviderObj, bDropToGround);

    // CRITICAL FIX: Don't store ejected magazine here!
    // Store it in OnMagInNotify AFTER ClearSlot() so it goes into the same slot
    // where the new magazine came from. This prevents magazine duplication.
    // See TarkovStyle_Ammo_System_Design.md Phase 7.
    if (bDropToGround)
    {
        // Emergency drop - magazine falls to ground, don't store
        EjectedMagazine = FSuspenseCoreMagazineInstance();
    }
}

void USuspenseCoreReloadAbility::OnMagInNotify()
{
    RELOAD_LOG(Verbose, TEXT("MagIn notify fired"));

    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (!MagProvider)
    {
        return;
    }

    UObject* ProviderObj = Cast<UObject>(MagProvider);
    ISuspenseCoreQuickSlotProvider* QuickSlotProvider = GetQuickSlotProvider();

    // Insert new magazine
    if (NewMagazine.IsValid())
    {
        ISuspenseCoreMagazineProvider::Execute_InsertMagazine(ProviderObj, NewMagazine);

        // Clear from quickslot if it came from there
        if (NewMagazineQuickSlotIndex >= 0 && QuickSlotProvider)
        {
            ISuspenseCoreQuickSlotProvider::Execute_ClearSlot(
                Cast<UObject>(QuickSlotProvider), NewMagazineQuickSlotIndex);
        }
    }

    // CRITICAL FIX: Store ejected magazine AFTER ClearSlot()
    // This ensures the ejected magazine goes into the same slot where new magazine came from
    // Prevents magazine duplication bug. See TarkovStyle_Ammo_System_Design.md Phase 7.
    if (EjectedMagazine.IsValid() && QuickSlotProvider)
    {
        int32 StoredSlotIndex;
        ISuspenseCoreQuickSlotProvider::Execute_StoreEjectedMagazine(
            Cast<UObject>(QuickSlotProvider), EjectedMagazine, StoredSlotIndex);

        RELOAD_LOG(Log, TEXT("Stored ejected magazine (%d rounds) in slot %d"),
            EjectedMagazine.CurrentRoundCount, StoredSlotIndex);

        // Clear reference
        EjectedMagazine = FSuspenseCoreMagazineInstance();
    }
}

void USuspenseCoreReloadAbility::OnRackStartNotify()
{
    RELOAD_LOG(Verbose, TEXT("RackStart notify fired"));
    // Visual/audio feedback here if needed
}

void USuspenseCoreReloadAbility::OnRackEndNotify()
{
    RELOAD_LOG(Verbose, TEXT("RackEnd notify fired"));

    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (!MagProvider)
    {
        return;
    }

    // Chamber a round
    ISuspenseCoreMagazineProvider::Execute_ChamberRound(Cast<UObject>(MagProvider));
}

void USuspenseCoreReloadAbility::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (bInterrupted)
    {
        // Reload was interrupted
        EndAbility(CachedSpecHandle, CachedActorInfo, CachedActivationInfo, true, true);
    }
    else
    {
        // Reload completed successfully
        BroadcastReloadCompleted();
        RELOAD_LOG(Log, TEXT("Reload completed successfully"));
        EndAbility(CachedSpecHandle, CachedActorInfo, CachedActivationInfo, true, false);
    }
}

void USuspenseCoreReloadAbility::OnMontageBlendOut(UAnimMontage* Montage, bool bInterrupted)
{
    // Optional: Handle blend out if needed
}

//==================================================================
// Internal Methods
//==================================================================

ISuspenseCoreMagazineProvider* USuspenseCoreReloadAbility::GetMagazineProvider() const
{
    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (AvatarActor)
    {
        // Check if avatar implements interface
        if (AvatarActor->GetClass()->ImplementsInterface(USuspenseCoreMagazineProvider::StaticClass()))
        {
            return Cast<ISuspenseCoreMagazineProvider>(AvatarActor);
        }

        // Check components
        TArray<UActorComponent*> Components;
        AvatarActor->GetComponents(Components);
        for (UActorComponent* Comp : Components)
        {
            if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreMagazineProvider::StaticClass()))
            {
                return Cast<ISuspenseCoreMagazineProvider>(Comp);
            }
        }
    }

    // Check owner actor
    AActor* OwnerActor = GetOwningActorFromActorInfo();
    if (OwnerActor && OwnerActor != AvatarActor)
    {
        TArray<UActorComponent*> Components;
        OwnerActor->GetComponents(Components);
        for (UActorComponent* Comp : Components)
        {
            if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreMagazineProvider::StaticClass()))
            {
                return Cast<ISuspenseCoreMagazineProvider>(Comp);
            }
        }
    }

    return nullptr;
}

ISuspenseCoreQuickSlotProvider* USuspenseCoreReloadAbility::GetQuickSlotProvider() const
{
    AActor* OwnerActor = GetOwningActorFromActorInfo();
    if (!OwnerActor)
    {
        return nullptr;
    }

    // Check if owner implements interface
    if (OwnerActor->GetClass()->ImplementsInterface(USuspenseCoreQuickSlotProvider::StaticClass()))
    {
        return Cast<ISuspenseCoreQuickSlotProvider>(OwnerActor);
    }

    // Check components
    TArray<UActorComponent*> Components;
    OwnerActor->GetComponents(Components);
    for (UActorComponent* Comp : Components)
    {
        if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreQuickSlotProvider::StaticClass()))
        {
            return Cast<ISuspenseCoreQuickSlotProvider>(Comp);
        }
    }

    return nullptr;
}

void USuspenseCoreReloadAbility::ApplyReloadEffects(const FGameplayAbilityActorInfo* ActorInfo)
{
    if (ReloadSpeedDebuffClass && ActorInfo->AbilitySystemComponent.IsValid())
    {
        FGameplayEffectContextHandle EffectContext = ActorInfo->AbilitySystemComponent->MakeEffectContext();
        EffectContext.AddSourceObject(this);

        FGameplayEffectSpecHandle SpecHandle = ActorInfo->AbilitySystemComponent->MakeOutgoingSpec(
            ReloadSpeedDebuffClass, 1.0f, EffectContext);

        if (SpecHandle.IsValid())
        {
            ReloadSpeedEffectHandle = ActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }
}

void USuspenseCoreReloadAbility::RemoveReloadEffects(const FGameplayAbilityActorInfo* ActorInfo)
{
    if (ReloadSpeedEffectHandle.IsValid() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        ActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(ReloadSpeedEffectHandle);
        ReloadSpeedEffectHandle.Invalidate();
    }
}

bool USuspenseCoreReloadAbility::PlayReloadMontage()
{
    UAnimMontage* Montage = GetMontageForReloadType(CurrentReloadType);
    if (!Montage)
    {
        RELOAD_LOG(Warning, TEXT("No montage for reload type %d"), static_cast<int32>(CurrentReloadType));
        return false;
    }

    ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!Character)
    {
        return false;
    }

    UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
    if (!AnimInstance)
    {
        return false;
    }

    // Calculate play rate to match desired duration
    float MontageLength = Montage->GetPlayLength();
    float PlayRate = (ReloadDuration > 0.0f) ? (MontageLength / ReloadDuration) : 1.0f;

    // Play montage
    float Duration = AnimInstance->Montage_Play(Montage, PlayRate);
    if (Duration <= 0.0f)
    {
        RELOAD_LOG(Warning, TEXT("Failed to play reload montage"));
        return false;
    }

    // Bind to montage end
    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &USuspenseCoreReloadAbility::OnMontageEnded);
    AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);

    // Bind to blend out
    FOnMontageBlendingOutStarted BlendOutDelegate;
    BlendOutDelegate.BindUObject(this, &USuspenseCoreReloadAbility::OnMontageBlendOut);
    AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, Montage);

    return true;
}

void USuspenseCoreReloadAbility::StopReloadMontage()
{
    ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!Character)
    {
        return;
    }

    UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance();
    if (!AnimInstance)
    {
        return;
    }

    UAnimMontage* Montage = GetMontageForReloadType(CurrentReloadType);
    if (Montage && AnimInstance->Montage_IsPlaying(Montage))
    {
        AnimInstance->Montage_Stop(0.2f, Montage);
    }
}

void USuspenseCoreReloadAbility::BroadcastReloadStarted()
{
    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (MagProvider)
    {
        ISuspenseCoreMagazineProvider::Execute_NotifyReloadStateChanged(
            Cast<UObject>(MagProvider), true, CurrentReloadType, ReloadDuration);
    }

    // EventBus broadcast
    PublishSimpleEvent(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Reload")));
}

void USuspenseCoreReloadAbility::BroadcastReloadCompleted()
{
    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (MagProvider)
    {
        ISuspenseCoreMagazineProvider::Execute_NotifyReloadStateChanged(
            Cast<UObject>(MagProvider), false, CurrentReloadType, 0.0f);
    }
}

void USuspenseCoreReloadAbility::BroadcastReloadCancelled()
{
    ISuspenseCoreMagazineProvider* MagProvider = GetMagazineProvider();
    if (MagProvider)
    {
        ISuspenseCoreMagazineProvider::Execute_NotifyReloadStateChanged(
            Cast<UObject>(MagProvider), false, ESuspenseCoreReloadType::None, 0.0f);
    }
}

#undef RELOAD_LOG
