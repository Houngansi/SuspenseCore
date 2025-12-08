// Copyright Suspense Team Team. All Rights Reserved.

#include "Widgets/HUD/SuspenseCrosshairWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "SuspenseCore/Delegates/SuspenseCoreEventManager.h"
#include "Math/UnrealMathUtility.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

USuspenseCrosshairWidget::USuspenseCrosshairWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Enable tick for smooth interpolation
    bEnableTick = true;

    // Set widget tag
    WidgetTag = FGameplayTag::RequestGameplayTag(TEXT("UI.HUD.Crosshair"));

    // Initialize default values
    CrosshairLength = 10.0f;
    CrosshairThickness = 2.0f;
    SpreadMultiplier = 20.0f;
    MinimumSpread = 5.0f;
    MaximumSpread = 100.0f;
    SpreadInterpSpeed = 10.0f;
    RecoveryInterpSpeed = 15.0f;
    CrosshairColor = FLinearColor::White;
    bShowDebugInfo = false;

    // Initialize state
    CurrentSpreadRadius = 0.0f;
    TargetSpreadRadius = 0.0f;
    BaseSpreadRadius = 0.0f;
    LastSpreadValue = 0.0f;
    LastRecoilValue = 0.0f;
    bCurrentlyFiring = false;
    bWasFiring = false;
    bCrosshairVisible = true;
}

void USuspenseCrosshairWidget::NativePreConstruct()
{
    Super::NativePreConstruct();

    // Update crosshair positions for design-time preview
    if (IsDesignTime())
    {
        UpdateCrosshairPositions();
    }
}

void USuspenseCrosshairWidget::InitializeWidget_Implementation()
{
    Super::InitializeWidget_Implementation();

    // Initialize spread values
    CurrentSpreadRadius = MinimumSpread;
    TargetSpreadRadius = MinimumSpread;
    BaseSpreadRadius = MinimumSpread;

    // Set initial visibility and positions
    SetCrosshairVisibility_Implementation(bCrosshairVisible);
    UpdateCrosshairPositions();

    // Subscribe to events
    SubscribeToEvents();

    UE_LOG(LogTemp, Log, TEXT("[MedComCrosshairWidget] Widget initialized - MinSpread: %.2f, MaxSpread: %.2f"),
        MinimumSpread, MaximumSpread);
}

void USuspenseCrosshairWidget::UninitializeWidget_Implementation()
{
    // Cleanup hit marker timer
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(HitMarkerTimerHandle);
    }

    // Unsubscribe from events
    UnsubscribeFromEvents();

    Super::UninitializeWidget_Implementation();

    UE_LOG(LogTemp, Log, TEXT("[MedComCrosshairWidget] Widget uninitialized"));
}

void USuspenseCrosshairWidget::UpdateWidget_Implementation(float DeltaTime)
{
    Super::UpdateWidget_Implementation(DeltaTime);

    // Choose interpolation speed based on firing state
    float InterpSpeed = bCurrentlyFiring ? SpreadInterpSpeed : RecoveryInterpSpeed;

    // Return to base spread when firing stops
    if (bWasFiring && !bCurrentlyFiring)
    {
        TargetSpreadRadius = BaseSpreadRadius;
        UE_LOG(LogTemp, VeryVerbose, TEXT("[MedComCrosshairWidget] Firing stopped, returning to base spread: %.2f"),
            BaseSpreadRadius);
    }

    // Interpolate spread radius for smooth animation
    float PreviousSpreadRadius = CurrentSpreadRadius;
    CurrentSpreadRadius = FMath::FInterpTo(CurrentSpreadRadius, TargetSpreadRadius, DeltaTime, InterpSpeed);

    // Log significant changes for debugging
    if (FMath::Abs(CurrentSpreadRadius - PreviousSpreadRadius) > 0.5f)
    {
        UE_LOG(LogTemp, VeryVerbose, TEXT("[MedComCrosshairWidget] Spread interpolating: %.2f -> %.2f (Target: %.2f)"),
            PreviousSpreadRadius, CurrentSpreadRadius, TargetSpreadRadius);
    }

    // Update crosshair element positions
    UpdateCrosshairPositions();

    // Display debug information
    if (bShowDebugInfo && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, DeltaTime, FColor::Yellow,
            FString::Printf(TEXT("Crosshair: Base=%.2f, Target=%.2f, Current=%.2f, Firing=%s"),
            BaseSpreadRadius, TargetSpreadRadius, CurrentSpreadRadius,
            bCurrentlyFiring ? TEXT("True") : TEXT("False")));
    }

    // Update firing state for next frame
    bWasFiring = bCurrentlyFiring;
}

