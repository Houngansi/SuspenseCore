// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseCoreQuickSlotHUDWidget.generated.h"

// Forward declarations
class UTexture2D;
struct FSuspenseCoreQuickSlot;
struct FSuspenseCoreMagazineInstance;

/**
 * Data for displaying a single QuickSlot on HUD
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreQuickSlotHUDData
{
	GENERATED_BODY()

	/** Slot index (0-3) */
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot")
	int32 SlotIndex = INDEX_NONE;

	/** Slot tag for identification */
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot")
	FGameplayTag SlotTag;

	/** Item ID in this slot (empty if slot is empty) */
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot")
	FName ItemID;

	/** Item display name */
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot")
	FText DisplayName;

	/** Item icon */
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** Quantity (for stackable items like ammo) */
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot")
	int32 Quantity = 0;

	/** For magazines: current round count */
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot|Magazine")
	int32 MagazineRounds = 0;

	/** For magazines: max capacity */
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot|Magazine")
	int32 MagazineCapacity = 0;

	/** For magazines: loaded ammo type */
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot|Magazine")
	FName LoadedAmmoType;

	/** Is this a magazine slot? */
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot")
	bool bIsMagazine = false;

	/** Is slot available for use? */
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot")
	bool bIsAvailable = true;

	/** Cooldown remaining (0 = ready) */
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot")
	float CooldownRemaining = 0.0f;

	/** Cooldown total duration */
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot")
	float CooldownDuration = 0.0f;

	/** Hotkey text (e.g., "4", "5", "6", "7") */
	UPROPERTY(BlueprintReadOnly, Category = "QuickSlot")
	FText HotkeyText;

	/** Is slot empty? */
	bool IsEmpty() const { return ItemID.IsNone(); }

	/** Get cooldown progress (0-1, 1 = ready) */
	float GetCooldownProgress() const
	{
		if (CooldownDuration <= 0.0f) return 1.0f;
		return FMath::Clamp(1.0f - (CooldownRemaining / CooldownDuration), 0.0f, 1.0f);
	}
};

UINTERFACE(MinimalAPI, BlueprintType)
class UISuspenseCoreQuickSlotHUDWidget : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for QuickSlot HUD widget
 * Displays 4 quick access slots on the game HUD (not inventory screen)
 *
 * Shows: Magazines, Consumables, Medical items, Grenades
 * Features: Hotkey display, Cooldown visualization, Magazine ammo count
 *
 * EventBus Events (subscribed):
 * - TAG_Equipment_Event_QuickSlot_Assigned
 * - TAG_Equipment_Event_QuickSlot_Cleared
 * - TAG_Equipment_Event_QuickSlot_Used
 * - TAG_Equipment_Event_QuickSlot_CooldownStarted
 * - TAG_Equipment_Event_QuickSlot_CooldownEnded
 * - TAG_Equipment_Event_Magazine_RoundsChanged
 */
class BRIDGESYSTEM_API ISuspenseCoreQuickSlotHUDWidget
{
	GENERATED_BODY()

public:
	//================================================
	// Initialization
	//================================================

	/**
	 * Initialize widget with player's QuickSlot component
	 * @param OwnerActor Actor that owns the QuickSlot component
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|QuickSlot")
	void InitializeQuickSlots(AActor* OwnerActor);

	/**
	 * Cleanup when widget is being destroyed
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|QuickSlot")
	void CleanupQuickSlots();

	//================================================
	// Slot Updates
	//================================================

	/**
	 * Update a single slot display
	 * @param SlotData Data for the slot to update
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|QuickSlot")
	void UpdateSlot(const FSuspenseCoreQuickSlotHUDData& SlotData);

	/**
	 * Update all slots at once
	 * @param AllSlots Array of 4 slot data structures
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|QuickSlot")
	void UpdateAllSlots(const TArray<FSuspenseCoreQuickSlotHUDData>& AllSlots);

	/**
	 * Clear a slot display (show empty state)
	 * @param SlotIndex Slot index (0-3)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|QuickSlot")
	void ClearSlot(int32 SlotIndex);

	//================================================
	// Visual Feedback
	//================================================

	/**
	 * Play slot use animation
	 * @param SlotIndex Slot that was used
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|QuickSlot")
	void PlaySlotUseAnimation(int32 SlotIndex);

	/**
	 * Update cooldown display for a slot
	 * @param SlotIndex Slot index
	 * @param RemainingTime Remaining cooldown time
	 * @param TotalTime Total cooldown duration
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|QuickSlot")
	void UpdateSlotCooldown(int32 SlotIndex, float RemainingTime, float TotalTime);

	/**
	 * Highlight a slot (e.g., when hovering or selecting)
	 * @param SlotIndex Slot to highlight (-1 to clear all highlights)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|QuickSlot")
	void HighlightSlot(int32 SlotIndex);

	/**
	 * Show/hide slot availability indicator
	 * @param SlotIndex Slot index
	 * @param bAvailable Is the slot available for use?
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|QuickSlot")
	void SetSlotAvailability(int32 SlotIndex, bool bAvailable);

	//================================================
	// Magazine-Specific
	//================================================

	/**
	 * Update magazine rounds display in slot
	 * @param SlotIndex Slot containing the magazine
	 * @param CurrentRounds Current round count
	 * @param MaxRounds Maximum capacity
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|QuickSlot|Magazine")
	void UpdateMagazineRounds(int32 SlotIndex, int32 CurrentRounds, int32 MaxRounds);

	//================================================
	// Visibility
	//================================================

	/**
	 * Show or hide the entire QuickSlot HUD
	 * @param bVisible Should be visible?
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|QuickSlot")
	void SetQuickSlotHUDVisible(bool bVisible);

	/**
	 * Check if QuickSlot HUD is visible
	 * @return True if visible
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|QuickSlot")
	bool IsQuickSlotHUDVisible() const;
};
