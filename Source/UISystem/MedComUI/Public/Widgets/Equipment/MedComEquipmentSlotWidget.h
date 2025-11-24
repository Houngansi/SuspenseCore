// MedComEquipmentSlotWidget.h
// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Base/MedComBaseSlotWidget.h"
#include "Types/UI/EquipmentUITypes.h"
#include "GameplayTagContainer.h"
#include "MedComEquipmentSlotWidget.generated.h"

class UImage;
class UTextBlock;
class UBorder;
class UProgressBar;
class USizeBox;
class UTexture2D;
class UMedComEquipmentContainerWidget;

/**
 * Equipment Slot Widget - SIMPLIFIED ARCHITECTURE
 * 
 * Responsibilities (UNCHANGED):
 *  - Visual representation of single equipment slot (1x1 cell)
 *  - Display slot type indicator (silhouette) when empty
 *  - Display equipped item icon when occupied
 *  - Handle slot-specific visual states (hover, selected, locked)
 *  - Minimal UI-level validation (item type compatibility hints)
 * 
 * Does NOT handle (delegated to higher layers):
 *  - Equipment operations (equip/unequip) - handled by Bridge
 *  - Multi-cell layout - equipment slots are always 1x1
 *  - Business logic validation - handled by ValidationService
 *  - Data fetching - receives ready FEquipmentSlotUIData from Container
 * 
 * NEW DATA FLOW (simplified):
 *  1. Container receives HandleEquipmentDataChanged from Bridge
 *  2. Container calls UpdateEquipmentSlot on this widget
 *  3. Widget updates CurrentEquipmentData field
 *  4. Widget calls UpdateVisualState which triggers UpdateItemIcon
 *  5. UpdateItemIcon uses CurrentEquipmentData.EquippedItem directly
 * 
 * OLD DATA FLOW (removed complexity):
 *  - No more waiting for GetEquipmentSlotsUIData calls
 *  - No more manual icon refresh requests
 *  - No more inconsistencies between data and display
 * 
 * Key improvements:
 *  - Always shows correct icon because data comes directly from cache
 *  - No race conditions between data updates and visual updates
 *  - Simpler debugging (single data source: CurrentEquipmentData)
 */
UCLASS()
class MEDCOMUI_API UMedComEquipmentSlotWidget : public UMedComBaseSlotWidget
{
    GENERATED_BODY()

public:
    UMedComEquipmentSlotWidget(const FObjectInitializer& ObjectInitializer);

    // ===== Equipment Slot API =====
    
    /**
     * Initialize slot with equipment data (primary setup)
     * Called once when slot widget is first created
     * Sets up slot type, allowed items, visual indicators
     * @param SlotData - Complete equipment slot configuration and state
     */
    UFUNCTION(BlueprintCallable, Category="Equipment Slot")
    virtual void InitializeEquipmentSlot(const FEquipmentSlotUIData& SlotData);

    /**
     * Update slot state with new equipment data (incremental updates)
     * Called whenever slot state changes (item equipped/unequipped)
     * CRITICAL: This is the main entry point for data updates from Container
     * @param SlotData - Updated equipment slot state from UIBridge cache
     */
    UFUNCTION(BlueprintCallable, Category="Equipment Slot")
    virtual void UpdateEquipmentSlot(const FEquipmentSlotUIData& SlotData);

    /**
     * Check if slot accepts specific item type (UI hint only, not validation)
     * Used for visual feedback during drag operations
     * Actual validation is done by ValidationService
     * @param ItemType - Gameplay tag representing item type
     * @return true if item type matches slot's allowed types
     */
    UFUNCTION(BlueprintCallable, Category="Equipment Slot")
    bool AcceptsItemType(const FGameplayTag& ItemType) const;

    /**
     * Get current equipment slot data
     * @return Current cached equipment slot state
     */
    UFUNCTION(BlueprintCallable, Category="Equipment Slot")
    const FEquipmentSlotUIData& GetEquipmentSlotData() const { return CurrentEquipmentData; }

    /**
     * Get parent equipment container widget
     * @return Owning container or nullptr
     */
    UFUNCTION(BlueprintCallable, Category="Equipment Slot")
    UMedComEquipmentContainerWidget* GetOwningEquipmentContainer() const;

protected:
    // ===== UUserWidget Overrides =====
    
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;

    // ===== Base Slot Interface (compatibility bridge) =====
    
    /**
     * Initialize with generic slot data (converts to equipment format)
     * Used for backward compatibility with base container systems
     */
    virtual void InitializeSlot_Implementation(
        const FSlotUIData& SlotData, const FItemUIData& ItemData) override;

    /**
     * Update with generic slot data (converts to equipment format)
     * Used for backward compatibility with base container systems
     */
    virtual void UpdateSlot_Implementation(
        const FSlotUIData& SlotData, const FItemUIData& ItemData) override;

    /**
     * Check if slot can be dragged (adds equipment-specific checks)
     */
    virtual bool CanBeDragged_Implementation() const override;

    /**
     * CRITICAL: Override from base class to use equipment-specific icon handling
     * This method is called by UpdateVisualState in base class
     * Uses CurrentEquipmentData.EquippedItem as single source of truth
     */
    virtual void UpdateItemIcon() override;

