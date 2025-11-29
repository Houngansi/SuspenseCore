// SuspenseCorePauseMenuWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCorePauseMenuWidget.generated.h"

class UButton;
class UTextBlock;
class UWidgetSwitcher;
class USuspenseCoreSaveManager;
class USuspenseCoreSaveLoadMenuWidget;
enum class ESuspenseCoreSaveLoadMode : uint8;

/**
 * USuspenseCorePauseMenuWidget
 *
 * In-game pause menu with:
 * - Continue (Resume)
 * - Save
 * - Load
 * - Exit to Lobby (Character Select)
 * - Quit Game
 *
 * Activate with ESC key.
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCorePauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCorePauseMenuWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════
	// UI BINDINGS
	// ═══════════════════════════════════════════════════════════════

	/** Title text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TitleText;

	/** Continue/Resume button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* ContinueButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* ContinueButtonText;

	/** Save button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* SaveButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* SaveButtonText;

	/** Load button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* LoadButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* LoadButtonText;

	/** Exit to Lobby button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* ExitToLobbyButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* ExitToLobbyButtonText;

	/** Quit Game button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* QuitButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* QuitButtonText;

	/** Save status text (shows "Saving..." or "Saved!") */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* SaveStatusText;

	// ═══════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PauseMenu|Config")
	FText Title = FText::FromString(TEXT("PAUSED"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PauseMenu|Config")
	FText ContinueText = FText::FromString(TEXT("CONTINUE"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PauseMenu|Config")
	FText SaveText = FText::FromString(TEXT("SAVE GAME"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PauseMenu|Config")
	FText LoadText = FText::FromString(TEXT("LOAD GAME"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PauseMenu|Config")
	FText ExitToLobbyText = FText::FromString(TEXT("EXIT TO LOBBY"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PauseMenu|Config")
	FText QuitText = FText::FromString(TEXT("QUIT GAME"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PauseMenu|Config")
	FText SavingText = FText::FromString(TEXT("Saving..."));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PauseMenu|Config")
	FText SavedText = FText::FromString(TEXT("Saved!"));

	/** Map to load when exiting to lobby */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PauseMenu|Config")
	FName LobbyMapName = TEXT("MainMenuMap");

	/**
	 * GameMode class path for menu maps (used when exiting to lobby).
	 * CRITICAL: Must be set for GameMode switching to work!
	 * Format: /Game/Blueprints/GameModes/BP_SuspenseCoreMenuGameMode.BP_SuspenseCoreMenuGameMode_C
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PauseMenu|Config|GameMode")
	FString MenuGameModePath;

	/**
	 * GameMode class path for game maps.
	 * CRITICAL: Must be set for GameMode switching to work!
	 * Format: /Game/Blueprints/GameModes/BP_SuspenseCoreGameMode.BP_SuspenseCoreGameMode_C
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PauseMenu|Config|GameMode")
	FString GameGameModePath;

	/**
	 * Save/Load menu widget class.
	 * If set, clicking Save/Load buttons will open this menu instead of quick save/load.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PauseMenu|Config|SaveLoad")
	TSubclassOf<USuspenseCoreSaveLoadMenuWidget> SaveLoadMenuWidgetClass;

	// ═══════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════

	/**
	 * Show pause menu and pause game.
	 */
	UFUNCTION(BlueprintCallable, Category = "PauseMenu")
	void ShowPauseMenu();

	/**
	 * Hide pause menu and resume game.
	 */
	UFUNCTION(BlueprintCallable, Category = "PauseMenu")
	void HidePauseMenu();

	/**
	 * Toggle pause menu visibility.
	 */
	UFUNCTION(BlueprintCallable, Category = "PauseMenu")
	void TogglePauseMenu();

	/**
	 * Check if menu is visible.
	 */
	UFUNCTION(BlueprintCallable, Category = "PauseMenu")
	bool IsMenuVisible() const { return bIsVisible; }

	/**
	 * Quick save (F5 binding).
	 */
	UFUNCTION(BlueprintCallable, Category = "PauseMenu")
	void QuickSave();

	/**
	 * Quick load (F9 binding).
	 */
	UFUNCTION(BlueprintCallable, Category = "PauseMenu")
	void QuickLoad();

	// ═══════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════

	/** Called when menu is shown */
	UFUNCTION(BlueprintImplementableEvent, Category = "PauseMenu|Events")
	void OnMenuShown();

	/** Called when menu is hidden */
	UFUNCTION(BlueprintImplementableEvent, Category = "PauseMenu|Events")
	void OnMenuHidden();

	/** Called before exiting to lobby */
	UFUNCTION(BlueprintImplementableEvent, Category = "PauseMenu|Events")
	void OnExitToLobby();

	/** Called before quitting game */
	UFUNCTION(BlueprintImplementableEvent, Category = "PauseMenu|Events")
	void OnQuitGame();

	// ═══════════════════════════════════════════════════════════════
	// DELEGATES
	// ═══════════════════════════════════════════════════════════════

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPauseMenuToggled);

	UPROPERTY(BlueprintAssignable, Category = "PauseMenu|Events")
	FOnPauseMenuToggled OnPauseMenuShown;

	UPROPERTY(BlueprintAssignable, Category = "PauseMenu|Events")
	FOnPauseMenuToggled OnPauseMenuHidden;

protected:
	// ═══════════════════════════════════════════════════════════════
	// UUserWidget
	// ═══════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// ═══════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════

	/** Is menu currently visible */
	bool bIsVisible = false;

	/** Cached save manager */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreSaveManager> CachedSaveManager;

	/** Created Save/Load menu widget */
	UPROPERTY()
	USuspenseCoreSaveLoadMenuWidget* SaveLoadMenuWidget;

	/** Setup button bindings */
	void SetupButtonBindings();

	/** Create Save/Load menu widget */
	void CreateSaveLoadMenu();

	/** Show Save/Load menu in specified mode */
	void ShowSaveLoadMenu(ESuspenseCoreSaveLoadMode Mode);

	/** Called when Save/Load menu closes */
	UFUNCTION()
	void OnSaveLoadMenuClosed();

	/** Update UI text */
	void UpdateUIDisplay();

	/** Set game paused state */
	void SetGamePaused(bool bPaused);

	/** Show save status */
	void ShowSaveStatus(bool bSaving);

	/** Hide save status after delay */
	void HideSaveStatusAfterDelay();

	/** Get save manager */
	USuspenseCoreSaveManager* GetSaveManager();

	// ═══════════════════════════════════════════════════════════════
	// BUTTON HANDLERS
	// ═══════════════════════════════════════════════════════════════

	UFUNCTION()
	void OnContinueButtonClicked();

	UFUNCTION()
	void OnSaveButtonClicked();

	UFUNCTION()
	void OnLoadButtonClicked();

	UFUNCTION()
	void OnExitToLobbyButtonClicked();

	UFUNCTION()
	void OnQuitButtonClicked();

	// ═══════════════════════════════════════════════════════════════
	// SAVE CALLBACKS
	// ═══════════════════════════════════════════════════════════════

	UFUNCTION()
	void OnSaveCompleted(bool bSuccess, const FString& ErrorMessage);

	UFUNCTION()
	void OnLoadCompleted(bool bSuccess, const FString& ErrorMessage);

	/** Timer for hiding status */
	FTimerHandle StatusHideTimer;
};
