// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Base/SuspenseBaseWidget.h"
#include "SuspenseCore/Interfaces/UI/ISuspenseCoreCrosshairWidget.h"
#include "SuspenseCrosshairWidget.generated.h"

// Forward declarations
class UImage;
class UCanvasPanel;

/**
 * Dynamic crosshair widget that adjusts based on weapon spread and recoil
 * 
 * Architecture principles:
 * - Inherits from USuspenseBaseWidget for standardized lifecycle
 * - Implements ISuspenseCrosshairWidgetInterface for crosshair-specific functionality
 * - Receives data only through interface methods and events
 * - No direct dependencies on weapon or game modules
 * - Uses EventDelegateManager for event subscription
 */
UCLASS()
class UISYSTEM_API USuspenseCrosshairWidget : public USuspenseBaseWidget, public ISuspenseCrosshairWidgetInterface
{
    GENERATED_BODY()

public:
    USuspenseCrosshairWidget(const FObjectInitializer& ObjectInitializer);

    //================================================
    // USuspenseBaseWidget Interface
    //================================================
    
    virtual void InitializeWidget_Implementation() override;
    virtual void UninitializeWidget_Implementation() override;
    virtual void UpdateWidget_Implementation(float DeltaTime) override;

    //================================================
    // ISuspenseCrosshairWidget Interface
    //================================================
    
    virtual void UpdateCrosshair_Implementation(float Spread, float Recoil, bool bIsFiring) override;
    virtual void SetCrosshairVisibility_Implementation(bool bVisible) override;
    virtual bool IsCrosshairVisible_Implementation() const override;
    virtual void SetCrosshairColor_Implementation(const FLinearColor& NewColor) override;
    virtual FLinearColor GetCrosshairColor_Implementation() const override;
    virtual void SetCrosshairType_Implementation(const FName& CrosshairType) override;
    virtual void SetMinimumSpread_Implementation(float MinSpread) override;
    virtual void SetMaximumSpread_Implementation(float MaxSpread) override;
    virtual void SetInterpolationSpeed_Implementation(float Speed) override;
    virtual void ShowHitMarker_Implementation(bool bHeadshot, bool bKill) override;

    //================================================
    // Additional Public Methods
    //================================================
    
    /** Reset crosshair to base spread immediately */
    UFUNCTION(BlueprintCallable, Category = "UI|Crosshair")
    void ResetToBaseSpread();
    
    /** Get current spread radius for debugging */
    UFUNCTION(BlueprintPure, Category = "UI|Crosshair")
    float GetCurrentSpread() const { return CurrentSpreadRadius; }
    
    /** Get base spread radius for debugging */
    UFUNCTION(BlueprintPure, Category = "UI|Crosshair")
    float GetBaseSpread() const { return BaseSpreadRadius; }
    
    /** Check if currently firing */
    UFUNCTION(BlueprintPure, Category = "UI|Crosshair")
    bool IsFiring() const { return bCurrentlyFiring; }

protected:
    //================================================
    // UUserWidget Interface
    //================================================
    
    virtual void NativePreConstruct() override;

    //================================================
    // Bound Widgets
    //================================================
    
    /** Main container for crosshair elements */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UCanvasPanel* CanvasPanel;

    /** Top crosshair line */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* TopCrosshair;

    /** Bottom crosshair line */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* BottomCrosshair;

    /** Left crosshair line */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* LeftCrosshair;

    /** Right crosshair line */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* RightCrosshair;
    
    //================================================
    // Configuration Properties
    //================================================
    
    /** Length of crosshair lines in pixels */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crosshair|Appearance")
    float CrosshairLength = 10.0f;

    /** Thickness of crosshair lines in pixels */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crosshair|Appearance")
    float CrosshairThickness = 2.0f;

    /** Multiplier applied to spread values for UI scaling */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crosshair|Spread")
    float SpreadMultiplier = 20.0f;

    /** Minimum spread radius in pixels */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crosshair|Spread")
    float MinimumSpread = 5.0f;

    /** Maximum spread radius in pixels */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crosshair|Spread")
    float MaximumSpread = 100.0f;

    /** Speed of spread increase when firing */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crosshair|Animation")
    float SpreadInterpSpeed = 10.0f;
    
    /** Speed of spread recovery when not firing */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crosshair|Animation")
    float RecoveryInterpSpeed = 15.0f;
    
    /** Default crosshair color */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crosshair|Appearance")
    FLinearColor CrosshairColor = FLinearColor::White;
    
    /** Show debug information */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crosshair|Debug")
    bool bShowDebugInfo = false;

    /** Hit marker display duration */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crosshair|HitMarker")
    float HitMarkerDuration = 0.2f;

    /** Hit marker color for normal hits */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crosshair|HitMarker")
    FLinearColor HitMarkerColor = FLinearColor::Red;

    /** Hit marker color for headshots */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crosshair|HitMarker")
    FLinearColor HeadshotMarkerColor = FLinearColor::Yellow;

    /** Hit marker color for kills */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Crosshair|HitMarker")
    FLinearColor KillMarkerColor = FLinearColor::Green;

private:
    //================================================
    // Internal Methods
    //================================================
    
    /** Update positions of crosshair elements */
    void UpdateCrosshairPositions();
    
    /** Subscribe to relevant events */
    void SubscribeToEvents();
    
    /** Unsubscribe from all events */
    void UnsubscribeFromEvents();
    
    /** Event handler for crosshair updates */
    void OnCrosshairUpdated(float Spread, float Recoil);
    
    /** Event handler for crosshair color changes */
    void OnCrosshairColorChanged(FLinearColor NewColor);
    
    /** Show hit marker with specified parameters */
    void DisplayHitMarker(bool bHeadshot, bool bKill);
    
    /** Hide hit marker */
    void HideHitMarker();

    //================================================
    // State Variables
    //================================================
    
    /** Current spread radius with interpolation */
    float CurrentSpreadRadius = 0.0f;
    
    /** Target spread radius */
    float TargetSpreadRadius = 0.0f;
    
    /** Base spread radius without firing effects */
    float BaseSpreadRadius = 0.0f;
    
    /** Last spread value for debugging */
    float LastSpreadValue = 0.0f;
    
    /** Last recoil value for debugging */
    float LastRecoilValue = 0.0f;
    
    /** Whether weapon is currently firing */
    bool bCurrentlyFiring = false;
    
    /** Whether weapon was firing in previous frame */
    bool bWasFiring = false;

    /** Whether crosshair is currently visible */
    bool bCrosshairVisible = true;

    /** Hit marker timer handle */
    FTimerHandle HitMarkerTimerHandle;

    //================================================
    // Event Subscription Handles
    //================================================
    
    /** Handle for crosshair update events */
    FDelegateHandle CrosshairUpdateHandle;
    
    /** Handle for crosshair color change events */
    FDelegateHandle CrosshairColorHandle;
};