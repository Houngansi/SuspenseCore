// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreWeaponAmmoComponent.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentAttributeComponent.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"

USuspenseCoreWeaponAmmoComponent::USuspenseCoreWeaponAmmoComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedComponent(true);

	bIsReloading = false;
	ReloadStartTime = 0.0f;
	bIsTacticalReload = false;
	LinkedAttributeComponent = nullptr;
	CachedWeaponAttributeSet = nullptr;
	CachedAmmoAttributeSet = nullptr;
	CachedMagazineSize = 0.0f;
	bMagazineSizeCached = false;
}

void USuspenseCoreWeaponAmmoComponent::BeginPlay()
{
	Super::BeginPlay();
	SUSPENSECORE_LOG(Log, TEXT("BeginPlay"));
}

void USuspenseCoreWeaponAmmoComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USuspenseCoreWeaponAmmoComponent, AmmoState);
	DOREPLIFETIME(USuspenseCoreWeaponAmmoComponent, bIsReloading);
	DOREPLIFETIME(USuspenseCoreWeaponAmmoComponent, ReloadStartTime);
	DOREPLIFETIME(USuspenseCoreWeaponAmmoComponent, bIsTacticalReload);
}

void USuspenseCoreWeaponAmmoComponent::Cleanup()
{
	SUSPENSECORE_LOG(Log, TEXT("Cleanup"));

	// Cancel any active reload
	if (bIsReloading)
	{
		CancelReload();
	}

	// Clear cached references
	CachedWeaponInterface = nullptr;
	LinkedAttributeComponent = nullptr;
	CachedWeaponAttributeSet = nullptr;
	CachedAmmoAttributeSet = nullptr;

	Super::Cleanup();
}

bool USuspenseCoreWeaponAmmoComponent::InitializeFromWeapon(TScriptInterface<ISuspenseWeapon> WeaponInterface)
{
	if (!WeaponInterface)
	{
		SUSPENSECORE_LOG(Error, TEXT("InitializeFromWeapon: Invalid weapon interface"));
		return false;
	}

	CachedWeaponInterface = WeaponInterface;
	SUSPENSECORE_LOG(Log, TEXT("InitializeFromWeapon: Success"));

	// Get initial ammo state from weapon data
	FSuspenseUnifiedItemData WeaponData;
	if (GetWeaponData(WeaponData))
	{
		UpdateMagazineSizeFromAttributes();
	}

	return true;
}

bool USuspenseCoreWeaponAmmoComponent::ConsumeAmmo(float Amount)
{
	if (!HasAmmo() || Amount <= 0.0f)
	{
		return false;
	}

	if (AmmoState.CurrentAmmo < Amount)
	{
		return false;
	}

	AmmoState.CurrentAmmo -= Amount;
	BroadcastAmmoChanged();
	SaveAmmoStateToWeapon();

	SUSPENSECORE_LOG(Verbose, TEXT("ConsumeAmmo: %f (Remaining: %f)"), Amount, AmmoState.CurrentAmmo);
	return true;
}

float USuspenseCoreWeaponAmmoComponent::AddAmmo(float Amount)
{
	if (Amount <= 0.0f)
	{
		return 0.0f;
	}

	float OldReserve = AmmoState.RemainingAmmo;
	AmmoState.RemainingAmmo += Amount;

	float ActualAdded = AmmoState.RemainingAmmo - OldReserve;
	BroadcastAmmoChanged();
	SaveAmmoStateToWeapon();

	SUSPENSECORE_LOG(Log, TEXT("AddAmmo: %f (Total Reserve: %f)"), ActualAdded, AmmoState.RemainingAmmo);
	return ActualAdded;
}

