// SuspenseCoreMainMenuWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/SuspenseCorePlayerData.h"
#include "SuspenseCoreMainMenuWidget.generated.h"

class UTextBlock;
class UButton;
class UWidgetSwitcher;
class UImage;
class USuspenseCorePlayerInfoWidget;
class USuspenseCoreRegistrationWidget;
class USuspenseCoreEventBus;
class ISuspenseCorePlayerRepository;

/**
 * USuspenseCoreMainMenuWidget
 *
 * Main menu widget that handles the complete menu flow:
 * - Registration screen for new players
 * - Main menu with player info and Play button
 * - Transitions between screens via EventBus
 *
 * Pure C++ implementation with Blueprint bindings.
 */
UCLASS()
class UISYSTEM_API USuspenseCoreMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreMainMenuWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Initialize and show appropriate screen based on saved data.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MainMenu")
	void InitializeMenu();

	/**
	 * Show registration screen.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MainMenu")
	void ShowRegistrationScreen();

	/**
	 * Show main menu screen with player data.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MainMenu")
	void ShowMainMenuScreen(const FString& PlayerId);

	/**
	 * Transition to game map.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MainMenu")
	void TransitionToGame();

	/**
	 * Get current player ID.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MainMenu")
	FString GetCurrentPlayerId() const { return CurrentPlayerId; }

	/**
	 * Check if player data exists.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MainMenu")
	bool HasExistingPlayer() const;

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Main Container
	// ═══════════════════════════════════════════════════════════════════════════

	/** Widget switcher for screen transitions */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UWidgetSwitcher* ScreenSwitcher;

	/** Background image */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* BackgroundImage;

	/** Game title text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* GameTitleText;

	/** Version text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* VersionText;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Registration Screen (Index 0)
	// ═══════════════════════════════════════════════════════════════════════════

	/** Registration widget (embedded or referenced) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	USuspenseCoreRegistrationWidget* RegistrationWidget;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Main Menu Screen (Index 1)
	// ═══════════════════════════════════════════════════════════════════════════

	/** Player info widget */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	USuspenseCorePlayerInfoWidget* PlayerInfoWidget;

	/** Play button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* PlayButton;

	/** Play button text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* PlayButtonText;

	/** Operators button (future - character select) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* OperatorsButton;

	/** Settings button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* SettingsButton;

	/** Quit button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* QuitButton;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Name of the game map to load when Play is clicked */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	FName GameMapName = TEXT("GameMap");

	/** Name of the character select map (future) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	FName CharacterSelectMapName = TEXT("CharacterSelectMap");

	/** Game title to display */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	FText GameTitle = FText::FromString(TEXT("SUSPENSE"));

	/** Version string */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	FText VersionString = FText::FromString(TEXT("v0.1.0 Alpha"));

	/** Index of registration screen in WidgetSwitcher */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	int32 RegistrationScreenIndex = 0;

	/** Index of main menu screen in WidgetSwitcher */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	int32 MainMenuScreenIndex = 1;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Current player ID */
	FString CurrentPlayerId;

	/** Cached player data */
	FSuspenseCorePlayerData CachedPlayerData;

	/** EventBus subscription handle */
	FSuspenseCoreSubscriptionHandle RegistrationEventHandle;

	/** Cached EventBus */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Setup button click handlers */
	void SetupButtonBindings();

	/** Subscribe to EventBus events */
	void SetupEventSubscriptions();

	/** Unsubscribe from events */
	void TeardownEventSubscriptions();

	/** Get player repository */
	ISuspenseCorePlayerRepository* GetRepository();

	/** Get or create repository */
	ISuspenseCorePlayerRepository* GetOrCreateRepository();

	/** Update UI elements */
	void UpdateUIDisplay();

	/** Handle registration success event */
	void OnRegistrationSuccess(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	// ═══════════════════════════════════════════════════════════════════════════
	// BUTTON HANDLERS
	// ═══════════════════════════════════════════════════════════════════════════

	UFUNCTION()
	void OnPlayButtonClicked();

	UFUNCTION()
	void OnOperatorsButtonClicked();

	UFUNCTION()
	void OnSettingsButtonClicked();

	UFUNCTION()
	void OnQuitButtonClicked();

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when transitioning to game */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|MainMenu")
	void OnTransitionToGame();

	/** Called when registration completes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|MainMenu")
	void OnRegistrationComplete(const FString& PlayerId);

	/** Called when screen changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "SuspenseCore|MainMenu")
	void OnScreenChanged(int32 NewScreenIndex);
};
