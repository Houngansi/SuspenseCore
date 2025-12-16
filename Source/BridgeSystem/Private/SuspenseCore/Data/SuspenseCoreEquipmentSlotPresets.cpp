// SuspenseCoreEquipmentSlotPresets.cpp
// SuspenseCore - Equipment Slot Presets DataAsset Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Data/SuspenseCoreEquipmentSlotPresets.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"

USuspenseCoreEquipmentSlotPresets::USuspenseCoreEquipmentSlotPresets()
{
	// Initialize with default presets if empty
	if (SlotPresets.Num() == 0)
	{
		SlotPresets = CreateDefaultPresets();
	}
}

bool USuspenseCoreEquipmentSlotPresets::GetPresetByType(EEquipmentSlotType SlotType, FEquipmentSlotConfig& OutConfig) const
{
	for (const FEquipmentSlotConfig& Preset : SlotPresets)
	{
		if (Preset.SlotType == SlotType)
		{
			OutConfig = Preset;
			return true;
		}
	}
	return false;
}

bool USuspenseCoreEquipmentSlotPresets::GetPresetByTag(const FGameplayTag& SlotTag, FEquipmentSlotConfig& OutConfig) const
{
	for (const FEquipmentSlotConfig& Preset : SlotPresets)
	{
		if (Preset.SlotTag == SlotTag)
		{
			OutConfig = Preset;
			return true;
		}
	}
	return false;
}

bool USuspenseCoreEquipmentSlotPresets::ValidatePresets() const
{
	for (const FEquipmentSlotConfig& Preset : SlotPresets)
	{
		if (!Preset.IsValid())
		{
			return false;
		}
	}
	return SlotPresets.Num() > 0;
}

#if WITH_EDITOR
void USuspenseCoreEquipmentSlotPresets::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Validate presets after edit
	if (!ValidatePresets())
	{
		UE_LOG(LogTemp, Warning, TEXT("USuspenseCoreEquipmentSlotPresets: Some presets have validation errors"));
	}
}
#endif

//========================================
// Static Factory Methods - Using Native Tags from BridgeSystem
//========================================

FEquipmentSlotConfig USuspenseCoreEquipmentSlotPresets::CreateSlotPreset(
	EEquipmentSlotType SlotType,
	const FGameplayTag& SlotTag,
	const FName& AttachmentSocket,
	const FGameplayTagContainer& AllowedTypes)
{
	FEquipmentSlotConfig Config;
	Config.SlotType = SlotType;
	Config.SlotTag = SlotTag;
	Config.AttachmentSocket = AttachmentSocket;
	Config.AllowedItemTypes = AllowedTypes;
	Config.bIsRequired = false;
	Config.bIsVisible = true;

	// Set display name based on slot type
	switch (SlotType)
	{
		case EEquipmentSlotType::PrimaryWeapon:
			Config.DisplayName = FText::FromString(TEXT("Primary Weapon"));
			break;
		case EEquipmentSlotType::SecondaryWeapon:
			Config.DisplayName = FText::FromString(TEXT("Secondary Weapon"));
			break;
		case EEquipmentSlotType::Holster:
			Config.DisplayName = FText::FromString(TEXT("Holster"));
			break;
		case EEquipmentSlotType::Scabbard:
			Config.DisplayName = FText::FromString(TEXT("Scabbard"));
			break;
		case EEquipmentSlotType::Headwear:
			Config.DisplayName = FText::FromString(TEXT("Headwear"));
			break;
		case EEquipmentSlotType::Earpiece:
			Config.DisplayName = FText::FromString(TEXT("Earpiece"));
			break;
		case EEquipmentSlotType::Eyewear:
			Config.DisplayName = FText::FromString(TEXT("Eyewear"));
			break;
		case EEquipmentSlotType::FaceCover:
			Config.DisplayName = FText::FromString(TEXT("Face Cover"));
			break;
		case EEquipmentSlotType::BodyArmor:
			Config.DisplayName = FText::FromString(TEXT("Body Armor"));
			break;
		case EEquipmentSlotType::TacticalRig:
			Config.DisplayName = FText::FromString(TEXT("Tactical Rig"));
			break;
		case EEquipmentSlotType::Backpack:
			Config.DisplayName = FText::FromString(TEXT("Backpack"));
			break;
		case EEquipmentSlotType::SecureContainer:
			Config.DisplayName = FText::FromString(TEXT("Secure Container"));
			break;
		case EEquipmentSlotType::QuickSlot1:
			Config.DisplayName = FText::FromString(TEXT("Quick Slot 1"));
			break;
		case EEquipmentSlotType::QuickSlot2:
			Config.DisplayName = FText::FromString(TEXT("Quick Slot 2"));
			break;
		case EEquipmentSlotType::QuickSlot3:
			Config.DisplayName = FText::FromString(TEXT("Quick Slot 3"));
			break;
		case EEquipmentSlotType::QuickSlot4:
			Config.DisplayName = FText::FromString(TEXT("Quick Slot 4"));
			break;
		case EEquipmentSlotType::Armband:
			Config.DisplayName = FText::FromString(TEXT("Armband"));
			break;
		default:
			Config.DisplayName = FText::FromString(TEXT("Equipment Slot"));
			break;
	}

	return Config;
}

