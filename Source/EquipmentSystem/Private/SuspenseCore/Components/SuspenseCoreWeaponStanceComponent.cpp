// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreWeaponStanceComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponAnimation.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Base/SuspenseCoreWeaponActor.h"

USuspenseCoreWeaponStanceComponent::USuspenseCoreWeaponStanceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	SetIsReplicatedByDefault(true);

	// Initialize weapon identity
	CurrentWeaponType = FGameplayTag();
	bWeaponDrawn = false;

	// Initialize combat states
	bIsAiming = false;
	bIsFiring = false;
	bIsReloading = false;
	bIsHoldingBreath = false;
	bIsMontageActive = false;

	// Initialize pose modifiers
	AimPoseAlpha = 0.0f;
	TargetAimPoseAlpha = 0.0f;
	GripModifier = 0.0f;
	WeaponLoweredAlpha = 0.0f;

	// Initialize procedural animation
	SwayMultiplier = 1.0f;
	RecoilAlpha = 0.0f;

	// Cache
	LastAnimationInterfaceCacheTime = -1000.0f;
}

void USuspenseCoreWeaponStanceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Weapon identity - replicated with OnRep callbacks
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponStanceComponent, CurrentWeaponType, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponStanceComponent, bWeaponDrawn, COND_None, REPNOTIFY_Always);

	// Combat states - replicated with OnRep callbacks
	// Note: All combat states use OnRep_CombatState for unified state update
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponStanceComponent, bIsAiming, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponStanceComponent, bIsFiring, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponStanceComponent, bIsReloading, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponStanceComponent, bIsHoldingBreath, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponStanceComponent, bIsWeaponBlocked, COND_None, REPNOTIFY_Always);
}

void USuspenseCoreWeaponStanceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateInterpolatedValues(DeltaTime);
}

// ============================================================================
// Equipment System API
// ============================================================================

void USuspenseCoreWeaponStanceComponent::OnEquipmentChanged(AActor* NewEquipmentActor)
{
	TrackedEquipmentActor = NewEquipmentActor;

	// Reset combat states when equipment changes
	bIsFiring = false;
	bIsReloading = false;
	RecoilAlpha = 0.0f;

	// Read pose indices from weapon actor (using getters)
	if (ASuspenseCoreWeaponActor* WeaponActor = Cast<ASuspenseCoreWeaponActor>(NewEquipmentActor))
	{
		GripID = WeaponActor->GetGripID();
		AimPose = WeaponActor->GetAimPose();
		StoredPose = WeaponActor->GetStoredPose();
		bModifyGrip = WeaponActor->GetModifyGrip();
		bCreateAimPose = WeaponActor->GetCreateAimPose();

		UE_LOG(LogTemp, Log, TEXT("[StanceComp] Loaded pose indices from weapon: GripID=%d, AimPose=%d, StoredPose=%d, ModifyGrip=%d, CreateAimPose=%d"),
			GripID, AimPose, StoredPose, bModifyGrip ? 1 : 0, bCreateAimPose ? 1 : 0);
	}
	else
	{
		// Reset to defaults when no weapon or non-WeaponActor equipment
		GripID = 0;
		AimPose = 0;
		StoredPose = 0;
		bModifyGrip = false;
		bCreateAimPose = false;
	}

	PushToAnimationLayer(/*bSkipIfNoInterface=*/true);
}

void USuspenseCoreWeaponStanceComponent::SetWeaponStance(const FGameplayTag& WeaponTypeTag, bool bImmediate)
{
	UE_LOG(LogTemp, Warning, TEXT("[StanceComp] SetWeaponStance called: %s (Current: %s)"),
		*WeaponTypeTag.ToString(), *CurrentWeaponType.ToString());

	if (CurrentWeaponType == WeaponTypeTag)
	{
		UE_LOG(LogTemp, Warning, TEXT("[StanceComp] Same weapon type, skipping"));
		return;
	}

	CurrentWeaponType = WeaponTypeTag;
	UE_LOG(LogTemp, Warning, TEXT("[StanceComp] Weapon type SET to: %s"), *CurrentWeaponType.ToString());

	// Reset pose modifiers for new weapon
	TargetAimPoseAlpha = 0.0f;
	GripModifier = 0.0f;
	WeaponLoweredAlpha = 0.0f;

	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			Owner->ForceNetUpdate();
		}
	}

	if (bImmediate)
	{
		AimPoseAlpha = TargetAimPoseAlpha;
		PushToAnimationLayer(/*bSkipIfNoInterface=*/false);
	}
}

