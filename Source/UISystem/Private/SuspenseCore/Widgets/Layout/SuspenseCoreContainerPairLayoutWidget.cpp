// SuspenseCoreContainerPairLayoutWidget.cpp
// SuspenseCore - Container Pair Layout Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/Layout/SuspenseCoreContainerPairLayoutWidget.h"
#include "SuspenseCore/Widgets/Base/SuspenseCoreBaseContainerWidget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIDataProvider.h"
#include "Components/CanvasPanel.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

USuspenseCoreContainerPairLayoutWidget::USuspenseCoreContainerPairLayoutWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreContainerPairLayoutWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// If BindWidget didn't find containers, try auto-discovery from child widgets
	if (!PrimaryContainer || !SecondaryContainer)
	{
		AutoDiscoverContainers();
	}

	UE_LOG(LogTemp, Log, TEXT("[ContainerPairLayout] Constructed - LayoutTag: %s, Primary: %d, Secondary: %d, PrimaryContainer: %s, SecondaryContainer: %s"),
		*LayoutTag.ToString(),
		static_cast<int32>(PrimaryContainerType),
		static_cast<int32>(SecondaryContainerType),
		PrimaryContainer ? *PrimaryContainer->GetClass()->GetName() : TEXT("NULL"),
		SecondaryContainer ? *SecondaryContainer->GetClass()->GetName() : TEXT("NULL"));

	// Validate container types match expected configuration
	ValidateContainerTypes();

	// Auto-bind to providers if configured
	if (bAutoBindOnConstruct)
	{
		BindToPlayerProviders();
	}

	bIsInitialized = true;
	bIsActive = true;

	K2_OnLayoutConstructed();
}

void USuspenseCoreContainerPairLayoutWidget::NativeDestruct()
{
	UnbindAllProviders();
	bIsInitialized = false;
	bIsActive = false;

	Super::NativeDestruct();
}

void USuspenseCoreContainerPairLayoutWidget::SetVisibility(ESlateVisibility InVisibility)
{
	ESlateVisibility OldVisibility = GetVisibility();
	Super::SetVisibility(InVisibility);

	// Detect transitions between visible and hidden
	bool bWasVisible = (OldVisibility == ESlateVisibility::Visible || OldVisibility == ESlateVisibility::SelfHitTestInvisible);
	bool bNowVisible = (InVisibility == ESlateVisibility::Visible || InVisibility == ESlateVisibility::SelfHitTestInvisible);

	if (bIsInitialized && bWasVisible != bNowVisible)
	{
		if (bNowVisible)
		{
			UE_LOG(LogTemp, Log, TEXT("[ContainerPairLayout] Visibility changed to visible - calling OnLayoutShown"));
			OnLayoutShown();
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[ContainerPairLayout] Visibility changed to hidden - calling OnLayoutHidden"));
			OnLayoutHidden();
		}
	}
}

TArray<USuspenseCoreBaseContainerWidget*> USuspenseCoreContainerPairLayoutWidget::GetAllContainers() const
{
	TArray<USuspenseCoreBaseContainerWidget*> Containers;

	if (PrimaryContainer)
	{
		Containers.Add(PrimaryContainer);
	}

	if (SecondaryContainer)
	{
		Containers.Add(SecondaryContainer);
	}

	return Containers;
}

USuspenseCoreBaseContainerWidget* USuspenseCoreContainerPairLayoutWidget::GetContainerByType(ESuspenseCoreContainerType ContainerType) const
{
	if (PrimaryContainer && PrimaryContainer->GetExpectedContainerType() == ContainerType)
	{
		return PrimaryContainer;
	}

	if (SecondaryContainer && SecondaryContainer->GetExpectedContainerType() == ContainerType)
	{
		return SecondaryContainer;
	}

	return nullptr;
}

void USuspenseCoreContainerPairLayoutWidget::BindToPlayerProviders()
{
	bool bAnyBound = false;

	// Bind primary container
	if (PrimaryContainer)
	{
		TScriptInterface<ISuspenseCoreUIDataProvider> Provider = FindProviderForType(PrimaryContainerType);
		if (Provider)
		{
			PrimaryContainer->BindToProvider(Provider);
			bAnyBound = true;
			UE_LOG(LogTemp, Log, TEXT("[ContainerPairLayout] Bound primary container (type=%d) to provider"),
				static_cast<int32>(PrimaryContainerType));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ContainerPairLayout] No provider found for primary container type %d"),
				static_cast<int32>(PrimaryContainerType));
		}
	}

	// Bind secondary container
	if (SecondaryContainer)
	{
		TScriptInterface<ISuspenseCoreUIDataProvider> Provider = FindProviderForType(SecondaryContainerType);
		if (Provider)
		{
			SecondaryContainer->BindToProvider(Provider);
			bAnyBound = true;
			UE_LOG(LogTemp, Log, TEXT("[ContainerPairLayout] Bound secondary container (type=%d) to provider"),
				static_cast<int32>(SecondaryContainerType));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[ContainerPairLayout] No provider found for secondary container type %d"),
				static_cast<int32>(SecondaryContainerType));
		}
	}

	if (bAnyBound)
	{
		K2_OnProvidersBound();
	}
}