bool USuspenseCoreWeaponAmmoComponent::StartReload(bool bForce)
{
	if (!CanReload() && !bForce)
	{
		SUSPENSECORE_LOG(Warning, TEXT("StartReload: Cannot reload"));
		return false;
	}

	if (bIsReloading)
	{
		SUSPENSECORE_LOG(Warning, TEXT("StartReload: Already reloading"));
		return false;
	}

	bIsReloading = true;
	bIsTacticalReload = AmmoState.CurrentAmmo > 0.0f;
	ReloadStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	float ReloadDuration = GetReloadTime(bIsTacticalReload);
	BroadcastWeaponReload(true, ReloadDuration);

	SUSPENSECORE_LOG(Log, TEXT("StartReload: Tactical=%d, Duration=%f"), bIsTacticalReload, ReloadDuration);

	// Apply reload effect if available
	ApplyReloadEffect();

	return true;
}

void USuspenseCoreWeaponAmmoComponent::CompleteReload()
{
	if (!bIsReloading)
	{
		SUSPENSECORE_LOG(Warning, TEXT("CompleteReload: Not reloading"));
		return;
	}

	float MagSize = GetMagazineSize();
	float AmmoNeeded = MagSize - AmmoState.CurrentAmmo;
	float AmmoToTransfer = FMath::Min(AmmoNeeded, AmmoState.RemainingAmmo);

	AmmoState.CurrentAmmo += AmmoToTransfer;
	AmmoState.RemainingAmmo -= AmmoToTransfer;

	bIsReloading = false;
	BroadcastWeaponReload(false, 0.0f);
	BroadcastAmmoChanged();
	SaveAmmoStateToWeapon();

	SUSPENSECORE_LOG(Log, TEXT("CompleteReload: Transferred %f ammo (Current: %f, Reserve: %f)"),
		AmmoToTransfer, AmmoState.CurrentAmmo, AmmoState.RemainingAmmo);
}

void USuspenseCoreWeaponAmmoComponent::CancelReload()
{
	if (!bIsReloading)
	{
		return;
	}

	bIsReloading = false;
	BroadcastWeaponReload(false, 0.0f);

	SUSPENSECORE_LOG(Log, TEXT("CancelReload"));
}

void USuspenseCoreWeaponAmmoComponent::SetAmmoState(const FSuspenseInventoryAmmoState& NewState)
{
	UpdateInternalAmmoState(NewState);
	BroadcastAmmoChanged();

	SUSPENSECORE_LOG(Log, TEXT("SetAmmoState: Current=%f, Reserve=%f"),
		AmmoState.CurrentAmmo, AmmoState.RemainingAmmo);
}

bool USuspenseCoreWeaponAmmoComponent::CanReload() const
{
	if (bIsReloading)
	{
		return false;
	}

	if (IsMagazineFull())
	{
		return false;
	}

	if (AmmoState.RemainingAmmo <= 0.0f)
	{
		return false;
	}

	return true;
}

bool USuspenseCoreWeaponAmmoComponent::HasAmmo() const
{
	return AmmoState.CurrentAmmo > 0.0f;
}

bool USuspenseCoreWeaponAmmoComponent::IsMagazineFull() const
{
	float MagSize = GetMagazineSize();
	return AmmoState.CurrentAmmo >= MagSize;
}

float USuspenseCoreWeaponAmmoComponent::GetMagazineSize() const
{
	if (bMagazineSizeCached)
	{
		return CachedMagazineSize;
	}

	// Try to get from AttributeSet
	if (CachedWeaponAttributeSet)
	{
		// Implementation stub - get from attribute set
		CachedMagazineSize = 30.0f; // Default value
		bMagazineSizeCached = true;
		return CachedMagazineSize;
	}

	// Fallback to item data
	FSuspenseUnifiedItemData WeaponData;
	if (GetWeaponData(WeaponData))
	{
		// Implementation stub - get from weapon data
		CachedMagazineSize = 30.0f; // Default value
		bMagazineSizeCached = true;
		return CachedMagazineSize;
	}

	return 30.0f; // Default fallback
}

float USuspenseCoreWeaponAmmoComponent::GetReloadTime(bool bTactical) const
{
	// Implementation stub - get from AttributeSet or item data
	return bTactical ? 2.0f : 2.5f;
}

FGameplayTag USuspenseCoreWeaponAmmoComponent::GetAmmoType() const
{
	// Implementation stub - get from weapon data
	return FGameplayTag();
}

