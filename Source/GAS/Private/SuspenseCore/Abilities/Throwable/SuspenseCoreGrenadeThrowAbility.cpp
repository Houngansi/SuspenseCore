// SuspenseCoreGrenadeThrowAbility.cpp
// Grenade throw ability with animation montage support
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Throwable/SuspenseCoreGrenadeThrowAbility.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreQuickSlotProvider.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Camera/CameraShakeBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreGrenade, Log, All);

#define GRENADE_LOG(Verbosity, Format, ...) \
    UE_LOG(LogSuspenseCoreGrenade, Verbosity, TEXT("[%s] " Format), *GetNameSafe(GetOwningActorFromActorInfo()), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

USuspenseCoreGrenadeThrowAbility::USuspenseCoreGrenadeThrowAbility()
{
    // Input binding - Fire (LMB) triggers throw when grenade is equipped
    // ActivationRequiredTags ensures this only works when State.GrenadeEquipped is present
    AbilityInputID = ESuspenseCoreAbilityInputID::Fire;

    // AbilityTags for activation via TryActivateAbilitiesByTag()
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(SuspenseCoreTags::Ability::Throwable::Grenade);
    SetAssetTags(AssetTags);

    // Default timing
    PrepareTime = 0.5f;
    MaxCookTime = 5.0f;
    ThrowCooldown = 1.0f;

    // Default physics
    OverhandThrowForce = 1500.0f;
    UnderhandThrowForce = 800.0f;
    RollThrowForce = 500.0f;
    OverhandUpAngle = 15.0f;

    // Camera shake for throw effect
    ThrowCameraShakeScale = 0.5f;

    // Initialize runtime state
    CurrentThrowType = ESuspenseCoreGrenadeThrowType::Overhand;
    bIsPreparing = false;
    bIsCooking = false;
    bPinPulled = false;
    CookStartTime = 0.0f;
    CurrentGrenadeSlotIndex = -1;

    // Ability configuration
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    bRetriggerInstancedAbility = false;

    // Network configuration
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

    // Blocking tags - can't throw while doing these
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Firing);
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Reloading);
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Disabled);

    // Tarkov-style flow: require grenade to be equipped first
    // State.GrenadeEquipped is granted by GA_GrenadeEquip
    // Use native tag to ensure exact match with what GA_GrenadeEquip grants
    ActivationRequiredTags.AddTag(SuspenseCoreTags::State::GrenadeEquipped);

    // Tags applied while throwing
    ActivationOwnedTags.AddTag(SuspenseCoreTags::State::ThrowingGrenade);

    // Cancel these abilities when throwing
    CancelAbilitiesWithTag.AddTag(SuspenseCoreTags::Ability::Sprint);
    CancelAbilitiesWithTag.AddTag(SuspenseCoreTags::Ability::Weapon::AimDownSight);

    // EventBus configuration
    bPublishAbilityEvents = true;
}

//==================================================================
// Runtime Accessors
//==================================================================

float USuspenseCoreGrenadeThrowAbility::GetCookTime() const
{
    if (!bIsCooking)
    {
        return 0.0f;
    }

    return GetWorld()->GetTimeSeconds() - CookStartTime;
}

void USuspenseCoreGrenadeThrowAbility::SetThrowType(ESuspenseCoreGrenadeThrowType NewType)
{
    if (!bPinPulled)
    {
        CurrentThrowType = NewType;
        GRENADE_LOG(Verbose, TEXT("SetThrowType: Changed to %d"), static_cast<int32>(NewType));
    }
    else
    {
        GRENADE_LOG(Warning, TEXT("SetThrowType: Cannot change throw type after pin is pulled"));
    }
}

void USuspenseCoreGrenadeThrowAbility::SetGrenadeInfo(FName InGrenadeID, int32 InSlotIndex)
{
    CurrentGrenadeID = InGrenadeID;
    CurrentGrenadeSlotIndex = InSlotIndex;
    bGrenadeInfoSet = true;

    GRENADE_LOG(Log, TEXT("SetGrenadeInfo (Tarkov-style): GrenadeID=%s, SlotIndex=%d"),
        *InGrenadeID.ToString(), InSlotIndex);
}

//==================================================================
// GameplayAbility Interface
//==================================================================

