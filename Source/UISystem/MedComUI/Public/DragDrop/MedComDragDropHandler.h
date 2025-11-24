// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Types/UI/ContainerUITypes.h"
#include "Types/Inventory/InventoryTypes.h"
#include "Widgets/Base/MedComBaseSlotWidget.h"
#include "Operations/InventoryResult.h"
#include "GameplayTagContainer.h"
#include "UObject/WeakInterfacePtr.h"
#include "MedComDragDropHandler.generated.h"

// Forward declarations
class UMedComDragDropOperation;
class UMedComDragVisualWidget;
class UMedComBaseSlotWidget;
class UMedComBaseContainerWidget;
class UMedComBaseLayoutWidget;
class UEventDelegateManager;
class IMedComInventoryUIBridgeWidget;
class IMedComEquipmentUIBridgeWidget;
class UWidget;

/**
 * Drop target information with validation
 */
USTRUCT(BlueprintType)
struct FDropTargetInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    UMedComBaseContainerWidget* Container = nullptr;
    
    UPROPERTY(BlueprintReadOnly)
    UMedComBaseSlotWidget* SlotWidget = nullptr;
    
    UPROPERTY(BlueprintReadOnly)
    int32 SlotIndex = INDEX_NONE;
    
    UPROPERTY(BlueprintReadOnly)
    bool bIsValid = false;
    
    UPROPERTY(BlueprintReadOnly)
    FText ValidationMessage;
    
    UPROPERTY(BlueprintReadOnly)
    FGameplayTag ContainerType;
};

/**
 * Smart drop configuration
 */
USTRUCT(BlueprintType)
struct FSmartDropConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableSmartDrop = true;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "50", ClampMax = "200"))
    float DetectionRadius = 100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
    float SnapStrength = 0.8f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "20"))
    float AnimationSpeed = 10.0f;
};

/**
 * Drop request data for processing
 */
USTRUCT(BlueprintType)
struct FDropRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGameplayTag SourceContainer;
    
    UPROPERTY(BlueprintReadOnly)
    FGameplayTag TargetContainer;
    
    UPROPERTY(BlueprintReadOnly)
    int32 TargetSlot = INDEX_NONE;
    
    UPROPERTY(BlueprintReadOnly)
    FDragDropUIData DragData;
    
    UPROPERTY(BlueprintReadOnly)
    FVector2D ScreenPosition = FVector2D::ZeroVector;
};

/**
 * Optimized drag & drop handler with performance improvements
 */
UCLASS()
class MEDCOMUI_API UMedComDragDropHandler : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // =====================================================
    // Subsystem Interface
    // =====================================================
    
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }
    
    UFUNCTION(BlueprintPure, Category = "DragDrop", meta = (WorldContext = "WorldContext"))
    static UMedComDragDropHandler* Get(const UObject* WorldContext);
    
    // =====================================================
    // Core Drag & Drop Operations
    // =====================================================
    
    UFUNCTION(BlueprintCallable, Category = "DragDrop")
    UMedComDragDropOperation* StartDragOperation(
        UMedComBaseSlotWidget* SourceSlot,
        const FPointerEvent& MouseEvent);
    
    UFUNCTION(BlueprintCallable, Category = "DragDrop")
    FInventoryOperationResult ProcessDrop(
        UMedComDragDropOperation* DragOperation,
        const FVector2D& ScreenPosition,
        UWidget* TargetWidget = nullptr);
    
    UFUNCTION(BlueprintCallable, Category = "DragDrop")
    FInventoryOperationResult ProcessDropRequest(const FDropRequest& Request);
    
    UFUNCTION(BlueprintCallable, Category = "DragDrop")
    FDropTargetInfo CalculateDropTarget(
        const FVector2D& ScreenPosition,
        const FIntPoint& ItemSize,
        bool bIsRotated) const;
    
    UFUNCTION(BlueprintCallable, Category = "DragDrop")
    bool ProcessContainerDrop(
        UMedComBaseContainerWidget* Container,
        UMedComDragDropOperation* DragOperation,
        UMedComBaseSlotWidget* SlotWidget,
        const FVector2D& ScreenPosition);
    
    // =====================================================
    // Visual Feedback
    // =====================================================
    
    UFUNCTION(BlueprintCallable, Category = "DragDrop|Visual")
    void UpdateDragVisual(
        UMedComDragDropOperation* DragOperation,
        bool bIsValidTarget);
    
    UFUNCTION(BlueprintCallable, Category = "DragDrop|Visual")
    void HighlightSlots(
        UMedComBaseContainerWidget* Container,
        const TArray<int32>& AffectedSlots,
        bool bIsValid);
    /**
     * Called during drag operation to update visuals
     * @param DragOperation Current drag operation
     * @param ScreenPosition Current cursor position
     */
    void OnDraggedUpdate(UMedComDragDropOperation* DragOperation, const FVector2D& ScreenPosition);
    
    UFUNCTION(BlueprintCallable, Category = "DragDrop|Visual")
    void ClearAllVisualFeedback();
    
    // =====================================================
    // Configuration
    // =====================================================
    
    UFUNCTION(BlueprintPure, Category = "DragDrop|Config")
    const FSmartDropConfig& GetSmartDropConfig() const { return SmartDropConfig; }
    
    UFUNCTION(BlueprintCallable, Category = "DragDrop|Config")
    void SetSmartDropConfig(const FSmartDropConfig& NewConfig) { SmartDropConfig = NewConfig; }
    
    // =====================================================
    // State Queries
    // =====================================================
    
    UFUNCTION(BlueprintPure, Category = "DragDrop|State")
    bool IsDragOperationActive() const { return ActiveOperation.IsValid(); }
    
    UFUNCTION(BlueprintPure, Category = "DragDrop|State")
    UMedComDragDropOperation* GetActiveOperation() const { return ActiveOperation.Get(); }
    /** Process highlight update immediately */
    void ProcessHighlightUpdate(UMedComBaseContainerWidget* Container, const FLinearColor& HighlightColor);
