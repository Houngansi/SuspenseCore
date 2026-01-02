// SuspenseCoreQuickSlotHUDWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreQuickSlotHUDWidget.h"
#include "SuspenseCoreQuickSlotHUDWidget.generated.h"

// Forward declarations
class UHorizontalBox;
class USuspenseCoreEventBus;
class USuspenseCoreQuickSlotEntry;

/**
 * USuspenseCoreQuickSlotHUDWidget
 *
 * HUD widget displaying quick access slots on the game screen.
 * PROCEDURALLY generates slot entries using SuspenseCoreQuickSlotEntry.
 *
 * Features:
 * - Procedural slot generation (SRP/OOP compliant)
 * - Real-time updates via EventBus (NO direct component polling!)
 * - Configurable slot count
 * - NO programmatic color changes - ALL colors from materials in Editor!
 *
 * Layout (horizontal):
 * ┌─────┬─────┬─────┬─────┐
 * │ [4] │ [5] │ [6] │ [7] │  ← Hotkeys
 * │ MAG │ MED │ GRN │     │  ← Item icons
 * │ 30  │ x2  │ x3  │     │  ← Quantity/Rounds
 * └─────┴─────┴─────┴─────┘
 *
 * EventBus Events (subscribed):
 * - TAG_Equipment_Event_QuickSlot_Assigned
 * - TAG_Equipment_Event_QuickSlot_Cleared
 * - TAG_Equipment_Event_QuickSlot_Used
 * - TAG_Equipment_Event_QuickSlot_CooldownStarted
 * - TAG_Equipment_Event_QuickSlot_CooldownEnded
 * - TAG_Equipment_Event_Magazine_RoundsChanged
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreQuickSlotHUDWidget : public UUserWidget, public ISuspenseCoreQuickSlotHUDWidgetInterface
{
	GENERATED_BODY()

public:
	USuspenseCoreQuickSlotHUDWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// ISuspenseCoreQuickSlotHUDWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void InitializeQuickSlots_Implementation(AActor* OwnerActor) override;
	virtual void CleanupQuickSlots_Implementation() override;
	virtual void UpdateSlot_Implementation(const FSuspenseCoreQuickSlotHUDData& SlotData) override;
	virtual void UpdateAllSlots_Implementation(const TArray<FSuspenseCoreQuickSlotHUDData>& AllSlots) override;
	virtual void ClearSlot_Implementation(int32 SlotIndex) override;
	virtual void PlaySlotUseAnimation_Implementation(int32 SlotIndex) override;
	virtual void UpdateSlotCooldown_Implementation(int32 SlotIndex, float RemainingTime, float TotalTime) override;
	virtual void HighlightSlot_Implementation(int32 SlotIndex) override;
	virtual void SetSlotAvailability_Implementation(int32 SlotIndex, bool bAvailable) override;
	virtual void UpdateMagazineRounds_Implementation(int32 SlotIndex, int32 CurrentRounds, int32 MaxRounds) override;
	virtual void SetQuickSlotHUDVisible_Implementation(bool bVisible) override;
	virtual bool IsQuickSlotHUDVisible_Implementation() const override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/** Force refresh all slots from component */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|QuickSlot")
	void RefreshAllSlots();

	/** Get slot entry by index */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|QuickSlot")
	USuspenseCoreQuickSlotEntry* GetSlotEntry(int32 SlotIndex) const;

	/** Get total slot count */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|QuickSlot")
	int32 GetSlotCount() const { return SlotCount; }

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when a slot is updated */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|QuickSlot|Events")
	void OnSlotUpdated(int32 SlotIndex, const FSuspenseCoreQuickSlotHUDData& SlotData);

	/** Called when a slot is used */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|QuickSlot|Events")
	void OnSlotUsed(int32 SlotIndex);

	/** Called when all slots are generated */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|QuickSlot|Events")
	void OnSlotsGenerated(int32 TotalSlots);

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - ALL MANDATORY
	// ═══════════════════════════════════════════════════════════════════════════

	/** Container for procedurally generated slot entries */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> SlotContainer;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Number of slots to generate */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|QuickSlot|Config")
	int32 SlotCount = 4;

	/** Slot entry widget class to spawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|QuickSlot|Config")
	TSubclassOf<USuspenseCoreQuickSlotEntry> SlotEntryClass;

	/** Hotkey texts for each slot (index-based) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|QuickSlot|Config")
	TArray<FText> HotkeyTexts = {
		FText::FromString(TEXT("4")),
		FText::FromString(TEXT("5")),
		FText::FromString(TEXT("6")),
		FText::FromString(TEXT("7"))
	};

	/** Spacing between slots */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|QuickSlot|Config")
	float SlotSpacing = 5.0f;

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// PROCEDURAL GENERATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Generate slot entries */
	void GenerateSlotEntries();

	/** Clear all generated slots */
	void ClearGeneratedSlots();

	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS
	// ═══════════════════════════════════════════════════════════════════════════

	void SetupEventSubscriptions();
	void TeardownEventSubscriptions();
	USuspenseCoreEventBus* GetEventBus() const;

	// EventBus handlers
	void OnQuickSlotAssignedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnQuickSlotClearedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnQuickSlotUsedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnQuickSlotCooldownStartedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnQuickSlotCooldownEndedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnMagazineRoundsChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	// ═══════════════════════════════════════════════════════════════════════════
	// HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	bool IsValidSlotIndex(int32 SlotIndex) const { return SlotIndex >= 0 && SlotIndex < SlotEntries.Num(); }

	// ═══════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Generated slot entries */
	UPROPERTY()
	TArray<TObjectPtr<USuspenseCoreQuickSlotEntry>> SlotEntries;

	FSuspenseCoreSubscriptionHandle QuickSlotAssignedHandle;
	FSuspenseCoreSubscriptionHandle QuickSlotClearedHandle;
	FSuspenseCoreSubscriptionHandle QuickSlotUsedHandle;
	FSuspenseCoreSubscriptionHandle QuickSlotCooldownStartedHandle;
	FSuspenseCoreSubscriptionHandle QuickSlotCooldownEndedHandle;
	FSuspenseCoreSubscriptionHandle MagazineRoundsChangedHandle;

	UPROPERTY()
	TWeakObjectPtr<AActor> OwnerActor;

	int32 HighlightedSlotIndex = INDEX_NONE;
	bool bIsInitialized = false;
};
