// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/UI/ISuspenseTooltipInterface.h"
#include "Types/UI/ContainerUITypes.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "SuspenseItemTooltipWidget.generated.h"

// Forward declarations
class UTextBlock;
class UImage;
class UVerticalBox;
class UAttributeSet;
class UGameplayEffect;

/**
 * Base tooltip widget for displaying item information
 * Enhanced with attribute display from DataTable
 */
UCLASS()
class UISYSTEM_API USuspenseItemTooltipWidget : public UUserWidget, public ISuspenseTooltipInterface
{
    GENERATED_BODY()

public:
    USuspenseItemTooltipWidget(const FObjectInitializer& ObjectInitializer);

    // === ISuspenseTooltipInterface Implementation ===
    virtual void ShowTooltip_Implementation(const FItemUIData& ItemData, const FVector2D& ScreenPosition) override;
    virtual void HideTooltip_Implementation() override;
    virtual void UpdateTooltipPosition_Implementation(const FVector2D& ScreenPosition) override;
    virtual bool IsTooltipVisible_Implementation() const override;
    virtual void SetTooltipAnchor_Implementation(const FVector2D& Anchor, const FVector2D& Pivot) override;

protected:
    // === UUserWidget Overrides ===
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // === Widget Bindings ===
    
    /** Item name display */
    UPROPERTY(BlueprintReadOnly, Category = "Tooltip|Widgets", meta = (BindWidget))
    UTextBlock* ItemNameText;
    
    /** Item description */
    UPROPERTY(BlueprintReadOnly, Category = "Tooltip|Widgets", meta = (BindWidget))
    UTextBlock* ItemDescription;
    
    /** Item icon */
    UPROPERTY(BlueprintReadOnly, Category = "Tooltip|Widgets", meta = (BindWidget))
    UImage* ItemIcon;
    
    /** Item type */
    UPROPERTY(BlueprintReadOnly, Category = "Tooltip|Widgets", meta = (BindWidget))
    UTextBlock* ItemTypeText;
    
    /** Grid size info */
    UPROPERTY(BlueprintReadOnly, Category = "Tooltip|Widgets", meta = (BindWidget))
    UTextBlock* GridSizeText;
    
    /** Stack information */
    UPROPERTY(BlueprintReadOnly, Category = "Tooltip|Widgets", meta = (BindWidget))
    UTextBlock* StackInfoText;
    
    /** Item weight */
    UPROPERTY(BlueprintReadOnly, Category = "Tooltip|Widgets", meta = (BindWidget))
    UTextBlock* WeightText;
    
    /** Equipment slot type */
    UPROPERTY(BlueprintReadOnly, Category = "Tooltip|Widgets", meta = (BindWidget))
    UTextBlock* EquipmentSlotText;
    
    /** Ammo information */
    UPROPERTY(BlueprintReadOnly, Category = "Tooltip|Widgets", meta = (BindWidget))
    UTextBlock* AmmoInfoText;
    
    /** Container for dynamic attribute display */
    UPROPERTY(BlueprintReadOnly, Category = "Tooltip|Widgets", meta = (BindWidget))
    UVerticalBox* AttributesContainer;
    
    /** Separator between basic info and attributes */
    UPROPERTY(BlueprintReadOnly, Category = "Tooltip|Widgets", meta = (BindWidget))
    UTextBlock* AttributesSeparator;

    // === Configuration ===
    
    /** Reference to item data table */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tooltip|Config")
    UDataTable* ItemDataTable;
    
    /** Offset from cursor position */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tooltip|Config")
    FVector2D MouseOffset;
    
    /** Minimum distance from screen edges */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tooltip|Config")
    float EdgePadding;
    
    /** Auto-adjust position to stay on screen */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tooltip|Config")
    bool bAutoAdjustPosition;
    
    /** Delay before hiding tooltip */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tooltip|Config")
    float HideDelay;
    
    /** Fade in animation duration */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tooltip|Config")
    float FadeInDuration;
    
    /** Fade out animation duration */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tooltip|Config")
    float FadeOutDuration;

    /** Enable smooth fade animations */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tooltip|Config")
    bool bEnableFadeAnimation = true;
    
    /** Instant show/hide mode (for competitive games) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tooltip|Config")
    bool bInstantMode = false;
    
private:
    /** Current item data being displayed */
    UPROPERTY()
    FItemUIData CurrentItemData;
    
    /** 
     * Full item data from DataTable - stored as pointer without UPROPERTY
     * This is a temporary reference that doesn't need serialization
     */
    const FSuspenseUnifiedItemData* FullItemData;
    
    /** Current mouse position in viewport coordinates */
    FVector2D CurrentMousePosition;
    
    /** Tooltip anchor point (0,0 = top-left, 1,1 = bottom-right) */
    FVector2D TooltipAnchor;
    
    /** Tooltip pivot point for positioning */
    FVector2D TooltipPivot;
    
    /** Timer for delayed hiding */
    FTimerHandle HideTimerHandle;
    
    /** Whether tooltip is currently fading */
    bool bIsFading;
    
    /** Current fade alpha */
    float CurrentFadeAlpha;
    
    /** Target fade alpha */
    float TargetFadeAlpha;
    
    /** Update displayed data from current item */
    void UpdateDisplayData();
    
    /** Load full item data from DataTable */
    void LoadFullItemData();
    
    /** Display weapon attributes */
    void DisplayWeaponAttributes();
    
    /** Display armor attributes */
    void DisplayArmorAttributes();
    
    /** Display ammo attributes */
    void DisplayAmmoAttributes();
    
    /** Extract attribute value from AttributeSet class */
    float ExtractAttributeValue(TSubclassOf<UAttributeSet> AttributeSetClass, 
                               TSubclassOf<UGameplayEffect> InitEffect,
                               const FString& AttributeName);
    
    /** Add attribute line to container */
    void AddAttributeLine(const FString& AttributeName, float Value, const FString& Format = TEXT("%.1f"));
    
    /** Clear all dynamic attribute lines */
    void ClearAttributeLines();
    
    /** Main positioning method */
    void RepositionTooltip();
    
    /** Get viewport size */
    FVector2D GetViewportSize() const;
    
    /** Convert GameplayTag to readable text */
    FText GameplayTagToText(const FGameplayTag& Tag) const;
    
    /** Format weight for display */
    FText FormatWeight(float Weight) const;
    
    /** Format grid size for display */
    FText FormatGridSize(const FIntPoint& GridSize) const;
    
    /** Handle fade animation */
    void UpdateFade(float DeltaTime);
    
    /** Start fade in */
    void StartFadeIn();
    
    /** Start fade out */
    void StartFadeOut();
    
    /** Apply fade to widget */
    void ApplyFade(float Alpha);
    
    /** Validate widget bindings */
    bool ValidateWidgetBindings() const;
};