protected:
    // Internal operations
    FSlotValidationResult ValidateDropPlacement(
        UMedComBaseContainerWidget* Container,
        const FDragDropUIData& DragData,
        int32 TargetSlot) const;
    
    FInventoryOperationResult ExecuteDrop(const FDropRequest& Request);
    FInventoryOperationResult RouteDropOperation(const FDropRequest& Request);
    FInventoryOperationResult HandleInventoryToInventory(const FDropRequest& Request);
    FInventoryOperationResult HandleEquipmentToInventory(const FDropRequest& Request);
    FInventoryOperationResult HandleInventoryToEquipment(const FDropRequest& Request);
    
    FDropTargetInfo FindContainerAtPosition(const FVector2D& ScreenPosition) const;
    FDropTargetInfo FindContainerInLayout(UMedComBaseLayoutWidget* LayoutWidget, const FVector2D& ScreenPosition) const;
    FDropTargetInfo FindNearestContainer(const FVector2D& ScreenPosition, float SearchRadius) const;
    UMedComBaseSlotWidget* FindNearestSlot(UMedComBaseContainerWidget* Container, const FVector2D& ScreenPosition) const;
    
    void ForceUpdateAllContainers();
    bool CalculateOccupiedSlots(
        UMedComBaseContainerWidget* Container,
        int32 AnchorSlot,
        const FIntPoint& ItemSize,
        bool bIsRotated,
        TArray<int32>& OutSlots) const;
    
    TScriptInterface<IMedComInventoryUIBridgeWidget> GetBridgeForContainer(const FGameplayTag& ContainerType) const;
    
    // Cache management
    void CacheContainer(UMedComBaseContainerWidget* Container);
    void ClearInvalidCaches();
    void UpdateContainerCache();
    
    FVector2D CalculateDragOffsetForSlot(
        UMedComBaseSlotWidget* Slot,
        const FGeometry& Geometry,
        const FPointerEvent& MouseEvent) const;
    
private:
    // Configuration
    UPROPERTY()
    FSmartDropConfig SmartDropConfig;
    
    // State
    UPROPERTY()
    TWeakObjectPtr<UMedComDragDropOperation> ActiveOperation;
    
    TSet<int32> CurrentHighlightedSlots;
    TWeakObjectPtr<UMedComBaseContainerWidget> HighlightedContainer;
    
    // Optimized hover caching
    mutable TWeakObjectPtr<UMedComBaseContainerWidget> CachedHoveredContainer;
    mutable FVector2D CachedHoverPosition;
    mutable float CachedHoverTime;
    mutable int32 LastHighlightedSlotCount;
    mutable FLinearColor LastHighlightColor;
    
    // Performance thresholds
    static constexpr float HOVER_UPDATE_THRESHOLD = 30.0f; // pixels
    static constexpr float HOVER_CACHE_LIFETIME = 0.3f; // seconds
    static constexpr float HIGHLIGHT_UPDATE_DELAY = 0.05f; // seconds
    
    // Delayed update timer
    FTimerHandle HighlightUpdateTimer;
    TArray<int32> PendingHighlightSlots;
    bool bPendingHighlightValid;
    
    // Cached references
    TMap<FGameplayTag, TWeakObjectPtr<UMedComBaseContainerWidget>> ContainerCache;
    TWeakInterfacePtr<IMedComInventoryUIBridgeWidget> InventoryBridge;
    TWeakInterfacePtr<IMedComEquipmentUIBridgeWidget> EquipmentBridge;
    
    UPROPERTY()
    UEventDelegateManager* CachedEventManager;
    
    float LastCacheValidationTime;
    static constexpr float CACHE_LIFETIME = 5.0f;
    
    // Logging control
    mutable int32 DebugLogCounter;
    static constexpr int32 DEBUG_LOG_FREQUENCY = 120; // Log every N frames
};