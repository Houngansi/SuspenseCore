// SuspenseCoreRegistrationWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCore/SuspenseCorePlayerData.h"
#include "SuspenseCoreRegistrationWidget.generated.h"

class UEditableTextBox;
class UButton;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class UImage;
class UBorder;
class UPanelWidget;
class USuspenseCoreEventBus;
class ISuspenseCorePlayerRepository;
class USuspenseCoreCharacterClassData;
class USuspenseCoreClassSelectionButtonWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSuspenseCoreOnRegistrationComplete, const FSuspenseCorePlayerData&, PlayerData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSuspenseCoreOnRegistrationError, const FString&, ErrorMessage);

/**
 * USuspenseCoreRegistrationWidget
 *
 * Pure C++ registration screen for player creation.
 * Tests the player repository system.
 *
 * Features:
 * - Player name input
 * - Display name input
 * - Create account button
 * - EventBus integration for notifications
 * - Repository pattern for persistence
 *
 * Usage in Blueprint:
 * 1. Create Widget BP inheriting from this class
 * 2. Bind UI elements (EditableTextBox, Button, TextBlock)
 * 3. Use BindWidget meta for auto-binding
 */
UCLASS()
class UISYSTEM_API USuspenseCoreRegistrationWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreRegistrationWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Attempt to create a new player account.
	 * Called when Create button is pressed.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Registration")
	void AttemptCreatePlayer();

	/**
	 * Validate the current input fields.
	 * @return true if all fields are valid
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Registration")
	bool ValidateInput() const;

	/**
	 * Get the entered display name.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Registration")
	FString GetEnteredDisplayName() const;

	/**
	 * Get the selected character class ID.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Registration")
	FString GetSelectedClassId() const;

	/**
	 * Select a character class by ID.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Registration")
	void SelectClass(const FString& ClassId);

	/**
	 * Clear all input fields.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Registration")
	void ClearInputFields();

	/**
	 * Show error message to user.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Registration")
	void ShowError(const FString& Message);

	/**
	 * Show success message to user.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Registration")
	void ShowSuccess(const FString& Message);

	/**
	 * Set the repository to use for player creation.
	 * If not set, will attempt to get from ServiceLocator.
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Registration")
	void SetPlayerRepository(TScriptInterface<ISuspenseCorePlayerRepository> InRepository);

	// ═══════════════════════════════════════════════════════════════════════════
	// EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when registration is successful */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Events")
	FSuspenseCoreOnRegistrationComplete OnRegistrationComplete;

	/** Called when registration fails */
	UPROPERTY(BlueprintAssignable, Category = "SuspenseCore|Events")
	FSuspenseCoreOnRegistrationError OnRegistrationError;

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UI BINDINGS (BindWidget for Blueprint connection)
	// ═══════════════════════════════════════════════════════════════════════════

	/** Input for player display name */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UEditableTextBox* DisplayNameInput;

	/** Create account button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UButton* CreateButton;

	/** Status/error message display */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* StatusText;

	/** Title text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TitleText;

	/** Container for additional fields (expandable) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UVerticalBox* AdditionalFieldsContainer;

	// ═══════════════════════════════════════════════════════════════════════════
	// CLASS SELECTION UI BINDINGS (Procedural)
	// ═══════════════════════════════════════════════════════════════════════════

	/** Container for procedurally created class selection buttons (HorizontalBox or VerticalBox) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UPanelWidget* ClassButtonContainer;

	/** Widget class for class selection buttons (set in Blueprint) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	TSubclassOf<USuspenseCoreClassSelectionButtonWidget> ClassButtonWidgetClass;

	/** Selected class name display */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* SelectedClassNameText;

	/** Selected class description display */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* SelectedClassDescriptionText;

	/** Selected class icon image (small icon from ClassData) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* ClassIconImage;

	/** Selected class portrait image (large portrait from ClassData) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* ClassPortraitImage;

	// ═══════════════════════════════════════════════════════════════════════════
	// LEGACY CLASS SELECTION BINDINGS (Deprecated - use ClassButtonContainer)
	// ═══════════════════════════════════════════════════════════════════════════

	/** @deprecated Use ClassButtonContainer with procedural class buttons */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, DeprecatedProperty))
	UHorizontalBox* ClassSelectionContainer;

	/** @deprecated Use ClassButtonContainer with procedural class buttons */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, DeprecatedProperty))
	UButton* AssaultClassButton;

	/** @deprecated Use ClassButtonContainer with procedural class buttons */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, DeprecatedProperty))
	UButton* MedicClassButton;

	/** @deprecated Use ClassButtonContainer with procedural class buttons */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, DeprecatedProperty))
	UButton* SniperClassButton;

	// ═══════════════════════════════════════════════════════════════════════════
	// NAVIGATION UI BINDINGS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Back button to return to character select (when characters exist) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* BackButton;

	/** Text for back button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* BackButtonText;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Minimum display name length */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	int32 MinDisplayNameLength = 3;

	/** Maximum display name length */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	int32 MaxDisplayNameLength = 32;

	/** Auto-close widget after successful registration */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	bool bAutoCloseOnSuccess = false;

	/** Delay before auto-close (seconds) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SuspenseCore|Config")
	float AutoCloseDelay = 2.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════════════════

	/** Cached EventBus reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	/** Player repository for persistence */
	UPROPERTY()
	TScriptInterface<ISuspenseCorePlayerRepository> PlayerRepository;

	/** Currently selected class ID */
	UPROPERTY()
	FString SelectedClassId;

	/** Created class button widgets (for procedural loading) */
	UPROPERTY()
	TArray<USuspenseCoreClassSelectionButtonWidget*> CreatedClassButtons;

	/** Is currently processing registration */
	bool bIsProcessing = false;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Setup button click handlers */
	void SetupButtonBindings();

	/** Get EventBus from EventManager */
	USuspenseCoreEventBus* GetEventBus() const;

	/** Get or create player repository */
	ISuspenseCorePlayerRepository* GetOrCreateRepository();

	/** Publish registration event to EventBus */
	void PublishRegistrationEvent(bool bSuccess, const FSuspenseCorePlayerData& PlayerData, const FString& ErrorMessage);

	/** Publish class preview event to EventBus for character preview components */
	void PublishClassPreviewEvent(const FString& ClassId);

	/** Handle create button click */
	UFUNCTION()
	void OnCreateButtonClicked();

	/** Handle input text changed (for validation) */
	UFUNCTION()
	void OnDisplayNameChanged(const FText& Text);

	/** Handle class button clicked (from procedural buttons) */
	UFUNCTION()
	void OnClassButtonClicked(const FString& ClassId);

	/** Handle back button click - return to character select */
	UFUNCTION()
	void OnBackButtonClicked();

	/** Setup class selection buttons (procedural or legacy) */
	void SetupClassSelectionBindings();

	/** Create class buttons procedurally from CharacterClassSubsystem */
	void CreateProceduralClassButtons();

	/** Update class selection UI based on selected class */
	void UpdateClassSelectionUI();

	/** Update selection state on all class buttons */
	void UpdateClassButtonSelectionStates();

	// ═══════════════════════════════════════════════════════════════════════════
	// LEGACY CLASS BUTTON HANDLERS (Deprecated)
	// ═══════════════════════════════════════════════════════════════════════════

	/** @deprecated Use OnClassButtonClicked instead */
	UFUNCTION()
	void OnAssaultClassClicked();

	/** @deprecated Use OnClassButtonClicked instead */
	UFUNCTION()
	void OnMedicClassClicked();

	/** @deprecated Use OnClassButtonClicked instead */
	UFUNCTION()
	void OnSniperClassClicked();

	/** Setup legacy class button bindings (deprecated) */
	void SetupLegacyClassBindings();

	/** Update UI state based on validation */
	void UpdateUIState();

	/** Handle auto-close timer */
	void HandleAutoClose();

private:
	/** Timer handle for auto-close */
	FTimerHandle AutoCloseTimerHandle;
};
