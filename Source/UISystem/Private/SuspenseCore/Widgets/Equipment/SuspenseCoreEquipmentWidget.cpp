// SuspenseCoreEquipmentWidget.cpp
// SuspenseCore - Equipment Container Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Equipment/SuspenseCoreEquipmentWidget.h"
#include "SuspenseCore/Widgets/Equipment/SuspenseCoreEquipmentSlotWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreEquipmentWidget::USuspenseCoreEquipmentWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DefaultSlotSize(80.0f, 80.0f)
{
	// Set expected container type for equipment
	ExpectedContainerType = ESuspenseCoreContainerType::Equipment;
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCoreEquipmentWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// If slot configs were set before construction, create widgets now
	if (SlotConfigs.Num() > 0 && SlotWidgetsByType.Num() == 0)
	{
		CreateSlotWidgets();
	}
}

//==================================================================
// ISuspenseCoreUIContainer Overrides
//==================================================================

UWidget* USuspenseCoreEquipmentWidget::GetSlotWidget(int32 SlotIndex) const
{
	if (SlotWidgetsArray.IsValidIndex(SlotIndex))
	{
		return SlotWidgetsArray[SlotIndex];
	}
	return nullptr;
}

TArray<UWidget*> USuspenseCoreEquipmentWidget::GetAllSlotWidgets() const
{
	TArray<UWidget*> Result;
	Result.Reserve(SlotWidgetsArray.Num());
	for (const auto& SlotWidget : SlotWidgetsArray)
	{
		if (SlotWidget)
		{
			Result.Add(SlotWidget);
		}
	}
	return Result;
}

//==================================================================
// Equipment-Specific API
//==================================================================

void USuspenseCoreEquipmentWidget::InitializeFromSlotConfigs(const TArray<FEquipmentSlotConfig>& InSlotConfigs)
{
	SlotConfigs = InSlotConfigs;

	// Clear existing slots
	ClearSlotWidgets();

	// Create new slots
	CreateSlotWidgets();
}

void USuspenseCoreEquipmentWidget::InitializeWithUIConfig(
	const TArray<FEquipmentSlotConfig>& InSlotConfigs,
	const TArray<FSuspenseCoreEquipmentSlotUIConfig>& InUIConfigs)
{
	SlotConfigs = InSlotConfigs;
	SlotUIConfigs = InUIConfigs;

	// Clear existing slots
	ClearSlotWidgets();

	// Create new slots with UI config
	CreateSlotWidgets();
}

USuspenseCoreEquipmentSlotWidget* USuspenseCoreEquipmentWidget::GetSlotByType(EEquipmentSlotType SlotType) const
{
	if (const TObjectPtr<USuspenseCoreEquipmentSlotWidget>* FoundSlot = SlotWidgetsByType.Find(SlotType))
	{
		return *FoundSlot;
	}
	return nullptr;
}

USuspenseCoreEquipmentSlotWidget* USuspenseCoreEquipmentWidget::GetSlotByTag(const FGameplayTag& SlotTag) const
{
	if (const TWeakObjectPtr<USuspenseCoreEquipmentSlotWidget>* FoundSlot = SlotWidgetsByTag.Find(SlotTag))
	{
		return FoundSlot->Get();
	}
	return nullptr;
}

void USuspenseCoreEquipmentWidget::UpdateSlotByType(EEquipmentSlotType SlotType, const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData)
{
	if (USuspenseCoreEquipmentSlotWidget* SlotWidget = GetSlotByType(SlotType))
	{
		SlotWidget->UpdateSlot(SlotData, ItemData);
	}
}

void USuspenseCoreEquipmentWidget::UpdateSlotByTag(const FGameplayTag& SlotTag, const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData)
{
	if (USuspenseCoreEquipmentSlotWidget* SlotWidget = GetSlotByTag(SlotTag))
	{
		SlotWidget->UpdateSlot(SlotData, ItemData);
	}
}

TArray<EEquipmentSlotType> USuspenseCoreEquipmentWidget::GetAllSlotTypes() const
{
	TArray<EEquipmentSlotType> SlotTypes;
	SlotWidgetsByType.GetKeys(SlotTypes);
	return SlotTypes;
}

bool USuspenseCoreEquipmentWidget::HasSlot(EEquipmentSlotType SlotType) const
{
	return SlotWidgetsByType.Contains(SlotType);
}

//==================================================================
// Override Points from Base
//==================================================================

void USuspenseCoreEquipmentWidget::CreateSlotWidgets_Implementation()
{
	if (SlotConfigs.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: No slot configs provided"));
		return;
	}

	if (!SlotContainer)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipmentWidget: SlotContainer not bound - create CanvasPanel named 'SlotContainer' in Blueprint"));
		return;
	}

	// Get or use default widget class
	TSubclassOf<USuspenseCoreEquipmentSlotWidget> WidgetClassToUse = SlotWidgetClass;
	if (!WidgetClassToUse)
	{
		WidgetClassToUse = USuspenseCoreEquipmentSlotWidget::StaticClass();
	}

	// Create slot widgets
	for (int32 Index = 0; Index < SlotConfigs.Num(); ++Index)
	{
		const FEquipmentSlotConfig& Config = SlotConfigs[Index];

		if (Config.SlotType == EEquipmentSlotType::None)
		{
			continue;
		}

		// Find UI config for this slot
		const FSuspenseCoreEquipmentSlotUIConfig* UIConfig = FindUIConfigForSlot(Config.SlotType);

		// Create slot widget
		USuspenseCoreEquipmentSlotWidget* SlotWidget = CreateSlotWidget(Config, UIConfig);
		if (SlotWidget)
		{
			// Store references
			SlotWidgetsByType.Add(Config.SlotType, SlotWidget);
			SlotWidgetsByTag.Add(Config.SlotTag, SlotWidget);
			SlotWidgetsArray.Add(SlotWidget);

			// Position if UI config available
			if (UIConfig)
			{
				PositionSlotWidget(SlotWidget, *UIConfig);
			}

			UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Created slot %s (Index: %d)"),
				*Config.DisplayName.ToString(), Index);
		}
	}

	// Update container data
	CachedContainerData.TotalSlots = SlotWidgetsArray.Num();
	CachedContainerData.LayoutType = ESuspenseCoreSlotLayoutType::Named;

	// Notify Blueprint
	K2_OnSlotsInitialized(SlotWidgetsArray.Num());

	UE_LOG(LogTemp, Log, TEXT("EquipmentWidget: Initialized %d equipment slots"), SlotWidgetsArray.Num());
}