TArray<FEquipmentSlotConfig> USuspenseCoreEquipmentSlotPresets::CreateDefaultPresets()
{
	using namespace SuspenseCoreTags;

	TArray<FEquipmentSlotConfig> Presets;
	Presets.Reserve(17);

	// ===== WEAPONS =====

	// Primary Weapon (AR, DMR, SR, Shotgun, LMG)
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Weapon::AR);
		AllowedTypes.AddTag(Item::Weapon::DMR);
		AllowedTypes.AddTag(Item::Weapon::SR);
		AllowedTypes.AddTag(Item::Weapon::Shotgun);
		AllowedTypes.AddTag(Item::Weapon::LMG);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::PrimaryWeapon,
			EquipmentSlot::PrimaryWeapon,
			TEXT("weapon_r"),
			AllowedTypes
		));
	}

	// Secondary Weapon (SMG, Shotgun, PDW)
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Weapon::SMG);
		AllowedTypes.AddTag(Item::Weapon::Shotgun);
		AllowedTypes.AddTag(Item::Weapon::PDW);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::SecondaryWeapon,
			EquipmentSlot::SecondaryWeapon,
			TEXT("spine_03"),
			AllowedTypes
		));
	}

	// Holster (Pistol, Revolver)
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Weapon::Pistol);
		AllowedTypes.AddTag(Item::Weapon::Revolver);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Holster,
			EquipmentSlot::Holster,
			TEXT("thigh_r"),
			AllowedTypes
		));
	}

	// Scabbard (Melee/Knife)
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Weapon::Melee::Knife);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Scabbard,
			EquipmentSlot::Scabbard,
			TEXT("spine_02"),
			AllowedTypes
		));
	}

	// ===== HEAD GEAR =====

	// Headwear (Helmet, Headwear)
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Armor::Helmet);
		AllowedTypes.AddTag(Item::Gear::Headwear);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Headwear,
			EquipmentSlot::Headwear,
			TEXT("head"),
			AllowedTypes
		));
	}

	// Earpiece
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::Earpiece);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Earpiece,
			EquipmentSlot::Earpiece,
			TEXT("head"),
			AllowedTypes
		));
	}

	// Eyewear
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::Eyewear);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Eyewear,
			EquipmentSlot::Eyewear,
			TEXT("head"),
			AllowedTypes
		));
	}

	// Face Cover
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::FaceCover);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::FaceCover,
			EquipmentSlot::FaceCover,
			TEXT("head"),
			AllowedTypes
		));
	}

	// ===== BODY GEAR =====

	// Body Armor
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Armor::BodyArmor);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::BodyArmor,
			EquipmentSlot::BodyArmor,
			TEXT("spine_03"),
			AllowedTypes
		));
	}

	// Tactical Rig
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::TacticalRig);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::TacticalRig,
			EquipmentSlot::TacticalRig,
			TEXT("spine_03"),
			AllowedTypes
		));
	}

	// ===== STORAGE =====

	// Backpack
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::Backpack);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Backpack,
			EquipmentSlot::Backpack,
			TEXT("spine_02"),
			AllowedTypes
		));
	}

	// Secure Container
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::SecureContainer);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::SecureContainer,
			EquipmentSlot::SecureContainer,
			NAME_None,
			AllowedTypes
		));
	}

	// ===== QUICK SLOTS =====
	{
		FGameplayTagContainer QuickSlotAllowedTypes;
		QuickSlotAllowedTypes.AddTag(Item::Consumable);
		QuickSlotAllowedTypes.AddTag(Item::Medical);
		QuickSlotAllowedTypes.AddTag(Item::Throwable);
		QuickSlotAllowedTypes.AddTag(Item::Ammo);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::QuickSlot1,
			EquipmentSlot::QuickSlot1,
			NAME_None,
			QuickSlotAllowedTypes
		));

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::QuickSlot2,
			EquipmentSlot::QuickSlot2,
			NAME_None,
			QuickSlotAllowedTypes
		));

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::QuickSlot3,
			EquipmentSlot::QuickSlot3,
			NAME_None,
			QuickSlotAllowedTypes
		));

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::QuickSlot4,
			EquipmentSlot::QuickSlot4,
			NAME_None,
			QuickSlotAllowedTypes
		));
	}

	// ===== SPECIAL =====

	// Armband
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::Armband);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Armband,
			EquipmentSlot::Armband,
			TEXT("upperarm_l"),
			AllowedTypes
		));
	}

	return Presets;
}
