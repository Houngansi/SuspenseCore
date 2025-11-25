// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Layout/SuspenseBaseLayoutWidget.h"
#include "SuspenseHorizontalLayoutWidget.generated.h"

// Forward declarations
class UHorizontalBox;
class USizeBox;

/**
 * Horizontal layout widget that arranges child widgets in a row
 * Supports flexible sizing and spacing configuration
 */
UCLASS()
class UISYSTEM_API USuspenseHorizontalLayoutWidget : public USuspenseBaseLayoutWidget
{
    GENERATED_BODY()

public:
    USuspenseHorizontalLayoutWidget(const FObjectInitializer& ObjectInitializer);

protected:
    //~ Begin USuspenseBaseLayoutWidget Interface
    virtual UPanelWidget* GetLayoutPanel() const override;
    virtual bool AddWidgetToPanel(UUserWidget* Widget, const FLayoutWidgetConfig* Config) override;
    //~ End USuspenseBaseLayoutWidget Interface

    /** Main horizontal box container */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UHorizontalBox* HorizontalContainer;

    /** Default horizontal alignment for widgets */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Horizontal")
    TEnumAsByte<EHorizontalAlignment> DefaultHorizontalAlignment = HAlign_Fill;

    /** Default vertical alignment for widgets */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Horizontal")
    TEnumAsByte<EVerticalAlignment> DefaultVerticalAlignment = VAlign_Fill;

    /** Whether to use size boxes for fixed-size widgets */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Horizontal")
    bool bUseSizeBoxes = false;

    /** Default widget width when using size boxes (0 = auto) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Horizontal", meta = (EditCondition = "bUseSizeBoxes"))
    float DefaultWidgetWidth = 0.0f;

    /** Default widget height when using size boxes (0 = auto) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout|Horizontal", meta = (EditCondition = "bUseSizeBoxes"))
    float DefaultWidgetHeight = 0.0f;
};