void USuspenseCoreWeaponAmmoComponent::LinkAttributeComponent(USuspenseCoreEquipmentAttributeComponent* AttributeComponent)
{
	if (AttributeComponent)
	{
		LinkedAttributeComponent = AttributeComponent;
		CachedWeaponAttributeSet = Cast<UWeaponAttributeSet>(AttributeComponent->GetWeaponAttributeSet());
		CachedAmmoAttributeSet = Cast<UAmmoAttributeSet>(AttributeComponent->GetAmmoAttributeSet());

		SUSPENSECORE_LOG(Log, TEXT("LinkAttributeComponent: Linked"));
		UpdateMagazineSizeFromAttributes();
	}
}

UWeaponAttributeSet* USuspenseCoreWeaponAmmoComponent::GetWeaponAttributeSet() const
{
	return CachedWeaponAttributeSet;
}

UAmmoAttributeSet* USuspenseCoreWeaponAmmoComponent::GetAmmoAttributeSet() const
{
	return CachedAmmoAttributeSet;
}

ISuspenseWeapon* USuspenseCoreWeaponAmmoComponent::GetWeaponInterface() const
{
	if (CachedWeaponInterface)
	{
		return Cast<ISuspenseWeapon>(CachedWeaponInterface.GetObject());
	}
	return nullptr;
}

bool USuspenseCoreWeaponAmmoComponent::GetWeaponData(FSuspenseUnifiedItemData& OutData) const
{
	return GetEquippedItemData(OutData);
}

void USuspenseCoreWeaponAmmoComponent::BroadcastAmmoChanged()
{
	BroadcastAmmoChanged(AmmoState.CurrentAmmo, AmmoState.RemainingAmmo, GetMagazineSize());
}

void USuspenseCoreWeaponAmmoComponent::ApplyReloadEffect()
{
	if (!CachedASC)
	{
		return;
	}

	// Implementation stub - apply reload gameplay effect
	SUSPENSECORE_LOG(Verbose, TEXT("ApplyReloadEffect"));
}

void USuspenseCoreWeaponAmmoComponent::UpdateMagazineSizeFromAttributes()
{
	bMagazineSizeCached = false;
	GetMagazineSize(); // Force recalculation
}

void USuspenseCoreWeaponAmmoComponent::ApplyDurabilityModifiers()
{
	// Implementation stub - apply durability modifiers to ammo operations
}

void USuspenseCoreWeaponAmmoComponent::SaveAmmoStateToWeapon()
{
	// Implementation stub - save ammo state to weapon item instance
	SUSPENSECORE_LOG(Verbose, TEXT("SaveAmmoStateToWeapon"));
}

bool USuspenseCoreWeaponAmmoComponent::ServerStartReload_Validate(bool bForce)
{
	return true;
}

void USuspenseCoreWeaponAmmoComponent::ServerStartReload_Implementation(bool bForce)
{
	StartReload(bForce);
}

bool USuspenseCoreWeaponAmmoComponent::ServerCompleteReload_Validate()
{
	return true;
}

void USuspenseCoreWeaponAmmoComponent::ServerCompleteReload_Implementation()
{
	CompleteReload();
}

void USuspenseCoreWeaponAmmoComponent::OnRep_AmmoState()
{
	SUSPENSECORE_LOG(Verbose, TEXT("OnRep_AmmoState: Current=%f, Reserve=%f"),
		AmmoState.CurrentAmmo, AmmoState.RemainingAmmo);

	BroadcastAmmoChanged();
}

void USuspenseCoreWeaponAmmoComponent::OnRep_ReloadState()
{
	SUSPENSECORE_LOG(Verbose, TEXT("OnRep_ReloadState: IsReloading=%d"), bIsReloading);

	if (bIsReloading)
	{
		float ReloadDuration = GetReloadTime(bIsTacticalReload);
		BroadcastWeaponReload(true, ReloadDuration);
	}
	else
	{
		BroadcastWeaponReload(false, 0.0f);
	}
}

void USuspenseCoreWeaponAmmoComponent::UpdateInternalAmmoState(const FSuspenseInventoryAmmoState& NewState)
{
	AmmoState = NewState;
}
