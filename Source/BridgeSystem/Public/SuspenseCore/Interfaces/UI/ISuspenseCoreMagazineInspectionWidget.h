// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseCoreMagazineInspectionWidget.generated.h"

// Forward declarations
class UTexture2D;

/**
 * Data for a single round in magazine inspection (Tarkov-style)
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreRoundSlotData
{
	GENERATED_BODY()

	/** Slot index in magazine (0 = first to be fired) */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Round")
	int32 SlotIndex = 0;

	/** Is this slot occupied? */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Round")
	bool bIsOccupied = false;

	/** Ammo type ID */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Round")
	FName AmmoID;

	/** Ammo display name */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Round")
	FText AmmoDisplayName;

	/** Ammo icon */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Round")
	TObjectPtr<UTexture2D> AmmoIcon = nullptr;

	/** Ammo rarity tag (for visual styling) */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Round")
	FGameplayTag AmmoRarityTag;

	/** Can this round be unloaded? */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Round")
	bool bCanUnload = true;
};

/**
 * Data for magazine inspection (Tarkov-style)
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreMagazineInspectionData
{
	GENERATED_BODY()

	//================================================
	// Magazine Identity
	//================================================

	/** Magazine instance ID */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Identity")
	FGuid MagazineInstanceID;

	/** Magazine type ID */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Identity")
	FName MagazineID;

	/** Magazine display name */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Identity")
	FText DisplayName;

	/** Magazine icon */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Identity")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** Rarity tag */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Identity")
	FGameplayTag RarityTag;

	//================================================
	// Capacity
	//================================================

	/** Maximum capacity */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Capacity")
	int32 MaxCapacity = 30;

	/** Current round count */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Capacity")
	int32 CurrentRounds = 0;

	//================================================
	// Compatibility
	//================================================

	/** Caliber tag */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Compatibility")
	FGameplayTag CaliberTag;

	/** Caliber display name */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Compatibility")
	FText CaliberDisplayName;

	//================================================
	// Rounds Array (each slot in magazine)
	//================================================

	/** All round slots in magazine (ordered from first-to-fire to last) */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Rounds")
	TArray<FSuspenseCoreRoundSlotData> RoundSlots;

	//================================================
	// Loading State
	//================================================

	/** Is ammo currently being loaded? */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Loading")
	bool bIsLoading = false;

	/** Current loading progress (0-1) */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Loading")
	float LoadingProgress = 0.0f;

	/** Slot being loaded/unloaded (-1 = none) */
	UPROPERTY(BlueprintReadOnly, Category = "Magazine|Loading")
	int32 ActiveSlotIndex = -1;

	//================================================
	// Helpers
	//================================================

	/** Get fill percentage */
	float GetFillPercent() const
	{
		if (MaxCapacity <= 0) return 0.0f;
		return FMath::Clamp((float)CurrentRounds / (float)MaxCapacity, 0.0f, 1.0f);
	}

	/** Is magazine empty? */
	bool IsEmpty() const { return CurrentRounds <= 0; }

	/** Is magazine full? */
	bool IsFull() const { return CurrentRounds >= MaxCapacity; }

	/** Get first empty slot index (-1 if full) */
	int32 GetFirstEmptySlot() const
	{
		for (int32 i = 0; i < RoundSlots.Num(); ++i)
		{
			if (!RoundSlots[i].bIsOccupied)
			{
				return i;
			}
		}
		return -1;
	}

	/** Get last occupied slot index (-1 if empty) */
	int32 GetLastOccupiedSlot() const
	{
		for (int32 i = RoundSlots.Num() - 1; i >= 0; --i)
		{
			if (RoundSlots[i].bIsOccupied)
			{
				return i;
			}
		}
		return -1;
	}
};

/**
 * Result of drop operation in magazine inspection
 */
