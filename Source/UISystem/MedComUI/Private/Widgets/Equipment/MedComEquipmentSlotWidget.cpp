// MedComEquipmentSlotWidget.cpp
// Copyright MedCom Team. All Rights Reserved.

#include "Widgets/Equipment/MedComEquipmentSlotWidget.h"
#include "Widgets/Equipment/MedComEquipmentContainerWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/GridSlot.h"

#include "Engine/Texture2D.h"
#include "GameplayTagContainer.h"

// ===== Constructor =====

UMedComEquipmentSlotWidget::UMedComEquipmentSlotWidget(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Disable durability/condition features (not implemented in current version)
    bShowDurability = false;
    bShowCondition = false;

    // Initialize slot type colors using Equipment.Slot.* taxonomy
    // Each slot type gets unique color for visual distinction
    
    // Weapon slots (red/orange tones)
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.PrimaryWeapon")),
        FLinearColor(0.85f, 0.25f, 0.25f, 1.f) // Bright red
    );
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.SecondaryWeapon")),
        FLinearColor(0.75f, 0.35f, 0.25f, 1.f) // Orange-red
    );
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Holster")),
        FLinearColor(0.70f, 0.30f, 0.30f, 1.f) // Dark red
    );
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Scabbard")),
        FLinearColor(0.65f, 0.35f, 0.35f, 1.f) // Muted red
    );

    // Head slots (blue tones)
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Headwear")),
        FLinearColor(0.25f, 0.45f, 0.85f, 1.f) // Bright blue
    );
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Earpiece")),
        FLinearColor(0.25f, 0.55f, 0.85f, 1.f) // Sky blue
    );
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Eyewear")),
        FLinearColor(0.30f, 0.60f, 0.85f, 1.f) // Light blue
    );
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.FaceCover")),
        FLinearColor(0.35f, 0.65f, 0.85f, 1.f) // Pale blue
    );

    // Body slots (orange/yellow tones)
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.BodyArmor")),
        FLinearColor(0.85f, 0.55f, 0.25f, 1.f) // Bright orange
    );
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.TacticalRig")),
        FLinearColor(0.85f, 0.65f, 0.25f, 1.f) // Golden orange
    );

    // Storage slots (green/cyan tones)
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Backpack")),
        FLinearColor(0.45f, 0.75f, 0.35f, 1.f) // Grass green
    );
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.SecureContainer")),
        FLinearColor(0.35f, 0.75f, 0.65f, 1.f) // Cyan-green
    );

    // Quick slots (purple/blue tones)
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.QuickSlot1")),
        FLinearColor(0.50f, 0.50f, 0.80f, 1.f) // Lavender
    );
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.QuickSlot2")),
        FLinearColor(0.55f, 0.55f, 0.80f, 1.f) // Light purple
    );
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.QuickSlot3")),
        FLinearColor(0.60f, 0.60f, 0.80f, 1.f) // Medium purple
    );
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.QuickSlot4")),
        FLinearColor(0.65f, 0.65f, 0.80f, 1.f) // Pale purple
    );

    // Special slots
    SlotTypeColors.Add(
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Armband")),
        FLinearColor(0.85f, 0.85f, 0.25f, 1.f) // Bright yellow
    );
}

// ===== UUserWidget Overrides =====

void UMedComEquipmentSlotWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    // Set root size box to equipment cell size (1x1 cell)
    if (RootSizeBox)
    {
        RootSizeBox->SetWidthOverride(EquipmentCellSize);
        RootSizeBox->SetHeightOverride(EquipmentCellSize);
    }
}

void UMedComEquipmentSlotWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Initialize visual elements
    UpdateSlotTypeDisplay();
    UpdateVisualState();

    UE_LOG(LogTemp, Verbose, TEXT("[EquipmentSlot] Constructed. Type=%s"),
        CurrentEquipmentData.SlotType.IsValid() ? 
            *CurrentEquipmentData.SlotType.ToString() : TEXT("<none>"));
}

// ===== Equipment Slot API =====

