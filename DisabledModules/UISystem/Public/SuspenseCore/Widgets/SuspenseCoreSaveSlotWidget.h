// SuspenseCoreSaveSlotWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCore/Save/SuspenseCoreSaveTypes.h"
#include "SuspenseCoreSaveSlotWidget.generated.h"

class UTextBlock;
class UButton;
class UImage;
class UBorder;

/**
 * Delegate fired when a save slot is selected
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSaveSlotSelected, int32, SlotIndex, bool, bIsEmpty);

/**
 * Delegate fired when delete is requested for a slot
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSaveSlotDeleteRequested, int32, SlotIndex);

/**
 * USuspenseCoreSaveSlotWidget
 *
 * Displays a single save slot with:
 * - Slot name and description
 * - Character info (name, level)
 * - Timestamp and playtime
 * - Location name
 * - Empty slot indicator
 * - Delete button (optional)
 *
 * Used in SuspenseCoreSaveLoadMenuWidget for displaying save/load options.
 */
UCLASS()
class UISYSTEM_API USuspenseCoreSaveSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreSaveSlotWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Initialize slot with save header data.
	 * @param InSlotIndex The slot index
	 * @param InHeader The save header data (empty if slot is unused)
	 * @param bInIsEmpty Whether the slot is empty
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|SaveSlot")
	void InitializeSlot(int32 InSlotIndex, const FSuspenseCoreSaveHeader& InHeader, bool bInIsEmpty);

	/**
	 * Set slot as empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|SaveSlot")
	void SetEmpty(int32 InSlotIndex);

	/**
	 * Set slot with data.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|SaveSlot")
	void SetSlotData(int32 InSlotIndex, const FSuspenseCoreSaveHeader& InHeader);

	/**
	 * Get current slot index.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|SaveSlot")
	int32 GetSlotIndex() const { return SlotIndex; }

	/**
	 * Check if slot is empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|SaveSlot")
	bool IsSlotEmpty() const { return bIsEmpty; }

	/**
	 * Set selected visual state.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|SaveSlot")
	void SetSelected(bool bSelected);

	/**
	 * Enable/disable delete button.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|SaveSlot")
	void SetDeleteEnabled(bool bEnabled);

	// ═══════════════════════════════════════════════════════════════════════════
	// DELEGATES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Fired when slot is clicked/selected */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|SaveSlot")
	FOnSaveSlotSelected OnSlotSelected;

	/** Fired when delete is requested */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|SaveSlot")
	FOnSaveSlotDeleteRequested OnDeleteRequested;

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Main clickable button for the slot */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* SlotButton;

	/** Background border for selection state */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UBorder* SlotBorder;

	/** Slot number/name text (e.g., "Slot 1", "Quick Save") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* SlotNameText;

	/** Character name text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* CharacterNameText;

	/** Character level text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* LevelText;

	/** Location name text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* LocationText;

	/** Save timestamp text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TimestampText;

	/** Total playtime text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* PlaytimeText;

	/** Empty slot indicator text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* EmptyText;

	/** Delete button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* DeleteButton;

	/** Screenshot/thumbnail image */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* ThumbnailImage;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Text shown for empty slots */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FText EmptySaveSlot = FText::FromString(TEXT("- Empty Slot -"));

	/** Text format for slot name */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FText SlotNameFormat = FText::FromString(TEXT("Slot {0}"));

	/** Text for Quick Save slot */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FText QuickSaveText = FText::FromString(TEXT("Quick Save"));

	/** Text for Auto Save slot */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FText AutoSaveText = FText::FromString(TEXT("Auto Save"));

	/** Color for normal state */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FLinearColor NormalColor = FLinearColor(0.1f, 0.1f, 0.1f, 0.8f);

	/** Color for selected state */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FLinearColor SelectedColor = FLinearColor(0.2f, 0.4f, 0.6f, 0.9f);

	/** Color for hovered state */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FLinearColor HoveredColor = FLinearColor(0.15f, 0.15f, 0.15f, 0.9f);

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Current slot index */
	int32 SlotIndex = -1;

	/** Whether this slot is empty */
	bool bIsEmpty = true;

	/** Whether this slot is currently selected */
	bool bIsSelected = false;

	/** Cached header data */
	FSuspenseCoreSaveHeader CachedHeader;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Update visual display based on current state */
	void UpdateDisplay();

	/** Format timestamp for display */
	FString FormatTimestamp(const FDateTime& Timestamp) const;

	/** Format playtime for display */
	FString FormatPlaytime(int64 TotalSeconds) const;

	/** Get display name for slot index */
	FString GetSlotDisplayName(int32 Index) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// BUTTON HANDLERS
	// ═══════════════════════════════════════════════════════════════════════════

	UFUNCTION()
	void OnSlotButtonClicked();

	UFUNCTION()
	void OnDeleteButtonClicked();

	UFUNCTION()
	void OnSlotButtonHovered();

	UFUNCTION()
	void OnSlotButtonUnhovered();

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when slot is selected */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|SaveSlot")
	void OnSlotSelectedEvent(int32 Index, bool bEmpty);

	/** Called when delete is requested */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|SaveSlot")
	void OnDeleteRequestedEvent(int32 Index);
};