bool USuspenseCoreGrenadeThrowAbility::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    GRENADE_LOG(Log, TEXT("CanActivateAbility: Starting validation"));

    // Super check includes ActivationRequiredTags (State.GrenadeEquipped)
    // This enforces Tarkov-style flow: grenade must be equipped first
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        GRENADE_LOG(Warning, TEXT("CanActivateAbility: Super check FAILED"));
        return false;
    }

    // Check if already throwing
    if (bIsPreparing || bIsCooking)
    {
        GRENADE_LOG(Warning, TEXT("CanActivateAbility: Already in throw sequence"));
        return false;
    }

    // Tarkov-style flow: If we passed Super check, State.GrenadeEquipped is present.
    // Trust the tag - grenade info will be looked up in ActivateAbility.
    // This avoids issues with FindGrenadeInQuickSlots failing due to timing/state changes.
    GRENADE_LOG(Log, TEXT("CanActivateAbility: PASSED (State.GrenadeEquipped verified by Super)"));
    return true;
}

void USuspenseCoreGrenadeThrowAbility::ActivateAbility(
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

    // Tarkov-style flow: grenade info was pre-set via SetGrenadeInfo()
    if (bGrenadeInfoSet && !CurrentGrenadeID.IsNone())
    {
        GRENADE_LOG(Log, TEXT("ActivateAbility: Using pre-set grenade info (Tarkov-style): %s"),
            *CurrentGrenadeID.ToString());
    }
    // Legacy flow: find grenade in QuickSlots
    else if (!FindGrenadeInQuickSlots(CurrentGrenadeSlotIndex, CurrentGrenadeID))
    {
        GRENADE_LOG(Warning, TEXT("ActivateAbility: No grenade found"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Start prepare phase
    bIsPreparing = true;
    bIsCooking = false;
    bPinPulled = false;

    // Apply effects (speed debuff)
    ApplyPrepareEffects();

    // Play montage
    if (!PlayThrowMontage())
    {
        GRENADE_LOG(Warning, TEXT("ActivateAbility: Failed to play throw montage"));
        RemovePrepareEffects();
        bIsPreparing = false;
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Broadcast prepare started
    OnPrepareStarted();
    BroadcastGrenadeEvent(SuspenseCoreTags::Event::Throwable::PrepareStarted);

    GRENADE_LOG(Log, TEXT("Grenade throw started: Grenade=%s, ThrowType=%d"),
        *CurrentGrenadeID.ToString(), static_cast<int32>(CurrentThrowType));

    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void USuspenseCoreGrenadeThrowAbility::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    // Clean up
    RemovePrepareEffects();
    StopThrowMontage();

    if (bWasCancelled)
    {
        // Only broadcast cancelled if pin wasn't pulled
        // (if pin was pulled, grenade must be thrown even if cancelled)
        if (!bPinPulled)
        {
            OnThrowCancelled();
            BroadcastGrenadeEvent(SuspenseCoreTags::Event::Throwable::Cancelled);
            PlaySound(CancelSound);
            GRENADE_LOG(Log, TEXT("Grenade throw cancelled (pin not pulled)"));
        }
        else
        {
            // Pin was pulled - force throw
            ExecuteThrow();
            GRENADE_LOG(Log, TEXT("Grenade force thrown (pin was pulled)"));
        }
    }

    bIsPreparing = false;
    bIsCooking = false;
    bPinPulled = false;
    bGrenadeInfoSet = false;
    CookStartTime = 0.0f;
    CurrentGrenadeSlotIndex = -1;
    CurrentGrenadeID = NAME_None;

    // Clear cached provider
    CachedQuickSlotProvider.Reset();

    // Tarkov-style flow: Cancel GrenadeEquipAbility after successful throw
    // This will remove State.GrenadeEquipped tag and restore previous weapon
    if (!bWasCancelled)
    {
        UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
        if (ASC)
        {
            // Cancel GA_GrenadeEquip by tag
            FGameplayTagContainer EquipTags;
            EquipTags.AddTag(SuspenseCoreTags::Ability::Throwable::Equip);
            ASC->CancelAbilities(&EquipTags);

            GRENADE_LOG(Log, TEXT("Cancelled GrenadeEquipAbility after successful throw"));
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void USuspenseCoreGrenadeThrowAbility::InputReleased(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    // Input released while cooking - throw the grenade
    if (bIsCooking && bPinPulled)
    {
        GRENADE_LOG(Log, TEXT("InputReleased: Throwing grenade after %.2f seconds cook time"),
            GetCookTime());

        // The actual throw happens via OnReleaseNotify from montage
        // If montage doesn't have release notify, it will throw on montage end
    }
    else if (bIsPreparing && !bPinPulled)
    {
        // Cancel if pin not yet pulled
        GRENADE_LOG(Log, TEXT("InputReleased: Cancelling throw (pin not pulled yet)"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
    }

    Super::InputReleased(Handle, ActorInfo, ActivationInfo);
}

//==================================================================
// Animation Notify Handlers
//==================================================================

void USuspenseCoreGrenadeThrowAbility::OnPinPullNotify()
{
    GRENADE_LOG(Log, TEXT("OnPinPullNotify: Pin pulled, grenade armed"));

    bPinPulled = true;
    bIsPreparing = false;

    // Play pin pull sound
    PlaySound(PinPullSound);

    // Notify blueprint
    OnGrenadePinPulled();

    // Broadcast event
    BroadcastGrenadeEvent(SuspenseCoreTags::Event::Throwable::PinPulled);
}

void USuspenseCoreGrenadeThrowAbility::OnReadyNotify()
{
    GRENADE_LOG(Log, TEXT("OnReadyNotify: Grenade ready, cooking starts"));

    bIsCooking = true;
    CookStartTime = GetWorld()->GetTimeSeconds();

    // Broadcast cooking started
    BroadcastGrenadeEvent(SuspenseCoreTags::Event::Throwable::CookingStarted);
}

void USuspenseCoreGrenadeThrowAbility::OnReleaseNotify()
{
    GRENADE_LOG(Log, TEXT("OnReleaseNotify: Throwing grenade"));

    // Execute the throw
    if (ExecuteThrow())
    {
        // Play throw sound
        PlaySound(ThrowSound);

        // Play camera shake (similar to FireAbility recoil shake)
        if (ThrowCameraShake)
        {
            ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
            if (Character)
            {
                APlayerController* PC = Cast<APlayerController>(Character->GetController());
                if (PC)
                {
                    PC->ClientStartCameraShake(ThrowCameraShake, ThrowCameraShakeScale);
                    GRENADE_LOG(Verbose, TEXT("Camera shake played: Scale=%.2f"), ThrowCameraShakeScale);
                }
            }
        }

        // Notify blueprint
        OnGrenadeThrown();

        // Broadcast thrown event
        BroadcastGrenadeEvent(SuspenseCoreTags::Event::Throwable::Thrown);

        GRENADE_LOG(Log, TEXT("Grenade thrown successfully. CookTime=%.2f"), GetCookTime());
    }
    else
    {
        GRENADE_LOG(Warning, TEXT("Failed to throw grenade"));
    }
}

void USuspenseCoreGrenadeThrowAbility::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (bInterrupted)
    {
        // Montage was interrupted
        EndAbility(CachedSpecHandle, CachedActorInfo, CachedActivationInfo, true, true);
    }
    else
    {
        // Montage completed successfully
        // If grenade wasn't thrown via notify, throw it now
        if (bPinPulled && !bIsCooking)
        {
            GRENADE_LOG(Log, TEXT("OnMontageEnded: Montage complete, executing throw"));
            ExecuteThrow();
        }

        GRENADE_LOG(Log, TEXT("Grenade throw completed"));
        EndAbility(CachedSpecHandle, CachedActorInfo, CachedActivationInfo, true, false);
    }
}

void USuspenseCoreGrenadeThrowAbility::OnMontageBlendOut(UAnimMontage* Montage, bool bInterrupted)
{
    // Optional: Handle blend out if needed
}

void USuspenseCoreGrenadeThrowAbility::OnAnimNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
    // Dispatch AnimNotify to appropriate handler based on notify name
    GRENADE_LOG(Log, TEXT("AnimNotify received: '%s'"), *NotifyName.ToString());

    // Phase 1: Pin Pull - "Continue" or "PinPull" marks pin removed
    if (NotifyName == FName("Continue") ||
        NotifyName == FName("PinPull") ||
        NotifyName == FName("Arm"))
    {
        GRENADE_LOG(Log, TEXT("  -> Phase 1: PinPull"));
        OnPinPullNotify();
    }
    // Phase 2: Ready - "ClipIn" or "Ready" marks grenade ready for cooking
    else if (NotifyName == FName("ClipIn") ||
             NotifyName == FName("Ready") ||
             NotifyName == FName("Armed"))
    {
        GRENADE_LOG(Log, TEXT("  -> Phase 2: Ready"));
        OnReadyNotify();
    }
    // Phase 3: Release - "Finalize" or "Release" marks throw
    else if (NotifyName == FName("Finalize") ||
             NotifyName == FName("Release") ||
             NotifyName == FName("Throw"))
    {
        GRENADE_LOG(Log, TEXT("  -> Phase 3: Release"));
        OnReleaseNotify();
    }
}

//==================================================================
// Internal Methods
//==================================================================

ISuspenseCoreQuickSlotProvider* USuspenseCoreGrenadeThrowAbility::GetQuickSlotProvider() const
{
    // Use cached provider if valid
    if (CachedQuickSlotProvider.IsValid())
    {
        return Cast<ISuspenseCoreQuickSlotProvider>(CachedQuickSlotProvider.Get());
    }

    // IMPORTANT: Use Avatar (Character), not Owner (PlayerState)!
    // QuickSlotComponent is on the Character, not PlayerState.
    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
        GRENADE_LOG(Warning, TEXT("GetQuickSlotProvider: No AvatarActor"));
        return nullptr;
    }

    // Check if avatar implements interface
    if (AvatarActor->GetClass()->ImplementsInterface(USuspenseCoreQuickSlotProvider::StaticClass()))
    {
        CachedQuickSlotProvider = AvatarActor;
        return Cast<ISuspenseCoreQuickSlotProvider>(AvatarActor);
    }

    // Check components on the avatar
    TArray<UActorComponent*> Components;
    AvatarActor->GetComponents(Components);
    for (UActorComponent* Comp : Components)
    {
        if (Comp && Comp->GetClass()->ImplementsInterface(USuspenseCoreQuickSlotProvider::StaticClass()))
        {
            CachedQuickSlotProvider = Comp;
            return Cast<ISuspenseCoreQuickSlotProvider>(Comp);
        }
    }

    return nullptr;
}

bool USuspenseCoreGrenadeThrowAbility::FindGrenadeInQuickSlots(int32& OutSlotIndex, FName& OutGrenadeID) const
{
    ISuspenseCoreQuickSlotProvider* QuickSlotProvider = GetQuickSlotProvider();
    if (!QuickSlotProvider)
    {
        return false;
    }

    UObject* ProviderObj = Cast<UObject>(QuickSlotProvider);

    // Look for first available grenade/throwable in QuickSlots
    // Grenades are identified by:
    // 1. Slot has an item (not a magazine)
    // 2. Item ID contains "Grenade" or "Throwable" keywords
    for (int32 i = 0; i < 4; ++i)
    {
        // Check if slot is ready and has an item
        if (!ISuspenseCoreQuickSlotProvider::Execute_IsSlotReady(ProviderObj, i))
        {
            continue;
        }

        if (!ISuspenseCoreQuickSlotProvider::Execute_HasItemInSlot(ProviderObj, i))
        {
            continue;
        }

        // Get slot data
        FSuspenseCoreQuickSlot SlotData = ISuspenseCoreQuickSlotProvider::Execute_GetQuickSlot(ProviderObj, i);
        if (!SlotData.HasItem())
        {
            continue;
        }

        // Check if this is NOT a magazine (magazines have separate handling)
        FSuspenseCoreMagazineInstance MagInstance;
        if (ISuspenseCoreQuickSlotProvider::Execute_GetMagazineFromSlot(ProviderObj, i, MagInstance) && MagInstance.IsValid())
        {
            // This slot contains a magazine, skip it
            continue;
        }

        // Assume non-magazine items in QuickSlots are grenades/throwables
        // (the slot assignment logic should only allow valid item types)
        FString ItemIDStr = SlotData.AssignedItemID.ToString();
        if (ItemIDStr.Contains(TEXT("Grenade")) ||
            ItemIDStr.Contains(TEXT("Throwable")) ||
            ItemIDStr.Contains(TEXT("Smoke")) ||
            ItemIDStr.Contains(TEXT("Flash")) ||
            ItemIDStr.Contains(TEXT("Frag")) ||
            ItemIDStr.Contains(TEXT("F1")) ||
            ItemIDStr.Contains(TEXT("RGD")) ||
            ItemIDStr.Contains(TEXT("M67")))
        {
            OutSlotIndex = i;
            OutGrenadeID = SlotData.AssignedItemID;
            GRENADE_LOG(Log, TEXT("Found grenade in QuickSlot %d: %s"), i, *OutGrenadeID.ToString());
            return true;
        }
    }

    OutSlotIndex = -1;
    OutGrenadeID = NAME_None;
    GRENADE_LOG(Verbose, TEXT("No grenade found in QuickSlots"));
    return false;
}

UAnimMontage* USuspenseCoreGrenadeThrowAbility::GetMontageForThrowType() const
{
    UAnimMontage* Montage = nullptr;
    const TCHAR* MontageName = TEXT("Unknown");

    switch (CurrentThrowType)
    {
        case ESuspenseCoreGrenadeThrowType::Overhand:
            Montage = OverhandThrowMontage.Get();
            MontageName = TEXT("OverhandThrowMontage");
            break;

        case ESuspenseCoreGrenadeThrowType::Underhand:
            Montage = UnderhandThrowMontage.Get();
            MontageName = TEXT("UnderhandThrowMontage");
            break;

        case ESuspenseCoreGrenadeThrowType::Roll:
            Montage = RollThrowMontage.Get();
            MontageName = TEXT("RollThrowMontage");
            break;

        default:
            GRENADE_LOG(Warning, TEXT("GetMontageForThrowType: Unknown throw type %d"), static_cast<int32>(CurrentThrowType));
            return nullptr;
    }

    if (!Montage)
    {
        GRENADE_LOG(Warning, TEXT("GetMontageForThrowType: '%s' is NOT SET in Blueprint! Set it in GA_GrenadeThrowAbility_C defaults."),
            MontageName);
    }

    return Montage;
}

float USuspenseCoreGrenadeThrowAbility::GetThrowForceForType() const
{
    switch (CurrentThrowType)
    {
        case ESuspenseCoreGrenadeThrowType::Overhand:
            return OverhandThrowForce;

        case ESuspenseCoreGrenadeThrowType::Underhand:
            return UnderhandThrowForce;

        case ESuspenseCoreGrenadeThrowType::Roll:
            return RollThrowForce;

        default:
            return OverhandThrowForce;
    }
}

bool USuspenseCoreGrenadeThrowAbility::PlayThrowMontage()
{
    UAnimMontage* Montage = GetMontageForThrowType();
    if (!Montage)
    {
        GRENADE_LOG(Warning, TEXT("PlayThrowMontage: No montage for throw type %d"), static_cast<int32>(CurrentThrowType));
        return false;
    }

    GRENADE_LOG(Log, TEXT("PlayThrowMontage: Got montage '%s' for throw type %d"),
        *Montage->GetName(), static_cast<int32>(CurrentThrowType));

    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    ACharacter* Character = Cast<ACharacter>(AvatarActor);
    if (!Character)
    {
        GRENADE_LOG(Warning, TEXT("PlayThrowMontage: AvatarActor '%s' is not ACharacter!"),
            AvatarActor ? *AvatarActor->GetName() : TEXT("NULL"));
        return false;
    }

    USkeletalMeshComponent* MeshComp = Character->GetMesh();
    if (!MeshComp)
    {
        GRENADE_LOG(Warning, TEXT("PlayThrowMontage: Character->GetMesh() returned NULL!"));
        return false;
    }

    UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
    if (!AnimInstance)
    {
        // Try to find AnimInstance on other skeletal mesh components (MetaHuman Body)
        GRENADE_LOG(Warning, TEXT("PlayThrowMontage: Primary mesh '%s' has no AnimInstance, searching other components..."),
            *MeshComp->GetName());

        TArray<USkeletalMeshComponent*> SkeletalMeshes;
        Character->GetComponents<USkeletalMeshComponent>(SkeletalMeshes);

        for (USkeletalMeshComponent* SMC : SkeletalMeshes)
        {
            if (SMC && SMC != MeshComp && SMC->GetAnimInstance())
            {
                AnimInstance = SMC->GetAnimInstance();
                GRENADE_LOG(Log, TEXT("PlayThrowMontage: Found AnimInstance on component '%s'"), *SMC->GetName());
                break;
            }
        }

        if (!AnimInstance)
        {
            GRENADE_LOG(Warning, TEXT("PlayThrowMontage: No AnimInstance found on any skeletal mesh!"));
            return false;
        }
    }

    GRENADE_LOG(Log, TEXT("PlayThrowMontage: Playing montage. Length=%.2f"),
        Montage->GetPlayLength());

    // Play montage at normal speed
    float Duration = AnimInstance->Montage_Play(Montage, 1.0f);
    if (Duration <= 0.0f)
    {
        GRENADE_LOG(Warning, TEXT("PlayThrowMontage: Montage_Play returned %.2f (failed). AnimInstance='%s', Montage='%s'"),
            Duration, *AnimInstance->GetClass()->GetName(), *Montage->GetName());
        return false;
    }

    GRENADE_LOG(Log, TEXT("PlayThrowMontage: SUCCESS! Duration=%.2f"), Duration);

    // Bind to montage end
    FOnMontageEnded EndDelegate;
    EndDelegate.BindUObject(this, &USuspenseCoreGrenadeThrowAbility::OnMontageEnded);
    AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);

    // Bind to blend out
    FOnMontageBlendingOutStarted BlendOutDelegate;
    BlendOutDelegate.BindUObject(this, &USuspenseCoreGrenadeThrowAbility::OnMontageBlendOut);
    AnimInstance->Montage_SetBlendingOutDelegate(BlendOutDelegate, Montage);

    // Bind to AnimNotify events for throw phase indicators
    CachedAnimInstance = AnimInstance;
    AnimInstance->OnPlayMontageNotifyBegin.AddDynamic(this, &USuspenseCoreGrenadeThrowAbility::OnAnimNotifyBegin);

    return true;
}

void USuspenseCoreGrenadeThrowAbility::StopThrowMontage()
{
    // Unbind AnimNotify callback
    if (CachedAnimInstance.IsValid())
    {
        CachedAnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &USuspenseCoreGrenadeThrowAbility::OnAnimNotifyBegin);
    }
    CachedAnimInstance.Reset();

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

    UAnimMontage* Montage = GetMontageForThrowType();
    if (Montage && AnimInstance->Montage_IsPlaying(Montage))
    {
        AnimInstance->Montage_Stop(0.2f, Montage);
    }
}

bool USuspenseCoreGrenadeThrowAbility::ExecuteThrow()
{
    AActor* OwnerActor = GetOwningActorFromActorInfo();
    if (!OwnerActor)
    {
        return false;
    }

    ACharacter* Character = Cast<ACharacter>(OwnerActor);

    // Calculate throw direction
    FVector ThrowDirection = OwnerActor->GetActorForwardVector();
    FVector ThrowLocation = OwnerActor->GetActorLocation();

    if (Character)
    {
        // Use camera/view direction for aiming
        FRotator ViewRotation = Character->GetControlRotation();

        // Apply upward angle for overhand throw
        if (CurrentThrowType == ESuspenseCoreGrenadeThrowType::Overhand)
        {
            ViewRotation.Pitch += OverhandUpAngle;
        }

        ThrowDirection = ViewRotation.Vector();

        // Offset spawn location to hand position
        ThrowLocation += Character->GetActorForwardVector() * 50.0f;
        ThrowLocation += FVector::UpVector * 50.0f;
    }

    // Get throw force
    float ThrowForce = GetThrowForceForType();

    // Consume grenade from QuickSlot
    ConsumeGrenade();

    // Broadcast throw event with spawn parameters
    // The actual spawning is handled by SuspenseCoreGrenadeHandler or listeners
    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(OwnerActor);
        EventData.SetString(TEXT("GrenadeID"), CurrentGrenadeID.ToString());
        EventData.SetVector(TEXT("ThrowLocation"), ThrowLocation);
        EventData.SetVector(TEXT("ThrowDirection"), ThrowDirection);
        EventData.SetFloat(TEXT("ThrowForce"), ThrowForce);
        EventData.SetFloat(TEXT("CookTime"), GetCookTime());
        EventData.SetInt(TEXT("ThrowType"), static_cast<int32>(CurrentThrowType));

        // Publish spawn request event
        EventBus->Publish(SuspenseCoreTags::Event::Throwable::SpawnRequested, EventData);

        GRENADE_LOG(Log, TEXT("ExecuteThrow: Published SpawnRequested event for %s, Force=%.0f, CookTime=%.2f"),
            *CurrentGrenadeID.ToString(), ThrowForce, GetCookTime());
    }

    bIsCooking = false;
    return true;
}

void USuspenseCoreGrenadeThrowAbility::ApplyPrepareEffects()
{
    if (PrepareSpeedDebuffClass && CachedActorInfo && CachedActorInfo->AbilitySystemComponent.IsValid())
    {
        FGameplayEffectContextHandle EffectContext = CachedActorInfo->AbilitySystemComponent->MakeEffectContext();
        EffectContext.AddSourceObject(this);

        FGameplayEffectSpecHandle SpecHandle = CachedActorInfo->AbilitySystemComponent->MakeOutgoingSpec(
            PrepareSpeedDebuffClass, 1.0f, EffectContext);

        if (SpecHandle.IsValid())
        {
            PrepareSpeedEffectHandle = CachedActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }
}

void USuspenseCoreGrenadeThrowAbility::RemovePrepareEffects()
{
    if (PrepareSpeedEffectHandle.IsValid() && CachedActorInfo && CachedActorInfo->AbilitySystemComponent.IsValid())
    {
        CachedActorInfo->AbilitySystemComponent->RemoveActiveGameplayEffect(PrepareSpeedEffectHandle);
        PrepareSpeedEffectHandle.Invalidate();
    }
}

void USuspenseCoreGrenadeThrowAbility::PlaySound(USoundBase* Sound)
{
    if (!Sound)
    {
        return;
    }

    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
        return;
    }

    // Play sound at character location (3D positional audio)
    UGameplayStatics::PlaySoundAtLocation(
        AvatarActor,
        Sound,
        AvatarActor->GetActorLocation(),
        AvatarActor->GetActorRotation(),
        1.0f,  // Volume multiplier
        1.0f,  // Pitch multiplier
        0.0f,  // Start time
        nullptr,  // Attenuation settings (use default from SoundBase)
        nullptr,  // Concurrency settings
        AvatarActor  // Owning actor
    );

    GRENADE_LOG(Verbose, TEXT("PlaySound: %s"), *Sound->GetName());
}

void USuspenseCoreGrenadeThrowAbility::BroadcastGrenadeEvent(FGameplayTag EventTag)
{
    USuspenseCoreEventBus* EventBus = GetEventBus();
    if (!EventBus)
    {
        return;
    }

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetAvatarActorFromActorInfo());
    EventData.SetString(TEXT("GrenadeID"), CurrentGrenadeID.ToString());
    EventData.SetInt(TEXT("ThrowType"), static_cast<int32>(CurrentThrowType));
    EventData.SetFloat(TEXT("CookTime"), GetCookTime());
    EventData.SetBool(TEXT("PinPulled"), bPinPulled);

    EventBus->Publish(EventTag, EventData);

    GRENADE_LOG(Verbose, TEXT("BroadcastGrenadeEvent: %s"), *EventTag.ToString());
}

void USuspenseCoreGrenadeThrowAbility::ConsumeGrenade()
{
    if (CurrentGrenadeSlotIndex < 0)
    {
        return;
    }

    ISuspenseCoreQuickSlotProvider* QuickSlotProvider = GetQuickSlotProvider();
    if (!QuickSlotProvider)
    {
        return;
    }

    UObject* ProviderObj = Cast<UObject>(QuickSlotProvider);

    // Clear the slot (removes the grenade)
    // Note: For stackable grenades, the inventory system should handle
    // decrementing the quantity. QuickSlots reference inventory items.
    ISuspenseCoreQuickSlotProvider::Execute_ClearSlot(ProviderObj, CurrentGrenadeSlotIndex);

    GRENADE_LOG(Log, TEXT("ConsumeGrenade: Cleared grenade from slot %d"), CurrentGrenadeSlotIndex);
}

#undef GRENADE_LOG