void USuspenseCrosshairWidget::UpdateCrosshair_Implementation(float Spread, float Recoil, bool bIsFiring)
{
    UE_LOG(LogTemp, VeryVerbose, TEXT("[MedComCrosshairWidget] UpdateCrosshair: Spread=%.2f, Recoil=%.2f, Firing=%s"),
        Spread, Recoil, bIsFiring ? TEXT("True") : TEXT("False"));

    // Store values for debugging
    LastSpreadValue = Spread;
    LastRecoilValue = Recoil;
    bCurrentlyFiring = bIsFiring;

    // Apply UI scaling to spread value
    float SpreadRadius = Spread * SpreadMultiplier;

    // Ensure minimum spread
    SpreadRadius = FMath::Max(MinimumSpread, SpreadRadius);

    // Update base spread only when not firing
    if (!bIsFiring && !bWasFiring)
    {
        BaseSpreadRadius = SpreadRadius;
        UE_LOG(LogTemp, Verbose, TEXT("[MedComCrosshairWidget] Updated base spread: %.2f"), BaseSpreadRadius);
    }

    // Clamp to maximum spread
    TargetSpreadRadius = FMath::Min(SpreadRadius, MaximumSpread);

    // Broadcast crosshair update through interface
    ISuspenseCrosshairWidgetInterface::BroadcastCrosshairUpdated(this, Spread, Recoil);
}

void USuspenseCrosshairWidget::SetCrosshairVisibility_Implementation(bool bVisible)
{
    bCrosshairVisible = bVisible;
    const ESlateVisibility VisibilityState = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden;

    if (TopCrosshair) TopCrosshair->SetVisibility(VisibilityState);
    if (BottomCrosshair) BottomCrosshair->SetVisibility(VisibilityState);
    if (LeftCrosshair) LeftCrosshair->SetVisibility(VisibilityState);
    if (RightCrosshair) RightCrosshair->SetVisibility(VisibilityState);

    UE_LOG(LogTemp, Log, TEXT("[MedComCrosshairWidget] Visibility set to: %s"),
        bVisible ? TEXT("Visible") : TEXT("Hidden"));
}

bool USuspenseCrosshairWidget::IsCrosshairVisible_Implementation() const
{
    return bCrosshairVisible;
}

void USuspenseCrosshairWidget::SetCrosshairColor_Implementation(const FLinearColor& NewColor)
{
    CrosshairColor = NewColor;
    UpdateCrosshairPositions(); // This also updates colors

    // Broadcast color change through interface
    ISuspenseCrosshairWidgetInterface::BroadcastCrosshairColorChanged(this, NewColor);

    UE_LOG(LogTemp, Log, TEXT("[MedComCrosshairWidget] Color changed to R=%.2f, G=%.2f, B=%.2f, A=%.2f"),
        NewColor.R, NewColor.G, NewColor.B, NewColor.A);
}

FLinearColor USuspenseCrosshairWidget::GetCrosshairColor_Implementation() const
{
    return CrosshairColor;
}

void USuspenseCrosshairWidget::SetCrosshairType_Implementation(const FName& CrosshairType)
{
    // Basic implementation - could be extended for different crosshair styles
    UE_LOG(LogTemp, Log, TEXT("[MedComCrosshairWidget] Crosshair type set to: %s"), *CrosshairType.ToString());
}

