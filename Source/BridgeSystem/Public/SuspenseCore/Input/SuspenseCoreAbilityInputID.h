#pragma once

#include "CoreMinimal.h"
#include "SuspenseCoreAbilityInputID.generated.h"

/** Официальный список InputID, который будет использоваться во всём проекте */
UENUM(BlueprintType)
enum class ESuspenseCoreAbilityInputID : uint8
{
	None      UMETA(DisplayName="None"),
	Confirm   UMETA(DisplayName="Confirm"),
	Cancel    UMETA(DisplayName="Cancel"),

	// базовые передвижения
	Jump      UMETA(DisplayName="Jump"),
	Sprint    UMETA(DisplayName="Sprint"),
	Crouch    UMETA(DisplayName="Crouch"),

	// действия
	Interact  UMETA(DisplayName="Interact"),

	// переключение оружия
	NextWeapon    UMETA(DisplayName="Next Weapon"),
	PrevWeapon    UMETA(DisplayName="Previous Weapon"),
	QuickSwitch   UMETA(DisplayName="Quick Switch"),
	WeaponToggle  UMETA(DisplayName="Weapon Toggle"),
	// прямой выбор слота оружия
	WeaponSlot1   UMETA(DisplayName="Weapon Slot 1"),
	WeaponSlot2   UMETA(DisplayName="Weapon Slot 2"),
	WeaponSlot3   UMETA(DisplayName="Weapon Slot 3"),
	WeaponSlot4   UMETA(DisplayName="Weapon Slot 4"),
	WeaponSlot5   UMETA(DisplayName="Weapon Slot 5"),
	MeleeWeapon   UMETA(DisplayName="Melee Weapon"),  // Key V → Slot 3

	// боевые действия с оружием
	Aim              UMETA(DisplayName="Aim"),
	Fire             UMETA(DisplayName="Fire"),
	Reload           UMETA(DisplayName="Reload"),
	EmergencyReload  UMETA(DisplayName="Emergency Reload"),
	FireModeSwitch   UMETA(DisplayName="Fire Mode Switch"),
	Inspect          UMETA(DisplayName="Inspect Weapon"),
	MeleeAttack      UMETA(DisplayName="Melee Attack"),
	ThrowGrenade     UMETA(DisplayName="Throw Grenade"),
	HoldBreath       UMETA(DisplayName="Hold Breath"),

	// QuickSlots (Tarkov-style magazine/item access)
	QuickSlot1       UMETA(DisplayName="Quick Slot 1"),
	QuickSlot2       UMETA(DisplayName="Quick Slot 2"),
	QuickSlot3       UMETA(DisplayName="Quick Slot 3"),
	QuickSlot4       UMETA(DisplayName="Quick Slot 4")
};
