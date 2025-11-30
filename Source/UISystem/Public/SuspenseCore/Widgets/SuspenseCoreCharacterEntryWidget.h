// SuspenseCoreCharacterEntryWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCoreCharacterEntryWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UBorder;

/**
 * USuspenseCoreCharacterEntryWidget
 *
 * Widget prefab for displaying a single character entry in the character select list.
 * Shows character avatar, name, level, and handles selection.
 *
 * Blueprint Structure:
 * [Border] "EntryBorder" (for selection highlight)
 * └── [Horizontal Box]
 *     ├── [Image] "AvatarImage" (64x64)
 *     ├── [Vertical Box]
 *     │   ├── [Text Block] "DisplayNameText"
 *     │   └── [Text Block] "LevelText"
 *     └── [Button] "SelectButton" (optional - whole widget is clickable)
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreCharacterEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreCharacterEntryWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT BINDINGS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Border for selection highlight */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UBorder* EntryBorder;

	/** Character avatar image */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* AvatarImage;

	/** Character display name */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* DisplayNameText;

	/** Character level text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* LevelText;

	/** Optional select button (if not using whole widget click) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* SelectButton;

	/** Main clickable button (wraps entire entry) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* EntryButton;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Default avatar texture when none specified */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Entry|Config")
	TObjectPtr<UTexture2D> DefaultAvatarTexture;

	/** Normal border color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Entry|Config")
	FLinearColor NormalBorderColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);

	/** Selected border color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Entry|Config")
	FLinearColor SelectedBorderColor = FLinearColor(0.0f, 0.5f, 1.0f, 1.0f);

	/** Hovered border color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Entry|Config")
	FLinearColor HoveredBorderColor = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/** Set character data to display */
	UFUNCTION(BlueprintCallable, Category = "Character Entry")
	void SetCharacterData(const FString& InPlayerId, const FString& InDisplayName, int32 InLevel, UTexture2D* InAvatarTexture = nullptr);

	/** Get the player ID this entry represents */
	UFUNCTION(BlueprintCallable, Category = "Character Entry")
	FString GetPlayerId() const { return PlayerId; }

	/** Set selected state */
	UFUNCTION(BlueprintCallable, Category = "Character Entry")
	void SetSelected(bool bInSelected);

	/** Check if selected */
	UFUNCTION(BlueprintCallable, Category = "Character Entry")
	bool IsSelected() const { return bIsSelected; }

	// ═══════════════════════════════════════════════════════════════════════════
	// DELEGATES
	// ═══════════════════════════════════════════════════════════════════════════

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterEntryClicked, const FString&, PlayerId);

	/** Called when this entry is clicked */
	UPROPERTY(BlueprintAssignable, Category = "Character Entry|Events")
	FOnCharacterEntryClicked OnEntryClicked;

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when selection state changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character Entry|Events")
	void OnSelectionChanged(bool bNewSelected);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════════════════

	/** Player ID this entry represents */
	UPROPERTY()
	FString PlayerId;

	/** Display name */
	UPROPERTY()
	FString DisplayName;

	/** Level */
	UPROPERTY()
	int32 Level = 1;

	/** Selected state */
	UPROPERTY()
	bool bIsSelected = false;

	/** Hovered state */
	UPROPERTY()
	bool bIsHovered = false;

	/** Update visual state based on selection/hover */
	void UpdateVisualState();

	/** Handle button click */
	UFUNCTION()
	void OnButtonClicked();
};
