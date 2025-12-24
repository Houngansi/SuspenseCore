// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreWeaponStanceComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponAnimation.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"

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

	// Weapon identity
	DOREPLIFETIME(USuspenseCoreWeaponStanceComponent, CurrentWeaponType);
	DOREPLIFETIME(USuspenseCoreWeaponStanceComponent, bWeaponDrawn);

	// Combat states
	DOREPLIFETIME(USuspenseCoreWeaponStanceComponent, bIsAiming);
	DOREPLIFETIME(USuspenseCoreWeaponStanceComponent, bIsFiring);
	DOREPLIFETIME(USuspenseCoreWeaponStanceComponent, bIsReloading);
	DOREPLIFETIME(USuspenseCoreWeaponStanceComponent, bIsHoldingBreath);
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

	PushToAnimationLayer(/*bSkipIfNoInterface=*/true);
}

void USuspenseCoreWeaponStanceComponent::SetWeaponStance(const FGameplayTag& WeaponTypeTag, bool bImmediate)
{
	if (CurrentWeaponType == WeaponTypeTag)
	{
		return;
	}

	CurrentWeaponType = WeaponTypeTag;

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
	if (bIsAiming == bNewAiming)
	{
		return;
	}

	bIsAiming = bNewAiming;
	TargetAimPoseAlpha = bNewAiming ? 1.0f : 0.0f;

	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			Owner->ForceNetUpdate();
		}
	}

	// Broadcast EventBus event
	BroadcastCombatStateEvent(bNewAiming
		? SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_AimStarted
		: SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_AimEnded);
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
	Snapshot.bIsMontageActive = bIsMontageActive;

	// Pose modifiers
	Snapshot.AimPoseAlpha = AimPoseAlpha;
	Snapshot.GripModifier = GripModifier;
	Snapshot.WeaponLoweredAlpha = WeaponLoweredAlpha;

	// Procedural
	Snapshot.SwayMultiplier = SwayMultiplier;
	Snapshot.RecoilAlpha = RecoilAlpha;

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
	// Update target aim pose based on replicated aiming state
	TargetAimPoseAlpha = bIsAiming ? 1.0f : 0.0f;

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

	// Publish event
	EventBus->Publish(EventTag, EventData);

	// Also publish generic stance changed event
	EventBus->Publish(SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_Weapon_Stance_Changed, EventData);
}
