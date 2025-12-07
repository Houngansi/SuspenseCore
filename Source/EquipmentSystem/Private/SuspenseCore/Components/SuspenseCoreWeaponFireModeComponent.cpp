// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreWeaponFireModeComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbility.h"
#include "Net/UnrealNetwork.h"

USuspenseCoreWeaponFireModeComponent::USuspenseCoreWeaponFireModeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedComponent(true);

	CurrentFireModeIndex = 0;
	bIsSwitching = false;
}

void USuspenseCoreWeaponFireModeComponent::BeginPlay()
{
	Super::BeginPlay();
	SUSPENSECORE_LOG(Log, TEXT("BeginPlay"));
}

void USuspenseCoreWeaponFireModeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USuspenseCoreWeaponFireModeComponent, CurrentFireModeIndex);
}

void USuspenseCoreWeaponFireModeComponent::Cleanup()
{
	SUSPENSECORE_LOG(Log, TEXT("Cleanup"));

	RemoveFireModeAbilities();
	ClearFireModes_Implementation();

	Super::Cleanup();
}

bool USuspenseCoreWeaponFireModeComponent::InitializeFromWeapon(TScriptInterface<ISuspenseWeapon> WeaponInterface)
{
	if (!WeaponInterface)
	{
		SUSPENSECORE_LOG(Error, TEXT("InitializeFromWeapon: Invalid weapon interface"));
		return false;
	}

	CachedWeaponInterface = WeaponInterface;

	FSuspenseUnifiedItemData WeaponData;
	if (!GetWeaponData(WeaponData))
	{
		SUSPENSECORE_LOG(Error, TEXT("InitializeFromWeapon: Failed to get weapon data"));
		return false;
	}

	LoadFireModesFromData(WeaponData);
	GrantFireModeAbilities();

	SUSPENSECORE_LOG(Log, TEXT("InitializeFromWeapon: Success, %d fire modes loaded"), FireModes.Num());
	return true;
}

bool USuspenseCoreWeaponFireModeComponent::InitializeFromWeaponData_Implementation(const FSuspenseUnifiedItemData& WeaponData)
{
	LoadFireModesFromData(WeaponData);
	GrantFireModeAbilities();
	return FireModes.Num() > 0;
}

void USuspenseCoreWeaponFireModeComponent::ClearFireModes_Implementation()
{
	RemoveFireModeAbilities();
	FireModes.Empty();
	BlockedFireModes.Empty();
	CurrentFireModeIndex = 0;

	SUSPENSECORE_LOG(Log, TEXT("ClearFireModes"));
}

bool USuspenseCoreWeaponFireModeComponent::CycleToNextFireMode_Implementation()
{
	if (FireModes.Num() <= 1 || bIsSwitching)
	{
		return false;
	}

	bIsSwitching = true;

	int32 NextIndex = (CurrentFireModeIndex + 1) % FireModes.Num();
	bool bSuccess = SetFireModeByIndex_Implementation(NextIndex);

	bIsSwitching = false;

	SUSPENSECORE_LOG(Log, TEXT("CycleToNextFireMode: %d -> %d"), CurrentFireModeIndex, NextIndex);
	return bSuccess;
}

bool USuspenseCoreWeaponFireModeComponent::CycleToPreviousFireMode_Implementation()
{
	if (FireModes.Num() <= 1 || bIsSwitching)
	{
		return false;
	}

	bIsSwitching = true;

	int32 PrevIndex = (CurrentFireModeIndex - 1 + FireModes.Num()) % FireModes.Num();
	bool bSuccess = SetFireModeByIndex_Implementation(PrevIndex);

	bIsSwitching = false;

	SUSPENSECORE_LOG(Log, TEXT("CycleToPreviousFireMode: %d -> %d"), CurrentFireModeIndex, PrevIndex);
	return bSuccess;
}

bool USuspenseCoreWeaponFireModeComponent::SetFireMode_Implementation(const FGameplayTag& FireModeTag)
{
	int32 Index = FindFireModeIndex(FireModeTag);
	if (Index == INDEX_NONE)
	{
		SUSPENSECORE_LOG(Warning, TEXT("SetFireMode: Fire mode not found: %s"), *FireModeTag.ToString());
		return false;
	}

	return SetFireModeByIndex_Implementation(Index);
}

