// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreWeaponStanceComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

USuspenseCoreWeaponStanceComponent::USuspenseCoreWeaponStanceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedComponent(true);

	bWeaponDrawn = false;
	AnimationInterfaceCacheLifetime = 0.25f;
	LastAnimationInterfaceCacheTime = -1000.0f;
}

void USuspenseCoreWeaponStanceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USuspenseCoreWeaponStanceComponent, CurrentWeaponType);
	DOREPLIFETIME(USuspenseCoreWeaponStanceComponent, bWeaponDrawn);
}

void USuspenseCoreWeaponStanceComponent::OnEquipmentChanged(AActor* NewEquipmentActor)
{
	TrackedEquipmentActor = NewEquipmentActor;

	// Clear cached animation interface when equipment changes
	CachedAnimationInterface = nullptr;
	LastAnimationInterfaceCacheTime = -1000.0f;

	UE_LOG(LogTemp, Log, TEXT("USuspenseCoreWeaponStanceComponent::OnEquipmentChanged: %s"),
		*GetNameSafe(NewEquipmentActor));
}

void USuspenseCoreWeaponStanceComponent::SetWeaponStance(const FGameplayTag& WeaponTypeTag, bool bImmediate)
{
	if (CurrentWeaponType == WeaponTypeTag)
	{
		return; // Already in this stance
	}

	CurrentWeaponType = WeaponTypeTag;
	PushToAnimationLayer(!bImmediate);

	UE_LOG(LogTemp, Log, TEXT("USuspenseCoreWeaponStanceComponent::SetWeaponStance: %s (Immediate: %d)"),
		*WeaponTypeTag.ToString(), bImmediate);
}

void USuspenseCoreWeaponStanceComponent::ClearWeaponStance(bool bImmediate)
{
	if (!CurrentWeaponType.IsValid())
	{
		return; // Already cleared
	}

	CurrentWeaponType = FGameplayTag();
	bWeaponDrawn = false;
	PushToAnimationLayer(!bImmediate);

	UE_LOG(LogTemp, Log, TEXT("USuspenseCoreWeaponStanceComponent::ClearWeaponStance (Immediate: %d)"),
		bImmediate);
}

void USuspenseCoreWeaponStanceComponent::SetWeaponDrawnState(bool bDrawn)
{
	if (bWeaponDrawn == bDrawn)
	{
		return; // Already in this state
	}

	bWeaponDrawn = bDrawn;
	PushToAnimationLayer(true);

	UE_LOG(LogTemp, Log, TEXT("USuspenseCoreWeaponStanceComponent::SetWeaponDrawnState: %d"), bDrawn);
}

TScriptInterface<ISuspenseWeaponAnimation> USuspenseCoreWeaponStanceComponent::GetAnimationInterface() const
{
	// Check if cache is valid
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float TimeSinceCache = CurrentTime - LastAnimationInterfaceCacheTime;

	if (CachedAnimationInterface && TimeSinceCache < AnimationInterfaceCacheLifetime)
	{
		return CachedAnimationInterface;
	}

	// Cache expired or invalid, refresh
	CachedAnimationInterface = nullptr;

	AActor* EquipmentActor = TrackedEquipmentActor.Get();
	if (!EquipmentActor)
	{
		// Try owner as fallback
		EquipmentActor = GetOwner();
	}

	if (EquipmentActor)
	{
		// Try to get animation interface from equipment actor
		if (EquipmentActor->Implements<USuspenseWeaponAnimation>())
		{
			CachedAnimationInterface = EquipmentActor;
			LastAnimationInterfaceCacheTime = CurrentTime;
		}
	}

	return CachedAnimationInterface;
}

void USuspenseCoreWeaponStanceComponent::OnRep_WeaponType()
{
	UE_LOG(LogTemp, Verbose, TEXT("USuspenseCoreWeaponStanceComponent::OnRep_WeaponType: %s"),
		*CurrentWeaponType.ToString());

	PushToAnimationLayer(true);
}

void USuspenseCoreWeaponStanceComponent::OnRep_DrawnState()
{
	UE_LOG(LogTemp, Verbose, TEXT("USuspenseCoreWeaponStanceComponent::OnRep_DrawnState: %d"),
		bWeaponDrawn);

	PushToAnimationLayer(true);
}

void USuspenseCoreWeaponStanceComponent::PushToAnimationLayer(bool bSkipIfNoInterface) const
{
	TScriptInterface<ISuspenseWeaponAnimation> AnimInterface = GetAnimationInterface();

	if (!AnimInterface && bSkipIfNoInterface)
	{
		// No interface available, skip silently
		return;
	}

	if (AnimInterface)
	{
		// Implementation stub - push stance to animation layer
		// This would call methods on the animation interface to update the animation state
		UE_LOG(LogTemp, Verbose, TEXT("USuspenseCoreWeaponStanceComponent::PushToAnimationLayer: Type=%s, Drawn=%d"),
			*CurrentWeaponType.ToString(), bWeaponDrawn);
	}
}
