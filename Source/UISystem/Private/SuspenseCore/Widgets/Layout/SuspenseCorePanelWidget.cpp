// SuspenseCorePanelWidget.cpp
// SuspenseCore - Panel Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Layout/SuspenseCorePanelWidget.h"
#include "SuspenseCore/Widgets/Base/SuspenseCoreBaseContainerWidget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIDataProvider.h"
#include "SuspenseCore/Subsystems/SuspenseCoreUIManager.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCorePanelWidget::USuspenseCorePanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bIsActive(false)
{
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCorePanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void USuspenseCorePanelWidget::NativeDestruct()
{
	// Clean up container widgets
	for (USuspenseCoreBaseContainerWidget* Container : ContainerWidgets)
	{
		if (Container)
		{
			Container->UnbindFromProvider();
			Container->RemoveFromParent();
		}
	}
	ContainerWidgets.Empty();
	ContainersByType.Empty();

	Super::NativeDestruct();
}

//==================================================================
// Initialization
//==================================================================

void USuspenseCorePanelWidget::InitializePanel(const FSuspenseCorePanelConfig& InPanelConfig)
{
	PanelConfig = InPanelConfig;

	// Create containers
	CreateContainers();

	// Bind to player providers automatically
	BindToPlayerProviders();

	// Notify Blueprint
	K2_OnPanelInitialized();
}

//==================================================================
// Container Management
//==================================================================

USuspenseCoreBaseContainerWidget* USuspenseCorePanelWidget::GetContainerByType(ESuspenseCoreContainerType ContainerType) const
{
	TObjectPtr<USuspenseCoreBaseContainerWidget> const* Found = ContainersByType.Find(ContainerType);
	return Found ? *Found : nullptr;
}

USuspenseCoreBaseContainerWidget* USuspenseCorePanelWidget::GetContainerAtIndex(int32 Index) const
{
	if (Index >= 0 && Index < ContainerWidgets.Num())
	{
		return ContainerWidgets[Index];
	}
	return nullptr;
}

bool USuspenseCorePanelWidget::BindContainerToProvider(ESuspenseCoreContainerType ContainerType, TScriptInterface<ISuspenseCoreUIDataProvider> Provider)
{
	USuspenseCoreBaseContainerWidget* Container = GetContainerByType(ContainerType);
	if (!Container)
	{
		return false;
	}

	Container->BindToProvider(Provider);
	return true;
}

void USuspenseCorePanelWidget::BindToPlayerProviders()
{
	USuspenseCoreUIManager* UIManager = USuspenseCoreUIManager::Get(this);
	if (!UIManager)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return;
	}

	// Bind each container type to appropriate provider
	for (const auto& Pair : ContainersByType)
	{
		ESuspenseCoreContainerType ContainerType = Pair.Key;
		USuspenseCoreBaseContainerWidget* Container = Pair.Value;

		if (!Container)
		{
			continue;
		}

		TScriptInterface<ISuspenseCoreUIDataProvider> Provider;

		switch (ContainerType)
		{
		case ESuspenseCoreContainerType::Inventory:
			Provider = UIManager->GetPlayerInventoryProvider(PC);
			break;

		case ESuspenseCoreContainerType::Equipment:
			Provider = UIManager->GetPlayerEquipmentProvider(PC);
			break;

		default:
			// Other types (Stash, Trader, Loot) require explicit binding
			break;
		}

		if (Provider)
		{
			Container->BindToProvider(Provider);
		}
	}
}

void USuspenseCorePanelWidget::BindSecondaryProvider(TScriptInterface<ISuspenseCoreUIDataProvider> Provider)
{
	if (!Provider)
	{
		return;
	}

	ISuspenseCoreUIDataProvider* ProviderInterface = Provider.GetInterface();
	if (!ProviderInterface)
	{
		return;
	}

	// Get the container type from the provider
	ESuspenseCoreContainerType ContainerType = ProviderInterface->GetContainerType();

	// Find matching container
	USuspenseCoreBaseContainerWidget* Container = GetContainerByType(ContainerType);
	if (Container)
	{
		Container->BindToProvider(Provider);
	}
}

