// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/UI/ContainerUITypes.h"
#include "Engine/StreamableManager.h"
#include "Components/TextBlock.h"
#include "SuspenseDragVisualWidget.generated.h"

// Forward declarations
class UImage;
class UTextBlock;
class UBorder;
class USizeBox;
class UWidgetAnimation;
class UOverlay;
class UTexture2D;

/**
 * Visual preview modes for drag operations
 */
UENUM(BlueprintType)
enum class EDragVisualMode : uint8
{
    Normal      UMETA(DisplayName = "Normal"),
    ValidTarget UMETA(DisplayName = "Valid Target"),
    InvalidTarget UMETA(DisplayName = "Invalid Target"),
    Snapping    UMETA(DisplayName = "Snapping"),
    Stacking    UMETA(DisplayName = "Stacking"),
    Rotating    UMETA(DisplayName = "Rotating")
};

/**
 * Optimized drag visual widget with caching and performance improvements
 */
UCLASS()
class UISYSTEM_API USuspenseDragVisualWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    USuspenseDragVisualWidget(const FObjectInitializer& ObjectInitializer);

    //~ Begin UUserWidget Interface
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    //~ End UUserWidget Interface

    /**
     * Initialize drag visual with validated data
     * @param InDragData Validated drag data
     * @param CellSize Size of grid cell
     * @return True if initialization succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "DragVisual")
    bool InitializeDragVisual(const FDragDropUIData& InDragData, float CellSize);

    /**
     * Set the drag data to display
     * @param InDragData Data about dragged item
     */
    UFUNCTION(BlueprintCallable, Category = "DragVisual")
    void SetDragData(const FDragDropUIData& InDragData);

    /**
     * Update validity state visual
     * @param bIsValid Whether current drop target is valid
     */
    UFUNCTION(BlueprintCallable, Category = "DragVisual")
    void UpdateValidState(bool bIsValid);

    /**
     * Set the size of grid cells for proper scaling
     * @param CellSize Size of one grid cell in pixels
     */
    UFUNCTION(BlueprintCallable, Category = "DragVisual")
    void SetCellSize(float CellSize);

    // Advanced Visual Features
    UFUNCTION(BlueprintCallable, Category = "DragVisual|Advanced")
    void ShowPlacementPreview(const FVector2D& ScreenPosition, bool bIsValid);
    
    UFUNCTION(BlueprintCallable, Category = "DragVisual|Advanced")
    void AnimateSnapFeedback(const FVector2D& TargetPosition, float SnapStrength);
    
    UFUNCTION(BlueprintCallable, Category = "DragVisual|Advanced")
    void PreviewRotation(bool bShowRotated);
    
    UFUNCTION(BlueprintCallable, Category = "DragVisual|Advanced")
    void UpdateStackingFeedback(int32 StackCount, int32 MaxStack);
    
    UFUNCTION(BlueprintCallable, Category = "DragVisual|Advanced")
    void SetVisualMode(EDragVisualMode NewMode);
    
    UFUNCTION(BlueprintPure, Category = "DragVisual|Advanced")
    EDragVisualMode GetVisualMode() const { return CurrentVisualMode; }

    UFUNCTION(BlueprintCallable, Category = "DragVisual")
    bool IsInitialized() const { return bIsInitialized; }

    UFUNCTION(BlueprintCallable, Category = "DragVisual")
    FDragDropUIData GetDragData() const { return DragData; }

    // Performance optimization
    UFUNCTION(BlueprintCallable, Category = "DragVisual")
    void SetLowPerformanceMode(bool bEnable);
	/**
		 * Set quantity text visibility
		 * @param bVisible Whether to show quantity text
		 */
	UFUNCTION(BlueprintCallable, Category = "DragVisual")
	void SetQuantityTextVisible(bool bVisible)
	{
		if (QuantityText)
		{
			QuantityText->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
protected:
    //~ UI Components (must be bound in Blueprint)
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    USizeBox* RootSizeBox;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UBorder* BackgroundBorder;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UImage* ItemIcon;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* QuantityText;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UOverlay* EffectsOverlay;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UImage* PreviewGhost;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UImage* SnapIndicator;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* StackingText;

    // Animations
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetAnimationOptional))
    UWidgetAnimation* SnapAnimation;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetAnimationOptional))
    UWidgetAnimation* InvalidAnimation;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetAnimationOptional))
    UWidgetAnimation* StackingAnimation;
    
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetAnimationOptional))
    UWidgetAnimation* RotationAnimation;

    // Configuration
    UPROPERTY(BlueprintReadOnly, Category = "DragVisual")
    FDragDropUIData DragData;

    UPROPERTY(BlueprintReadOnly, Category = "DragVisual")
    float GridCellSize;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DragVisual|Colors")
    FLinearColor ValidDropColor;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DragVisual|Colors")
    FLinearColor InvalidDropColor;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DragVisual|Colors")
    FLinearColor SnapColor;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DragVisual|Colors", meta = (ClampMin = "0", ClampMax = "1"))
    float PreviewOpacity = 0.5f;

    // State
    UPROPERTY(BlueprintReadOnly, Category = "DragVisual|State")
    EDragVisualMode CurrentVisualMode;
    
    UPROPERTY(BlueprintReadOnly, Category = "DragVisual|State")
    bool bIsShowingRotationPreview;
    
    UPROPERTY(BlueprintReadOnly, Category = "DragVisual|State")
    FVector2D CurrentSnapTarget;
    
    UPROPERTY(BlueprintReadOnly, Category = "DragVisual|State")
    float CurrentSnapStrength;
    
    float SnapAnimationTime;
    float RotationAnimationTime;

    // Methods
    UFUNCTION(BlueprintCallable, Category = "DragVisual")
    bool UpdateVisuals();

    UFUNCTION(BlueprintCallable, Category = "DragVisual")
    bool ValidateWidgetBindings() const;

    UFUNCTION(BlueprintCallable, Category = "DragVisual")
    void ResetVisual();

    UFUNCTION(BlueprintCallable, Category = "DragVisual")
    void AutoBindWidgets();
    
    virtual void UpdateAnimations(float DeltaTime);
    virtual void ApplyVisualMode();
    void PlayModeAnimation(EDragVisualMode Mode);

    UFUNCTION(BlueprintImplementableEvent, Category = "DragVisual")
    void OnDragVisualCreated();

    UFUNCTION(BlueprintImplementableEvent, Category = "DragVisual")
    void OnDragVisualDestroyed();
    
    UFUNCTION(BlueprintImplementableEvent, Category = "DragVisual")
    void OnVisualModeChanged(EDragVisualMode NewMode);

private:
    // Performance optimizations
    void LoadIconAsync(const FString& IconPath);
    void OnIconLoaded();
    void UpdateVisualsInternal();
    void InvalidateVisual();
	// Add to private members
	bool bCurrentlyValid = true;
    
	// Helper method
	UBorder* GetHighlightBorder() const;
    // Cached data
    UPROPERTY(Transient)
    bool bIsInitialized;

    UPROPERTY(Transient)
    bool bWidgetsValidated;
    
    // Icon loading
    TSharedPtr<FStreamableHandle> IconStreamingHandle;
    TSoftObjectPtr<UTexture2D> PendingIconTexture;
    
    // Performance mode
    bool bLowPerformanceMode;
    float LastVisualUpdateTime;
    static constexpr float VISUAL_UPDATE_THROTTLE = 0.033f; // ~30 FPS for visuals

    
    // Invalidation tracking
    bool bNeedsVisualUpdate;
    
    // Static texture cache (shared between all instances)
    static TMap<FString, TWeakObjectPtr<UTexture2D>> IconTextureCache;
    static FCriticalSection IconCacheMutex;
};