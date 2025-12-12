// Copyright Suspense Team Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SuspenseCore/Types/UI/SuspenseCoreContainerUITypes.h"
#include "SuspenseCore/Types/Inventory/SuspenseCoreInventoryTypes.h"
#include "Widgets/Base/SuspenseBaseSlotWidget.h"
#include "SuspenseCore/Operations/SuspenseCoreInventoryResult.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "UObject/WeakInterfacePtr.h"
#include "SuspenseDragDropHandler.generated.h"

// Forward declarations
class USuspenseDragDropOperation;
class USuspenseDragVisualWidget;
class USuspenseBaseSlotWidget;
class USuspenseBaseContainerWidget;
class USuspenseBaseLayoutWidget;
class USuspenseEventManager;
class ISuspenseInventoryUIBridgeInterface;
class UWidget;

/**
 * Drop target information with validation
 */
USTRUCT(BlueprintType)
struct FDropTargetInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    USuspenseBaseContainerWidget* Container = nullptr;

    UPROPERTY(BlueprintReadOnly)
    USuspenseBaseSlotWidget* SlotWidget = nullptr;

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
class UISYSTEM_API USuspenseDragDropHandler : public UGameInstanceSubsystem
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
    static USuspenseDragDropHandler* Get(const UObject* WorldContext);

    // =====================================================
    // Core Drag & Drop Operations
    // =====================================================

    UFUNCTION(BlueprintCallable, Category = "DragDrop")
    USuspenseDragDropOperation* StartDragOperation(
        USuspenseBaseSlotWidget* SourceSlot,
        const FPointerEvent& MouseEvent);

    UFUNCTION(BlueprintCallable, Category = "DragDrop")
    FSuspenseInventoryOperationResult ProcessDrop(
        USuspenseDragDropOperation* DragOperation,
        const FVector2D& ScreenPosition,
        UWidget* TargetWidget = nullptr);

    UFUNCTION(BlueprintCallable, Category = "DragDrop")
    FSuspenseInventoryOperationResult ProcessDropRequest(const FDropRequest& Request);

    UFUNCTION(BlueprintCallable, Category = "DragDrop")
    FDropTargetInfo CalculateDropTarget(
        const FVector2D& ScreenPosition,
        const FIntPoint& ItemSize,
        bool bIsRotated) const;

    UFUNCTION(BlueprintCallable, Category = "DragDrop")
    bool ProcessContainerDrop(
        USuspenseBaseContainerWidget* Container,
        USuspenseDragDropOperation* DragOperation,
        USuspenseBaseSlotWidget* SlotWidget,
        const FVector2D& ScreenPosition);

    // =====================================================
    // Visual Feedback
    // =====================================================

    UFUNCTION(BlueprintCallable, Category = "DragDrop|Visual")
    void UpdateDragVisual(
        USuspenseDragDropOperation* DragOperation,
        bool bIsValidTarget);

    UFUNCTION(BlueprintCallable, Category = "DragDrop|Visual")
    void HighlightSlots(
        USuspenseBaseContainerWidget* Container,
        const TArray<int32>& AffectedSlots,
        bool bIsValid);
    /**
     * Called during drag operation to update visuals
     * @param DragOperation Current drag operation
     * @param ScreenPosition Current cursor position
     */
    void OnDraggedUpdate(USuspenseDragDropOperation* DragOperation, const FVector2D& ScreenPosition);

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
    USuspenseDragDropOperation* GetActiveOperation() const { return ActiveOperation.Get(); }
    /** Process highlight update immediately */
    void ProcessHighlightUpdate(USuspenseBaseContainerWidget* Container, const FLinearColor& HighlightColor);
protected:
    // Internal operations
    FSlotValidationResult ValidateDropPlacement(
        USuspenseBaseContainerWidget* Container,
        const FDragDropUIData& DragData,
        int32 TargetSlot) const;

    FSuspenseInventoryOperationResult ExecuteDrop(const FDropRequest& Request);
    FSuspenseInventoryOperationResult RouteDropOperation(const FDropRequest& Request);
    FSuspenseInventoryOperationResult HandleInventoryToInventory(const FDropRequest& Request);
    FSuspenseInventoryOperationResult HandleEquipmentToInventory(const FDropRequest& Request);
    FSuspenseInventoryOperationResult HandleInventoryToEquipment(const FDropRequest& Request);

    FDropTargetInfo FindContainerAtPosition(const FVector2D& ScreenPosition) const;
    FDropTargetInfo FindContainerInLayout(USuspenseBaseLayoutWidget* LayoutWidget, const FVector2D& ScreenPosition) const;
    FDropTargetInfo FindNearestContainer(const FVector2D& ScreenPosition, float SearchRadius) const;
    USuspenseBaseSlotWidget* FindNearestSlot(USuspenseBaseContainerWidget* Container, const FVector2D& ScreenPosition) const;

    void ForceUpdateAllContainers();
    bool CalculateOccupiedSlots(
        USuspenseBaseContainerWidget* Container,
        int32 AnchorSlot,
        const FIntPoint& ItemSize,
        bool bIsRotated,
        TArray<int32>& OutSlots) const;

    TScriptInterface<ISuspenseInventoryUIBridgeInterface> GetBridgeForContainer(const FGameplayTag& ContainerType) const;

    // Cache management
    void CacheContainer(USuspenseBaseContainerWidget* Container);
    void ClearInvalidCaches();
    void UpdateContainerCache();

    FVector2D CalculateDragOffsetForSlot(
        USuspenseBaseSlotWidget* Slot,
        const FGeometry& Geometry,
        const FPointerEvent& MouseEvent) const;

private:
    // Configuration
    UPROPERTY()
    FSmartDropConfig SmartDropConfig;

    // State
    UPROPERTY()
    TWeakObjectPtr<USuspenseDragDropOperation> ActiveOperation;

    TSet<int32> CurrentHighlightedSlots;
    TWeakObjectPtr<USuspenseBaseContainerWidget> HighlightedContainer;

    // Optimized hover caching
    mutable TWeakObjectPtr<USuspenseBaseContainerWidget> CachedHoveredContainer;
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
    TMap<FGameplayTag, TWeakObjectPtr<USuspenseBaseContainerWidget>> ContainerCache;
    TWeakInterfacePtr<ISuspenseInventoryUIBridgeInterface> InventoryBridge;
    //TWeakInterfacePtr<ISuspenseEquipmentUIBridgeInterface> EquipmentBridge;

    UPROPERTY()
    USuspenseCoreEventManager* CachedEventManager;

    float LastCacheValidationTime;
    static constexpr float CACHE_LIFETIME = 5.0f;

    // Logging control
    mutable int32 DebugLogCounter;
    static constexpr int32 DEBUG_LOG_FREQUENCY = 120; // Log every N frames
};
