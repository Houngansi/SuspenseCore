// SuspenseCoreEquipmentSlotWidget.cpp
// SuspenseCore - Equipment Slot Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Equipment/SuspenseCoreEquipmentSlotWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Styling/SlateBrush.h"

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