bool USuspenseCoreContainerPairLayoutWidget::BindProviderToContainer(ESuspenseCoreContainerType ContainerType, TScriptInterface<ISuspenseCoreUIDataProvider> Provider)
{
	if (!Provider)
	{
		return false;
	}

	USuspenseCoreBaseContainerWidget* Container = GetContainerByType(ContainerType);
	if (!Container)
	{
		// Try matching by configuration
		if (ContainerType == PrimaryContainerType && PrimaryContainer)
		{
			Container = PrimaryContainer;
		}
		else if (ContainerType == SecondaryContainerType && SecondaryContainer)
		{
			Container = SecondaryContainer;
		}
	}

	if (Container)
	{
		Container->BindToProvider(Provider);
		return true;
	}

	return false;
}

void USuspenseCoreContainerPairLayoutWidget::UnbindAllProviders()
{
	if (PrimaryContainer)
	{
		PrimaryContainer->UnbindFromProvider();
	}

	if (SecondaryContainer)
	{
		SecondaryContainer->UnbindFromProvider();
	}
}

void USuspenseCoreContainerPairLayoutWidget::RefreshAllContainers()
{
	if (PrimaryContainer)
	{
		PrimaryContainer->RefreshFromProvider();
	}

	if (SecondaryContainer)
	{
		SecondaryContainer->RefreshFromProvider();
	}
}

void USuspenseCoreContainerPairLayoutWidget::OnLayoutShown()
{
	bIsActive = true;
	RefreshAllContainers();
	K2_OnLayoutShown();

	UE_LOG(LogTemp, Log, TEXT("[ContainerPairLayout] Layout shown - %s"), *LayoutTag.ToString());
}

void USuspenseCoreContainerPairLayoutWidget::OnLayoutHidden()
{
	bIsActive = false;
	K2_OnLayoutHidden();

	UE_LOG(LogTemp, Log, TEXT("[ContainerPairLayout] Layout hidden - %s"), *LayoutTag.ToString());
}

TScriptInterface<ISuspenseCoreUIDataProvider> USuspenseCoreContainerPairLayoutWidget::FindProviderForType(ESuspenseCoreContainerType ContainerType) const
{
	// Get owning player
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ContainerPairLayout] No owning player controller"));
		return nullptr;
	}

	// Search on Pawn
	APawn* Pawn = PC->GetPawn();
	if (Pawn)
	{
		// Check all components
		TArray<UActorComponent*> Components;
		Pawn->GetComponents(Components);

		for (UActorComponent* Component : Components)
		{
			if (Component->Implements<USuspenseCoreUIDataProvider>())
			{
				ISuspenseCoreUIDataProvider* Provider = Cast<ISuspenseCoreUIDataProvider>(Component);
				if (Provider && Provider->GetContainerType() == ContainerType)
				{
					TScriptInterface<ISuspenseCoreUIDataProvider> Result;
					Result.SetInterface(Provider);
					Result.SetObject(Component);
					return Result;
				}
			}
		}
	}

	// Search on PlayerState
	APlayerState* PS = PC->GetPlayerState<APlayerState>();
	if (PS)
	{
		TArray<UActorComponent*> Components;
		PS->GetComponents(Components);

		for (UActorComponent* Component : Components)
		{
			if (Component->Implements<USuspenseCoreUIDataProvider>())
			{
				ISuspenseCoreUIDataProvider* Provider = Cast<ISuspenseCoreUIDataProvider>(Component);
				if (Provider && Provider->GetContainerType() == ContainerType)
				{
					TScriptInterface<ISuspenseCoreUIDataProvider> Result;
					Result.SetInterface(Provider);
					Result.SetObject(Component);
					return Result;
				}
			}
		}
	}

	return nullptr;
}