void USuspenseCoreWeaponStanceComponent::ClearWeaponStance(bool bImmediate)
{
	SetWeaponStance(FGameplayTag(), bImmediate);
}

void USuspenseCoreWeaponStanceComponent::SetWeaponDrawnState(bool bDrawn)
{
	if (bWeaponDrawn == bDrawn)
	{
		return;
	}

	bWeaponDrawn = bDrawn;

	// Reset aim when holstering
	if (!bDrawn)
	{
		bIsAiming = false;
		TargetAimPoseAlpha = 0.0f;
	}

	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			Owner->ForceNetUpdate();
		}
	}

	// Broadcast EventBus event
	BroadcastCombatStateEvent(bDrawn
		? SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_Drawn
		: SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_Holstered);

	PushToAnimationLayer(/*bSkipIfNoInterface=*/true);
}

// ============================================================================
// Combat State API
// ============================================================================

void USuspenseCoreWeaponStanceComponent::SetAiming(bool bNewAiming)
{
	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] ═══════════════════════════════════════════════════════"));
	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] StanceComponent::SetAiming called with bNewAiming=%s"),
		bNewAiming ? TEXT("TRUE") : TEXT("FALSE"));
	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] Current bIsAiming=%s, TargetAimPoseAlpha=%.2f"),
		bIsAiming ? TEXT("TRUE") : TEXT("FALSE"), TargetAimPoseAlpha);

	if (bIsAiming == bNewAiming)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] SetAiming: Same value, early return (no change)"));
		return;
	}

	bIsAiming = bNewAiming;
	TargetAimPoseAlpha = bNewAiming ? 1.0f : 0.0f;

	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] SetAiming: bIsAiming SET to %s, TargetAimPoseAlpha SET to %.2f"),
		bIsAiming ? TEXT("TRUE") : TEXT("FALSE"), TargetAimPoseAlpha);

	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			Owner->ForceNetUpdate();
			UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] SetAiming: ForceNetUpdate called on %s"), *Owner->GetName());
		}
	}

	// Broadcast EventBus event
	BroadcastCombatStateEvent(bNewAiming
		? SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_AimStarted
		: SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_AimEnded);

	UE_LOG(LogTemp, Warning, TEXT("[ADS DEBUG] SetAiming: EventBus event broadcasted (%s)"),
		bNewAiming ? TEXT("AimStarted") : TEXT("AimEnded"));
}

void USuspenseCoreWeaponStanceComponent::SetFiring(bool bNewFiring)
{
	if (bIsFiring == bNewFiring)
	{
		return;
	}

	bIsFiring = bNewFiring;

	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			Owner->ForceNetUpdate();
		}
	}

	// Broadcast EventBus event
	BroadcastCombatStateEvent(bNewFiring
		? SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_FireStarted
		: SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_FireEnded);
}

void USuspenseCoreWeaponStanceComponent::SetReloading(bool bNewReloading)
{
	if (bIsReloading == bNewReloading)
	{
		return;
	}

	bIsReloading = bNewReloading;

	// Cancel aiming when reloading
	if (bNewReloading && bIsAiming)
	{
		SetAiming(false);
	}

	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			Owner->ForceNetUpdate();
		}
	}

	// Broadcast EventBus event
	BroadcastCombatStateEvent(bNewReloading
		? SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_ReloadStarted
		: SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_ReloadEnded);
}

void USuspenseCoreWeaponStanceComponent::SetHoldingBreath(bool bNewHoldingBreath)
{
	if (bIsHoldingBreath == bNewHoldingBreath)
	{
		return;
	}

	bIsHoldingBreath = bNewHoldingBreath;

	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			Owner->ForceNetUpdate();
		}
	}

	// Broadcast EventBus event
	BroadcastCombatStateEvent(bNewHoldingBreath
		? SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_HoldBreathStarted
		: SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_HoldBreathEnded);
}

void USuspenseCoreWeaponStanceComponent::SetMontageActive(bool bNewMontageActive)
{
	// Montage state is local only, not replicated
	bIsMontageActive = bNewMontageActive;
}