bool USuspenseCoreWeaponFireModeComponent::SetFireModeByIndex_Implementation(int32 Index)
{
	if (!FireModes.IsValidIndex(Index))
	{
		SUSPENSECORE_LOG(Error, TEXT("SetFireModeByIndex: Invalid index: %d"), Index);
		return false;
	}

	const FFireModeRuntimeData& NewMode = FireModes[Index];
	if (IsFireModeBlocked_Implementation(NewMode.FireModeTag))
	{
		SUSPENSECORE_LOG(Warning, TEXT("SetFireModeByIndex: Fire mode blocked: %s"), *NewMode.FireModeTag.ToString());
		return false;
	}

	CurrentFireModeIndex = Index;
	BroadcastFireModeChanged();

	SUSPENSECORE_LOG(Log, TEXT("SetFireModeByIndex: %d (%s)"), Index, *NewMode.FireModeTag.ToString());
	return true;
}

FGameplayTag USuspenseCoreWeaponFireModeComponent::GetCurrentFireMode_Implementation() const
{
	if (FireModes.IsValidIndex(CurrentFireModeIndex))
	{
		return FireModes[CurrentFireModeIndex].FireModeTag;
	}
	return FGameplayTag();
}

FFireModeRuntimeData USuspenseCoreWeaponFireModeComponent::GetCurrentFireModeData_Implementation() const
{
	if (FireModes.IsValidIndex(CurrentFireModeIndex))
	{
		return FireModes[CurrentFireModeIndex];
	}
	return FFireModeRuntimeData();
}

bool USuspenseCoreWeaponFireModeComponent::IsFireModeAvailable_Implementation(const FGameplayTag& FireModeTag) const
{
	int32 Index = FindFireModeIndex(FireModeTag);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	return !IsFireModeBlocked_Implementation(FireModeTag);
}

TArray<FFireModeRuntimeData> USuspenseCoreWeaponFireModeComponent::GetAllFireModes_Implementation() const
{
	return FireModes;
}

TArray<FGameplayTag> USuspenseCoreWeaponFireModeComponent::GetAvailableFireModes_Implementation() const
{
	TArray<FGameplayTag> AvailableModes;
	for (const FFireModeRuntimeData& Mode : FireModes)
	{
		if (!IsFireModeBlocked_Implementation(Mode.FireModeTag))
		{
			AvailableModes.Add(Mode.FireModeTag);
		}
	}
	return AvailableModes;
}

int32 USuspenseCoreWeaponFireModeComponent::GetAvailableFireModeCount_Implementation() const
{
	return GetAvailableFireModes_Implementation().Num();
}

bool USuspenseCoreWeaponFireModeComponent::SetFireModeEnabled_Implementation(const FGameplayTag& FireModeTag, bool bEnabled)
{
	// Implementation stub - enable/disable specific fire mode
	SUSPENSECORE_LOG(Log, TEXT("SetFireModeEnabled: %s = %d"), *FireModeTag.ToString(), bEnabled);
	return true;
}

void USuspenseCoreWeaponFireModeComponent::SetFireModeBlocked_Implementation(const FGameplayTag& FireModeTag, bool bBlocked)
{
	if (bBlocked)
	{
		BlockedFireModes.Add(FireModeTag);
	}
	else
	{
		BlockedFireModes.Remove(FireModeTag);
	}

	SUSPENSECORE_LOG(Log, TEXT("SetFireModeBlocked: %s = %d"), *FireModeTag.ToString(), bBlocked);
}

bool USuspenseCoreWeaponFireModeComponent::IsFireModeBlocked_Implementation(const FGameplayTag& FireModeTag) const
{
	return BlockedFireModes.Contains(FireModeTag);
}

bool USuspenseCoreWeaponFireModeComponent::GetFireModeData_Implementation(const FGameplayTag& FireModeTag, FFireModeRuntimeData& OutData) const
{
	int32 Index = FindFireModeIndex(FireModeTag);
	if (Index == INDEX_NONE)
	{
		return false;
	}

	OutData = FireModes[Index];
	return true;
}

TSubclassOf<UGameplayAbility> USuspenseCoreWeaponFireModeComponent::GetFireModeAbility_Implementation(const FGameplayTag& FireModeTag) const
{
	FFireModeRuntimeData ModeData;
	if (GetFireModeData_Implementation(FireModeTag, ModeData))
	{
		return ModeData.AbilityClass;
	}
	return nullptr;
}