void USuspenseCoreEquipmentWidget::UpdateSlotWidget_Implementation(int32 SlotIndex, const FSuspenseCoreSlotUIData& SlotData, const FSuspenseCoreItemUIData& ItemData)
{
	if (SlotWidgetsArray.IsValidIndex(SlotIndex))
	{
		SlotWidgetsArray[SlotIndex]->UpdateSlot(SlotData, ItemData);
	}
}

void USuspenseCoreEquipmentWidget::ClearSlotWidgets_Implementation()
{
	// Remove all slot widgets
	for (auto& SlotWidget : SlotWidgetsArray)
	{
		if (SlotWidget)
		{
			SlotWidget->RemoveFromParent();
		}
	}

	SlotWidgetsByType.Empty();
	SlotWidgetsByTag.Empty();
	SlotWidgetsArray.Empty();
}

//==================================================================
// Slot Creation
//==================================================================

USuspenseCoreEquipmentSlotWidget* USuspenseCoreEquipmentWidget::CreateSlotWidget(
	const FEquipmentSlotConfig& SlotConfig,
	const FSuspenseCoreEquipmentSlotUIConfig* UIConfig)
{
	if (!SlotContainer)
	{
		return nullptr;
	}

	// Determine widget class
	TSubclassOf<USuspenseCoreEquipmentSlotWidget> WidgetClassToUse = SlotWidgetClass;
	if (!WidgetClassToUse)
	{
		WidgetClassToUse = USuspenseCoreEquipmentSlotWidget::StaticClass();
	}

	// Create widget
	USuspenseCoreEquipmentSlotWidget* SlotWidget = CreateWidget<USuspenseCoreEquipmentSlotWidget>(this, WidgetClassToUse);
	if (!SlotWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("EquipmentWidget: Failed to create slot widget for %s"), *SlotConfig.DisplayName.ToString());
		return nullptr;
	}

	// Initialize from config
	SlotWidget->InitializeFromConfig(SlotConfig);

	// Set slot size
	FVector2D SlotSize = DefaultSlotSize;
	if (UIConfig && UIConfig->SlotSize.X > 0 && UIConfig->SlotSize.Y > 0)
	{
		SlotSize.X = UIConfig->SlotSize.X * DefaultSlotSize.X;
		SlotSize.Y = UIConfig->SlotSize.Y * DefaultSlotSize.Y;
	}
	SlotWidget->SetSlotSize(SlotSize);

	// Set empty slot icon if provided in UI config
	if (UIConfig && UIConfig->EmptySlotIcon.IsValid())
	{
		SlotWidget->SetEmptySlotIconPath(UIConfig->EmptySlotIcon);
	}

	// Add to container
	SlotContainer->AddChild(SlotWidget);

	return SlotWidget;
}

void USuspenseCoreEquipmentWidget::PositionSlotWidget(USuspenseCoreEquipmentSlotWidget* SlotWidget, const FSuspenseCoreEquipmentSlotUIConfig& UIConfig)
{
	if (!SlotWidget || !SlotContainer)
	{
		return;
	}

	// Get canvas slot
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(SlotWidget->Slot);
	if (CanvasSlot)
	{
		// Set position from UI config
		CanvasSlot->SetPosition(UIConfig.LayoutPosition);

		// Set size
		FVector2D SlotSize = DefaultSlotSize;
		if (UIConfig.SlotSize.X > 0 && UIConfig.SlotSize.Y > 0)
		{
			SlotSize.X = UIConfig.SlotSize.X * DefaultSlotSize.X;
			SlotSize.Y = UIConfig.SlotSize.Y * DefaultSlotSize.Y;
		}
		CanvasSlot->SetSize(SlotSize);

		// Anchor to top-left for absolute positioning
		CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
		CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
	}
}

//==================================================================
// Private Helpers
//==================================================================

const FSuspenseCoreEquipmentSlotUIConfig* USuspenseCoreEquipmentWidget::FindUIConfigForSlot(EEquipmentSlotType SlotType) const
{
	for (const FSuspenseCoreEquipmentSlotUIConfig& Config : SlotUIConfigs)
	{
		// Match by slot type tag
		FString ExpectedTagName = FString::Printf(TEXT("Equipment.Slot.%s"),
			*UEnum::GetValueAsString(SlotType).RightChop(FString(TEXT("EEquipmentSlotType::")).Len()));
		FGameplayTag ExpectedTag = FGameplayTag::RequestGameplayTag(*ExpectedTagName, false);

		if (Config.SlotTypeTag == ExpectedTag)
		{
			return &Config;
		}
	}
	return nullptr;
}

int32 USuspenseCoreEquipmentWidget::GetSlotIndexForType(EEquipmentSlotType SlotType) const
{
	for (int32 Index = 0; Index < SlotWidgetsArray.Num(); ++Index)
	{
		if (SlotWidgetsArray[Index] && SlotWidgetsArray[Index]->GetSlotType() == SlotType)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}