void USuspenseCrosshairWidget::SetMinimumSpread_Implementation(float MinSpread)
{
    float OldMinimumSpread = MinimumSpread;
    MinimumSpread = FMath::Max(1.0f, MinSpread);

    // Adjust base spread if it's below new minimum
    if (BaseSpreadRadius < MinimumSpread)
    {
        BaseSpreadRadius = MinimumSpread;

        // Reset if not currently firing
        if (!bCurrentlyFiring)
        {
            ResetToBaseSpread();
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[MedComCrosshairWidget] Minimum spread changed: %.2f -> %.2f"),
        OldMinimumSpread, MinimumSpread);
}

void USuspenseCrosshairWidget::SetMaximumSpread_Implementation(float MaxSpread)
{
    MaximumSpread = FMath::Max(MinimumSpread + 1.0f, MaxSpread);

    // Clamp current values if they exceed new maximum
    if (TargetSpreadRadius > MaximumSpread)
    {
        TargetSpreadRadius = MaximumSpread;
    }
    if (CurrentSpreadRadius > MaximumSpread)
    {
        CurrentSpreadRadius = MaximumSpread;
    }

    UE_LOG(LogTemp, Log, TEXT("[MedComCrosshairWidget] Maximum spread set to: %.2f"), MaximumSpread);
}

void USuspenseCrosshairWidget::SetInterpolationSpeed_Implementation(float Speed)
{
    SpreadInterpSpeed = FMath::Max(0.1f, Speed);
    UE_LOG(LogTemp, Log, TEXT("[MedComCrosshairWidget] Interpolation speed set to: %.2f"), SpreadInterpSpeed);
}

void USuspenseCrosshairWidget::ShowHitMarker_Implementation(bool bHeadshot, bool bKill)
{
    DisplayHitMarker(bHeadshot, bKill);
}

void USuspenseCrosshairWidget::ResetToBaseSpread()
{
    // Immediately reset to base spread without animation
    TargetSpreadRadius = BaseSpreadRadius;
    CurrentSpreadRadius = BaseSpreadRadius;

    // Update positions
    UpdateCrosshairPositions();

    // Reset firing flags
    bCurrentlyFiring = false;
    bWasFiring = false;

    UE_LOG(LogTemp, Log, TEXT("[MedComCrosshairWidget] Force reset to base spread: %.2f"), BaseSpreadRadius);
}

void USuspenseCrosshairWidget::UpdateCrosshairPositions()
{
    if (!TopCrosshair || !BottomCrosshair || !LeftCrosshair || !RightCrosshair)
        return;

    // Get canvas panel slots for each crosshair element
    UCanvasPanelSlot* TopSlot = Cast<UCanvasPanelSlot>(TopCrosshair->Slot);
    UCanvasPanelSlot* BottomSlot = Cast<UCanvasPanelSlot>(BottomCrosshair->Slot);
    UCanvasPanelSlot* LeftSlot = Cast<UCanvasPanelSlot>(LeftCrosshair->Slot);
    UCanvasPanelSlot* RightSlot = Cast<UCanvasPanelSlot>(RightCrosshair->Slot);

    if (!TopSlot || !BottomSlot || !LeftSlot || !RightSlot)
    {
        UE_LOG(LogTemp, Error, TEXT("[MedComCrosshairWidget] Failed to get CanvasPanelSlots for crosshair elements"));
        return;
    }

    // Update sizes
    FVector2D VerticalSize(CrosshairThickness, CrosshairLength);
    FVector2D HorizontalSize(CrosshairLength, CrosshairThickness);

    TopSlot->SetSize(VerticalSize);
    BottomSlot->SetSize(VerticalSize);
    LeftSlot->SetSize(HorizontalSize);
    RightSlot->SetSize(HorizontalSize);

    // Calculate center position
    FVector2D WidgetSize = GetPaintSpaceGeometry().GetLocalSize();
    FVector2D CenterPos = WidgetSize * 0.5f;

    // Calculate offsets based on current spread
    float TopOffset = -CurrentSpreadRadius - CrosshairLength;
    float BottomOffset = CurrentSpreadRadius;
    float LeftOffset = -CurrentSpreadRadius - CrosshairLength;
    float RightOffset = CurrentSpreadRadius;

    // Set positions
    TopSlot->SetPosition(FVector2D(CenterPos.X - CrosshairThickness * 0.5f, CenterPos.Y + TopOffset));
    BottomSlot->SetPosition(FVector2D(CenterPos.X - CrosshairThickness * 0.5f, CenterPos.Y + BottomOffset));
    LeftSlot->SetPosition(FVector2D(CenterPos.X + LeftOffset, CenterPos.Y - CrosshairThickness * 0.5f));
    RightSlot->SetPosition(FVector2D(CenterPos.X + RightOffset, CenterPos.Y - CrosshairThickness * 0.5f));

    // Update colors
    TopCrosshair->SetColorAndOpacity(CrosshairColor);
    BottomCrosshair->SetColorAndOpacity(CrosshairColor);
    LeftCrosshair->SetColorAndOpacity(CrosshairColor);
    RightCrosshair->SetColorAndOpacity(CrosshairColor);
}

void USuspenseCrosshairWidget::SubscribeToEvents()
{
    if (USuspenseEventManager* EventManager = USuspenseBaseWidget::GetDelegateManager())
    {
        // Subscribe to crosshair updates
        CrosshairUpdateHandle = EventManager->SubscribeToCrosshairUpdated(
            [this](float Spread, float Recoil)
            {
                OnCrosshairUpdated(Spread, Recoil);
            });

        // Subscribe to crosshair color changes
        CrosshairColorHandle = EventManager->SubscribeToCrosshairColorChanged(
            [this](FLinearColor NewColor)
            {
                OnCrosshairColorChanged(NewColor);
            });

        UE_LOG(LogTemp, Log, TEXT("[MedComCrosshairWidget] Subscribed to events"));
    }
}

void USuspenseCrosshairWidget::UnsubscribeFromEvents()
{
    if (USuspenseEventManager* EventManager = USuspenseBaseWidget::GetDelegateManager())
    {
        if (CrosshairUpdateHandle.IsValid())
        {
            EventManager->UniversalUnsubscribe(CrosshairUpdateHandle);
            CrosshairUpdateHandle.Reset();
        }

        if (CrosshairColorHandle.IsValid())
        {
            EventManager->UniversalUnsubscribe(CrosshairColorHandle);
            CrosshairColorHandle.Reset();
        }

        UE_LOG(LogTemp, Log, TEXT("[MedComCrosshairWidget] Unsubscribed from events"));
    }
}

void USuspenseCrosshairWidget::OnCrosshairUpdated(float Spread, float Recoil)
{
    // Update crosshair with spread and recoil values
    // Note: We don't have firing state from this event, so we keep the current state
    UpdateCrosshair_Implementation(Spread, Recoil, bCurrentlyFiring);
}

void USuspenseCrosshairWidget::OnCrosshairColorChanged(FLinearColor NewColor)
{
    SetCrosshairColor_Implementation(NewColor);
}

void USuspenseCrosshairWidget::DisplayHitMarker(bool bHeadshot, bool bKill)
{
    // Store original color
    FLinearColor OriginalColor = CrosshairColor;

    // Choose hit marker color
    FLinearColor HitColor;
    if (bKill)
    {
        HitColor = KillMarkerColor;
    }
    else if (bHeadshot)
    {
        HitColor = HeadshotMarkerColor;
    }
    else
    {
        HitColor = HitMarkerColor;
    }

    // Set hit marker color
    CrosshairColor = HitColor;
    UpdateCrosshairPositions();

    // Clear existing timer
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(HitMarkerTimerHandle);

        // Set timer to restore original color
        World->GetTimerManager().SetTimer(HitMarkerTimerHandle,
            [this, OriginalColor]()
            {
                HideHitMarker();
                CrosshairColor = OriginalColor;
                UpdateCrosshairPositions();
            },
            HitMarkerDuration, false);
    }

    UE_LOG(LogTemp, Log, TEXT("[MedComCrosshairWidget] Hit marker displayed - Headshot: %s, Kill: %s"),
        bHeadshot ? TEXT("Yes") : TEXT("No"), bKill ? TEXT("Yes") : TEXT("No"));
}

void USuspenseCrosshairWidget::HideHitMarker()
{
    // Clear hit marker timer
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(HitMarkerTimerHandle);
    }
}
