// SuspenseCoreClassSelectionButtonWidget.h
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SuspenseCoreClassSelectionButtonWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UBorder;
class USuspenseCoreCharacterClassData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClassButtonClicked, const FString&, ClassId);

/**
 * USuspenseCoreClassSelectionButtonWidget
 *
 * Individual class selection button widget for the registration screen.
 * Displays class icon, name, and handles selection.
 * Created procedurally by RegistrationWidget for each available class.
 *
 * Blueprint Structure:
 * [Border] "ButtonBorder" (for selection highlight)
 * └── [Button] "SelectButton"
 *     └── [Vertical Box]
 *         ├── [Image] "ClassIconImage" (64x64)
 *         └── [Text Block] "ClassNameText"
 */
UCLASS(Blueprintable, BlueprintType)
class UISYSTEM_API USuspenseCoreClassSelectionButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USuspenseCoreClassSelectionButtonWidget(const FObjectInitializer& ObjectInitializer);

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT BINDINGS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Border for selection highlight */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UBorder* ButtonBorder;

	/** Clickable button */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UButton* SelectButton;

	/** Class icon image */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UImage* ClassIconImage;

	/** Class name text */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	UTextBlock* ClassNameText;

	// ═══════════════════════════════════════════════════════════════════════════
	// CONFIGURATION
	// ═══════════════════════════════════════════════════════════════════════════

	/** Normal border color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class Button|Config")
	FLinearColor NormalBorderColor = FLinearColor(0.2f, 0.2f, 0.2f, 1.0f);

	/** Selected border color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class Button|Config")
	FLinearColor SelectedBorderColor = FLinearColor(0.3f, 0.6f, 1.0f, 1.0f);

	/** Hovered border color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Class Button|Config")
	FLinearColor HoveredBorderColor = FLinearColor(0.4f, 0.4f, 0.4f, 1.0f);

	// ═══════════════════════════════════════════════════════════════════════════
	// PUBLIC API
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Set the class data for this button.
	 * Updates icon, name, and stores class ID for click events.
	 */
	UFUNCTION(BlueprintCallable, Category = "Class Button")
	void SetClassData(USuspenseCoreCharacterClassData* InClassData);

	/**
	 * Set class data by ID (loads from CharacterClassSubsystem).
	 */
	UFUNCTION(BlueprintCallable, Category = "Class Button")
	void SetClassById(const FString& InClassId);

	/**
	 * Get the class ID this button represents.
	 */
	UFUNCTION(BlueprintCallable, Category = "Class Button")
	FString GetClassId() const { return ClassId; }

	/**
	 * Set the selected state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Class Button")
	void SetSelected(bool bInSelected);

	/**
	 * Check if this button is selected.
	 */
	UFUNCTION(BlueprintCallable, Category = "Class Button")
	bool IsSelected() const { return bIsSelected; }

	// ═══════════════════════════════════════════════════════════════════════════
	// DELEGATES
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when this class button is clicked */
	UPROPERTY(BlueprintAssignable, Category = "Class Button|Events")
	FOnClassButtonClicked OnClassButtonClicked;

	// ═══════════════════════════════════════════════════════════════════════════
	// BLUEPRINT EVENTS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Called when selection state changes - implement in Blueprint for custom animations */
	UFUNCTION(BlueprintImplementableEvent, Category = "Class Button|Events")
	void OnSelectionChanged(bool bNewSelected);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	// ═══════════════════════════════════════════════════════════════════════════
	// INTERNAL
	// ═══════════════════════════════════════════════════════════════════════════

	/** Class ID this button represents */
	UPROPERTY()
	FString ClassId;

	/** Cached class data reference */
	UPROPERTY()
	TWeakObjectPtr<USuspenseCoreCharacterClassData> CachedClassData;

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
