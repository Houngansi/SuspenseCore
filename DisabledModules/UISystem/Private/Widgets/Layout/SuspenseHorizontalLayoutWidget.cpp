// Copyright Suspense Team Team. All Rights Reserved.

#include "Widgets/Layout/SuspenseHorizontalLayoutWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"

USuspenseHorizontalLayoutWidget::USuspenseHorizontalLayoutWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    DefaultHorizontalAlignment = HAlign_Fill;
    DefaultVerticalAlignment = VAlign_Fill;
    bUseSizeBoxes = false;
    DefaultWidgetWidth = 0.0f;
    DefaultWidgetHeight = 0.0f;
}

UPanelWidget* USuspenseHorizontalLayoutWidget::GetLayoutPanel() const
{
    return HorizontalContainer;
}

bool USuspenseHorizontalLayoutWidget::AddWidgetToPanel(UUserWidget* Widget, const FLayoutWidgetConfig* Config)
{
    if (!HorizontalContainer || !Widget)
    {
        return false;
    }

    UWidget* WidgetToAdd = Widget;

    // Wrap in SizeBox if requested
    if (bUseSizeBoxes && WidgetTree)
    {
        USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        if (SizeBox)
        {
            if (DefaultWidgetWidth > 0)
            {
                SizeBox->SetWidthOverride(DefaultWidgetWidth);
            }
            if (DefaultWidgetHeight > 0)
            {
                SizeBox->SetHeightOverride(DefaultWidgetHeight);
            }

            SizeBox->AddChild(Widget);
            WidgetToAdd = SizeBox;
        }
    }

    // Add to horizontal box
    UHorizontalBoxSlot* localSlot = HorizontalContainer->AddChildToHorizontalBox(WidgetToAdd);
    if (!localSlot)
    {
        return false;
    }

    // Configure slot
    if (Config)
    {
        localSlot->SetPadding(Config->Padding);
        
        // Set size based on weight
        if (Config->SizeWeight > 0)
        {
            localSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            localSlot->SetHorizontalAlignment(DefaultHorizontalAlignment);
            localSlot->SetVerticalAlignment(DefaultVerticalAlignment);
        }
        else
        {
            localSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
            localSlot->SetHorizontalAlignment(HAlign_Left);
            localSlot->SetVerticalAlignment(DefaultVerticalAlignment);
        }
    }
    else
    {
        // Default configuration
        localSlot->SetPadding(FMargin(4.0f));
        localSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        localSlot->SetHorizontalAlignment(DefaultHorizontalAlignment);
        localSlot->SetVerticalAlignment(DefaultVerticalAlignment);
    }

    return true;
}