void UMedComEquipmentSlotWidget::InitializeEquipmentSlot(
    const FEquipmentSlotUIData& SlotData)
{
    UE_LOG(LogTemp, Log, 
        TEXT("[EquipmentSlot] InitializeEquipmentSlot: Slot %d, Type=%s, Occupied=%s"),
        SlotData.SlotIndex, 
        *SlotData.SlotType.ToString(),
        SlotData.bIsOccupied ? TEXT("true") : TEXT("false"));

    // Store equipment data as single source of truth
    CurrentEquipmentData = SlotData;

    // Convert to base slot format for parent class initialization
    FSlotUIData BaseSlot;
    BaseSlot.SlotIndex = SlotData.SlotIndex;
    BaseSlot.GridX = SlotData.GridPosition.X;
    BaseSlot.GridY = SlotData.GridPosition.Y;
    BaseSlot.bIsOccupied = SlotData.bIsOccupied;
    BaseSlot.bIsAnchor = SlotData.bIsOccupied; // Equipment slots are always anchors
    BaseSlot.bIsPartOfItem = SlotData.bIsOccupied; // 1x1 slot = always full item
    BaseSlot.SlotType = SlotData.SlotType;
    BaseSlot.AllowedItemTypes = SlotData.AllowedItemTypes;

    FItemUIData BaseItem;
    if (SlotData.bIsOccupied && SlotData.EquippedItem.IsValid())
    {
        BaseItem = SlotData.EquippedItem;
    }

    // Initialize parent class with base data
    Super::InitializeSlot_Implementation(BaseSlot, BaseItem);

    // Update equipment-specific visuals
    UpdateSlotTypeDisplay();
    UpdateRequirementsDisplay();
    UpdateDurabilityDisplay();
}

void UMedComEquipmentSlotWidget::UpdateEquipmentSlot(
    const FEquipmentSlotUIData& SlotData)
{
    UE_LOG(LogTemp, Warning, TEXT("=== UpdateEquipmentSlot START ==="));
    UE_LOG(LogTemp, Warning, TEXT("Slot %d: Type=%s, Occupied=%s"), 
        SlotData.SlotIndex, 
        *SlotData.SlotType.ToString(),
        SlotData.bIsOccupied ? TEXT("YES") : TEXT("NO"));
    
    // CRITICAL: Store equipment data FIRST
    // This ensures UpdateItemIcon has correct data when called
    CurrentEquipmentData = SlotData;
    
    if (SlotData.bIsOccupied)
    {
        UE_LOG(LogTemp, Warning, TEXT("Item: ID=%s, InstanceID=%s, HasIcon=%s"), 
            *SlotData.EquippedItem.ItemID.ToString(),
            *SlotData.EquippedItem.ItemInstanceID.ToString(),
            SlotData.EquippedItem.GetIcon() ? TEXT("YES") : TEXT("NO"));
    }
    
    // Convert to base format for parent class
    FSlotUIData BaseSlot;
    BaseSlot.SlotIndex = SlotData.SlotIndex;
    BaseSlot.GridX = SlotData.GridPosition.X;
    BaseSlot.GridY = SlotData.GridPosition.Y;
    BaseSlot.bIsOccupied = SlotData.bIsOccupied;
    BaseSlot.bIsAnchor = SlotData.bIsOccupied;
    BaseSlot.bIsPartOfItem = SlotData.bIsOccupied;
    BaseSlot.SlotType = SlotData.SlotType;
    BaseSlot.AllowedItemTypes = SlotData.AllowedItemTypes;

    FItemUIData BaseItem;
    if (SlotData.bIsOccupied && SlotData.EquippedItem.IsValid())
    {
        BaseItem = SlotData.EquippedItem;
    }

    // Update parent class (will call UpdateVisualState which calls UpdateItemIcon)
    Super::UpdateSlot_Implementation(BaseSlot, BaseItem);

    // Update equipment-specific elements
    UpdateSlotTypeDisplay();
    UpdateRequirementsDisplay();
    UpdateDurabilityDisplay();
    
    UE_LOG(LogTemp, Warning, TEXT("=== UpdateEquipmentSlot END ==="));
}

