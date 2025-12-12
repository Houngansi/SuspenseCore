// SuspenseCoreSaveLoadMenuWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCore/Save/SuspenseCoreSaveTypes.h"
#include "SuspenseCoreSaveLoadMenuWidget.generated.h"

class UTextBlock;
class UScrollBox;
class USuspenseCoreButtonWidget;
class UVerticalBox;
class USuspenseCoreSaveSlotWidget;
class USuspenseCoreSaveManager;

/**
 * Mode for the Save/Load menu
 */
UENUM(BlueprintType)
enum class ESuspenseCoreSaveLoadMode : uint8
{
	Save	UMETA(DisplayName = "Save Mode"),
	Load	UMETA(DisplayName = "Load Mode")
};

/**
 * Delegate for menu closed event
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSaveLoadMenuClosed);

/**
 * Delegate for save/load operation completed
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSaveLoadOperationCompleted, bool, bSuccess, const FString&, Message);

/**
 * USuspenseCoreSaveLoadMenuWidget
 *
 * Full-featured Save/Load menu that displays:
 * - List of save slots (including Quick Save, Auto Save, and manual slots)
 * - Slot information (character, level, location, timestamp)
 * - Save/Load/Delete operations
 * - Confirmation dialogs
 *
 * Can be used in both Save and Load modes.
 * Integrates with SuspenseCoreSaveManager for all operations.
 *
 * IMPORTANT: This widget handles Input Mode switching internally.
 * When shown, it sets UI mode. When hidden, it restores Game mode.
 */
