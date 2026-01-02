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
class UImage;
class UTextBlock;
class UProgressBar;
class UBorder;
class USuspenseCoreEventBus;

// Constant for number of quick slots
#define SUSPENSECORE_HUD_QUICKSLOT_COUNT 4

/**
 * USuspenseCoreQuickSlotHUDWidget
 *
 * HUD widget displaying 4 quick access slots on the game screen.
 * Shows magazines, consumables, medical items, and grenades.
 *
 * Features:
 * - Real-time updates via EventBus (NO direct component polling!)
 * - Hotkey indicators (4, 5, 6, 7)
 * - Cooldown visualization
 * - Magazine round count display
 * - Use animations
 * - NO programmatic color changes - ALL colors from materials in Editor!
 *
 * Layout (horizontal):
 * ┌─────┬─────┬─────┬─────┐
 * │ [4] │ [5] │ [6] │ [7] │  ← Hotkey
 * │ MAG │ MED │ GRN │     │  ← Item icon
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
class UISYSTEM_API USuspenseCoreQuickSlotHUDWidget : public UUserWidget, public ISuspenseCoreQuickSlotHUDWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreQuickSlotHUDWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

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

	/** Get slot data by index */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|QuickSlot")
	FSuspenseCoreQuickSlotHUDData GetSlotData(int32 SlotIndex) const;

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when a slot is updated */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|QuickSlot|Events")
	void OnSlotUpdated(int32 SlotIndex, const FSuspenseCoreQuickSlotHUDData& SlotData);

	/** Called when a slot is used */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|QuickSlot|Events")
	void OnSlotUsed(int32 SlotIndex);

	/** Called when cooldown starts */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|QuickSlot|Events")
	void OnSlotCooldownStarted(int32 SlotIndex, float Duration);

	/** Called when cooldown ends */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|QuickSlot|Events")
	void OnSlotCooldownEnded(int32 SlotIndex);

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Slot Container
	// ═══════════════════════════════════════════════════════════════════════════

	/** Container for all slot widgets */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UHorizontalBox> SlotContainer;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Slot 0 (QuickSlot1)
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> Slot0Border;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Slot0Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> Slot0QuantityText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> Slot0HotkeyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Slot0CooldownBar;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Slot 1 (QuickSlot2)
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> Slot1Border;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Slot1Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> Slot1QuantityText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> Slot1HotkeyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Slot1CooldownBar;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Slot 2 (QuickSlot3)
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> Slot2Border;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Slot2Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> Slot2QuantityText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> Slot2HotkeyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Slot2CooldownBar;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Slot 3 (QuickSlot4)
	// ═══════════════════════════════════════════════════════════════════════════

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> Slot3Border;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> Slot3Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> Slot3QuantityText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> Slot3HotkeyText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Slot3CooldownBar;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Default hotkey texts for slots */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|QuickSlot|Config")
	TArray<FText> DefaultHotkeyTexts = {
		FText::FromString(TEXT("4")),
		FText::FromString(TEXT("5")),
		FText::FromString(TEXT("6")),
		FText::FromString(TEXT("7"))
	};

	/** Empty slot texture */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|QuickSlot|Config")
	TObjectPtr<UTexture2D> EmptySlotTexture;

	/** Enable smooth cooldown interpolation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|QuickSlot|Config")
	bool bSmoothCooldown = true;

	/** Cooldown bar interpolation speed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|QuickSlot|Config", meta = (EditCondition = "bSmoothCooldown"))
	float CooldownInterpSpeed = 10.0f;

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Setup EventBus subscriptions */
	void SetupEventSubscriptions();

	/** Teardown EventBus subscriptions */
	void TeardownEventSubscriptions();

	/** Get EventBus */
	USuspenseCoreEventBus* GetEventBus() const;

	// EventBus handlers
	void OnQuickSlotAssignedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnQuickSlotClearedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnQuickSlotUsedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnQuickSlotCooldownStartedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnQuickSlotCooldownEndedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
	void OnMagazineRoundsChangedEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL HELPERS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Get UI elements for a slot index */
	void GetSlotElements(int32 SlotIndex, UBorder*& OutBorder, UImage*& OutIcon,
		UTextBlock*& OutQuantity, UTextBlock*& OutHotkey, UProgressBar*& OutCooldown) const;

	/** Update single slot UI */
	void UpdateSlotUI(int32 SlotIndex, const FSuspenseCoreQuickSlotHUDData& SlotData);

	/** Update cooldown progress bars */
	void UpdateCooldowns(float DeltaTime);

	/** Validate slot index */
	bool IsValidSlotIndex(int32 SlotIndex) const { return SlotIndex >= 0 && SlotIndex < SUSPENSECORE_HUD_QUICKSLOT_COUNT; }

	// ═══════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Cached EventBus */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** EventBus subscription handles */
	FSuspenseCoreSubscriptionHandle QuickSlotAssignedHandle;
	FSuspenseCoreSubscriptionHandle QuickSlotClearedHandle;
	FSuspenseCoreSubscriptionHandle QuickSlotUsedHandle;
	FSuspenseCoreSubscriptionHandle QuickSlotCooldownStartedHandle;
	FSuspenseCoreSubscriptionHandle QuickSlotCooldownEndedHandle;
	FSuspenseCoreSubscriptionHandle MagazineRoundsChangedHandle;

	/** Cached slot data */
	TArray<FSuspenseCoreQuickSlotHUDData> CachedSlotData;

	/** Current cooldown display values (for interpolation) */
	TArray<float> DisplayedCooldownProgress;

	/** Target cooldown progress values */
	TArray<float> TargetCooldownProgress;

	/** Owner actor reference */
	UPROPERTY()
	TWeakObjectPtr<AActor> OwnerActor;

	/** Currently highlighted slot (-1 = none) */
	int32 HighlightedSlotIndex = INDEX_NONE;

	/** Is widget initialized? */
	bool bIsInitialized = false;
};