void USuspenseCorePanelWidget::RefreshAllContainers()
{
	for (USuspenseCoreBaseContainerWidget* Container : ContainerWidgets)
	{
		if (Container && Container->IsBoundToProvider())
		{
			Container->RefreshFromProvider();
		}
	}
}

//==================================================================
// Panel State
//==================================================================

void USuspenseCorePanelWidget::OnPanelShown()
{
	bIsActive = true;

	// Refresh all containers
	RefreshAllContainers();

	// Notify Blueprint
	K2_OnPanelShown();
}

void USuspenseCorePanelWidget::OnPanelHidden()
{
	bIsActive = false;

	// Clear any selection/highlights
	for (USuspenseCoreBaseContainerWidget* Container : ContainerWidgets)
	{
		if (Container)
		{
			Container->ClearSelection();
			Container->ClearHighlights();
		}
	}

	// Notify Blueprint
	K2_OnPanelHidden();
}

//==================================================================
// Container Creation
//==================================================================

void USuspenseCorePanelWidget::CreateContainers_Implementation()
{
	// Clear existing containers
	for (USuspenseCoreBaseContainerWidget* Container : ContainerWidgets)
	{
		if (Container)
		{
			Container->RemoveFromParent();
		}
	}
	ContainerWidgets.Empty();
	ContainersByType.Empty();

	// Get layout container
	UPanelWidget* LayoutContainer = nullptr;
	if (PanelConfig.bHorizontalLayout && HorizontalContainerBox)
	{
		LayoutContainer = HorizontalContainerBox;
		HorizontalContainerBox->ClearChildren();
	}
	else if (VerticalContainerBox)
	{
		LayoutContainer = VerticalContainerBox;
		VerticalContainerBox->ClearChildren();
	}

	if (!LayoutContainer)
	{
		return;
	}

	// Create container for each type in config
	for (ESuspenseCoreContainerType ContainerType : PanelConfig.ContainerTypes)
	{
		// Get widget class for this type
		TSubclassOf<USuspenseCoreBaseContainerWidget> WidgetClass = GetWidgetClassForContainerType(ContainerType);
		if (!WidgetClass)
		{
			continue;
		}

		// Create widget
		USuspenseCoreBaseContainerWidget* Container = CreateWidget<USuspenseCoreBaseContainerWidget>(GetOwningPlayer(), WidgetClass);
		if (!Container)
		{
			continue;
		}

		// Add to layout
		if (HorizontalContainerBox && PanelConfig.bHorizontalLayout)
		{
			UHorizontalBoxSlot* HBoxSlot = HorizontalContainerBox->AddChildToHorizontalBox(Container);
			if (HBoxSlot)
			{
				HBoxSlot->SetHorizontalAlignment(HAlign_Fill);
				HBoxSlot->SetVerticalAlignment(VAlign_Fill);
				HBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}
		else if (VerticalContainerBox)
		{
			UVerticalBoxSlot* VBoxSlot = VerticalContainerBox->AddChildToVerticalBox(Container);
			if (VBoxSlot)
			{
				VBoxSlot->SetHorizontalAlignment(HAlign_Fill);
				VBoxSlot->SetVerticalAlignment(VAlign_Fill);
				VBoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
		}

		// Track container
		ContainerWidgets.Add(Container);
		ContainersByType.Add(ContainerType, Container);

		// Notify Blueprint
		K2_OnContainerCreated(Container, ContainerType);
	}
}

TSubclassOf<USuspenseCoreBaseContainerWidget> USuspenseCorePanelWidget::GetWidgetClassForContainerType_Implementation(ESuspenseCoreContainerType ContainerType)
{
	switch (ContainerType)
	{
	case ESuspenseCoreContainerType::Inventory:
		return InventoryWidgetClass;

	case ESuspenseCoreContainerType::Equipment:
		return EquipmentWidgetClass;

	case ESuspenseCoreContainerType::Stash:
		return StashWidgetClass;

	case ESuspenseCoreContainerType::Trader:
		return TraderWidgetClass;

	case ESuspenseCoreContainerType::Loot:
		return LootWidgetClass;

	default:
		return nullptr;
	}
}
