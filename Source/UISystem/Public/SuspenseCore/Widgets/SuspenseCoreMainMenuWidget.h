// SuspenseCoreMainMenuWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/SuspenseCorePlayerData.h"
#include "SuspenseCoreCharacterSelectWidget.h"
#include "SuspenseCoreMainMenuWidget.generated.h"

class UTextBlock;
class UButton;
class UWidgetSwitcher;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTextureRenderTarget2D;
class USkeletalMesh;
class UAnimInstance;
class USuspenseCorePlayerInfoWidget;
class USuspenseCoreRegistrationWidget;
class USuspenseCoreCharacterSelectWidget;
class USuspenseCoreEventBus;
class ISuspenseCorePlayerRepository;
class ASuspenseCoreCharacterPreviewActor;

/**
 * USuspenseCoreMainMenuWidget
 *
 * Main menu widget that handles the complete menu flow:
 * - Registration screen for new players (Index 0)
 * - Main menu panel with character select, player info and Play button (Index 1)
 *
 * Screen Flow:
 * - Start → No saves? → Registration (Index 0)
 * - Start → Has saves? → Main Menu Panel (Index 1)
 * - Registration → Success → Main Menu Panel (Index 1)
 * - Main Menu Panel → Character Select embedded
 * - Main Menu Panel → Play → Game Map
 *
 * Save Location: [Project]/Saved/Players/[PlayerId].json
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
	 * Show character select screen (legacy - redirects to ShowMainMenuPanel).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MainMenu")
	void ShowCharacterSelectScreen();

	/**
	 * Show registration screen (Index 0).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MainMenu")
	void ShowRegistrationScreen();

	/**
	 * Show main menu panel with CharacterSelect + PlayerInfo (Index 1).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MainMenu")
	void ShowMainMenuPanel();

	/**
	 * Show main menu screen with player data (legacy - calls SelectPlayer + ShowMainMenuPanel).
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MainMenu")
	void ShowMainMenuScreen(const FString& PlayerId);

	/**
	 * Select a player and update PlayerInfo display.
	 * Does not switch screens.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|MainMenu")
	void SelectPlayer(const FString& PlayerId);

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

	/** Registration widget for new players */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	USuspenseCoreRegistrationWidget* RegistrationWidget;

	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS - Main Menu Panel (Index 1)
	// Contains: Character Select + Player Info + Buttons
	// ═══════════════════════════════════════════════════════════════════════════

	/** Character select widget (embedded in Main Menu Panel) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	USuspenseCoreCharacterSelectWidget* CharacterSelectWidget;

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
	// UI BINDINGS - Character Preview (Render Target)
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Image widget for displaying character render target.
	 * Assign render target material to display the character preview.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* CharacterPreviewImage;

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

	/** Index of main menu panel (with CharacterSelect + PlayerInfo) in WidgetSwitcher */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	int32 MainMenuScreenIndex = 1;

	/**
	 * GameMode class path for game maps.
	 * CRITICAL: Must be set for GameMode switching to work!
	 * Format: /Game/Blueprints/GameModes/BP_SuspenseCoreGameMode.BP_SuspenseCoreGameMode_C
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Config|GameMode")
	FString GameGameModePath;

	/**
	 * GameMode class path for menu maps.
	 * CRITICAL: Must be set for GameMode switching to work!
	 * Format: /Game/Blueprints/GameModes/BP_SuspenseCoreMenuGameMode.BP_SuspenseCoreMenuGameMode_C
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Config|GameMode")
	FString MenuGameModePath;

	// ═══════════════════════════════════════════════════════════════════════════
	// CHARACTER PREVIEW CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Base material for character preview display.
	 * Must have a TextureParameter named 'RenderTargetTexture'.
	 * If not set, will create a simple unlit material.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Config|CharacterPreview")
	UMaterialInterface* CharacterPreviewBaseMaterial;

	/**
	 * Class of preview actor to spawn.
	 * Default: ASuspenseCoreCharacterPreviewActor
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Config|CharacterPreview")
	TSubclassOf<ASuspenseCoreCharacterPreviewActor> PreviewActorClass;

	/**
	 * Spawn location for preview actor (should be hidden under map).
	 * Default: (0, 0, -10000)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Config|CharacterPreview")
	FVector PreviewActorSpawnLocation = FVector(0.0f, 0.0f, -10000.0f);

	/**
	 * Skeletal mesh to use for preview.
	 * If not set, will try to copy from player character.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Config|CharacterPreview")
	USkeletalMesh* PreviewMesh;

	/**
	 * Animation blueprint class for preview mesh.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "SuspenseCore|Config|CharacterPreview")
	TSubclassOf<UAnimInstance> PreviewAnimClass;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL STATE
	// ═══════════════════════════════════════════════════════════════════════════

	/** Current player ID */
	FString CurrentPlayerId;

	/** Cached player data */
	FSuspenseCorePlayerData CachedPlayerData;

	/** EventBus subscription handle for registration */
	FSuspenseCoreSubscriptionHandle RegistrationEventHandle;

	/** EventBus subscription handle for character select */
	FSuspenseCoreSubscriptionHandle CharacterSelectEventHandle;

	/** EventBus subscription handle for create new character */
	FSuspenseCoreSubscriptionHandle CreateNewCharacterEventHandle;

	/** EventBus subscription handle for character highlighted */
	FSuspenseCoreSubscriptionHandle CharacterHighlightedEventHandle;

	/** EventBus subscription handle for character deleted */
	FSuspenseCoreSubscriptionHandle CharacterDeletedEventHandle;

	/** Cached EventBus */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** EventBus subscription handle for render target ready */
	FSuspenseCoreSubscriptionHandle RenderTargetReadyEventHandle;

	/** Dynamic material instance for character preview */
	UPROPERTY()
	UMaterialInstanceDynamic* CharacterPreviewMaterial;

	/** Cached render target from character (received via EventBus) */
	UPROPERTY()
	UTextureRenderTarget2D* CachedRenderTarget;

	/** Spawned preview actor */
	UPROPERTY()
	TWeakObjectPtr<ASuspenseCoreCharacterPreviewActor> SpawnedPreviewActor;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Setup button click handlers */
	void SetupButtonBindings();

	/** Subscribe to EventBus events (primary inter-widget communication per architecture docs) */
	void SetupEventSubscriptions();

	/** Unsubscribe from events */
	void TeardownEventSubscriptions();

	/** Get player repository */
	ISuspenseCorePlayerRepository* GetRepository();

	/** Get or create repository */
	ISuspenseCorePlayerRepository* GetOrCreateRepository();

	/** Update UI elements */
	void UpdateUIDisplay();

	/** Update character preview image with render target (received via EventBus) */
	void UpdateCharacterPreviewImage(UTextureRenderTarget2D* RenderTarget);

	/** Clear character preview */
	void ClearCharacterPreview();

	/** Spawn preview actor */
	void SpawnPreviewActor();

	/** Destroy preview actor */
	void DestroyPreviewActor();

	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTBUS HANDLERS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Handle registration success event */
	void OnRegistrationSuccess(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle character selected event */
	void OnCharacterSelected(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle create new character event */
	void OnCreateNewCharacter(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle character highlighted event (updates PlayerInfo) */
	void OnCharacterHighlighted(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle character deleted event */
	void OnCharacterDeleted(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

	/** Handle render target ready event from character */
	void OnRenderTargetReady(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);

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
