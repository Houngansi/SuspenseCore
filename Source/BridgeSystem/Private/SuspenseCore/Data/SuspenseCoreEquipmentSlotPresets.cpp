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
	//
	// LAYOUT STRUCTURE (Panel ~780x500):
	// - LEFT COLUMN (X=16): Weapons stacked vertically
	// - CENTER COLUMN (X=352): Head gear (top) + Body gear (below)
	// - RIGHT COLUMN (X=568): Storage (Backpack, Secure Container)
	// - BOTTOM ROW (Y=424): Quick slots
	//
	// Reference: Escape from Tarkov uses 64px base grid cells
	// =============================================================================

	constexpr float CellSize = 64.0f;  // Base grid cell (1x1)
	constexpr float Gap = 8.0f;        // Gap between slots

	// Column X positions:
	constexpr float ColWeapons = 16.0f;      // Weapons column
	constexpr float ColCenter = 352.0f;      // Head + Body column
	constexpr float ColStorage = 568.0f;     // Storage column

	// ===== WEAPONS (Left Column) =====
	// Long weapons displayed horizontally, 2 cells tall for better visibility

	// Primary Weapon (AR, DMR, SR, Shotgun, LMG) - 5x2 cells = 320x128
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
			FVector2D(ColWeapons, 16),                      // Position
			FVector2D(CellSize * 5, CellSize * 2)           // 320x128 (5x2 cells)
		));
	}

	// Secondary Weapon (SMG, Shotgun, PDW) - 4x2 cells = 256x128
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
			FVector2D(ColWeapons, 16 + CellSize * 2 + Gap),  // Y = 16 + 128 + 8 = 152
			FVector2D(CellSize * 4, CellSize * 2)            // 256x128 (4x2 cells)
		));
	}

	// Holster (Pistol, Revolver) - 2x2 cells = 128x128
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Weapon::Pistol);
		AllowedTypes.AddTag(Item::Weapon::Revolver);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Holster,
			EquipmentSlot::Holster,
			TEXT("thigh_r"),
			AllowedTypes,
			FVector2D(ColWeapons, 16 + CellSize * 4 + Gap * 2),  // Y = 16 + 256 + 16 = 288
			FVector2D(CellSize * 2, CellSize * 2)                 // 128x128 (2x2 cells)
		));
	}

	// Scabbard (Melee/Knife) - 1x2 cells = 64x128
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Weapon::Melee::Knife);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Scabbard,
			EquipmentSlot::Scabbard,
			TEXT("spine_02"),
			AllowedTypes,
			FVector2D(ColWeapons + CellSize * 2 + Gap, 16 + CellSize * 4 + Gap * 2),  // Right of holster
			FVector2D(CellSize, CellSize * 2)                 // 64x128 (1x2 cells)
		));
	}

	// ===== HEAD GEAR (Center Column - Top) =====

	// Headwear (Helmet) - 2x2 cells = 128x128
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Armor::Helmet);
		AllowedTypes.AddTag(Item::Gear::Headwear);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Headwear,
			EquipmentSlot::Headwear,
			TEXT("head"),
			AllowedTypes,
			FVector2D(ColCenter, 16),
			FVector2D(CellSize * 2, CellSize * 2)  // 128x128 (2x2 cells)
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
			FVector2D(ColCenter + CellSize * 2 + Gap, 16),  // Right of headwear
			FVector2D(CellSize, CellSize)                    // 64x64 (1x1 cell)
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
			FVector2D(ColCenter + CellSize * 2 + Gap, 16 + CellSize + Gap),  // Below earpiece
			FVector2D(CellSize, CellSize)                    // 64x64 (1x1 cell)
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
			FVector2D(ColCenter, 16 + CellSize * 2 + Gap),  // Below headwear
			FVector2D(CellSize, CellSize)                    // 64x64 (1x1 cell)
		));
	}

	// ===== BODY GEAR (Center Column - Below Head) =====
	// Body Y starts at: 16 + 128 (headwear) + 8 + 64 (face cover) + 8 = 224

	// Body Armor - 3x3 cells = 192x192
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Armor::BodyArmor);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::BodyArmor,
			EquipmentSlot::BodyArmor,
			TEXT("spine_03"),
			AllowedTypes,
			FVector2D(ColCenter, 16 + CellSize * 2 + Gap + CellSize + Gap),  // Y = 224
			FVector2D(CellSize * 3, CellSize * 3)            // 192x192 (3x3 cells)
		));
	}

	// Tactical Rig - 2x3 cells = 128x192
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::TacticalRig);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::TacticalRig,
			EquipmentSlot::TacticalRig,
			TEXT("spine_03"),
			AllowedTypes,
			FVector2D(ColCenter + CellSize * 3 + Gap, 16 + CellSize * 2 + Gap + CellSize + Gap),  // Right of armor
			FVector2D(CellSize * 2, CellSize * 3)            // 128x192 (2x3 cells)
		));
	}

	// ===== STORAGE (Right Column) =====

	// Backpack - 3x3 cells = 192x192
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::Backpack);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Backpack,
			EquipmentSlot::Backpack,
			TEXT("spine_02"),
			AllowedTypes,
			FVector2D(ColStorage, 16),
			FVector2D(CellSize * 3, CellSize * 3)            // 192x192 (3x3 cells)
		));
	}

	// Secure Container - 2x2 cells = 128x128
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::SecureContainer);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::SecureContainer,
			EquipmentSlot::SecureContainer,
			NAME_None,
			AllowedTypes,
			FVector2D(ColStorage, 16 + CellSize * 3 + Gap),  // Below backpack, Y = 216
			FVector2D(CellSize * 2, CellSize * 2)            // 128x128 (2x2 cells)
		));
	}

	// Armband - 1x1 cell = 64x64
	{
		FGameplayTagContainer AllowedTypes;
		AllowedTypes.AddTag(Item::Gear::Armband);

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::Armband,
			EquipmentSlot::Armband,
			TEXT("upperarm_l"),
			AllowedTypes,
			FVector2D(ColStorage + CellSize * 2 + Gap, 16 + CellSize * 3 + Gap),  // Right of secure
			FVector2D(CellSize, CellSize)                    // 64x64 (1x1 cell)
		));
	}

	// ===== QUICK SLOTS (Bottom Row) =====
	// Y = Body Armor bottom + gap = 224 + 192 + 8 = 424
	{
		FGameplayTagContainer QuickSlotAllowedTypes;
		QuickSlotAllowedTypes.AddTag(Item::Consumable);
		QuickSlotAllowedTypes.AddTag(Item::Medical);
		QuickSlotAllowedTypes.AddTag(Item::Throwable);
		QuickSlotAllowedTypes.AddTag(Item::Ammo);

		const float QuickSlotY = 16 + CellSize * 2 + Gap + CellSize + Gap + CellSize * 3 + Gap;  // 424
		const float QuickSlotStartX = ColCenter;  // Aligned with body gear

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::QuickSlot1,
			EquipmentSlot::QuickSlot1,
			NAME_None,
			QuickSlotAllowedTypes,
			FVector2D(QuickSlotStartX, QuickSlotY),
			FVector2D(CellSize, CellSize)                    // 64x64 (1x1 cell)
		));

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::QuickSlot2,
			EquipmentSlot::QuickSlot2,
			NAME_None,
			QuickSlotAllowedTypes,
			FVector2D(QuickSlotStartX + CellSize + Gap, QuickSlotY),
			FVector2D(CellSize, CellSize)                    // 64x64 (1x1 cell)
		));

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::QuickSlot3,
			EquipmentSlot::QuickSlot3,
			NAME_None,
			QuickSlotAllowedTypes,
			FVector2D(QuickSlotStartX + (CellSize + Gap) * 2, QuickSlotY),
			FVector2D(CellSize, CellSize)                    // 64x64 (1x1 cell)
		));

		Presets.Add(CreateSlotPreset(
			EEquipmentSlotType::QuickSlot4,
			EquipmentSlot::QuickSlot4,
			NAME_None,
			QuickSlotAllowedTypes,
			FVector2D(QuickSlotStartX + (CellSize + Gap) * 3, QuickSlotY),
			FVector2D(CellSize, CellSize)                    // 64x64 (1x1 cell)
		));
	}

	return Presets;
}