// ===== Base Slot Interface (compatibility bridge) =====

UMedComEquipmentContainerWidget* UMedComEquipmentSlotWidget::GetOwningEquipmentContainer() const
{
    return Cast<UMedComEquipmentContainerWidget>(GetOwningContainer());
}

void UMedComEquipmentSlotWidget::InitializeSlot_Implementation(
    const FSlotUIData& SlotData, const FItemUIData& ItemData)
{
    Super::InitializeSlot_Implementation(SlotData, ItemData);

    // Sync equipment structure if initialized with base data only
    if (!CurrentEquipmentData.SlotType.IsValid() && SlotData.SlotType.IsValid())
    {
        CurrentEquipmentData.SlotIndex = SlotData.SlotIndex;
        CurrentEquipmentData.SlotType = SlotData.SlotType;
        CurrentEquipmentData.GridPosition = FIntPoint(SlotData.GridX, SlotData.GridY);
        CurrentEquipmentData.AllowedItemTypes = SlotData.AllowedItemTypes;
        CurrentEquipmentData.bIsOccupied = SlotData.bIsOccupied;

        if (SlotData.bIsOccupied && ItemData.IsValid())
        {
            CurrentEquipmentData.EquippedItem = ItemData;
        }

        UpdateSlotTypeDisplay();
        UpdateRequirementsDisplay();
        UpdateDurabilityDisplay();
    }
}

void UMedComEquipmentSlotWidget::UpdateSlot_Implementation(
    const FSlotUIData& SlotData, const FItemUIData& ItemData)
{
    Super::UpdateSlot_Implementation(SlotData, ItemData);

    // Keep equipment data in sync
    CurrentEquipmentData.bIsOccupied = SlotData.bIsOccupied;

    if (SlotData.bIsOccupied && ItemData.IsValid())
    {
        CurrentEquipmentData.EquippedItem = ItemData;
        ApplyItemIcon(ItemData);
    }
    else
    {
        CurrentEquipmentData.EquippedItem = FItemUIData();
        ClearItemIcon();
    }

    UpdateRequirementsDisplay();
    UpdateDurabilityDisplay();
}

bool UMedComEquipmentSlotWidget::CanBeDragged_Implementation() const
{
    if (!Super::CanBeDragged_Implementation())
    {
        return false;
    }

    // Add equipment-specific drag restrictions
    if (CurrentEquipmentData.bIsLocked)
    {
        return false;
    }

    return true;
}

// ===== Visual State Management =====

void UMedComEquipmentSlotWidget::UpdateVisualState()
{
    // Call parent to handle base visual updates
    Super::UpdateVisualState();

    // CRITICAL: Update item icon AFTER base state update
    // This ensures icon is always in sync with current data
    UpdateItemIcon();
    
    // Update slot type border color based on state
    if (SlotTypeBorder)
    {
        FLinearColor BorderColor = GetSlotTypeColor();

        // Modify color based on slot state
        if (bIsLocked)
        {
            BorderColor *= 0.5f; // Darken for locked state
            BorderColor.A = 1.f;
        }
        else if (bIsSelected)
        {
            BorderColor *= 1.15f; // Brighten for selected state
            BorderColor.A = 1.f;
        }

        SlotTypeBorder->SetBrushColor(BorderColor);
    }
}