void USuspenseCoreWeaponStanceComponent::SetWeaponBlocked(bool bNewBlocked)
{
	if (bIsWeaponBlocked == bNewBlocked)
	{
		return;
	}

	bIsWeaponBlocked = bNewBlocked;

	// Update BlockDistance for AnimBP (used by Apply Additive alpha)
	// 1.0 = fully blocked, 0.0 = not blocked
	BlockDistance = bNewBlocked ? 1.0f : 0.0f;

	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			Owner->ForceNetUpdate();
		}
	}

	// Broadcast EventBus event
	BroadcastCombatStateEvent(bNewBlocked
		? SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_BlockStarted
		: SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_BlockEnded);
}

// ============================================================================
// Pose Modifier API
// ============================================================================

void USuspenseCoreWeaponStanceComponent::SetTargetAimPose(float TargetAlpha)
{
	TargetAimPoseAlpha = FMath::Clamp(TargetAlpha, 0.0f, 1.0f);
}

void USuspenseCoreWeaponStanceComponent::SetGripModifier(float NewGripModifier)
{
	GripModifier = FMath::Clamp(NewGripModifier, 0.0f, 1.0f);
}

void USuspenseCoreWeaponStanceComponent::SetWeaponLowered(float LoweredAlpha)
{
	WeaponLoweredAlpha = FMath::Clamp(LoweredAlpha, 0.0f, 1.0f);
}

void USuspenseCoreWeaponStanceComponent::SetAimPose(int32 NewAimPose)
{
	AimPose = NewAimPose;
}

void USuspenseCoreWeaponStanceComponent::SetStoredPose(int32 NewStoredPose)
{
	StoredPose = NewStoredPose;
}

void USuspenseCoreWeaponStanceComponent::SetGripID(int32 NewGripID)
{
	GripID = NewGripID;
}

void USuspenseCoreWeaponStanceComponent::SetModifyGrip(bool bNewModifyGrip)
{
	bModifyGrip = bNewModifyGrip;
}

void USuspenseCoreWeaponStanceComponent::SetCreateAimPose(bool bNewCreateAimPose)
{
	bCreateAimPose = bNewCreateAimPose;
}

// ============================================================================
// IK Transform API
// ============================================================================

void USuspenseCoreWeaponStanceComponent::SetAimTransform(const FTransform& NewTransform)
{
	AimTransform = NewTransform;
}

void USuspenseCoreWeaponStanceComponent::SetRightHandTransform(const FTransform& NewTransform)
{
	RightHandTransform = NewTransform;
}

void USuspenseCoreWeaponStanceComponent::SetLeftHandTransform(const FTransform& NewTransform)
{
	LeftHandTransform = NewTransform;
}

void USuspenseCoreWeaponStanceComponent::SetWeaponTransforms(const FTransform& InAimTransform, const FTransform& InRightHand, const FTransform& InLeftHand)
{
	AimTransform = InAimTransform;
	RightHandTransform = InRightHand;
	LeftHandTransform = InLeftHand;
}

// ============================================================================
// Procedural Animation API
// ============================================================================

void USuspenseCoreWeaponStanceComponent::AddRecoil(float RecoilAmount)
{
	RecoilAlpha = FMath::Clamp(RecoilAlpha + RecoilAmount, 0.0f, 1.0f);
}

void USuspenseCoreWeaponStanceComponent::SetSwayMultiplier(float NewMultiplier)
{
	SwayMultiplier = FMath::Max(0.0f, NewMultiplier);
}

void USuspenseCoreWeaponStanceComponent::SetStoredRecoil(float NewStoredRecoil)
{
	StoredRecoil = NewStoredRecoil;
}

void USuspenseCoreWeaponStanceComponent::SetAdditivePitch(float NewAdditivePitch)
{
	AdditivePitch = NewAdditivePitch;
}

void USuspenseCoreWeaponStanceComponent::SetBlockDistance(float NewBlockDistance)
{
	BlockDistance = FMath::Max(0.0f, NewBlockDistance);
}

void USuspenseCoreWeaponStanceComponent::SetSightDistance(float NewSightDistance)
{
	SightDistance = FMath::Max(0.0f, NewSightDistance);
}

