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
	const FGameplayTagContainer& AllowedTypes,
	const FVector2D& UIPos,
	const FVector2D& UISlotSize)
{
	FEquipmentSlotConfig Config;
	Config.SlotType = SlotType;
	Config.SlotTag = SlotTag;
	Config.AttachmentSocket = AttachmentSocket;
	Config.AllowedItemTypes = AllowedTypes;
	Config.bIsRequired = false;
	Config.bIsVisible = true;
	Config.UIPosition = UIPos;
	Config.UISize = UISlotSize;

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

	// =============================================================================
	// TARKOV-STYLE EQUIPMENT SLOT LAYOUT
	// =============================================================================
	// Base cell size: 64x64 pixels (industry standard for grid inventories at 1080p)
	// All slots use multiples of 64px for consistent grid alignment
	// Layout designed for ~600x500 pixel equipment panel
	//
	// Reference: Escape from Tarkov uses 64px base grid cells
	// Items occupy NxM cells based on their physical size
	// =============================================================================

	constexpr float CellSize = 64.0f;  // Base grid cell (1x1)
	constexpr float Gap = 8.0f;        // Gap between slots

	// Layout regions (X positions):
	// - Weapons column: X = 16
	// - Character/Head column: X = 200
	// - Storage column: X = 408

	// ===== WEAPONS (Left Column) =====
	// Long weapons displayed horizontally, emphasizing their length

	// Primary Weapon (AR, DMR, SR, Shotgun, LMG) - 5x1 cells = 320x64
	// Horizontal slot for long rifles/shotguns
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
			AllowedTypes,
			FVector2D(16, 24),                    // Position
			FVector2D(CellSize * 5, CellSize)    // 320x64 (5x1 cells)
		));
	}

	// Secondary Weapon (SMG, Shotgun, PDW) - 4x1 cells = 256x64
	// Slightly shorter horizontal slot for compact weapons
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Weapon::SMG);
		AllowedTypes.AddTag(Item::Weapon::Shotgun);
		AllowedTypes.AddTag(Item::Weapon::PDW);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::SecondaryWeapon,
			EquipmentSlot::SecondaryWeapon,
			TEXT("spine_03"),
			AllowedTypes,
			FVector2D(16, 24 + CellSize + Gap),  // Below primary
			FVector2D(CellSize * 4, CellSize)    // 256x64 (4x1 cells)
		));
	}

	// Holster (Pistol, Revolver) - 2x1 cells = 128x64
	// Horizontal slot for sidearms
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Weapon::Pistol);
		AllowedTypes.AddTag(Item::Weapon::Revolver);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Holster,
			EquipmentSlot::Holster,
			TEXT("thigh_r"),
			AllowedTypes,
			FVector2D(16, 24 + (CellSize + Gap) * 2),  // Below secondary
			FVector2D(CellSize * 2, CellSize)          // 128x64 (2x1 cells)
		));
	}

	// Scabbard (Melee/Knife) - 1x2 cells = 64x128
	// Vertical slot for bladed weapons
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Weapon::Melee::Knife);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Scabbard,
			EquipmentSlot::Scabbard,
			TEXT("spine_02"),
			AllowedTypes,
			FVector2D(16 + CellSize * 2 + Gap, 24 + (CellSize + Gap) * 2),  // Right of holster
			FVector2D(CellSize, CellSize * 2)    // 64x128 (1x2 cells)
		));
	}

	// ===== HEAD GEAR (Center-Top) =====
	// Positioned around where character head would be shown

	// Headwear (Helmet) - 2x2 cells = 128x128
	// Large slot for helmets/hats - dominant head slot
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Armor::Helmet);
		AllowedTypes.AddTag(Item::Gear::Headwear);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Headwear,
			EquipmentSlot::Headwear,
			TEXT("head"),
			AllowedTypes,
			FVector2D(216, 16),                   // Center-top
			FVector2D(CellSize * 2, CellSize * 2) // 128x128 (2x2 cells)
		));
	}

	// Earpiece - 1x1 cell = 64x64
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::Earpiece);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Earpiece,
			EquipmentSlot::Earpiece,
			TEXT("head"),
			AllowedTypes,
			FVector2D(216 + CellSize * 2 + Gap, 16),  // Right of headwear
			FVector2D(CellSize, CellSize)             // 64x64 (1x1 cell)
		));
	}

	// Eyewear - 1x1 cell = 64x64
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::Eyewear);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Eyewear,
			EquipmentSlot::Eyewear,
			TEXT("head"),
			AllowedTypes,
			FVector2D(216 + CellSize * 2 + Gap, 16 + CellSize + Gap),  // Below earpiece
			FVector2D(CellSize, CellSize)             // 64x64 (1x1 cell)
		));
	}

	// Face Cover - 1x1 cell = 64x64
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::FaceCover);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::FaceCover,
			EquipmentSlot::FaceCover,
			TEXT("head"),
			AllowedTypes,
			FVector2D(216, 16 + CellSize * 2 + Gap),  // Below headwear
			FVector2D(CellSize, CellSize)             // 64x64 (1x1 cell)
		));
	}

	// ===== BODY GEAR (Center) =====
	// Large prominent slots for armor and rig - visual hierarchy

	// Body Armor - 3x3 cells = 192x192
	// Dominant center slot showing equipped armor
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Armor::BodyArmor);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::BodyArmor,
			EquipmentSlot::BodyArmor,
			TEXT("spine_03"),
			AllowedTypes,
			FVector2D(200, 160),                     // Center body area
			FVector2D(CellSize * 3, CellSize * 3)   // 192x192 (3x3 cells)
		));
	}

	// Tactical Rig - 2x3 cells = 128x192
	// Tall slot next to body armor
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::TacticalRig);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::TacticalRig,
			EquipmentSlot::TacticalRig,
			TEXT("spine_03"),
			AllowedTypes,
			FVector2D(200 + CellSize * 3 + Gap, 160),  // Right of body armor
			FVector2D(CellSize * 2, CellSize * 3)      // 128x192 (2x3 cells)
		));
	}

	// ===== STORAGE (Right Column) =====

	// Backpack - 3x3 cells = 192x192
	// Large slot showing backpack (indicates storage capacity)
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::Backpack);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Backpack,
			EquipmentSlot::Backpack,
			TEXT("spine_02"),
			AllowedTypes,
			FVector2D(424, 16),                      // Top-right
			FVector2D(CellSize * 3, CellSize * 3)   // 192x192 (3x3 cells)
		));
	}

	// Secure Container - 2x2 cells = 128x128
	// Protected storage slot
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::SecureContainer);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::SecureContainer,
			EquipmentSlot::SecureContainer,
			NAME_None,
			AllowedTypes,
			FVector2D(424, 16 + CellSize * 3 + Gap),  // Below backpack
			FVector2D(CellSize * 2, CellSize * 2)     // 128x128 (2x2 cells)
		));
	}

	// ===== QUICK SLOTS (Bottom Row) =====
	// 1x1 cells for quick access items (meds, grenades, etc.)
	{
		FGameplayTagContainer QuickSlotAllowedTypes;
		QuickSlotAllowedTypes.AddTag(Item::Consumable);
		QuickSlotAllowedTypes.AddTag(Item::Medical);
		QuickSlotAllowedTypes.AddTag(Item::Throwable);
		QuickSlotAllowedTypes.AddTag(Item::Ammo);

		const float QuickSlotY = 376;  // Bottom row
		const float QuickSlotStartX = 200;  // Centered under body

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::QuickSlot1,
			EquipmentSlot::QuickSlot1,
			NAME_None,
			QuickSlotAllowedTypes,
			FVector2D(QuickSlotStartX, QuickSlotY),
			FVector2D(CellSize, CellSize)  // 64x64 (1x1 cell)
		));

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::QuickSlot2,
			EquipmentSlot::QuickSlot2,
			NAME_None,
			QuickSlotAllowedTypes,
			FVector2D(QuickSlotStartX + CellSize + Gap, QuickSlotY),
			FVector2D(CellSize, CellSize)  // 64x64 (1x1 cell)
		));

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::QuickSlot3,
			EquipmentSlot::QuickSlot3,
			NAME_None,
			QuickSlotAllowedTypes,
			FVector2D(QuickSlotStartX + (CellSize + Gap) * 2, QuickSlotY),
			FVector2D(CellSize, CellSize)  // 64x64 (1x1 cell)
		));

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::QuickSlot4,
			EquipmentSlot::QuickSlot4,
			NAME_None,
			QuickSlotAllowedTypes,
			FVector2D(QuickSlotStartX + (CellSize + Gap) * 3, QuickSlotY),
			FVector2D(CellSize, CellSize)  // 64x64 (1x1 cell)
		));
	}

	// ===== SPECIAL =====

	// Armband - 1x1 cell = 64x64
	// Team identification
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::Armband);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Armband,
			EquipmentSlot::Armband,
			TEXT("upperarm_l"),
			AllowedTypes,
			FVector2D(424 + CellSize * 2 + Gap, 16 + CellSize * 3 + Gap),  // Right of secure
			FVector2D(CellSize, CellSize)  // 64x64 (1x1 cell)
		));
	}

	return Presets;
}