void UMedComEquipmentSlotWidget::UpdateSlotTypeDisplay()
{
    // Slot type icon (silhouette) - show ONLY when slot is empty
    if (SlotTypeIcon)
    {
        if (CurrentEquipmentData.bIsOccupied)
        {
            // Slot occupied - hide silhouette to show item icon
            SlotTypeIcon->SetVisibility(ESlateVisibility::Collapsed);
        }
        else if (UTexture2D* Icon = GetSlotTypeIcon())
        {
            // Slot empty - show silhouette to indicate slot type
            SlotTypeIcon->SetBrushFromTexture(Icon);
            SlotTypeIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            // No silhouette configured - hide
            SlotTypeIcon->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Slot name text - show only for empty slots
    if (SlotNameText)
    {
        if (bShowSlotName && !CurrentEquipmentData.bIsOccupied)
        {
            SlotNameText->SetText(CurrentEquipmentData.SlotName);
            SlotNameText->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            SlotNameText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Slot border color
    if (SlotTypeBorder)
    {
        SlotTypeBorder->SetBrushColor(GetSlotTypeColor());
    }
}

void UMedComEquipmentSlotWidget::UpdateRequirementsDisplay()
{
    // Placeholder for future requirement indicators
    // (level, class, attributes, etc.)
    // Currently no UI requirements implemented
}

void UMedComEquipmentSlotWidget::UpdateDurabilityDisplay()
{
    // Hide durability/condition indicators (not implemented in current version)
    if (DurabilityBar)
    {
        DurabilityBar->SetVisibility(
            bShowDurability ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed
        );
        
        if (!bShowDurability)
        {
            DurabilityBar->SetPercent(0.f);
        }
    }

    if (ConditionIndicator)
    {
        ConditionIndicator->SetVisibility(
            bShowCondition ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed
        );
    }
}

// ===== CRITICAL: Icon Management Methods =====

void UMedComEquipmentSlotWidget::UpdateItemIcon()
{
    UE_LOG(LogTemp, VeryVerbose, 
        TEXT("[EquipmentSlot %d] UpdateItemIcon: Occupied=%s"), 
        CurrentEquipmentData.SlotIndex,
        CurrentEquipmentData.bIsOccupied ? TEXT("YES") : TEXT("NO"));

    // CRITICAL: Use CurrentEquipmentData as single source of truth
    // NEW ARCHITECTURE: This data is always fresh from UIBridge cache
    // OLD ARCHITECTURE: This data could be stale until next refresh
    
    if (CurrentEquipmentData.bIsOccupied && CurrentEquipmentData.EquippedItem.IsValid())
    {
        // Slot has item - show item icon
        ApplyItemIcon(CurrentEquipmentData.EquippedItem);
    }
    else
    {
        // Slot empty - clear item icon and show slot type silhouette
        ClearItemIcon();
    }
}

void UMedComEquipmentSlotWidget::ApplyItemIcon(const FItemUIData& ItemData)
{
    UE_LOG(LogTemp, Warning, TEXT("=== ApplyItemIcon START ==="));
    UE_LOG(LogTemp, Warning, TEXT("Slot %d: Applying icon for item %s"), 
        CurrentEquipmentData.SlotIndex, *ItemData.ItemID.ToString());
    
    if (!ItemIcon) 
    { 
        UE_LOG(LogTemp, Error, 
            TEXT("ItemIcon widget is NULL! Cannot display item icon"));
        return; 
    }

    // Validate item data
    if (!ItemData.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("ItemData is not valid, clearing icon"));
        ClearItemIcon();
        return;
    }

    // Get icon texture from item data
    UTexture2D* IconTexture = ItemData.GetIcon();
    
    UE_LOG(LogTemp, Warning, TEXT("IconTexture: %s"), 
        IconTexture ? *IconTexture->GetName() : TEXT("NULL"));
    
    if (!IconTexture)
    {
        UE_LOG(LogTemp, Error, 
            TEXT("No icon texture available for item %s"), 
            *ItemData.ItemID.ToString());
        ClearItemIcon();
        return;
    }

    // CRITICAL: Set texture through SetBrushFromTexture
    // This is the correct way to update UImage brush
    UE_LOG(LogTemp, Warning, 
        TEXT("Setting brush from texture: %s"), *IconTexture->GetName());
    
    ItemIcon->SetBrushFromTexture(IconTexture, true); // true = match size
    
    // CRITICAL: Make item icon visible
    ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
    
    // CRITICAL: Hide slot type silhouette so item icon shows clearly
    if (SlotTypeIcon)
    {
        SlotTypeIcon->SetVisibility(ESlateVisibility::Collapsed);
        UE_LOG(LogTemp, VeryVerbose, 
            TEXT("SlotTypeIcon hidden to show item icon"));
    }
    
    // Set full opacity and neutral color
    ItemIcon->SetOpacity(1.0f);
    ItemIcon->SetColorAndOpacity(FLinearColor::White);
    
    // Apply rotation if item is rotated
    const float RotationAngle = ItemData.bIsRotated ? 90.f : 0.f;
    ItemIcon->SetRenderTransformAngle(RotationAngle);
    ItemIcon->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    
    UE_LOG(LogTemp, VeryVerbose, 
        TEXT("ItemIcon rotation set to: %.1f degrees"), RotationAngle);
    
    // Handle quantity text for stackable items
    if (ItemCountText)
    {
        if (ItemData.Quantity > 1)
        {
            ItemCountText->SetText(FText::AsNumber(ItemData.Quantity));
            ItemCountText->SetVisibility(ESlateVisibility::HitTestInvisible);
            
            UE_LOG(LogTemp, VeryVerbose, 
                TEXT("Item quantity displayed: %d"), ItemData.Quantity);
        }
        else
        {
            ItemCountText->SetText(FText::GetEmpty());
            ItemCountText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    
    // Verify final state for debugging
    const FSlateBrush& CurrentBrush = ItemIcon->GetBrush();
    UObject* BrushResource = CurrentBrush.GetResourceObject();
    
    UE_LOG(LogTemp, Warning, TEXT("Final brush resource: %s"), 
        BrushResource ? *BrushResource->GetName() : TEXT("NULL"));
    
    UE_LOG(LogTemp, Warning, TEXT("=== ApplyItemIcon END - Success ==="));
}

void UMedComEquipmentSlotWidget::ClearItemIcon()
{
    UE_LOG(LogTemp, VeryVerbose, 
        TEXT("[EquipmentSlot %d] ClearItemIcon called"), 
        CurrentEquipmentData.SlotIndex);
    
    if (ItemIcon)
    {
        // CRITICAL: Clear texture completely, not just hide
        // This prevents showing stale icon when slot is re-occupied
        ItemIcon->SetBrushFromTexture(nullptr);
        ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
        ItemIcon->SetRenderTransformAngle(0.f);
    }
    
    // CRITICAL: Show slot type silhouette for empty slot
    // This helps user identify what type of item belongs in this slot
    if (SlotTypeIcon)
    {
        if (UTexture2D* SlotIcon = GetSlotTypeIcon())
        {
            SlotTypeIcon->SetBrushFromTexture(SlotIcon);
            SlotTypeIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
    }
    
    // Clear quantity text
    if (ItemCountText)
    {
        ItemCountText->SetText(FText::GetEmpty());
        ItemCountText->SetVisibility(ESlateVisibility::Collapsed);
    }
}

// ===== Helper Methods =====

bool UMedComEquipmentSlotWidget::AcceptsItemType(const FGameplayTag& ItemType) const
{
    if (!ItemType.IsValid())
    {
        return false;
    }

    // UI-level hint only - actual validation done by ValidationService
    // Empty AllowedItemTypes means "accept any" (permissive default)
    const bool bAllowedListEmpty = CurrentEquipmentData.AllowedItemTypes.IsEmpty();
    
    return bAllowedListEmpty || CurrentEquipmentData.AllowedItemTypes.HasTag(ItemType);
}

UTexture2D* UMedComEquipmentSlotWidget::GetSlotTypeIcon() const
{
    // Try to find slot-specific icon
    if (const TObjectPtr<UTexture2D>* IconPtr = 
        SlotTypeIcons.Find(CurrentEquipmentData.SlotType))
    {
        return IconPtr->Get();
    }
    
    // Fallback to default icon
    return DefaultSlotIcon.Get();
}

FLinearColor UMedComEquipmentSlotWidget::GetSlotTypeColor() const
{
    // Try to find slot-specific color
    if (const FLinearColor* ColorPtr = 
        SlotTypeColors.Find(CurrentEquipmentData.SlotType))
    {
        return *ColorPtr;
    }
    
    // Fallback to default gray color
    return DefaultSlotColor;
}