// ============================================================================
// Animation System API
// ============================================================================

FSuspenseCoreWeaponStanceSnapshot USuspenseCoreWeaponStanceComponent::GetStanceSnapshot() const
{
	FSuspenseCoreWeaponStanceSnapshot Snapshot;

	// Weapon identity
	Snapshot.WeaponType = CurrentWeaponType;
	Snapshot.bIsDrawn = bWeaponDrawn;

	// Combat states
	Snapshot.bIsAiming = bIsAiming;
	Snapshot.bIsFiring = bIsFiring;
	Snapshot.bIsReloading = bIsReloading;
	Snapshot.bIsHoldingBreath = bIsHoldingBreath;
	Snapshot.bIsWeaponBlocked = bIsWeaponBlocked;
	Snapshot.bIsMontageActive = bIsMontageActive;

	// Legacy compatibility (inverse/derived values)
	Snapshot.bIsHolstered = !bWeaponDrawn;
	Snapshot.bModifyGrip = bModifyGrip;
	Snapshot.bCreateAimPose = bCreateAimPose;

	// Pose indices
	Snapshot.AimPose = AimPose;
	Snapshot.StoredPose = StoredPose;
	Snapshot.GripID = GripID;

	// Pose modifiers
	Snapshot.AimPoseAlpha = AimPoseAlpha;
	Snapshot.GripModifier = GripModifier;
	Snapshot.WeaponLoweredAlpha = WeaponLoweredAlpha;

	// IK Transforms
	Snapshot.AimTransform = AimTransform;
	Snapshot.RightHandTransform = RightHandTransform;
	Snapshot.LeftHandTransform = LeftHandTransform;

	// Procedural animation
	Snapshot.SwayMultiplier = SwayMultiplier;
	Snapshot.RecoilAlpha = RecoilAlpha;
	Snapshot.StoredRecoil = StoredRecoil;
	Snapshot.AdditivePitch = AdditivePitch;
	Snapshot.BlockDistance = BlockDistance;

	// Aim target
	Snapshot.SightDistance = SightDistance;

	return Snapshot;
}

TScriptInterface<ISuspenseCoreWeaponAnimation> USuspenseCoreWeaponStanceComponent::GetAnimationInterface() const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// Cache still valid?
	if (CachedAnimationInterface.GetInterface() &&
		(Now - LastAnimationInterfaceCacheTime) < AnimationInterfaceCacheLifetime)
	{
		return CachedAnimationInterface;
	}

	// Option 1: Owner implements the interface
	if (AActor* OwnerActor = GetOwner())
	{
		if (OwnerActor->GetClass()->ImplementsInterface(USuspenseCoreWeaponAnimation::StaticClass()))
		{
			CachedAnimationInterface.SetObject(OwnerActor);
			CachedAnimationInterface.SetInterface(Cast<ISuspenseCoreWeaponAnimation>(OwnerActor));
			LastAnimationInterfaceCacheTime = Now;
			return CachedAnimationInterface;
		}
	}

	// Option 2: Equipment actor implements the interface
	if (AActor* EquipmentActor = TrackedEquipmentActor.Get())
	{
		if (EquipmentActor->GetClass()->ImplementsInterface(USuspenseCoreWeaponAnimation::StaticClass()))
		{
			CachedAnimationInterface.SetObject(EquipmentActor);
			CachedAnimationInterface.SetInterface(Cast<ISuspenseCoreWeaponAnimation>(EquipmentActor));
			LastAnimationInterfaceCacheTime = Now;
			return CachedAnimationInterface;
		}
	}

	// No interface found
	CachedAnimationInterface = nullptr;
	LastAnimationInterfaceCacheTime = Now;
	return CachedAnimationInterface;
}

// ============================================================================
// Replication Callbacks
// ============================================================================

void USuspenseCoreWeaponStanceComponent::OnRep_WeaponType()
{
	PushToAnimationLayer(/*bSkipIfNoInterface=*/true);
}

void USuspenseCoreWeaponStanceComponent::OnRep_DrawnState()
{
	// Sync local aim state with drawn state
	if (!bWeaponDrawn)
	{
		TargetAimPoseAlpha = 0.0f;
	}

	PushToAnimationLayer(/*bSkipIfNoInterface=*/true);
}

