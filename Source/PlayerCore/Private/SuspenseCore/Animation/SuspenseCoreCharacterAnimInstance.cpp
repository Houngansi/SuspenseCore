// SuspenseCoreCharacterAnimInstance.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Animation/SuspenseCoreCharacterAnimInstance.h"
#include "SuspenseCore/Characters/SuspenseCoreCharacter.h"

#if WITH_EQUIPMENT_SYSTEM
#include "SuspenseCore/Components/SuspenseCoreWeaponStanceComponent.h"
#endif

USuspenseCoreCharacterAnimInstance::USuspenseCoreCharacterAnimInstance()
{
}

void USuspenseCoreCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	// Cache character reference
	APawn* OwnerPawn = TryGetPawnOwner();
	if (OwnerPawn)
	{
		CachedCharacter = Cast<ASuspenseCoreCharacter>(OwnerPawn);

#if WITH_EQUIPMENT_SYSTEM
		CachedStanceComponent = OwnerPawn->FindComponentByClass<USuspenseCoreWeaponStanceComponent>();
#endif
	}
}

void USuspenseCoreCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!TryGetPawnOwner())
	{
		return;
	}

	// Update weapon data from StanceComponent
	UpdateWeaponData();
}

void USuspenseCoreCharacterAnimInstance::UpdateWeaponData()
{
#if WITH_EQUIPMENT_SYSTEM
	if (!CachedStanceComponent.IsValid())
	{
		// No stance component - reset weapon data
		CurrentWeaponType = FGameplayTag();
		bHasWeaponEquipped = false;
		bIsWeaponDrawn = false;
		bIsHolstered = true;
		bIsAiming = false;
		bIsFiring = false;
		bIsReloading = false;
		bIsHoldingBreath = false;
		bIsWeaponMontageActive = false;
		bModifyGrip = false;
		bCreateAimPose = false;
		GripID = 0;
		AimPose = 0;
		StoredPose = 0;
		AimingAlpha = 0.0f;
		GripModifier = 0.0f;
		WeaponLoweredAlpha = 0.0f;
		RecoilAlpha = 0.0f;
		StoredRecoil = 0.0f;
		AdditivePitch = 0.0f;
		BlockDistance = 0.0f;
		SightDistance = 200.0f;
		return;
	}

	USuspenseCoreWeaponStanceComponent* StanceComp = CachedStanceComponent.Get();

	// Get stance snapshot from component
	const FSuspenseCoreWeaponStanceSnapshot Snapshot = StanceComp->GetStanceSnapshot();

	// Weapon identity
	CurrentWeaponType = Snapshot.WeaponType;
	bHasWeaponEquipped = CurrentWeaponType.IsValid();
	bIsWeaponDrawn = Snapshot.bIsDrawn;

	// Combat states
	bIsAiming = Snapshot.bIsAiming;
	bIsFiring = Snapshot.bIsFiring;
	bIsReloading = Snapshot.bIsReloading;
	bIsHoldingBreath = Snapshot.bIsHoldingBreath;
	bIsWeaponMontageActive = Snapshot.bIsMontageActive;

	// Compatibility flags
	bIsHolstered = Snapshot.bIsHolstered;
	bModifyGrip = Snapshot.bModifyGrip;
	bCreateAimPose = Snapshot.bCreateAimPose;

	// Pose indices
	AimPose = Snapshot.AimPose;
	StoredPose = Snapshot.StoredPose;
	GripID = Snapshot.GripID;

	// Pose modifiers
	AimingAlpha = Snapshot.AimPoseAlpha;
	GripModifier = Snapshot.GripModifier;
	WeaponLoweredAlpha = Snapshot.WeaponLoweredAlpha;

	// Procedural animation
	RecoilAlpha = Snapshot.RecoilAlpha;
	StoredRecoil = Snapshot.StoredRecoil;
	AdditivePitch = Snapshot.AdditivePitch;
	BlockDistance = Snapshot.BlockDistance;

	// Aim target
	SightDistance = Snapshot.SightDistance;

#else
	// Equipment system disabled - use character's basic weapon flag
	if (CachedCharacter.IsValid())
	{
		bHasWeaponEquipped = CachedCharacter->HasWeapon();
		bIsWeaponDrawn = bHasWeaponEquipped;
	}
#endif
}

ASuspenseCoreCharacter* USuspenseCoreCharacterAnimInstance::GetSuspenseCoreCharacter() const
{
	return CachedCharacter.Get();
}

FName USuspenseCoreCharacterAnimInstance::GetLegacyRowNameFromArchetype() const
{
	return GetLegacyRowNameFromArchetypeTag(CurrentWeaponType);
}

FName USuspenseCoreCharacterAnimInstance::GetLegacyRowNameFromArchetypeTag(const FGameplayTag& WeaponArchetype)
{
	if (!WeaponArchetype.IsValid())
	{
		return FName("SMG"); // Default
	}

	const FString TagString = WeaponArchetype.ToString();

	// Sniper check first (more specific)
	if (TagString.Contains(TEXT("Sniper")))
	{
		return FName("Sniper");
	}

	// Rifle (Assault, DMR) -> SMG (legacy naming)
	if (TagString.Contains(TEXT("Weapon.Rifle")))
	{
		return FName("SMG");
	}

	// SMG
	if (TagString.Contains(TEXT("Weapon.SMG")))
	{
		return FName("SMG");
	}

	// Pistol
	if (TagString.Contains(TEXT("Weapon.Pistol")))
	{
		return FName("Pistol");
	}

	// Shotgun
	if (TagString.Contains(TEXT("Weapon.Shotgun")))
	{
		return FName("Shotgun");
	}

	// Melee Knife
	if (TagString.Contains(TEXT("Weapon.Melee.Knife")))
	{
		return FName("Knife");
	}

	// Melee Blunt -> Special
	if (TagString.Contains(TEXT("Weapon.Melee")))
	{
		return FName("Special");
	}

	// Heavy -> Special
	if (TagString.Contains(TEXT("Weapon.Heavy")))
	{
		return FName("Special");
	}

	// Throwable -> Frag
	if (TagString.Contains(TEXT("Weapon.Throwable")))
	{
		return FName("Frag");
	}

	// Default fallback
	return FName("SMG");
}
