// SuspenseCoreEquipmentSlotWidget.cpp
// SuspenseCore - Equipment Slot Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Equipment/SuspenseCoreEquipmentSlotWidget.h"
#include "SuspenseCore/Widgets/DragDrop/SuspenseCoreDragDropOperation.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Styling/SlateBrush.h"
#include "Blueprint/DragDropOperation.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreEquipmentSlotWidget::USuspenseCoreEquipmentSlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, SlotType(EEquipmentSlotType::None)
	, EmptySlotIconTint(FLinearColor(0.3f, 0.3f, 0.3f, 0.5f))
	, bHadItemBefore(false)
{
	// Equipment slots are typically larger than inventory slots
	SlotSize = FVector2D(80.0f, 80.0f);
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCoreEquipmentSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Show empty slot icon initially
	UpdateEmptySlotIcon();
}

void USuspenseCoreEquipmentSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// Set empty slot icon tint in editor preview
	if (EmptySlotIcon)
	{
		EmptySlotIcon->SetColorAndOpacity(EmptySlotIconTint);
	}
}

//==================================================================
// Equipment Slot Configuration
//==================================================================

void USuspenseCoreEquipmentSlotWidget::InitializeFromConfig(const FEquipmentSlotConfig& InSlotConfig)
{
	SlotType = InSlotConfig.SlotType;
	SlotTypeTag = InSlotConfig.SlotTag;
	DisplayName = InSlotConfig.DisplayName;
	AllowedItemTypes = InSlotConfig.AllowedItemTypes;

	// Update slot data with slot type info
	CachedSlotData.SlotTypeTag = SlotTypeTag;

	// Notify Blueprint
	K2_OnConfigInitialized(InSlotConfig);

	// Update visuals
	UpdateVisuals();
}

void USuspenseCoreEquipmentSlotWidget::SetSlotType(EEquipmentSlotType InSlotType)
{
	SlotType = InSlotType;

	// Auto-generate tag if not set
	if (!SlotTypeTag.IsValid())
	{
		FString TagName = FString::Printf(TEXT("Equipment.Slot.%s"),
			*UEnum::GetValueAsString(InSlotType).RightChop(FString(TEXT("EEquipmentSlotType::")).Len()));
		SlotTypeTag = FGameplayTag::RequestGameplayTag(*TagName, false);
	}
}

bool USuspenseCoreEquipmentSlotWidget::CanAcceptItemType(const FGameplayTag& ItemTypeTag) const
{
	// If no restrictions, accept all
	if (AllowedItemTypes.IsEmpty())
	{
		return true;
	}

	// Check if item type matches any allowed type
	return AllowedItemTypes.HasTag(ItemTypeTag);
}

//==================================================================
// Empty Slot Icon
//==================================================================

void USuspenseCoreEquipmentSlotWidget::SetEmptySlotIconPath(const FSoftObjectPath& InIconPath)
{
	EmptySlotIconPath = InIconPath;

	// Clear cached texture to force reload
	CachedEmptySlotIconTexture = nullptr;

	// Update visuals if slot is empty
	if (IsEmpty())
	{
		UpdateEmptySlotIcon();
	}
}

void USuspenseCoreEquipmentSlotWidget::SetEmptySlotIconTexture(UTexture2D* InTexture)
{
	CachedEmptySlotIconTexture = InTexture;

	// Update visuals if slot is empty
	if (IsEmpty())
	{
		UpdateEmptySlotIcon();
	}
}

//==================================================================
// Visual Updates
//==================================================================

void USuspenseCoreEquipmentSlotWidget::UpdateVisuals_Implementation()
{
	// Call base implementation for background and highlight
	Super::UpdateVisuals_Implementation();

	// Track item state changes for events
	bool bHasItemNow = CachedSlotData.IsOccupied();

	if (bHasItemNow && !bHadItemBefore)
	{
		K2_OnItemEquipped(CachedItemData);
	}
	else if (!bHasItemNow && bHadItemBefore)
	{
		K2_OnItemUnequipped();
	}

	bHadItemBefore = bHasItemNow;

	// Update empty slot icon visibility
	UpdateEmptySlotIcon();
}

