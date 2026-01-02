// SuspenseCoreQuickSlotEntry.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreQuickSlotHUDWidget.h"
#include "SuspenseCoreQuickSlotEntry.generated.h"

// Forward declarations
class UImage;
class UTextBlock;
class UProgressBar;
class UBorder;

/**
 * USuspenseCoreQuickSlotEntry
 *
 * Single QuickSlot entry component - SRP compliant.
 * Used by SuspenseCoreQuickSlotHUDWidget for procedural slot generation.
 *
 * Layout:
 * ┌─────────────┐
 * │    [4]      │  ← Hotkey
 * │   [ICON]    │  ← Item icon
 * │    30       │  ← Quantity/Rounds
 * │ ░░░░░░░░░░  │  ← Cooldown bar
 * └─────────────┘
 *
 * All components are MANDATORY (BindWidget).
 * NO programmatic colors - all from materials in Editor!
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreQuickSlotEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreQuickSlotEntry(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/** Initialize slot with index and hotkey */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|QuickSlot")
	void InitializeSlot(int32 InSlotIndex, const FText& InHotkeyText);

	/** Update slot with data */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|QuickSlot")
	void UpdateSlotData(const FSuspenseCoreQuickSlotHUDData& SlotData);

	/** Clear slot to empty state */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|QuickSlot")
	void ClearSlot();

	/** Update cooldown display */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|QuickSlot")
	void UpdateCooldown(float Progress);

	/** Set cooldown target for interpolation */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|QuickSlot")
	void SetCooldownTarget(float RemainingTime, float TotalTime);

	/** Update magazine rounds display */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|QuickSlot")
	void UpdateMagazineRounds(int32 CurrentRounds, int32 MaxRounds);

	/** Set slot availability state */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|QuickSlot")
	void SetAvailable(bool bAvailable);

	/** Set slot highlighted state */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|QuickSlot")
	void SetHighlighted(bool bHighlighted);

	/** Play use animation */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|QuickSlot")
	void PlayUseAnimation();

	/** Get slot index */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|QuickSlot")
	int32 GetSlotIndex() const { return SlotIndex; }

	/** Is slot empty? */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|QuickSlot")
	bool IsEmpty() const { return bIsEmpty; }

	/** Is slot on cooldown? */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|QuickSlot")
	bool IsOnCooldown() const { return bIsOnCooldown; }

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when slot is used (for animations) */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|QuickSlot|Events")
	void OnSlotUsedBP();

	/** Called when cooldown starts */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|QuickSlot|Events")
	void OnCooldownStartedBP(float Duration);

	/** Called when cooldown ends */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|QuickSlot|Events")
	void OnCooldownEndedBP();

	/** Called when availability changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|QuickSlot|Events")
	void OnAvailabilityChangedBP(bool bNewAvailable);

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - ALL MANDATORY
	// ═══════════════════════════════════════════════════════════════════════════

	/** Slot border/background */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UBorder> SlotBorder;

	/** Item icon */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> ItemIcon;

	/** Quantity/Rounds text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> QuantityText;

	/** Hotkey text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> HotkeyText;

	/** Cooldown progress bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> CooldownBar;

	/** Highlight overlay */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> HighlightOverlay;

	/** Unavailable overlay */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> UnavailableOverlay;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Empty slot texture */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|QuickSlot|Config")
	TObjectPtr<UTexture2D> EmptySlotTexture;

	/** Smooth cooldown interpolation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|QuickSlot|Config")
	bool bSmoothCooldown = true;

	/** Cooldown interpolation speed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|QuickSlot|Config")
	float CooldownInterpSpeed = 10.0f;

private:
	// ═══════════════════════════════════════════════════════════════════════════
	// STATE
	// ═══════════════════════════════════════════════════════════════════════════

	int32 SlotIndex = INDEX_NONE;

	float DisplayedCooldown = 0.0f;
	float TargetCooldown = 0.0f;
	float TotalCooldownTime = 0.0f;

	bool bIsEmpty = true;
	bool bIsOnCooldown = false;
	bool bIsAvailable = true;
	bool bIsHighlighted = false;
};
