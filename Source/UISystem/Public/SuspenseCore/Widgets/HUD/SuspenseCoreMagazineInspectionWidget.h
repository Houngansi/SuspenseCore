// SuspenseCoreMagazineInspectionWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreMagazineInspectionWidget.h"
#include "SuspenseCoreMagazineInspectionWidget.generated.h"

// Forward declarations
class UTextBlock;
class UImage;
class UProgressBar;
class UVerticalBox;
class UWrapBox;
class UBorder;
class UButton;
class USuspenseCoreEventBus;

/**
 * USuspenseCoreMagazineInspectionWidget
 *
 * Tarkov-style magazine inspection widget displaying:
 * - Magazine info header (name, caliber, capacity)
 * - Grid of round slots showing each round in magazine
 * - Loading/unloading progress visualization
 * - Drag&Drop zone for loading ammo
 *
 * Features:
 * - Real-time updates via EventBus
 * - Per-slot loading animations
 * - Drag&Drop integration with AmmoLoadingService
 * - Visual feedback for compatible/incompatible ammo
 * - ALL components are MANDATORY (BindWidget)
 * - NO programmatic color changes - ALL colors from materials in Editor!
 *
 * Layout:
 * ┌─────────────────────────────────────────────────────────┐
 * │  [X] STANAG 30-round Magazine      5.56x45mm NATO      │
 * ├─────────────────────────────────────────────────────────┤
 * │                                                         │
 * │   [●][●][●][●][●][●][●][●][●][●]  ← 10 rounds per row  │
 * │   [●][●][●][●][●][●][●][○][○][○]  ← ● = loaded         │
 * │   [○][○][○][○][○][○][○][○][○][○]  ← ○ = empty          │
 * │                                                         │
 * ├─────────────────────────────────────────────────────────┤
 * │  27/30 loaded          Drag ammo here to load           │
 * │  ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░   │
 * └─────────────────────────────────────────────────────────┘
 *
 * EventBus Events (subscribed):
 * - TAG_Ammo_Event_Loading_Started
 * - TAG_Ammo_Event_Loading_Progress
 * - TAG_Ammo_Event_Loading_Completed
 * - TAG_Ammo_Event_Loading_Cancelled
 * - TAG_Ammo_Event_Unloading_Started
 * - TAG_Ammo_Event_Unloading_Completed
 *
 * @see ISuspenseCoreMagazineInspectionWidgetInterface
 * @see USuspenseCoreAmmoLoadingService
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreMagazineInspectionWidget : public UUserWidget, public ISuspenseCoreMagazineInspectionWidgetInterface
{
	GENERATED_BODY()

public:
	USuspenseCoreMagazineInspectionWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Drag & Drop support
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// ISuspenseCoreMagazineInspectionWidget Implementation
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void OpenInspection_Implementation(const FSuspenseCoreMagazineInspectionData& InspectionData) override;
	virtual void CloseInspection_Implementation() override;
	virtual void UpdateInspection_Implementation(const FSuspenseCoreMagazineInspectionData& InspectionData) override;
	virtual void StartLoadingSlot_Implementation(int32 SlotIndex, float LoadTime) override;
	virtual void CompleteLoadingSlot_Implementation(int32 SlotIndex, const FSuspenseCoreRoundSlotData& RoundData) override;
	virtual void StartUnloadingSlot_Implementation(int32 SlotIndex, float UnloadTime) override;
	virtual void CompleteUnloadingSlot_Implementation(int32 SlotIndex) override;
	virtual void CancelLoadingOperation_Implementation() override;
	virtual ESuspenseCoreMagazineDropResult OnAmmoDropped_Implementation(FName AmmoID, int32 Quantity) override;
	virtual void SetDropHighlight_Implementation(bool bHighlight, bool bIsCompatible) override;
	virtual bool IsInspectionVisible_Implementation() const override;
	virtual FSuspenseCoreMagazineInspectionData GetCurrentInspectionData_Implementation() const override;
	virtual FGuid GetInspectedMagazineID_Implementation() const override;

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when inspection opens */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Magazine|Inspection|Events")
	void OnInspectionOpened();

	/** Called when inspection closes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Magazine|Inspection|Events")
	void OnInspectionClosed();

	/** Called when a round starts loading */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Magazine|Inspection|Events")
	void OnRoundLoadingStarted(int32 SlotIndex);

	/** Called when a round finishes loading */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Magazine|Inspection|Events")
	void OnRoundLoadingCompleted(int32 SlotIndex);

	/** Called when a round is clicked (for unload) */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Magazine|Inspection|Events")
	void OnRoundClicked(int32 SlotIndex);

	/** Called when ammo is dropped (result) */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|Magazine|Inspection|Events")
	void OnAmmoDropResult(ESuspenseCoreMagazineDropResult Result);

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - ALL MANDATORY (BindWidget)
	// ═══════════════════════════════════════════════════════════════════════════

	// --- Header ---

	/** Close button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> CloseButton;

	/** Magazine name text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MagazineNameText;

	/** Magazine icon */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> MagazineIcon;

	/** Caliber text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CaliberText;

	// --- Round Slots Container ---

	/** Container for round slot widgets (WrapBox for grid layout) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWrapBox> RoundSlotsContainer;

	// --- Footer ---

	/** Current rounds text (e.g., "27/30") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> RoundsCountText;

	/** Fill progress bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> FillProgressBar;

	/** Hint text for drag&drop */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> HintText;

	// --- Drop Zone ---

	/** Drop zone border (highlights when dragging ammo over) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> DropZoneBorder;

	// --- Loading Indicator ---

	/** Loading operation progress bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> LoadingProgressBar;

	/** Loading operation text (e.g., "Loading round 15...") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> LoadingStatusText;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Widget class for individual round slot (Blueprint with slot visuals) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Magazine|Inspection|Config")
	TSubclassOf<UUserWidget> RoundSlotWidgetClass;

	/** Number of slots per row in grid */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Magazine|Inspection|Config", meta = (ClampMin = "5", ClampMax = "15"))
	int32 SlotsPerRow = 10;

	/** Hint text when magazine can accept ammo */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Magazine|Inspection|Config")
	FText DropHintText = NSLOCTEXT("MagInspection", "DropHint", "Drag ammo here to load");

	/** Hint text when magazine is full */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Magazine|Inspection|Config")
	FText FullHintText = NSLOCTEXT("MagInspection", "FullHint", "Magazine is full");

	/** Hint text when no compatible ammo */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Magazine|Inspection|Config")
	FText NoAmmoHintText = NSLOCTEXT("MagInspection", "NoAmmoHint", "No compatible ammunition");

	/** Loading status format */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Magazine|Inspection|Config")
	FText LoadingStatusFormat = NSLOCTEXT("MagInspection", "LoadingFormat", "Loading round {0}...");

	/** Unloading status format */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Magazine|Inspection|Config")
	FText UnloadingStatusFormat = NSLOCTEXT("MagInspection", "UnloadingFormat", "Unloading round {0}...");

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS
	// ═══════════════════════════════════════════════════════════════════════════

	void SetupEventSubscriptions();
	void TeardownEventSubscriptions();
	USuspenseCoreEventBus* GetEventBus() const;

	// EventBus handlers
	void OnAmmoLoadingStartedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnAmmoLoadingProgressEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnAmmoLoadingCompletedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnAmmoLoadingCancelledEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	void RebuildRoundSlots();
	void UpdateRoundSlot(int32 SlotIndex, const FSuspenseCoreRoundSlotData& SlotData);
	void UpdateFooterUI();
	void UpdateLoadingUI();
	void ClearRoundSlots();

	UFUNCTION()
	void OnCloseButtonClicked();

	void HandleRoundSlotClicked(int32 SlotIndex);

	/** Check if item is compatible ammo for this magazine */
	bool IsCompatibleAmmo(const struct FSuspenseCoreItemUIData& ItemData) const;

	/** Publish ammo load request to EventBus */
	void RequestAmmoLoad(FName AmmoID, int32 Quantity);

	// ═══════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	FSuspenseCoreSubscriptionHandle LoadingStartedHandle;
	FSuspenseCoreSubscriptionHandle LoadingProgressHandle;
	FSuspenseCoreSubscriptionHandle LoadingCompletedHandle;
	FSuspenseCoreSubscriptionHandle LoadingCancelledHandle;

	/** Cached inspection data */
	FSuspenseCoreMagazineInspectionData CachedInspectionData;

	/** Created round slot widgets */
	UPROPERTY()
	TArray<TObjectPtr<UUserWidget>> RoundSlotWidgets;

	/** Is inspection panel visible? */
	bool bIsVisible = false;

	/** Is loading operation in progress? */
	bool bIsLoadingInProgress = false;

	/** Current loading slot index */
	int32 LoadingSlotIndex = -1;

	/** Loading operation progress */
	float LoadingProgress = 0.0f;

	/** Loading operation total time */
	float LoadingTotalTime = 0.0f;

	/** Is unloading (vs loading)? */
	bool bIsUnloading = false;
};
