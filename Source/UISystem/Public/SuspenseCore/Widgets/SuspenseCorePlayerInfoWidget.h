// SuspenseCorePlayerInfoWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCore/SuspenseCorePlayerData.h"
#include "SuspenseCorePlayerInfoWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
class UButton;
class USuspenseCoreEventBus;
class ISuspenseCorePlayerRepository;
struct FSuspenseCoreSubscriptionHandle;

/**
 * USuspenseCorePlayerInfoWidget
 *
 * Displays player information from the repository.
 * Pure C++ implementation for testing player data persistence.
 *
 * Features:
 * - Display player ID, name, level, XP
 * - Show currency (soft/hard)
 * - Show stats (K/D, wins, etc.)
 * - Auto-refresh from repository
 * - EventBus updates for real-time changes
 */
UCLASS()
class UISYSTEM_API USuspenseCorePlayerInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCorePlayerInfoWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Load and display player data by ID.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|PlayerInfo")
	void LoadPlayerData(const FString& PlayerId);

	/**
	 * Display provided player data directly.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|PlayerInfo")
	void DisplayPlayerData(const FSuspenseCorePlayerData& PlayerData);

	/**
	 * Refresh current player data from repository.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|PlayerInfo")
	void RefreshData();

	/**
	 * Clear displayed data.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|PlayerInfo")
	void ClearDisplay();

	/**
	 * Get currently displayed player ID.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|PlayerInfo")
	FString GetCurrentPlayerId() const { return CurrentPlayerId; }

	/**
	 * Is player data loaded?
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|PlayerInfo")
	bool HasPlayerData() const { return !CurrentPlayerId.IsEmpty(); }

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Player display name */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* DisplayNameText;

	/** Player ID (truncated for display) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* PlayerIdText;

	/** Player level */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* LevelText;

	/** XP progress bar */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UProgressBar* XPProgressBar;

	/** XP text (current/needed) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* XPText;

	/** Soft currency amount */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* SoftCurrencyText;

	/** Hard currency amount */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* HardCurrencyText;

	/** Stats: Kills */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* KillsText;

	/** Stats: Deaths */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* DeathsText;

	/** Stats: K/D Ratio */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* KDRatioText;

	/** Stats: Wins */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* WinsText;

	/** Stats: Matches */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* MatchesText;

	/** Stats: Playtime */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* PlaytimeText;

	/** Refresh button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* RefreshButton;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Auto-refresh interval (0 = disabled) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	float AutoRefreshInterval = 0.0f;

	/** Subscribe to EventBus for real-time updates */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	bool bSubscribeToEvents = true;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════════════════

	/** Currently displayed player ID */
	FString CurrentPlayerId;

	/** Cached player data */
	FSuspenseCorePlayerData CachedPlayerData;

	/** EventBus subscription handle */
	FSuspenseCoreSubscriptionHandle ProgressionEventHandle;

	/** Cached EventBus */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Auto-refresh timer handle */
	FTimerHandle AutoRefreshTimerHandle;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Setup button handlers */
	void SetupButtonBindings();

	/** Subscribe to EventBus events */
	void SetupEventSubscriptions();

	/** Unsubscribe from events */
	void TeardownEventSubscriptions();

	/** Get player repository */
	ISuspenseCorePlayerRepository* GetRepository();

	/** Update all UI elements from cached data */
	void UpdateUIFromData();

	/** Format large numbers (1234567 -> 1.2M) */
	FString FormatLargeNumber(int64 Value) const;

	/** Format playtime (seconds -> HH:MM:SS or Xh Ym) */
	FString FormatPlaytime(int64 Seconds) const;

	/** Handle refresh button click */
	UFUNCTION()
	void OnRefreshButtonClicked();

	/** Handle progression event from EventBus */
	void OnProgressionEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Start auto-refresh timer */
	void StartAutoRefresh();

	/** Stop auto-refresh timer */
	void StopAutoRefresh();
};