UCLASS()
class UISYSTEM_API USuspenseCoreSaveLoadMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreSaveLoadMenuWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Show the menu in specified mode.
	 * @param Mode Save or Load mode
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|SaveLoad")
	void ShowMenu(ESuspenseCoreSaveLoadMode Mode);

	/**
	 * Hide the menu.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|SaveLoad")
	void HideMenu();

	/**
	 * Toggle menu visibility.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|SaveLoad")
	void ToggleMenu(ESuspenseCoreSaveLoadMode Mode);

	/**
	 * Check if menu is visible.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|SaveLoad")
	bool IsMenuVisible() const { return bIsVisible; }

	/**
	 * Get current mode.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|SaveLoad")
	ESuspenseCoreSaveLoadMode GetCurrentMode() const { return CurrentMode; }

	/**
	 * Refresh the slot list.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|SaveLoad")
	void RefreshSlots();

	/**
	 * Set the currently selected slot.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|SaveLoad")
	void SelectSlot(int32 SlotIndex);

	// ═══════════════════════════════════════════════════════════════════════════
	// DELEGATES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Fired when menu is closed */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|SaveLoad")
	FOnSaveLoadMenuClosed OnMenuClosed;

	/** Fired when save/load operation completes */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|SaveLoad")
	FOnSaveLoadOperationCompleted OnOperationCompleted;

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Title text ("Save Game" or "Load Game") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TitleText;

	/** Scroll box containing slot widgets */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UScrollBox* SlotsScrollBox;

	/** Vertical box for slot entries */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UVerticalBox* SlotsContainer;

	/** Action button (Save/Load based on mode) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	USuspenseCoreButtonWidget* ActionButton;

	/** Delete selected slot button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	USuspenseCoreButtonWidget* DeleteButton;

	/** Close/Back button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	USuspenseCoreButtonWidget* CloseButton;

	/** Status message text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* StatusText;

	/** Confirmation overlay */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UWidget* ConfirmationOverlay;

	/** Confirmation message text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* ConfirmationText;

	/** Confirm button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	USuspenseCoreButtonWidget* ConfirmButton;

	/** Cancel button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	USuspenseCoreButtonWidget* CancelButton;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Widget class for individual save slots */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	TSubclassOf<USuspenseCoreSaveSlotWidget> SaveSlotWidgetClass;

	/** Title text for Save mode */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FText SaveModeTitle = FText::FromString(TEXT("SAVE GAME"));

	/** Title text for Load mode */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FText LoadModeTitle = FText::FromString(TEXT("LOAD GAME"));

	/** Save button text */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FText SaveButtonText = FText::FromString(TEXT("SAVE"));

	/** Load button text */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FText LoadButtonText = FText::FromString(TEXT("LOAD"));

	/** Confirm overwrite message */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FText ConfirmOverwriteText = FText::FromString(TEXT("Overwrite existing save?"));

	/** Confirm delete message */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FText ConfirmDeleteText = FText::FromString(TEXT("Delete this save?"));

	/** Confirm load message */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FText ConfirmLoadText = FText::FromString(TEXT("Load this save? Unsaved progress will be lost."));

	/** Success save message */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FText SaveSuccessText = FText::FromString(TEXT("Game saved successfully!"));

	/** Success load message */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FText LoadSuccessText = FText::FromString(TEXT("Game loaded successfully!"));

	/** Failed message */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	FText OperationFailedText = FText::FromString(TEXT("Operation failed!"));

	/** Whether to show Quick Save slot */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	bool bShowQuickSaveSlot = true;

	/** Whether to show Auto Save slot */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	bool bShowAutoSaveSlot = true;

	/** Number of manual save slots to show */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	int32 NumManualSlots = 10;

	/** Status message display duration */
	UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
	float StatusMessageDuration = 3.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Current mode */
	ESuspenseCoreSaveLoadMode CurrentMode = ESuspenseCoreSaveLoadMode::Load;

	/** Is menu visible */
	bool bIsVisible = false;

	/** Currently selected slot index (-1 for none) */
	int32 SelectedSlotIndex = -1;

	/** Is selected slot empty */
	bool bSelectedSlotEmpty = true;

	/** Pending operation slot (for confirmation) */
	int32 PendingOperationSlot = -1;

	/** Type of pending operation */
	enum class EPendingOperation
	{
		None,
		Save,
		Load,
		Delete
	};
	EPendingOperation PendingOperation = EPendingOperation::None;

	/** Cached save manager reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreSaveManager> CachedSaveManager;

	/** Created slot widgets */
	UPROPERTY()
	TArray<TObjectPtr<USuspenseCoreSaveSlotWidget>> SlotWidgets;

	/** Timer handle for status message */
	FTimerHandle StatusTimerHandle;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Setup button bindings */
	void SetupButtonBindings();

	/** Get save manager */
	USuspenseCoreSaveManager* GetSaveManager();

	/** Create slot widgets */
	void CreateSlotWidgets();

	/** Clear slot widgets */
	void ClearSlotWidgets();

	/** Update slot widget data */
	void UpdateSlotWidget(USuspenseCoreSaveSlotWidget* Widget, int32 SlotIndex);

	/** Update UI based on current mode */
	void UpdateModeDisplay();

	/** Update action button state */
	void UpdateActionButtonState();

	/** Show confirmation dialog */
	void ShowConfirmation(EPendingOperation Operation, int32 SlotIndex);

	/** Hide confirmation dialog */
	void HideConfirmation();

	/** Execute the pending operation */
	void ExecutePendingOperation();

	/** Show status message */
	void ShowStatus(const FText& Message);

	/** Hide status message */
	void HideStatus();

	/** Perform save operation */
	void PerformSave(int32 SlotIndex);

	/** Perform load operation */
	void PerformLoad(int32 SlotIndex);

	/** Perform delete operation */
	void PerformDelete(int32 SlotIndex);

	/** Set input mode for UI */
	void SetUIInputMode();

	/** Restore input mode for game */
	void RestoreGameInputMode();

	// ═══════════════════════════════════════════════════════════════════════════
	// BUTTON HANDLERS
	// ═══════════════════════════════════════════════════════════════════════════

	UFUNCTION()
	void OnActionButtonClicked(USuspenseCoreButtonWidget* Button);

	UFUNCTION()
	void OnDeleteButtonClicked(USuspenseCoreButtonWidget* Button);

	UFUNCTION()
	void OnCloseButtonClicked(USuspenseCoreButtonWidget* Button);

	UFUNCTION()
	void OnConfirmButtonClicked(USuspenseCoreButtonWidget* Button);

	UFUNCTION()
	void OnCancelButtonClicked(USuspenseCoreButtonWidget* Button);

	// ═══════════════════════════════════════════════════════════════════════════
	// SLOT WIDGET CALLBACKS
	// ═══════════════════════════════════════════════════════════════════════════

	UFUNCTION()
	void OnSlotSelected(int32 SlotIndex, bool bIsEmpty);

	UFUNCTION()
	void OnSlotDeleteRequested(int32 SlotIndex);

	// ═══════════════════════════════════════════════════════════════════════════
	// SAVE MANAGER CALLBACKS
	// ═══════════════════════════════════════════════════════════════════════════

	UFUNCTION()
	void OnSaveCompleted(bool bSuccess, const FString& ErrorMessage);

	UFUNCTION()
	void OnLoadCompleted(bool bSuccess, const FString& ErrorMessage);

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when menu is shown */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|SaveLoad")
	void OnMenuShown(ESuspenseCoreSaveLoadMode Mode);

	/** Called when menu is hidden */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|SaveLoad")
	void OnMenuHidden();

	/** Called when a slot is selected */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|SaveLoad")
	void OnSlotSelectedEvent(int32 SlotIndex, bool bIsEmpty);

	/** Called when operation succeeds */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|SaveLoad")
	void OnOperationSucceeded(ESuspenseCoreSaveLoadMode Mode, int32 SlotIndex);

	/** Called when operation fails */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|SaveLoad")
	void OnOperationFailed(ESuspenseCoreSaveLoadMode Mode, int32 SlotIndex, const FString& ErrorMessage);
};
