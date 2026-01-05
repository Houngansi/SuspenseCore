// GA_QuickSlotUse.h
// QuickSlot-specific Item Use Ability
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// - Extends GA_ItemUse with QuickSlot-specific request building
// - Maps input to QuickSlot index (keys 4-7 → slots 0-3)
// - Works with ISuspenseCoreItemUseService.UseQuickSlot()
//
// USAGE:
// - Create subclasses GA_QuickSlot1Use through GA_QuickSlot4Use
// - Each is bound to ESuspenseCoreAbilityInputID::QuickSlot1-4
// - UI hotkeys 4,5,6,7 trigger the corresponding ability

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Abilities/ItemUse/GA_ItemUse.h"
#include "GA_QuickSlotUse.generated.h"

/**
 * UGA_QuickSlotUse
 *
 * QuickSlot-specific item use ability.
 * Builds item use request from QuickSlot data.
 *
 * FEATURES:
 * - Automatically builds request from QuickSlot content
 * - Supports all item types assignable to QuickSlots
 * - Magazine swap, consumables, grenades, etc.
 *
 * @see GA_ItemUse
 * @see ISuspenseCoreItemUseService::UseQuickSlot
 */
UCLASS(Abstract, Blueprintable)
class GAS_API UGA_QuickSlotUse : public UGA_ItemUse
{
	GENERATED_BODY()

public:
	UGA_QuickSlotUse();

	//==================================================================
	// Configuration
	//==================================================================

	/** Which QuickSlot this ability controls (0-3 → keys 4-7) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ItemUse|QuickSlot",
		meta = (ClampMin = "0", ClampMax = "3"))
	int32 SlotIndex;

protected:
	//==================================================================
	// GA_ItemUse Overrides
	//==================================================================

	virtual FSuspenseCoreItemUseRequest BuildItemUseRequest(
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayEventData* TriggerEventData) const override;
};

//==================================================================
// Concrete QuickSlot Abilities (one per slot)
//==================================================================

/**
 * QuickSlot 1 (Key 4)
 */
UCLASS()
class GAS_API UGA_QuickSlot1Use : public UGA_QuickSlotUse
{
	GENERATED_BODY()

public:
	UGA_QuickSlot1Use()
	{
		SlotIndex = 0;
		AbilityInputID = ESuspenseCoreAbilityInputID::QuickSlot1;
	}
};

/**
 * QuickSlot 2 (Key 5)
 */
UCLASS()
class GAS_API UGA_QuickSlot2Use : public UGA_QuickSlotUse
{
	GENERATED_BODY()

public:
	UGA_QuickSlot2Use()
	{
		SlotIndex = 1;
		AbilityInputID = ESuspenseCoreAbilityInputID::QuickSlot2;
	}
};

/**
 * QuickSlot 3 (Key 6)
 */
UCLASS()
class GAS_API UGA_QuickSlot3Use : public UGA_QuickSlotUse
{
	GENERATED_BODY()

public:
	UGA_QuickSlot3Use()
	{
		SlotIndex = 2;
		AbilityInputID = ESuspenseCoreAbilityInputID::QuickSlot3;
	}
};

/**
 * QuickSlot 4 (Key 7)
 */
UCLASS()
class GAS_API UGA_QuickSlot4Use : public UGA_QuickSlotUse
{
	GENERATED_BODY()

public:
	UGA_QuickSlot4Use()
	{
		SlotIndex = 3;
		AbilityInputID = ESuspenseCoreAbilityInputID::QuickSlot4;
	}
};