void USuspenseCoreEquipmentSlotWidget::UpdateItemIcon()
{
	if (!ItemIcon)
	{
		return;
	}

	if (CachedSlotData.IsOccupied() && CachedItemData.IconPath.IsValid())
	{
		// Hide empty slot icon when item is present
		if (EmptySlotIcon)
		{
			EmptySlotIcon->SetVisibility(ESlateVisibility::Collapsed);
		}

		// Try synchronous load first
		if (UTexture2D* IconTexture = Cast<UTexture2D>(CachedItemData.IconPath.TryLoad()))
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(IconTexture);
			Brush.ImageSize = SlotSize * 0.85f; // 85% of slot size for padding
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.Tiling = ESlateBrushTileType::NoTile;

			ItemIcon->SetBrush(Brush);
			ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
			ItemIcon->SetColorAndOpacity(FLinearColor::White);

			// Equipment items typically don't rotate
			ItemIcon->SetRenderTransformAngle(0.0f);

			UE_LOG(LogTemp, Log, TEXT("EquipmentSlot[%s]: Loaded item icon"), *SlotTypeTag.ToString());
		}
		else
		{
			// Async load
			FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
			TWeakObjectPtr<UImage> WeakIcon(ItemIcon);
			TWeakObjectPtr<USuspenseCoreEquipmentSlotWidget> WeakThis(this);
			FSoftObjectPath IconPath = CachedItemData.IconPath;
			FVector2D CapturedSize = SlotSize;

			StreamableManager.RequestAsyncLoad(
				IconPath,
				FStreamableDelegate::CreateLambda([WeakIcon, WeakThis, IconPath, CapturedSize]()
				{
					USuspenseCoreEquipmentSlotWidget* This = WeakThis.Get();
					UImage* Icon = WeakIcon.Get();

					if (Icon && This)
					{
						if (UTexture2D* LoadedTexture = Cast<UTexture2D>(IconPath.ResolveObject()))
						{
							FSlateBrush Brush;
							Brush.SetResourceObject(LoadedTexture);
							Brush.ImageSize = CapturedSize * 0.85f;
							Brush.DrawAs = ESlateBrushDrawType::Image;
							Brush.Tiling = ESlateBrushTileType::NoTile;

							Icon->SetBrush(Brush);
							Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
							Icon->SetColorAndOpacity(FLinearColor::White);
						}
					}
				}),
				FStreamableManager::AsyncLoadHighPriority
			);

			// Show loading state
			ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		// No item - hide item icon, show empty slot icon
		ItemIcon->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USuspenseCoreEquipmentSlotWidget::UpdateEmptySlotIcon()
{
	if (!EmptySlotIcon)
	{
		return;
	}

	// Show empty slot icon only when slot is empty
	if (IsEmpty())
	{
		// Load and display empty slot icon
		if (CachedEmptySlotIconTexture)
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(CachedEmptySlotIconTexture);
			Brush.ImageSize = SlotSize * 0.7f; // 70% of slot size for silhouette
			Brush.DrawAs = ESlateBrushDrawType::Image;
			Brush.Tiling = ESlateBrushTileType::NoTile;

			EmptySlotIcon->SetBrush(Brush);
			EmptySlotIcon->SetColorAndOpacity(EmptySlotIconTint);
			EmptySlotIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else if (EmptySlotIconPath.IsValid())
		{
			// Try to load from path
			if (UTexture2D* IconTexture = Cast<UTexture2D>(EmptySlotIconPath.TryLoad()))
			{
				CachedEmptySlotIconTexture = IconTexture;

				FSlateBrush Brush;
				Brush.SetResourceObject(IconTexture);
				Brush.ImageSize = SlotSize * 0.7f;
				Brush.DrawAs = ESlateBrushDrawType::Image;
				Brush.Tiling = ESlateBrushTileType::NoTile;

				EmptySlotIcon->SetBrush(Brush);
				EmptySlotIcon->SetColorAndOpacity(EmptySlotIconTint);
				EmptySlotIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				// Async load
				FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
				TWeakObjectPtr<UImage> WeakIcon(EmptySlotIcon);
				TWeakObjectPtr<USuspenseCoreEquipmentSlotWidget> WeakThis(this);
				FSoftObjectPath IconPath = EmptySlotIconPath;
				FVector2D CapturedSize = SlotSize;
				FLinearColor CapturedTint = EmptySlotIconTint;

				StreamableManager.RequestAsyncLoad(
					IconPath,
					FStreamableDelegate::CreateLambda([WeakIcon, WeakThis, IconPath, CapturedSize, CapturedTint]()
					{
						USuspenseCoreEquipmentSlotWidget* This = WeakThis.Get();
						UImage* Icon = WeakIcon.Get();

						if (Icon && This)
						{
							if (UTexture2D* LoadedTexture = Cast<UTexture2D>(IconPath.ResolveObject()))
							{
								This->CachedEmptySlotIconTexture = LoadedTexture;

								FSlateBrush Brush;
								Brush.SetResourceObject(LoadedTexture);
								Brush.ImageSize = CapturedSize * 0.7f;
								Brush.DrawAs = ESlateBrushDrawType::Image;
								Brush.Tiling = ESlateBrushTileType::NoTile;

								Icon->SetBrush(Brush);
								Icon->SetColorAndOpacity(CapturedTint);
								Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
							}
						}
					}),
					FStreamableManager::AsyncLoadHighPriority
				);

				EmptySlotIcon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		else
		{
			// No empty slot icon configured - just hide
			EmptySlotIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	else
	{
		// Slot occupied - hide empty slot icon
		EmptySlotIcon->SetVisibility(ESlateVisibility::Collapsed);
	}
}

//==================================================================
// ISuspenseCoreSlotUI Interface Implementation
//==================================================================

void USuspenseCoreEquipmentSlotWidget::InitializeSlot_Implementation(const FSlotUIData& SlotData, const FItemUIData& ItemData)
{
	// Convert from interface types to internal types
	CachedSlotData.SlotIndex = SlotData.SlotIndex;
	CachedSlotData.SlotTypeTag = SlotData.SlotType; // FSlotUIData uses SlotType
	CachedSlotData.State = SlotData.bIsOccupied ? ESuspenseCoreUISlotState::Occupied : ESuspenseCoreUISlotState::Empty;
	CachedSlotData.bIsAnchor = SlotData.bIsAnchor;

	// Check if item data is valid (by checking if ItemID is valid)
	if (!ItemData.ItemID.IsNone())
	{
		CachedItemData.InstanceID = ItemData.ItemInstanceID;
		CachedItemData.ItemID = ItemData.ItemID;
		CachedItemData.IconPath = FSoftObjectPath(ItemData.IconAssetPath);
		CachedItemData.DisplayName = ItemData.DisplayName;
		CachedItemData.GridSize = ItemData.GridSize;
		CachedItemData.ItemType = ItemData.ItemType;
	}
	else
	{
		CachedItemData = FSuspenseCoreItemUIData();
	}

	UpdateVisuals();
}

void USuspenseCoreEquipmentSlotWidget::UpdateSlot_Implementation(const FSlotUIData& SlotData, const FItemUIData& ItemData)
{
	// Same as Initialize for equipment slots
	InitializeSlot_Implementation(SlotData, ItemData);
}

void USuspenseCoreEquipmentSlotWidget::UpdateSlotData(const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData)
{
	// Update cached data with extended types
	CachedSlotData = SlotData;
	CachedItemData = ItemData;

	// Update visuals
	UpdateVisuals();

	// Notify Blueprint about equipped/unequipped state changes
	bool bHasItemNow = SlotData.IsOccupied();
	if (bHasItemNow && !bHadItemBefore)
	{
		K2_OnItemEquipped(ItemData);
	}
	else if (!bHasItemNow && bHadItemBefore)
	{
		K2_OnItemUnequipped();
	}
	bHadItemBefore = bHasItemNow;
}

void USuspenseCoreEquipmentSlotWidget::SetSelected_Implementation(bool bInIsSelected)
{
	bIsSelected = bInIsSelected;
	if (bIsSelected)
	{
		SetHighlightState(ESuspenseCoreUISlotState::Selected);
	}
	else
	{
		SetHighlightState(CachedSlotData.IsOccupied() ? ESuspenseCoreUISlotState::Occupied : ESuspenseCoreUISlotState::Empty);
	}
}

void USuspenseCoreEquipmentSlotWidget::SetHighlighted_Implementation(bool bIsHighlighted, const FLinearColor& HighlightColor)
{
	if (bIsHighlighted)
	{
		SetHighlightState(ESuspenseCoreUISlotState::Highlighted);
	}
	else
	{
		SetHighlightState(ESuspenseCoreUISlotState::Empty);
	}
}

int32 USuspenseCoreEquipmentSlotWidget::GetSlotIndex_Implementation() const
{
	return CachedSlotData.SlotIndex;
}

FGuid USuspenseCoreEquipmentSlotWidget::GetItemInstanceID_Implementation() const
{
	return CachedItemData.InstanceID;
}

bool USuspenseCoreEquipmentSlotWidget::IsOccupied_Implementation() const
{
	return !IsEmpty();
}

void USuspenseCoreEquipmentSlotWidget::SetLocked_Implementation(bool bIsLocked)
{
	if (bIsLocked)
	{
		CachedSlotData.State = ESuspenseCoreUISlotState::Locked;
	}
	UpdateVisuals();
}

//==================================================================
// ISuspenseCoreDraggable Interface Implementation
//==================================================================

bool USuspenseCoreEquipmentSlotWidget::CanBeDragged_Implementation() const
{
	// Can drag if slot has an item and is not locked
	return !IsEmpty() && CachedSlotData.State != ESuspenseCoreUISlotState::Locked;
}

FDragDropUIData USuspenseCoreEquipmentSlotWidget::GetDragData_Implementation() const
{
	FDragDropUIData DragData;

	if (!IsEmpty())
	{
		DragData.SourceSlotIndex = CachedSlotData.SlotIndex;
		// Convert cached internal data to interface FItemUIData
		DragData.ItemData.ItemInstanceID = CachedItemData.InstanceID;
		DragData.ItemData.ItemID = CachedItemData.ItemID;
		DragData.ItemData.IconAssetPath = CachedItemData.IconPath.ToString();
		DragData.ItemData.DisplayName = CachedItemData.DisplayName;
		DragData.ItemData.GridSize = CachedItemData.GridSize;
		DragData.ItemData.ItemType = CachedItemData.ItemType;
		// SourceContainerType is a FGameplayTag, use Equipment tag
		DragData.SourceContainerType = FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Container.Equipment"));
		DragData.bIsValid = true;
	}

	return DragData;
}

void USuspenseCoreEquipmentSlotWidget::OnDragStarted_Implementation()
{
	// Visual feedback when drag starts
	SetHighlightState(ESuspenseCoreUISlotState::Dragging);

	UE_LOG(LogTemp, Log, TEXT("EquipmentSlot[%s]: Drag started"), *SlotTypeTag.ToString());
}

void USuspenseCoreEquipmentSlotWidget::OnDragEnded_Implementation(bool bWasDropped)
{
	// Clear drag visual feedback
	SetHighlightState(ESuspenseCoreUISlotState::Empty);

	UE_LOG(LogTemp, Log, TEXT("EquipmentSlot[%s]: Drag ended, dropped=%d"), *SlotTypeTag.ToString(), bWasDropped ? 1 : 0);
}

void USuspenseCoreEquipmentSlotWidget::UpdateDragVisual_Implementation(bool bIsValidTarget)
{
	// Update visual during drag based on target validity
	if (bIsValidTarget)
	{
		SetHighlightState(ESuspenseCoreUISlotState::DropTargetValid);
	}
	else
	{
		SetHighlightState(ESuspenseCoreUISlotState::DropTargetInvalid);
	}
}

//==================================================================
// ISuspenseCoreDropTarget Interface Implementation
//==================================================================

FSlotValidationResult USuspenseCoreEquipmentSlotWidget::CanAcceptDrop_Implementation(const UDragDropOperation* DragOperation) const
{
	FSlotValidationResult Result;
	Result.bIsValid = false;

	if (!DragOperation)
	{
		Result.ErrorMessage = NSLOCTEXT("SuspenseCore", "InvalidDragOp", "Invalid drag operation");
		return Result;
	}

	// Cast to our drag operation type
	const USuspenseCoreDragDropOperation* SuspenseDragOp = Cast<USuspenseCoreDragDropOperation>(DragOperation);
	if (!SuspenseDragOp)
	{
		Result.ErrorMessage = NSLOCTEXT("SuspenseCore", "InvalidDragOp", "Invalid drag operation");
		return Result;
	}

	const FSuspenseCoreDragData& DragData = SuspenseDragOp->GetDragData();

	// Check if slot can accept this item type
	if (!CanAcceptItemType(DragData.Item.ItemType))
	{
		Result.ErrorMessage = FText::Format(
			NSLOCTEXT("SuspenseCore", "ItemTypeNotAllowed", "Cannot equip {0} in this slot"),
			DragData.Item.DisplayName
		);
		return Result;
	}

	// Check if slot is locked
	if (CachedSlotData.State == ESuspenseCoreUISlotState::Locked)
	{
		Result.ErrorMessage = NSLOCTEXT("SuspenseCore", "SlotLocked", "Slot is locked");
		return Result;
	}

	// Valid drop target
	Result.bIsValid = true;
	Result.ErrorMessage = FText::GetEmpty();
	return Result;
}

bool USuspenseCoreEquipmentSlotWidget::HandleDrop_Implementation(UDragDropOperation* DragOperation)
{
	// Validate drop first
	FSlotValidationResult Validation = CanAcceptDrop_Implementation(DragOperation);
	if (!Validation.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentSlot[%s]: Drop rejected - %s"),
			*SlotTypeTag.ToString(), *Validation.ErrorMessage.ToString());
		return false;
	}

	// Drop handling is delegated to the container widget via provider
	// This method mainly validates - actual transfer happens through provider
	UE_LOG(LogTemp, Log, TEXT("EquipmentSlot[%s]: Drop accepted"), *SlotTypeTag.ToString());
	return true;
}

void USuspenseCoreEquipmentSlotWidget::NotifyDragEnter_Implementation(UDragDropOperation* DragOperation)
{
	if (!DragOperation)
	{
		SetHighlightState(ESuspenseCoreUISlotState::DropTargetInvalid);
		return;
	}

	// Show visual feedback when drag enters slot
	FSlotValidationResult Validation = CanAcceptDrop_Implementation(DragOperation);

	if (Validation.bIsValid)
	{
		SetHighlightState(ESuspenseCoreUISlotState::DropTargetValid);
	}
	else
	{
		SetHighlightState(ESuspenseCoreUISlotState::DropTargetInvalid);
	}
}

void USuspenseCoreEquipmentSlotWidget::NotifyDragLeave_Implementation()
{
	// Clear highlight when drag leaves
	SetHighlightState(ESuspenseCoreUISlotState::Empty);
}

int32 USuspenseCoreEquipmentSlotWidget::GetDropTargetSlot_Implementation() const
{
	return CachedSlotData.SlotIndex;
}