    // ===== Visual State Management =====
    
    /**
     * Update all visual elements based on current state
     * Handles slot type color, borders, highlights, etc.
     * Calls UpdateItemIcon to refresh item display
     */
    virtual void UpdateVisualState() override;

    /**
     * Update slot type visual indicators
     * Shows/hides slot type silhouette based on occupation state
     * Updates slot name text for empty slots
     * Applies slot type specific colors
     */
    virtual void UpdateSlotTypeDisplay();

    /**
     * Update requirement indicators (level, class, etc.)
     * Currently not used - reserved for future features
     */
    virtual void UpdateRequirementsDisplay();

    /**
     * Update durability/condition bars
     * Currently disabled - equipment durability not implemented yet
     */
    virtual void UpdateDurabilityDisplay();

    // ===== Icon Management =====
    
    /**
     * Get icon texture for current slot type
     * Returns slot-specific silhouette icon for visual identification
     * @return Texture for slot type or default if not configured
     */
    UTexture2D* GetSlotTypeIcon() const;

    /**
     * Get color for current slot type
     * Each slot type has unique color for visual distinction
     * @return Color for slot type or default gray
     */
    FLinearColor GetSlotTypeColor() const;

    /**
     * Apply item icon to ItemIcon widget
     * CRITICAL: This is where equipped item becomes visible
     * Handles icon texture loading, rotation, quantity display
     * Hides slot type silhouette when item is shown
     * @param ItemData - Item UI data containing icon and metadata
     */
    void ApplyItemIcon(const FItemUIData& ItemData);

    /**
     * Clear item icon and restore slot type silhouette
     * Called when item is unequipped or slot becomes empty
     * Shows slot type silhouette again for empty slot
     */
    void ClearItemIcon();

protected:
    // ===== UI Widget Bindings (all optional - can be unbound in BP) =====
    
    /** Border showing slot type color */
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    UBorder* SlotTypeBorder = nullptr;

    /** Image showing slot type silhouette (visible when empty) */
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    UImage* SlotTypeIcon = nullptr;

    /** Text showing slot name (visible when empty) */
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    UTextBlock* SlotNameText = nullptr;

    /** Progress bar for item durability (currently unused) */
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    UProgressBar* DurabilityBar = nullptr;

    /** Image indicator for item condition (currently unused) */
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    UImage* ConditionIndicator = nullptr;

    /** Text showing item quantity for stackable items */
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional))
    UTextBlock* ItemCountText = nullptr;

    // ===== Configuration =====
    
    /** Show slot name text for empty slots */
    UPROPERTY(EditAnywhere, Category="Equipment Slot")
    bool bShowSlotName = true;

    /** Show durability bar (currently disabled) */
    UPROPERTY(EditAnywhere, Category="Equipment Slot")
    bool bShowDurability = false;

    /** Show condition indicator (currently disabled) */
    UPROPERTY(EditAnywhere, Category="Equipment Slot")
    bool bShowCondition = false;

    /** Base size for equipment cell (1x1 slots) */
    UPROPERTY(EditAnywhere, Category="Equipment Slot", 
        meta=(ClampMin="8.0", ClampMax="256.0"))
    float EquipmentCellSize = 48.f;

    /** Padding between cells */
    UPROPERTY(EditAnywhere, Category="Equipment Slot", 
        meta=(ClampMin="0.0", ClampMax="10.0"))
    float CellPadding = 2.f;

    // ===== Visual Assets (per slot type using Equipment.Slot.* tags) =====
    
    /**
     * Map of slot type tags to silhouette icons
     * Each equipment slot type can have unique visual indicator
     * Shown when slot is empty to help identify slot purpose
     */
    UPROPERTY(EditAnywhere, Category="Equipment Slot")
    TMap<FGameplayTag, TObjectPtr<UTexture2D>> SlotTypeIcons;

    /**
     * Map of slot type tags to border colors
     * Each equipment slot type has unique color for visual distinction
     * Applied to SlotTypeBorder widget
     */
    UPROPERTY(EditAnywhere, Category="Equipment Slot")
    TMap<FGameplayTag, FLinearColor> SlotTypeColors;

    /** Default icon for slots without specific icon configured */
    UPROPERTY(EditAnywhere, Category="Equipment Slot")
    TObjectPtr<UTexture2D> DefaultSlotIcon = nullptr;

    /** Default color for slots without specific color configured */
    UPROPERTY(EditAnywhere, Category="Equipment Slot")
    FLinearColor DefaultSlotColor = FLinearColor(0.3f, 0.3f, 0.3f, 1.f);

    // ===== State =====
    
    /**
     * CRITICAL: Current equipment slot data
     * This is the single source of truth for this widget
     * Updated by Container via UpdateEquipmentSlot
     * Used by UpdateItemIcon to display correct item
     * 
     * NEW ARCHITECTURE: This field is always in sync with UIBridge cache
     * OLD ARCHITECTURE: Could be stale until next GetEquipmentSlotsUIData call
     */
    UPROPERTY(BlueprintReadOnly, Category="Equipment Slot")
    FEquipmentSlotUIData CurrentEquipmentData;
};