// SuspenseCoreContextMenuWidget.cpp
// SuspenseCore - Item Context Menu Widget Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Widgets/ContextMenu/SuspenseCoreContextMenuWidget.h"
#include "SuspenseCore/Events/UI/SuspenseCoreUIEvents.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

//==================================================================
// Constructor
//==================================================================

USuspenseCoreContextMenuWidget::USuspenseCoreContextMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ScreenEdgePadding(10.0f)
	, SlotIndex(INDEX_NONE)
	, SelectedActionIndex(0)
{
	// Start hidden
	SetVisibility(ESlateVisibility::Collapsed);

	// Need to receive focus for keyboard input
	SetIsFocusable(true);
}

//==================================================================
// UUserWidget Interface
//==================================================================

void USuspenseCoreContextMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void USuspenseCoreContextMenuWidget::NativeDestruct()
{
	Actions.Empty();
	Super::NativeDestruct();
}

FReply USuspenseCoreContextMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Check if click was outside menu
	if (!InGeometry.IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		Hide();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USuspenseCoreContextMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	FKey Key = InKeyEvent.GetKey();

	// Navigation
	if (Key == EKeys::Up || Key == EKeys::W)
	{
		NavigateUp();
		return FReply::Handled();
	}
	else if (Key == EKeys::Down || Key == EKeys::S)
	{
		NavigateDown();
		return FReply::Handled();
	}
	// Confirm
	else if (Key == EKeys::Enter || Key == EKeys::SpaceBar)
	{
		ExecuteSelectedAction();
		return FReply::Handled();
	}
	// Cancel
	else if (Key == EKeys::Escape)
	{
		Hide();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

//==================================================================
// Context Menu Control
//==================================================================

void USuspenseCoreContextMenuWidget::ShowForItem(
	const FSuspenseCoreItemUIData& ItemData,
	const FGuid& InContainerID,
	int32 InSlotIndex,
	const FVector2D& ScreenPosition,
	const TArray<FGameplayTag>& AvailableActions)
{
	CurrentItemData = ItemData;
	ContainerID = InContainerID;
	SlotIndex = InSlotIndex;
	SelectedActionIndex = 0;

	// Create action buttons
	CreateActionButtons(AvailableActions);

	// Position menu
	FVector2D BestPosition = CalculateBestPosition(ScreenPosition);
	SetRenderTranslation(BestPosition);

	// Show and focus
	SetVisibility(ESlateVisibility::Visible);

	// Take keyboard focus
	if (APlayerController* PC = GetOwningPlayer())
	{
		SetUserFocus(PC);
	}

	// Update visual selection
	UpdateSelectionVisual();
}

void USuspenseCoreContextMenuWidget::Hide()
{
	SetVisibility(ESlateVisibility::Collapsed);

	// Clear actions
	if (ActionContainer)
	{
		ActionContainer->ClearChildren();
	}
	Actions.Empty();

	CurrentItemData = FSuspenseCoreItemUIData();
	ContainerID = FGuid();
	SlotIndex = INDEX_NONE;
}

void USuspenseCoreContextMenuWidget::ExecuteSelectedAction()
{
	if (SelectedActionIndex < 0 || SelectedActionIndex >= Actions.Num())
	{
		return;
	}

	const FSuspenseCoreContextMenuAction& Action = Actions[SelectedActionIndex];
	if (!Action.bEnabled)
	{
		return;
	}

	// Broadcast action
	OnActionSelected.Broadcast(Action.ActionTag, ContainerID, SlotIndex);

	// Notify Blueprint
	K2_OnActionExecuted(Action.ActionTag);

	// Hide menu after action
	Hide();
}

void USuspenseCoreContextMenuWidget::NavigateUp()
{
	if (Actions.Num() == 0)
	{
		return;
	}

	// Find previous enabled action
	int32 StartIndex = SelectedActionIndex;
	do
	{
		SelectedActionIndex = (SelectedActionIndex - 1 + Actions.Num()) % Actions.Num();
	}
	while (!Actions[SelectedActionIndex].bEnabled && SelectedActionIndex != StartIndex);

	UpdateSelectionVisual();
}

void USuspenseCoreContextMenuWidget::NavigateDown()
{
	if (Actions.Num() == 0)
	{
		return;
	}

	// Find next enabled action
	int32 StartIndex = SelectedActionIndex;
	do
	{
		SelectedActionIndex = (SelectedActionIndex + 1) % Actions.Num();
	}
	while (!Actions[SelectedActionIndex].bEnabled && SelectedActionIndex != StartIndex);

	UpdateSelectionVisual();
}

//==================================================================
// Action Management
//==================================================================

void USuspenseCoreContextMenuWidget::CreateActionButtons_Implementation(const TArray<FGameplayTag>& ActionTags)
{
	// Clear existing
	if (ActionContainer)
	{
		ActionContainer->ClearChildren();
	}
	Actions.Empty();

	if (!ActionContainer)
	{
		return;
	}

	for (const FGameplayTag& ActionTag : ActionTags)
	{
		// Create action data
		FSuspenseCoreContextMenuAction Action;
		Action.ActionTag = ActionTag;
		Action.DisplayText = GetActionDisplayText(ActionTag);
		Action.bEnabled = true;

		// Try Blueprint customization first
		UButton* CustomButton = K2_OnCreateActionButton(Action);
		if (CustomButton)
		{
			Action.ActionButton = CustomButton;
		}
		else
		{
			// Create default button
			UButton* Button = NewObject<UButton>(this);
			if (Button)
			{
				UTextBlock* Text = NewObject<UTextBlock>(Button);
				if (Text)
				{
					Text->SetText(Action.DisplayText);
					Button->AddChild(Text);
				}

				Action.ActionButton = Button;
			}
		}

		if (Action.ActionButton)
		{
			// Add to container
			UVerticalBoxSlot* Slot = ActionContainer->AddChildToVerticalBox(Action.ActionButton);
			if (Slot)
			{
				Slot->SetPadding(FMargin(2.0f));
			}

			// Bind click - we use a workaround since dynamic delegates don't pass parameters easily
			// In production code, you'd use a custom button class or wrapper
		}

		Actions.Add(Action);
	}
}

FText USuspenseCoreContextMenuWidget::GetActionDisplayText_Implementation(const FGameplayTag& ActionTag)
{
	// Map action tags to display text
	static TMap<FGameplayTag, FText> ActionTextMap;
	if (ActionTextMap.Num() == 0)
	{
		ActionTextMap.Add(TAG_SuspenseCore_UIAction_Use, NSLOCTEXT("SuspenseCore", "ActionUse", "Use"));
		ActionTextMap.Add(TAG_SuspenseCore_UIAction_Equip, NSLOCTEXT("SuspenseCore", "ActionEquip", "Equip"));
		ActionTextMap.Add(TAG_SuspenseCore_UIAction_Unequip, NSLOCTEXT("SuspenseCore", "ActionUnequip", "Unequip"));
		ActionTextMap.Add(TAG_SuspenseCore_UIAction_Drop, NSLOCTEXT("SuspenseCore", "ActionDrop", "Drop"));
		ActionTextMap.Add(TAG_SuspenseCore_UIAction_Split, NSLOCTEXT("SuspenseCore", "ActionSplit", "Split Stack"));
		ActionTextMap.Add(TAG_SuspenseCore_UIAction_Examine, NSLOCTEXT("SuspenseCore", "ActionExamine", "Examine"));
		ActionTextMap.Add(TAG_SuspenseCore_UIAction_Discard, NSLOCTEXT("SuspenseCore", "ActionDiscard", "Discard"));
		ActionTextMap.Add(TAG_SuspenseCore_UIAction_Sell, NSLOCTEXT("SuspenseCore", "ActionSell", "Sell"));
		ActionTextMap.Add(TAG_SuspenseCore_UIAction_Buy, NSLOCTEXT("SuspenseCore", "ActionBuy", "Buy"));
	}

	const FText* Found = ActionTextMap.Find(ActionTag);
	if (Found)
	{
		return *Found;
	}

	// Fallback to tag name
	return FText::FromName(ActionTag.GetTagName());
}

void USuspenseCoreContextMenuWidget::OnActionButtonClicked(FGameplayTag ActionTag)
{
	// Find action index
	for (int32 i = 0; i < Actions.Num(); ++i)
	{
		if (Actions[i].ActionTag == ActionTag)
		{
			SelectedActionIndex = i;
			ExecuteSelectedAction();
			return;
		}
	}
}

void USuspenseCoreContextMenuWidget::UpdateSelectionVisual()
{
	for (int32 i = 0; i < Actions.Num(); ++i)
	{
		const FSuspenseCoreContextMenuAction& Action = Actions[i];
		if (!Action.ActionButton)
		{
			continue;
		}

		// Update button visual based on selection
		// In a full implementation, you'd change button style, add selection indicator, etc.
		if (i == SelectedActionIndex)
		{
			// Selected state
			Action.ActionButton->SetKeyboardFocus();
		}
	}
}

FVector2D USuspenseCoreContextMenuWidget::CalculateBestPosition(const FVector2D& DesiredPosition)
{
	FVector2D Result = DesiredPosition;

	// Get viewport size
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D ViewportSize;
		GEngine->GameViewport->GetViewportSize(ViewportSize);

		// Get menu size
		FVector2D MenuSize = GetDesiredSize();

		// Clamp X to stay on screen
		if (Result.X + MenuSize.X > ViewportSize.X - ScreenEdgePadding)
		{
			Result.X = ViewportSize.X - MenuSize.X - ScreenEdgePadding;
		}
		Result.X = FMath::Max(Result.X, ScreenEdgePadding);

		// Clamp Y to stay on screen
		if (Result.Y + MenuSize.Y > ViewportSize.Y - ScreenEdgePadding)
		{
			Result.Y = ViewportSize.Y - MenuSize.Y - ScreenEdgePadding;
		}
		Result.Y = FMath::Max(Result.Y, ScreenEdgePadding);
	}

	return Result;
}
