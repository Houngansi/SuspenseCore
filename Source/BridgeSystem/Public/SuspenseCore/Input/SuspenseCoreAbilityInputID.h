#ifndef SUSPENSECORE_INPUT_SUSPENSECOREABILITYINPUTID_H
#define SUSPENSECORE_INPUT_SUSPENSECOREABILITYINPUTID_H

#include "SuspenseAbilityInputID.generated.h"

/** Официальный список InputID, который будет использоваться во всём проекте */
UENUM(BlueprintType)
enum class ESuspenseAbilityInputID : uint8
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

	// боевые действия с оружием ― добавим позже (Fire, Reload, ADS и т.д.)
};

#endif // SUSPENSECORE_INPUT_SUSPENSECOREABILITYINPUTID_H