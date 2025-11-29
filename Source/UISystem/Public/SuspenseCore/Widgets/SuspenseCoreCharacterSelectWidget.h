// SuspenseCoreCharacterSelectWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCore/Data/SuspenseCorePlayerData.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreCharacterSelectWidget.generated.h"

class UButton;
class UTextBlock;
class UScrollBox;
class UVerticalBox;
class USuspenseCoreEventBus;
class ISuspenseCorePlayerRepository;
class USuspenseCoreCharacterEntryWidget;

/**
 * Character entry item for the character list
 */
USTRUCT(BlueprintType)
struct FSuspenseCoreCharacterEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString PlayerId;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly)
	FDateTime LastPlayed;

	FSuspenseCoreCharacterEntry() = default;

	FSuspenseCoreCharacterEntry(const FSuspenseCorePlayerData& PlayerData)
		: PlayerId(PlayerData.PlayerId)
		, DisplayName(PlayerData.DisplayName)
		, Level(PlayerData.Level)
		, LastPlayed(PlayerData.LastLoginAt)
	{
	}
};

/**
 * USuspenseCoreCharacterSelectWidget
 *
 * Widget for selecting existing characters or creating new ones.
 * Part of the main menu flow: CharacterSelect -> MainMenu or Registration
 *
 * Flow:
 * - Shows list of existing characters (if any)
 * - "Create New Character" button leads to Registration
 * - Selecting a character leads to MainMenu
 * - "Delete Character" option for each character
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreCharacterSelectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreCharacterSelectWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT BINDINGS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Container for character list items */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UScrollBox* CharacterListScrollBox;

	/** Alternative: Vertical box for character buttons */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UVerticalBox* CharacterListBox;

	/** Button to create new character */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* CreateNewButton;

	/** Text for create button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* CreateNewButtonText;

	/** Title text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* TitleText;

	/** Status/info text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* StatusText;

	/** Play button - starts game with selected character */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* PlayButton;

	/** Text for play button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* PlayButtonText;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Class for individual character entry widgets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Select|Config")
	TSubclassOf<UUserWidget> CharacterEntryWidgetClass;

	/** Title text to display */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Select|Config")
	FText Title = FText::FromString(TEXT("SELECT CHARACTER"));

	/** Text for create new button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Select|Config")
	FText CreateNewText = FText::FromString(TEXT("CREATE NEW CHARACTER"));

	/** Text when no characters exist */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Select|Config")
	FText NoCharactersText = FText::FromString(TEXT("No characters found. Create your first character!"));

	/** Text for play button */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Select|Config")
	FText PlayText = FText::FromString(TEXT("PLAY"));

	/** Text for play button when no character selected */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Select|Config")
	FText SelectCharacterText = FText::FromString(TEXT("Select a character"));

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/** Refresh the character list from repository */
	UFUNCTION(BlueprintCallable, Category = "Character Select")
	void RefreshCharacterList();

	/** Get all loaded character entries */
	UFUNCTION(BlueprintCallable, Category = "Character Select")
	TArray<FSuspenseCoreCharacterEntry> GetCharacterEntries() const { return CharacterEntries; }

	/** Select a character by ID */
	UFUNCTION(BlueprintCallable, Category = "Character Select")
	void SelectCharacter(const FString& PlayerId);

	/** Delete a character by ID */
	UFUNCTION(BlueprintCallable, Category = "Character Select")
	void DeleteCharacter(const FString& PlayerId);

	/** Request to create new character (navigates to registration) */
	UFUNCTION(BlueprintCallable, Category = "Character Select")
	void RequestCreateNewCharacter();

	/** Highlight a character (visual selection without confirming) */
	UFUNCTION(BlueprintCallable, Category = "Character Select")
	void HighlightCharacter(const FString& PlayerId);

	/** Get currently highlighted character ID */
	UFUNCTION(BlueprintCallable, Category = "Character Select")
	FString GetHighlightedPlayerId() const { return HighlightedPlayerId; }

	/** Play with currently highlighted character */
	UFUNCTION(BlueprintCallable, Category = "Character Select")
	void PlayWithHighlightedCharacter();

	/** Check if a character is highlighted */
	UFUNCTION(BlueprintCallable, Category = "Character Select")
	bool HasHighlightedCharacter() const { return !HighlightedPlayerId.IsEmpty(); }

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when a character is selected */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character Select|Events")
	void OnCharacterSelected(const FString& PlayerId, const FSuspenseCoreCharacterEntry& Entry);

	/** Called when create new character is requested */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character Select|Events")
	void OnCreateNewRequested();

	/** Called when a character is deleted */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character Select|Events")
	void OnCharacterDeleted(const FString& PlayerId);

	/** Called when character list is refreshed */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character Select|Events")
	void OnCharacterListRefreshed(int32 CharacterCount);

	// ═══════════════════════════════════════════════════════════════════════════
	// DELEGATES (C++ events)
	// ═══════════════════════════════════════════════════════════════════════════

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCharacterSelectedDelegate, const FString&, PlayerId, const FSuspenseCoreCharacterEntry&, Entry);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCreateNewRequestedDelegate);
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterDeletedDelegate, const FString&, PlayerId);

	UPROPERTY(BlueprintAssignable, Category = "Character Select|Events")
	FOnCharacterSelectedDelegate OnCharacterSelectedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Character Select|Events")
	FOnCreateNewRequestedDelegate OnCreateNewRequestedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Character Select|Events")
	FOnCharacterDeletedDelegate OnCharacterDeletedDelegate;

protected:
	// ═══════════════════════════════════════════════════════════════════════════
	// UUserWidget Interface
	// ═══════════════════════════════════════════════════════════════════════════

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════════════════

	/** Cached character entries */
	UPROPERTY()
	TArray<FSuspenseCoreCharacterEntry> CharacterEntries;

	/** Map of buttons to player IDs for click handling */
	UPROPERTY()
	TMap<UButton*, FString> ButtonToPlayerIdMap;

	/** Map of entry widgets to player IDs */
	UPROPERTY()
	TMap<USuspenseCoreCharacterEntryWidget*, FString> EntryWidgetMap;

	/** Currently highlighted (selected) player ID */
	UPROPERTY()
	FString HighlightedPlayerId;

	/** Currently highlighted entry data */
	UPROPERTY()
	FSuspenseCoreCharacterEntry HighlightedEntry;

	/** Get repository interface */
	ISuspenseCorePlayerRepository* GetOrCreateRepository();

	/** Get event bus */
	USuspenseCoreEventBus* GetEventBus() const;

	/** Update UI display */
	void UpdateUIDisplay();

	/** Build character list UI */
	void BuildCharacterListUI();

	/** Create button for a character entry (simple fallback) */
	UButton* CreateCharacterButton(const FSuspenseCoreCharacterEntry& Entry);

	/** Publish event to EventBus */
	void PublishCharacterSelectEvent(const FGameplayTag& EventTag, const FString& PlayerId);

	/** Handle create new button click */
	UFUNCTION()
	void OnCreateNewButtonClicked();

	/** Handle character button click - looks up PlayerId from button map */
	UFUNCTION()
	void OnCharacterButtonClicked();

	/** Handle play button click */
	UFUNCTION()
	void OnPlayButtonClicked();

	/** Handle entry widget clicked */
	UFUNCTION()
	void OnEntryWidgetClicked(const FString& PlayerId);

	/** Update play button state based on selection */
	void UpdatePlayButtonState();

	/** Last clicked button (for lookup) */
	UPROPERTY()
	UButton* LastClickedButton;
};