void USuspenseCoreWeaponStanceComponent::OnRep_CombatState()
{
	// ═══════════════════════════════════════════════════════════════════════════════
	// Called when any combat state (bIsAiming, bIsFiring, bIsReloading, bIsHoldingBreath)
	// is replicated from server. Update local interpolated values based on new states.
	//
	// Per documentation: "Visual modifiers (Alpha values) are NOT replicated -
	// they are calculated locally on each client based on replicated bool states."
	// ═══════════════════════════════════════════════════════════════════════════════

	// Update target aim pose alpha based on aiming state
	// This drives the AimPoseAlpha interpolation in TickComponent
	TargetAimPoseAlpha = bIsAiming ? 1.0f : 0.0f;

	// Cancel aiming interpolation immediately when reloading starts
	if (bIsReloading && bIsAiming)
	{
		// Note: Server will handle SetAiming(false), but we can start interpolating locally
		TargetAimPoseAlpha = 0.0f;
	}

	// Notify animation layer of state change (optional - AnimInstance pulls via GetStanceSnapshot)
	PushToAnimationLayer(/*bSkipIfNoInterface=*/true);
}

// ============================================================================
// Internal Methods
// ============================================================================

void USuspenseCoreWeaponStanceComponent::PushToAnimationLayer(bool bSkipIfNoInterface) const
{
	const TScriptInterface<ISuspenseCoreWeaponAnimation> AnimI = GetAnimationInterface();
	if (!AnimI.GetInterface())
	{
		if (!bSkipIfNoInterface)
		{
			// No interface available
		}
		return;
	}

	// The animation interface is a data provider, not a state receiver
	// AnimInstance will call GetStanceSnapshot() to get current state
}

void USuspenseCoreWeaponStanceComponent::UpdateInterpolatedValues(float DeltaTime)
{
	// Interpolate aim pose alpha
	if (!FMath::IsNearlyEqual(AimPoseAlpha, TargetAimPoseAlpha, 0.001f))
	{
		AimPoseAlpha = FMath::FInterpTo(AimPoseAlpha, TargetAimPoseAlpha, DeltaTime, AimInterpSpeed);
	}
	else
	{
		AimPoseAlpha = TargetAimPoseAlpha;
	}

	// Decay recoil over time
	if (RecoilAlpha > 0.0f)
	{
		RecoilAlpha = FMath::FInterpTo(RecoilAlpha, 0.0f, DeltaTime, RecoilRecoverySpeed);
		if (RecoilAlpha < 0.001f)
		{
			RecoilAlpha = 0.0f;
		}
	}
}

// ============================================================================
// EventBus Integration
// ============================================================================

USuspenseCoreEventBus* USuspenseCoreWeaponStanceComponent::GetEventBus() const
{
	// Return cached EventBus if valid
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	// Try to get EventBus from EventManager using static accessor
	if (USuspenseCoreEventManager* EventManager = USuspenseCoreEventManager::Get(this))
	{
		CachedEventBus = EventManager->GetEventBus();
		return CachedEventBus.Get();
	}

	return nullptr;
}

void USuspenseCoreWeaponStanceComponent::BroadcastCombatStateEvent(FGameplayTag EventTag) const
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	// Create event data with owner as source
	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetOwner());

	// Add weapon type tag to Tags container
	if (CurrentWeaponType.IsValid())
	{
		EventData.AddTag(CurrentWeaponType);
	}

	// Store weapon type name as string for easier access
	EventData.SetString(FName("WeaponType"), CurrentWeaponType.IsValid() ? CurrentWeaponType.ToString() : TEXT("None"));

	// Combat states
	EventData.SetBool(FName("IsDrawn"), bWeaponDrawn);
	EventData.SetBool(FName("IsAiming"), bIsAiming);
	EventData.SetBool(FName("IsFiring"), bIsFiring);
	EventData.SetBool(FName("IsReloading"), bIsReloading);
	EventData.SetBool(FName("IsHoldingBreath"), bIsHoldingBreath);
	EventData.SetBool(FName("IsWeaponBlocked"), bIsWeaponBlocked);

	// Publish event
	EventBus->Publish(EventTag, EventData);

	// Also publish generic stance changed event
	EventBus->Publish(SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_Changed, EventData);
}