int32 USuspenseCoreWeaponFireModeComponent::GetFireModeInputID_Implementation(const FGameplayTag& FireModeTag) const
{
	FFireModeRuntimeData ModeData;
	if (GetFireModeData_Implementation(FireModeTag, ModeData))
	{
		return ModeData.InputID;
	}
	return INDEX_NONE;
}

USuspenseEventManager* USuspenseCoreWeaponFireModeComponent::GetDelegateManager() const
{
	return USuspenseCoreEquipmentComponentBase::GetDelegateManager();
}

ISuspenseWeapon* USuspenseCoreWeaponFireModeComponent::GetWeaponInterface() const
{
	if (CachedWeaponInterface)
	{
		return Cast<ISuspenseWeapon>(CachedWeaponInterface.GetObject());
	}
	return nullptr;
}

bool USuspenseCoreWeaponFireModeComponent::GetWeaponData(FSuspenseUnifiedItemData& OutData) const
{
	return GetEquippedItemData(OutData);
}

void USuspenseCoreWeaponFireModeComponent::LoadFireModesFromData(const FSuspenseUnifiedItemData& WeaponData)
{
	FireModes.Empty();

	// Implementation stub - load fire modes from weapon data
	SUSPENSECORE_LOG(Log, TEXT("LoadFireModesFromData"));

	// Example: Add a default fire mode
	// This would normally come from WeaponData.FireModes or similar
	/*
	FFireModeRuntimeData DefaultMode;
	DefaultMode.FireModeTag = FGameplayTag::RequestGameplayTag(FName("Weapon.FireMode.Single"));
	DefaultMode.DisplayName = FText::FromString("Single");
	DefaultMode.bIsEnabled = true;
	FireModes.Add(DefaultMode);
	*/

	CurrentFireModeIndex = 0;
}

void USuspenseCoreWeaponFireModeComponent::GrantFireModeAbilities()
{
	if (!CachedASC)
	{
		SUSPENSECORE_LOG(Warning, TEXT("GrantFireModeAbilities: No ASC"));
		return;
	}

	RemoveFireModeAbilities();

	for (const FFireModeRuntimeData& Mode : FireModes)
	{
		if (!Mode.AbilityClass)
		{
			continue;
		}

		FGameplayAbilitySpecHandle Handle = GrantAbility_Implementation(Mode.AbilityClass, 1, Mode.InputID);
		if (Handle.IsValid())
		{
			AbilityHandles.Add(Mode.FireModeTag, Handle);
			SUSPENSECORE_LOG(Log, TEXT("GrantFireModeAbilities: Granted %s"), *Mode.FireModeTag.ToString());
		}
	}
}

void USuspenseCoreWeaponFireModeComponent::RemoveFireModeAbilities()
{
	if (!CachedASC)
	{
		return;
	}

	for (const auto& Pair : AbilityHandles)
	{
		if (Pair.Value.IsValid())
		{
			RemoveAbility_Implementation(Pair.Value);
			SUSPENSECORE_LOG(Verbose, TEXT("RemoveFireModeAbilities: Removed %s"), *Pair.Key.ToString());
		}
	}

	AbilityHandles.Empty();
}

int32 USuspenseCoreWeaponFireModeComponent::FindFireModeIndex(const FGameplayTag& FireModeTag) const
{
	for (int32 i = 0; i < FireModes.Num(); ++i)
	{
		if (FireModes[i].FireModeTag == FireModeTag)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

void USuspenseCoreWeaponFireModeComponent::BroadcastFireModeChanged()
{
	if (FireModes.IsValidIndex(CurrentFireModeIndex))
	{
		const FFireModeRuntimeData& CurrentMode = FireModes[CurrentFireModeIndex];
		BroadcastFireModeChanged(CurrentMode.FireModeTag, CurrentMode.DisplayName);
	}
}

void USuspenseCoreWeaponFireModeComponent::OnRep_CurrentFireModeIndex()
{
	SUSPENSECORE_LOG(Verbose, TEXT("OnRep_CurrentFireModeIndex: %d"), CurrentFireModeIndex);
	BroadcastFireModeChanged();
}