UENUM(BlueprintType)
enum class ESuspenseCoreMagazineDropResult : uint8
{
	/** Drop accepted, ammo loaded */
	Loaded,
	/** Incompatible caliber */
	IncompatibleCaliber,
	/** Magazine is full */
	MagazineFull,
	/** Invalid ammo type */
	InvalidAmmo,
	/** Already loading */
	Busy,
	/** Generic failure */
	Failed
};

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseCoreMagazineInspectionWidgetInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for Magazine Inspection widget (Tarkov-style)
 *
 * Displays magazine contents with each round visible:
 * - Visual representation of each round slot
 * - Drag&Drop support for loading/unloading rounds
 * - Real-time loading progress visualization
 *
 * Used for detailed magazine management:
 * - Right-click on magazine → "Inspect"
 * - Drag ammo stack onto magazine to load
 * - Click on round to unload
 *
 * Visual Layout (30-round magazine):
 * ┌─────────────────────────────────────────────────────────┐
 * │  STANAG 30-round Magazine           5.56x45mm NATO     │
 * ├─────────────────────────────────────────────────────────┤
 * │  [1][2][3][4][5][6][7][8][9][10]                        │
 * │  [●][●][●][●][●][●][●][●][●][●]   ← Loaded rounds      │
 * │  [11][12][13][14][15][16][17][18][19][20]               │
 * │  [●][●][●][●][●][●][●][○][○][○]   ← Partial row        │
 * │  [21][22][23][24][25][26][27][28][29][30]               │
 * │  [○][○][○][○][○][○][○][○][○][○]   ← Empty slots        │
 * ├─────────────────────────────────────────────────────────┤
 * │  27/30 rounds loaded    Drag ammo here to load          │
 * │  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░                          │
 * └─────────────────────────────────────────────────────────┘
 */
class BRIDGESYSTEM_API ISuspenseCoreMagazineInspectionWidgetInterface
{
	GENERATED_BODY()

public:
	//================================================
	// Show/Hide
	//================================================

	/**
	 * Open magazine inspection panel
	 * @param InspectionData Magazine data to display
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Magazine|Inspection")
	void OpenInspection(const FSuspenseCoreMagazineInspectionData& InspectionData);

	/**
	 * Close magazine inspection panel
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Magazine|Inspection")
	void CloseInspection();

	/**
	 * Update inspection data (while open)
	 * @param InspectionData Updated magazine data
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Magazine|Inspection")
	void UpdateInspection(const FSuspenseCoreMagazineInspectionData& InspectionData);

	//================================================
	// Loading Visualization
	//================================================

	/**
	 * Start loading animation for specific slot
	 * @param SlotIndex Slot being loaded
	 * @param LoadTime Time to load this round
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Magazine|Inspection")
	void StartLoadingSlot(int32 SlotIndex, float LoadTime);

	/**
	 * Complete loading animation for slot
	 * @param SlotIndex Slot that was loaded
	 * @param RoundData Data of loaded round
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Magazine|Inspection")
	void CompleteLoadingSlot(int32 SlotIndex, const FSuspenseCoreRoundSlotData& RoundData);

	/**
	 * Start unloading animation for specific slot
	 * @param SlotIndex Slot being unloaded
	 * @param UnloadTime Time to unload this round
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Magazine|Inspection")
	void StartUnloadingSlot(int32 SlotIndex, float UnloadTime);

	/**
	 * Complete unloading animation for slot
	 * @param SlotIndex Slot that was unloaded
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Magazine|Inspection")
	void CompleteUnloadingSlot(int32 SlotIndex);

	/**
	 * Cancel current loading/unloading operation
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Magazine|Inspection")
	void CancelLoadingOperation();

	//================================================
	// Drag&Drop
	//================================================

	/**
	 * Handle ammo dropped on magazine (from inventory)
	 * @param AmmoID Ammo type ID
	 * @param Quantity How many rounds being dropped
	 * @return Drop result
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Magazine|Inspection")
	ESuspenseCoreMagazineDropResult OnAmmoDropped(FName AmmoID, int32 Quantity);

	/**
	 * Set drop highlight state (when ammo is being dragged over)
	 * @param bHighlight Should highlight drop zone?
	 * @param bIsCompatible Is the dragged ammo compatible?
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Magazine|Inspection")
	void SetDropHighlight(bool bHighlight, bool bIsCompatible);

	//================================================
	// State
	//================================================

	/**
	 * Is inspection panel currently visible?
	 * @return True if visible
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Magazine|Inspection")
	bool IsInspectionVisible() const;

	/**
	 * Get current inspection data
	 * @return Current magazine inspection data
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Magazine|Inspection")
	FSuspenseCoreMagazineInspectionData GetCurrentInspectionData() const;

	/**
	 * Get magazine instance ID being inspected
	 * @return Magazine instance GUID
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Magazine|Inspection")
	FGuid GetInspectedMagazineID() const;
};