void USuspenseCoreContainerPairLayoutWidget::ValidateContainerTypes()
{
	// Validate primary container type matches
	if (PrimaryContainer)
	{
		ESuspenseCoreContainerType ActualType = PrimaryContainer->GetExpectedContainerType();
		if (ActualType != PrimaryContainerType)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[ContainerPairLayout] Primary container type mismatch! Config expects %d, widget is %d. Using widget type."),
				static_cast<int32>(PrimaryContainerType),
				static_cast<int32>(ActualType));
			// Use the widget's type as authoritative
			PrimaryContainerType = ActualType;
		}
	}

	// Validate secondary container type matches
	if (SecondaryContainer)
	{
		ESuspenseCoreContainerType ActualType = SecondaryContainer->GetExpectedContainerType();
		if (ActualType != SecondaryContainerType)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[ContainerPairLayout] Secondary container type mismatch! Config expects %d, widget is %d. Using widget type."),
				static_cast<int32>(SecondaryContainerType),
				static_cast<int32>(ActualType));
			// Use the widget's type as authoritative
			SecondaryContainerType = ActualType;
		}
	}
}

void USuspenseCoreContainerPairLayoutWidget::AutoDiscoverContainers()
{
	UE_LOG(LogTemp, Log, TEXT("[ContainerPairLayout] AutoDiscoverContainers - scanning child widgets..."));

	// Get root widget (usually CanvasPanel or similar)
	UWidget* RootWidget = GetRootWidget();
	if (!RootWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ContainerPairLayout] No root widget found"));
		return;
	}

	// Recursively scan all child widgets
	TArray<USuspenseCoreBaseContainerWidget*> FoundContainers;
	ScanWidgetTreeForContainers(RootWidget, FoundContainers);

	UE_LOG(LogTemp, Log, TEXT("[ContainerPairLayout] Found %d container widgets"), FoundContainers.Num());

	// Assign containers based on their ExpectedContainerType matching our configured types
	for (USuspenseCoreBaseContainerWidget* Container : FoundContainers)
	{
		ESuspenseCoreContainerType ContainerType = Container->GetExpectedContainerType();

		UE_LOG(LogTemp, Log, TEXT("[ContainerPairLayout] Found container: %s (type=%d)"),
			*Container->GetClass()->GetName(), static_cast<int32>(ContainerType));

		if (!PrimaryContainer && ContainerType == PrimaryContainerType)
		{
			PrimaryContainer = Container;
			UE_LOG(LogTemp, Log, TEXT("[ContainerPairLayout] Assigned as PrimaryContainer"));
		}
		else if (!SecondaryContainer && ContainerType == SecondaryContainerType)
		{
			SecondaryContainer = Container;
			UE_LOG(LogTemp, Log, TEXT("[ContainerPairLayout] Assigned as SecondaryContainer"));
		}
	}

	// If we still don't have both, try assigning by order (first = primary, second = secondary)
	if ((!PrimaryContainer || !SecondaryContainer) && FoundContainers.Num() >= 2)
	{
		if (!PrimaryContainer)
		{
			PrimaryContainer = FoundContainers[0];
			UE_LOG(LogTemp, Log, TEXT("[ContainerPairLayout] Assigned first container as PrimaryContainer (fallback)"));
		}
		if (!SecondaryContainer)
		{
			for (USuspenseCoreBaseContainerWidget* Container : FoundContainers)
			{
				if (Container != PrimaryContainer)
				{
					SecondaryContainer = Container;
					UE_LOG(LogTemp, Log, TEXT("[ContainerPairLayout] Assigned second container as SecondaryContainer (fallback)"));
					break;
				}
			}
		}
	}
}

void USuspenseCoreContainerPairLayoutWidget::ScanWidgetTreeForContainers(UWidget* Widget, TArray<USuspenseCoreBaseContainerWidget*>& OutContainers)
{
	if (!Widget)
	{
		return;
	}

	// Check if this widget is a container
	if (USuspenseCoreBaseContainerWidget* Container = Cast<USuspenseCoreBaseContainerWidget>(Widget))
	{
		OutContainers.Add(Container);
	}

	// Check if it's a panel with children
	if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
	{
		for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
		{
			ScanWidgetTreeForContainers(Panel->GetChildAt(i), OutContainers);
		}
	}

	// Check if it's a UserWidget with nested content
	if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
	{
		// Don't recurse into the same widget
		if (UserWidget != this)
		{
			UWidget* NestedRoot = UserWidget->GetRootWidget();
			if (NestedRoot)
			{
				ScanWidgetTreeForContainers(NestedRoot, OutContainers);
			}
		}
	}
}
