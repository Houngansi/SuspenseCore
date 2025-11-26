// Copyright Suspense Team Team. All Rights Reserved.

#include "DragDrop/SuspenseDragDropHandler.h"
#include "Widgets/DragDrop/SuspenseDragDropOperation.h"
#include "Widgets/DragDrop/SuspenseDragVisualWidget.h"
#include "Widgets/Base/SuspenseBaseSlotWidget.h"
#include "Widgets/Base/SuspenseBaseContainerWidget.h"
#include "Delegates/EventDelegateManager.h"
#include "Interfaces/UI/ISuspenseInventoryUIBridgeWidget.h"
#include "Interfaces/UI/ISuspenseEquipmentUIBridgeWidget.h"
#include "Widgets/Layout/SuspenseBaseLayoutWidget.h"
#include "Engine/World.h"
#include "ObjectArray.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/Widget.h"

// =====================================================
// Subsystem Interface
// =====================================================

void USuspenseDragDropHandler::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    
    // Initialize configuration
    SmartDropConfig.bEnableSmartDrop = true;
    SmartDropConfig.DetectionRadius = 100.0f;
    SmartDropConfig.SnapStrength = 0.8f;
    SmartDropConfig.AnimationSpeed = 10.0f;
    
    // Get event manager
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        CachedEventManager = GameInstance->GetSubsystem<UEventDelegateManager>();
    }
    
    LastCacheValidationTime = 0.0f;
    CachedHoverTime = 0.0f;
    DebugLogCounter = 0;
    LastHighlightedSlotCount = 0;
    LastHighlightColor = FLinearColor::White;
}

void USuspenseDragDropHandler::Deinitialize()
{
    // Clear all state
    ClearAllVisualFeedback();
    ActiveOperation.Reset();
    ContainerCache.Empty();
    
    // Clear cache
    CachedHoveredContainer.Reset();
    
    // Clear timers
    if (UWorld* World = GetWorld())
    {
        if (HighlightUpdateTimer.IsValid())
        {
            World->GetTimerManager().ClearTimer(HighlightUpdateTimer);
        }
    }
    
    // Clear bridge references
    InventoryBridge.Reset();
    EquipmentBridge.Reset();
    
    // Clear other references
    CachedEventManager = nullptr;
    
    Super::Deinitialize();
}

USuspenseDragDropHandler* USuspenseDragDropHandler::Get(const UObject* WorldContext)
{
    if (!WorldContext)
    {
        return nullptr;
    }
    
    UWorld* World = WorldContext->GetWorld();
    if (!World)
    {
        return nullptr;
    }
    
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        return nullptr;
    }
    
    return GameInstance->GetSubsystem<USuspenseDragDropHandler>();
}

// =====================================================
// Core Drag & Drop Operations
// =====================================================

USuspenseDragDropOperation* USuspenseDragDropHandler::StartDragOperation(
    USuspenseBaseSlotWidget* SourceSlot,
    const FPointerEvent& MouseEvent)
{
    if (!SourceSlot)
    {
        return nullptr;
    }
    
    // Clear any previous operation
    if (ActiveOperation.IsValid())
    {
        ClearAllVisualFeedback();
    }
    
    // Get drag data from slot
    if (!SourceSlot->GetClass()->ImplementsInterface(USuspenseDraggable::StaticClass()))
    {
        return nullptr;
    }
    
    FDragDropUIData DragData = ISuspenseDraggableInterface::Execute_GetDragData(SourceSlot);
    if (!DragData.IsValidDragData())
    {
        return nullptr;
    }
    
    // Create drag operation
    USuspenseDragDropOperation* DragOp = NewObject<USuspenseDragDropOperation>();
    if (!DragOp)
    {
        return nullptr;
    }
    
    // Calculate drag offset
    FVector2D DragOffset = CalculateDragOffsetForSlot(
        SourceSlot,
        SourceSlot->GetCachedGeometry(), 
        MouseEvent
    );
    
    if (!DragOp->InitializeOperation(DragData, SourceSlot, DragOffset, this))
    {
        DragOp->ConditionalBeginDestroy();
        return nullptr;
    }
    
    // Create visual widget through container
    if (USuspenseBaseContainerWidget* OwningContainer = SourceSlot->GetOwningContainer())
    {
        if (USuspenseDragVisualWidget* DragVisual = OwningContainer->CreateDragVisualWidget(DragData))
        {
            DragOp->DefaultDragVisual = DragVisual;
        }
        else
        {
            // Fallback: create simple visual
            if (UUserWidget* DefaultVisual = CreateWidget<UUserWidget>(GetWorld(), UUserWidget::StaticClass()))
            {
                DragOp->DefaultDragVisual = DefaultVisual;
            }
        }
    }
    
    // Store active operation
    ActiveOperation = DragOp;
    
    // Send drag started event
    if (CachedEventManager)
    {
        CachedEventManager->OnUIDragStarted.Broadcast(SourceSlot, DragData);
    }
    
    return DragOp;
}

FInventoryOperationResult USuspenseDragDropHandler::ProcessDrop(
    USuspenseDragDropOperation* DragOperation,
    const FVector2D& ScreenPosition,
    UWidget* TargetWidget)
{
    // Validate operation
    if (!DragOperation || !DragOperation->IsValidOperation())
    {
        return FInventoryOperationResult::Failure(
            EInventoryErrorCode::InvalidItem,
            FText::FromString(TEXT("Invalid drag operation")),
            TEXT("ProcessDrop"),
            nullptr
        );
    }
    
    // Clear active operation
    if (ActiveOperation.Get() == DragOperation)
    {
        ActiveOperation.Reset();
    }
    
    // Get drag data
    const FDragDropUIData& DragData = DragOperation->GetDragData();
    
    // Find drop target
    FDropTargetInfo DropTarget = CalculateDropTarget(
        ScreenPosition,
        DragData.GetEffectiveSize(),
        DragData.ItemData.bIsRotated
    );
    
    if (!DropTarget.bIsValid)
    {
        ClearAllVisualFeedback();
        
        return FInventoryOperationResult::Failure(
            EInventoryErrorCode::InvalidSlot,
            FText::FromString(TEXT("No valid drop target")),
            TEXT("ProcessDrop"),
            nullptr
        );
    }
    
    // Create drop request
    FDropRequest Request;
    Request.SourceContainer = DragData.SourceContainerType;
    Request.TargetContainer = DropTarget.ContainerType;
    Request.TargetSlot = DropTarget.SlotIndex;
    Request.DragData = DragData;
    Request.ScreenPosition = ScreenPosition;
    
    // Process the drop
    FInventoryOperationResult Result = ProcessDropRequest(Request);
    
    // Clear visual feedback
    ClearAllVisualFeedback();
    
    // Send completion event
    if (CachedEventManager)
    {
        CachedEventManager->OnUIDragCompleted.Broadcast(
            nullptr,
            DropTarget.Container,
            Result.IsSuccess()
        );
    }
    
    return Result;
}

FInventoryOperationResult USuspenseDragDropHandler::ProcessDropRequest(const FDropRequest& Request)
{
    // Validate request
    if (!Request.DragData.IsValidDragData())
    {
        return FInventoryOperationResult::Failure(
            EInventoryErrorCode::InvalidItem,
            FText::FromString(TEXT("Invalid drag data")),
            TEXT("ProcessDropRequest"),
            nullptr
        );
    }
    
    if (Request.TargetSlot < 0)
    {
        return FInventoryOperationResult::Failure(
            EInventoryErrorCode::InvalidSlot,
            FText::FromString(TEXT("Invalid target slot")),
            TEXT("ProcessDropRequest"),
            nullptr
        );
    }
    
    // Route to appropriate handler
    return RouteDropOperation(Request);
}

FDropTargetInfo USuspenseDragDropHandler::CalculateDropTarget(
    const FVector2D& ScreenPosition,
    const FIntPoint& ItemSize,
    bool bIsRotated) const
{
    FDropTargetInfo Result;
    
    // Find container at position
    Result = FindContainerAtPosition(ScreenPosition);
    
    if (!Result.Container)
    {
        const float SearchRadius = 50.0f;
        Result = FindNearestContainer(ScreenPosition, SearchRadius);
        
        if (!Result.Container)
        {
            return Result; // Return invalid result
        }
    }
    
    // Get slot at position
    Result.SlotWidget = Result.Container->GetSlotAtScreenPosition(ScreenPosition);
    
    if (!Result.SlotWidget)
    {
        Result.SlotWidget = FindNearestSlot(Result.Container, ScreenPosition);
        
        if (!Result.SlotWidget)
        {
            Result.bIsValid = false;
            return Result;
        }
    }
    
    // Get slot index
    if (Result.SlotWidget->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
    {
        Result.SlotIndex = ISuspenseSlotUIInterface::Execute_GetSlotIndex(Result.SlotWidget);
        Result.ContainerType = ISuspenseContainerUIInterface::Execute_GetContainerType(Result.Container);
    }
    else
    {
        Result.bIsValid = false;
        return Result;
    }
    
    // Smart drop zone detection
    if (SmartDropConfig.bEnableSmartDrop && Result.SlotIndex >= 0)
    {
        FSmartDropZone SmartZone = Result.Container->FindBestDropZone(
            ScreenPosition,
            ItemSize,
            bIsRotated
        );
        
        if (SmartZone.bIsValid && SmartZone.SlotIndex != Result.SlotIndex)
        {
            Result.SlotIndex = SmartZone.SlotIndex;
            Result.SlotWidget = Result.Container->GetSlotWidget(SmartZone.SlotIndex);
        }
    }
    
    // Validate placement
    if (Result.SlotIndex >= 0 && Result.Container)
    {
        // Get effective size
        FIntPoint EffectiveSize = bIsRotated ? 
            FIntPoint(ItemSize.Y, ItemSize.X) : ItemSize;
        
        // Calculate occupied slots
        TArray<int32> OccupiedSlots;
        bool bFitsInBounds = Result.Container->CalculateOccupiedSlots(
            Result.SlotIndex,
            EffectiveSize,
            bIsRotated,
            OccupiedSlots
        );
        
        bool bCanPlace = bFitsInBounds;
        
        // Additional validation if needed
        if (bCanPlace && ActiveOperation.IsValid())
        {
            FSlotValidationResult ValidationResult = ISuspenseContainerUIInterface::Execute_CanAcceptDrop(
                Result.Container,
                ActiveOperation.Get(),
                Result.SlotIndex
            );
            
            bCanPlace = ValidationResult.bIsValid;
        }
        
        Result.bIsValid = bCanPlace;
        
        UE_LOG(LogTemp, VeryVerbose, TEXT("[DragDropHandler] Drop target: Slot=%d, Valid=%s"), 
            Result.SlotIndex, 
            Result.bIsValid ? TEXT("YES") : TEXT("NO"));
    }
    else
    {
        Result.bIsValid = false;
    }
    
    return Result;
}

void USuspenseDragDropHandler::OnDraggedUpdate(USuspenseDragDropOperation* DragOperation, const FVector2D& ScreenPosition)
{
    if (!DragOperation || !DragOperation->IsValidOperation())
    {
        return;
    }
    
    // Throttle updates for performance
    static FVector2D LastUpdatePosition = FVector2D::ZeroVector;
    static float LastUpdateTime = 0.0f;
    static bool LastValidState = false;
    
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    float DistanceMoved = FVector2D::Distance(ScreenPosition, LastUpdatePosition);
    
    // Skip update if position hasn't changed significantly
    if (DistanceMoved < 5.0f && (CurrentTime - LastUpdateTime) < 0.033f) // ~30 FPS
    {
        return;
    }
    
    LastUpdatePosition = ScreenPosition;
    LastUpdateTime = CurrentTime;
    
    // Calculate drop target
    const FDragDropUIData& DragData = DragOperation->GetDragData();
    FDropTargetInfo DropTarget = CalculateDropTarget(
        ScreenPosition,
        DragData.GetEffectiveSize(),
        DragData.ItemData.bIsRotated
    );
    
    // Only update visual if validity state changed
    if (DropTarget.bIsValid != LastValidState)
    {
        UpdateDragVisual(DragOperation, DropTarget.bIsValid);
        LastValidState = DropTarget.bIsValid;
        
        UE_LOG(LogTemp, Log, TEXT("[DragDropHandler] Drag validity changed to: %s at (%.1f, %.1f)"), 
            DropTarget.bIsValid ? TEXT("VALID") : TEXT("INVALID"),
            ScreenPosition.X, 
            ScreenPosition.Y);
    }
    
    // Update slot highlights
    if (DropTarget.Container && DropTarget.SlotIndex >= 0)
    {
        TArray<int32> OccupiedSlots;
        DropTarget.Container->CalculateOccupiedSlots(
            DropTarget.SlotIndex,
            DragData.GetEffectiveSize(),
            DragData.ItemData.bIsRotated,
            OccupiedSlots
        );
        
        if (OccupiedSlots.Num() > 0)
        {
            HighlightSlots(DropTarget.Container, OccupiedSlots, DropTarget.bIsValid);
        }
    }
    else
    {
        ClearAllVisualFeedback();
    }
}

bool USuspenseDragDropHandler::ProcessContainerDrop(
    USuspenseBaseContainerWidget* Container,
    USuspenseDragDropOperation* DragOperation,
    USuspenseBaseSlotWidget* SlotWidget,
    const FVector2D& ScreenPosition)
{
    if (!Container || !DragOperation || !SlotWidget)
    {
        return false;
    }
    
    // Get target slot index
    int32 TargetSlot = ISuspenseSlotUIInterface::Execute_GetSlotIndex(SlotWidget);
    
    // Create drop request
    FDropRequest Request;
    Request.DragData = DragOperation->GetDragData();
    Request.SourceContainer = Request.DragData.SourceContainerType;
    Request.TargetContainer = ISuspenseContainerUIInterface::Execute_GetContainerType(Container);
    Request.TargetSlot = TargetSlot;
    Request.ScreenPosition = ScreenPosition;
    
    // Process drop
    FInventoryOperationResult Result = ProcessDropRequest(Request);
    
    return Result.IsSuccess();
}

// =====================================================
// Visual Feedback (Optimized)
// =====================================================

void USuspenseDragDropHandler::UpdateDragVisual(
    USuspenseDragDropOperation* DragOperation,
    bool bIsValidTarget)
{
    if (!DragOperation || !DragOperation->DefaultDragVisual)
    {
        return;
    }
    
    if (USuspenseDragVisualWidget* DragVisual = Cast<USuspenseDragVisualWidget>(DragOperation->DefaultDragVisual))
    {
        DragVisual->UpdateValidState(bIsValidTarget);
    }
}

void USuspenseDragDropHandler::HighlightSlots(
    USuspenseBaseContainerWidget* Container,
    const TArray<int32>& AffectedSlots,
    bool bIsValid)
{
    if (!Container)
    {
        return;
    }
    
    // Optimization: check for changes
    FLinearColor NewColor = bIsValid ? 
        FLinearColor(0.0f, 1.0f, 0.0f, 0.5f) :  // Green for valid
        FLinearColor(1.0f, 0.0f, 0.0f, 0.5f);   // Red for invalid
    
    bool bNeedsUpdate = false;
    
    // Check if container changed
    if (HighlightedContainer.Get() != Container)
    {
        ClearAllVisualFeedback();
        HighlightedContainer = Container;
        bNeedsUpdate = true;
    }
    
    // Check if slots changed
    TSet<int32> NewHighlights(AffectedSlots);
    if (CurrentHighlightedSlots.Num() != NewHighlights.Num() || 
        !CurrentHighlightedSlots.Includes(NewHighlights) ||
        LastHighlightColor != NewColor)
    {
        bNeedsUpdate = true;
    }
    
    if (!bNeedsUpdate)
    {
        return; // Nothing changed
    }
    
    // Save for deferred update
    PendingHighlightSlots = AffectedSlots;
    bPendingHighlightValid = bIsValid;
    LastHighlightColor = NewColor;
    
    // Apply highlight immediately for better responsiveness
    ProcessHighlightUpdate(Container, NewColor);
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("[DragDropHandler] Highlighting %d slots with color %s"), 
        AffectedSlots.Num(), 
        bIsValid ? TEXT("GREEN") : TEXT("RED"));
}

void USuspenseDragDropHandler::ProcessHighlightUpdate(USuspenseBaseContainerWidget* Container, const FLinearColor& HighlightColor)
{
    if (!Container)
    {
        return;
    }
    
    // Remove old highlights efficiently
    TSet<int32> NewHighlightSet(PendingHighlightSlots);
    TSet<int32> ToRemove = CurrentHighlightedSlots.Difference(NewHighlightSet);
    
    // Clear slots that are no longer highlighted
    for (int32 SlotIdx : ToRemove)
    {
        if (USuspenseBaseSlotWidget* Slot = Container->GetSlotWidget(SlotIdx))
        {
            if (Slot->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
            {
                ISuspenseSlotUIInterface::Execute_SetHighlighted(Slot, false, FLinearColor::White);
            }
        }
    }
    
    // Apply new highlights to ALL pending slots
    for (int32 SlotIdx : PendingHighlightSlots)
    {
        if (USuspenseBaseSlotWidget* Slot = Container->GetSlotWidget(SlotIdx))
        {
            if (Slot->GetClass()->ImplementsInterface(USuspenseSlotUI::StaticClass()))
            {
                ISuspenseSlotUIInterface::Execute_SetHighlighted(Slot, true, HighlightColor);
                
                UE_LOG(LogTemp, VeryVerbose, TEXT("[DragDropHandler] Highlighted slot %d with color (%.2f, %.2f, %.2f, %.2f)"), 
                    SlotIdx, 
                    HighlightColor.R, 
                    HighlightColor.G, 
                    HighlightColor.B, 
                    HighlightColor.A);
            }
        }
    }
    
    // Update tracked state
    CurrentHighlightedSlots = NewHighlightSet;
    LastHighlightedSlotCount = CurrentHighlightedSlots.Num();
}

void USuspenseDragDropHandler::ClearAllVisualFeedback()
{
    // Cancel pending highlight updates
    if (UWorld* World = GetWorld())
    {
        if (HighlightUpdateTimer.IsValid())
        {
            World->GetTimerManager().ClearTimer(HighlightUpdateTimer);
        }
    }
    
    // Clear slot highlights
    if (HighlightedContainer.IsValid())
    {
        for (int32 SlotIdx : CurrentHighlightedSlots)
        {
            if (USuspenseBaseSlotWidget* Slot = HighlightedContainer->GetSlotWidget(SlotIdx))
            {
                ISuspenseSlotUIInterface::Execute_SetHighlighted(Slot, false, FLinearColor::White);
            }
        }
    }
    
    CurrentHighlightedSlots.Empty();
    HighlightedContainer.Reset();
    PendingHighlightSlots.Empty();
    LastHighlightedSlotCount = 0;
}

// =====================================================
// Optimized Container Finding
// =====================================================

FDropTargetInfo USuspenseDragDropHandler::FindContainerAtPosition(const FVector2D& ScreenPosition) const
{
    FDropTargetInfo Result;
    
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    
    // Check hover cache first
    if (CachedHoveredContainer.IsValid())
    {
        float DistanceFromCache = FVector2D::Distance(ScreenPosition, CachedHoverPosition);
        float TimeSinceCache = CurrentTime - CachedHoverTime;
        
        // Use cache if position hasn't changed much
        if (DistanceFromCache < HOVER_UPDATE_THRESHOLD && TimeSinceCache < HOVER_CACHE_LIFETIME)
        {
            USuspenseBaseContainerWidget* Container = CachedHoveredContainer.Get();
            if (Container && Container->IsVisible() && 
                Container->GetCachedGeometry().IsUnderLocation(ScreenPosition))
            {
                Result.Container = Container;
                Result.ContainerType = ISuspenseContainerUIInterface::Execute_GetContainerType(Container);
                Result.bIsValid = true;
                return Result;
            }
        }
    }
    
    // Check known containers from cache first (much faster)
    for (const auto& CachePair : ContainerCache)
    {
        if (CachePair.Value.IsValid())
        {
            USuspenseBaseContainerWidget* Container = CachePair.Value.Get();
            if (Container && Container->IsVisible() && 
                Container->GetCachedGeometry().IsUnderLocation(ScreenPosition))
            {
                Result.Container = Container;
                Result.ContainerType = CachePair.Key;
                Result.bIsValid = true;
                
                // Update hover cache
                CachedHoveredContainer = Container;
                CachedHoverPosition = ScreenPosition;
                CachedHoverTime = CurrentTime;
                
                return Result;
            }
        }
    }
    
    // Only do expensive search if cache is stale
    if (CurrentTime - LastCacheValidationTime > CACHE_LIFETIME)
    {
        const_cast<USuspenseDragDropHandler*>(this)->UpdateContainerCache();
    }
    
    return Result;
}

FDropTargetInfo USuspenseDragDropHandler::FindContainerInLayout(
    USuspenseBaseLayoutWidget* LayoutWidget, 
    const FVector2D& ScreenPosition) const
{
    FDropTargetInfo Result;
    
    if (!LayoutWidget)
    {
        return Result;
    }
    
    // Get all widgets in layout
    TArray<UUserWidget*> LayoutChildren = LayoutWidget->GetLayoutWidgets_Implementation();
    
    // Check each child widget
    for (UUserWidget* ChildWidget : LayoutChildren)
    {
        if (!ChildWidget || !ChildWidget->IsVisible())
        {
            continue;
        }
        
        // Check if it's a container
        if (USuspenseBaseContainerWidget* Container = Cast<USuspenseBaseContainerWidget>(ChildWidget))
        {
            const FGeometry& ContainerGeometry = Container->GetCachedGeometry();
            
            if (ContainerGeometry.IsUnderLocation(ScreenPosition))
            {
                Result.Container = Container;
                Result.ContainerType = ISuspenseContainerUIInterface::Execute_GetContainerType(Container);
                Result.bIsValid = true;
                
                // Cache this container
                const_cast<USuspenseDragDropHandler*>(this)->CacheContainer(Container);
                
                return Result;
            }
        }
    }
    
    // Also check widgets by tags
    TArray<FGameplayTag> AllTags = LayoutWidget->GetAllWidgetTags();
    for (const FGameplayTag& Tag : AllTags)
    {
        if (UUserWidget* TaggedWidget = LayoutWidget->GetWidgetByTag(Tag))
        {
            if (USuspenseBaseContainerWidget* Container = Cast<USuspenseBaseContainerWidget>(TaggedWidget))
            {
                if (Container->IsVisible() && Container->GetCachedGeometry().IsUnderLocation(ScreenPosition))
                {
                    Result.Container = Container;
                    Result.ContainerType = ISuspenseContainerUIInterface::Execute_GetContainerType(Container);
                    Result.bIsValid = true;
                    
                    const_cast<USuspenseDragDropHandler*>(this)->CacheContainer(Container);
                    
                    return Result;
                }
            }
        }
    }
    
    return Result;
}

FDropTargetInfo USuspenseDragDropHandler::FindNearestContainer(
    const FVector2D& ScreenPosition, 
    float SearchRadius) const
{
    FDropTargetInfo Result;
    float NearestDistance = SearchRadius;
    
    // Check cached containers only
    for (const auto& CachePair : ContainerCache)
    {
        if (!CachePair.Value.IsValid())
            continue;
            
        USuspenseBaseContainerWidget* Container = CachePair.Value.Get();
        if (!Container || !Container->IsVisible())
            continue;
        
        const FGeometry& Geom = Container->GetCachedGeometry();
        FVector2D ContainerCenter = Geom.GetAbsolutePosition() + Geom.GetLocalSize() * 0.5f;
        
        float Distance = FVector2D::Distance(ScreenPosition, ContainerCenter);
        if (Distance < NearestDistance)
        {
            NearestDistance = Distance;
            Result.Container = Container;
            Result.ContainerType = CachePair.Key;
            Result.bIsValid = true;
        }
    }
    
    return Result;
}

USuspenseBaseSlotWidget* USuspenseDragDropHandler::FindNearestSlot(
    USuspenseBaseContainerWidget* Container, 
    const FVector2D& ScreenPosition) const
{
    if (!Container)
        return nullptr;
    
    USuspenseBaseSlotWidget* NearestSlot = nullptr;
    float NearestDistance = MAX_FLT;
    
    TArray<USuspenseBaseSlotWidget*> AllSlots = Container->GetAllSlotWidgets();
    
    for (USuspenseBaseSlotWidget* Slot : AllSlots)
    {
        if (!Slot || !Slot->IsVisible())
            continue;
        
        const FGeometry& SlotGeom = Slot->GetCachedGeometry();
        FVector2D SlotCenter = SlotGeom.GetAbsolutePosition() + SlotGeom.GetLocalSize() * 0.5f;
        
        float Distance = FVector2D::Distance(ScreenPosition, SlotCenter);
        if (Distance < NearestDistance)
        {
            NearestDistance = Distance;
            NearestSlot = Slot;
        }
    }
    
    return NearestSlot;
}

void USuspenseDragDropHandler::ForceUpdateAllContainers()
{
    // This should be called rarely, only when containers are added/removed
    ContainerCache.Empty();
    
    if (!GetWorld())
        return;
    
    // Find all containers directly
    TArray<UUserWidget*> AllWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
        GetWorld(),
        AllWidgets,
        USuspenseBaseContainerWidget::StaticClass(),
        false
    );
    
    for (UUserWidget* Widget : AllWidgets)
    {
        if (USuspenseBaseContainerWidget* Container = Cast<USuspenseBaseContainerWidget>(Widget))
        {
            if (Container->IsVisible())
            {
                FGameplayTag ContainerType = ISuspenseContainerUIInterface::Execute_GetContainerType(Container);
                ContainerCache.Add(ContainerType, Container);
            }
        }
    }
    
    // Also search in layouts
    TArray<UUserWidget*> LayoutWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
        GetWorld(),
        LayoutWidgets,
        USuspenseBaseLayoutWidget::StaticClass(),
        false
    );
    
    for (UUserWidget* Widget : LayoutWidgets)
    {
        if (USuspenseBaseLayoutWidget* Layout = Cast<USuspenseBaseLayoutWidget>(Widget))
        {
            if (!Layout->IsVisible())
                continue;
            
            TArray<UUserWidget*> LayoutChildren = Layout->GetLayoutWidgets_Implementation();
            
            for (UUserWidget* Child : LayoutChildren)
            {
                if (USuspenseBaseContainerWidget* Container = Cast<USuspenseBaseContainerWidget>(Child))
                {
                    if (Container->IsVisible())
                    {
                        FGameplayTag ContainerType = ISuspenseContainerUIInterface::Execute_GetContainerType(Container);
                        ContainerCache.Add(ContainerType, Container);
                    }
                }
            }
        }
    }
    
    LastCacheValidationTime = GetWorld()->GetTimeSeconds();
}

// =====================================================
// Internal Operations
// =====================================================

FSlotValidationResult USuspenseDragDropHandler::ValidateDropPlacement(
    USuspenseBaseContainerWidget* Container,
    const FDragDropUIData& DragData,
    int32 TargetSlot) const
{
    if (!Container)
    {
        return FSlotValidationResult::Failure(
            FText::FromString(TEXT("Invalid container"))
        );
    }
    
    // Calculate occupied slots
    TArray<int32> OccupiedSlots;
    const bool bFits = CalculateOccupiedSlots(
        Container,
        TargetSlot,
        DragData.GetEffectiveSize(),
        DragData.ItemData.bIsRotated,
        OccupiedSlots
    );
    
    if (!bFits)
    {
        return FSlotValidationResult::Failure(
            FText::FromString(TEXT("Item doesn't fit at this position"))
        );
    }
    
    return FSlotValidationResult::Success();
}

FInventoryOperationResult USuspenseDragDropHandler::ExecuteDrop(const FDropRequest& Request)
{
    // Send drop event
    if (CachedEventManager)
    {
        // Find target container widget
        USuspenseBaseContainerWidget* TargetContainer = nullptr;
        
        if (auto* CachedContainer = ContainerCache.Find(Request.TargetContainer))
        {
            TargetContainer = CachedContainer->Get();
        }
        
        if (TargetContainer)
        {
            CachedEventManager->OnUIItemDropped.Broadcast(
                TargetContainer,
                Request.DragData,
                Request.TargetSlot
            );
        }
    }
    
    return FInventoryOperationResult::Success(TEXT("ExecuteDrop"));
}

FInventoryOperationResult USuspenseDragDropHandler::RouteDropOperation(const FDropRequest& Request)
{
    // Determine operation type
    bool bSourceIsInventory = Request.SourceContainer.MatchesTag(
        FGameplayTag::RequestGameplayTag(TEXT("Container.Inventory"))
    );
    bool bTargetIsInventory = Request.TargetContainer.MatchesTag(
        FGameplayTag::RequestGameplayTag(TEXT("Container.Inventory"))
    );
    
    bool bSourceIsEquipment = Request.SourceContainer.MatchesTag(
        FGameplayTag::RequestGameplayTag(TEXT("Container.Equipment"))
    );
    bool bTargetIsEquipment = Request.TargetContainer.MatchesTag(
        FGameplayTag::RequestGameplayTag(TEXT("Container.Equipment"))
    );
    
    // Route to appropriate handler
    if (bSourceIsInventory && bTargetIsInventory)
    {
        return HandleInventoryToInventory(Request);
    }
    else if (bSourceIsEquipment && bTargetIsInventory)
    {
        return HandleEquipmentToInventory(Request);
    }
    else if (bSourceIsInventory && bTargetIsEquipment)
    {
        return HandleInventoryToEquipment(Request);
    }
    else
    {
        return FInventoryOperationResult::Failure(
            EInventoryErrorCode::UnknownError,
            FText::FromString(TEXT("Unsupported drop operation")),
            TEXT("RouteDropOperation"),
            nullptr
        );
    }
}

FInventoryOperationResult USuspenseDragDropHandler::HandleInventoryToInventory(const FDropRequest& Request)
{
    // Get inventory bridge
    TScriptInterface<ISuspenseInventoryUIBridgeWidget> Bridge = GetBridgeForContainer(Request.TargetContainer);
    if (!Bridge.GetInterface())
    {
        return FInventoryOperationResult::Failure(
            EInventoryErrorCode::NotInitialized,
            FText::FromString(TEXT("Inventory bridge not available")),
            TEXT("HandleInventoryToInventory"),
            nullptr
        );
    }
    
    // Execute through bridge
    return ExecuteDrop(Request);
}

FInventoryOperationResult USuspenseDragDropHandler::HandleEquipmentToInventory(const FDropRequest& Request)
{
    // Create unequip request (matching EquipmentTypes.h)
    FEquipmentOperationRequest UnequipRequest;
    UnequipRequest.OperationType   = EEquipmentOperationType::Unequip;
    UnequipRequest.SourceSlotIndex = Request.DragData.SourceSlotIndex;
    UnequipRequest.TargetSlotIndex = Request.TargetSlot;
    UnequipRequest.Timestamp       = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Preserve item-identifying details via Parameters
    UnequipRequest.Parameters.Add(TEXT("ItemID"),         Request.DragData.ItemData.ItemID.ToString());
    UnequipRequest.Parameters.Add(TEXT("ItemInstanceID"), Request.DragData.ItemData.ItemInstanceID.ToString());
    UnequipRequest.Parameters.Add(TEXT("Quantity"),       FString::FromInt(Request.DragData.ItemData.Quantity));

    // Optional: pass source container context (it's a GameplayTag in this codebase)
    if (Request.SourceContainer.IsValid())
    {
        UnequipRequest.Parameters.Add(TEXT("SourceContainer"), Request.SourceContainer.ToString());
    }

    // Dispatch through event system
    if (CachedEventManager)
    {
        CachedEventManager->BroadcastEquipmentOperationRequest(UnequipRequest);
        return FInventoryOperationResult::Success(TEXT("HandleEquipmentToInventory"));
    }

    return FInventoryOperationResult::Failure(
        EInventoryErrorCode::UnknownError,
        FText::FromString(TEXT("Event manager not available")),
        TEXT("HandleEquipmentToInventory"),
        nullptr
    );
}

FInventoryOperationResult USuspenseDragDropHandler::HandleInventoryToEquipment(const FDropRequest& Request)
{
    // Execute through bridge/event system
    return ExecuteDrop(Request);
}

bool USuspenseDragDropHandler::CalculateOccupiedSlots(
    USuspenseBaseContainerWidget* Container,
    int32 AnchorSlot,
    const FIntPoint& ItemSize,
    bool bIsRotated,
    TArray<int32>& OutSlots) const
{
    if (!Container)
    {
        return false;
    }
    
    return Container->CalculateOccupiedSlots(AnchorSlot, ItemSize, bIsRotated, OutSlots);
}

TScriptInterface<ISuspenseInventoryUIBridgeWidget> USuspenseDragDropHandler::GetBridgeForContainer(
    const FGameplayTag& ContainerType) const
{
    // Check if it's inventory container
    if (ContainerType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Container.Inventory"))))
    {
        if (InventoryBridge.IsValid())
        {
            return ISuspenseInventoryUIBridgeWidget::MakeScriptInterface(InventoryBridge.Get());
        }
        
        return ISuspenseInventoryUIBridgeWidget::GetGlobalBridge(GetWorld());
    }
    
    // Return empty TScriptInterface for unsupported container types
    return TScriptInterface<ISuspenseInventoryUIBridgeWidget>();
}

// =====================================================
// Cache Management
// =====================================================

void USuspenseDragDropHandler::CacheContainer(USuspenseBaseContainerWidget* Container)
{
    if (!Container)
    {
        return;
    }
    
    FGameplayTag ContainerType = ISuspenseContainerUIInterface::Execute_GetContainerType(Container);
    ContainerCache.Add(ContainerType, Container);
}

void USuspenseDragDropHandler::ClearInvalidCaches()
{
    // Remove invalid container references
    for (auto It = ContainerCache.CreateIterator(); It; ++It)
    {
        if (!It.Value().IsValid())
        {
            It.RemoveCurrent();
        }
    }
    
    // Clear hover cache if invalid
    if (!CachedHoveredContainer.IsValid())
    {
        CachedHoveredContainer.Reset();
        CachedHoverTime = 0.0f;
    }
}

void USuspenseDragDropHandler::UpdateContainerCache()
{
    ClearInvalidCaches();
    
    // Only do full update if really needed
    if (ContainerCache.Num() == 0)
    {
        ForceUpdateAllContainers();
    }
    
    // Update bridges
    if (!InventoryBridge.IsValid())
    {
        ISuspenseInventoryUIBridgeWidget* GlobalInventoryBridge = 
            ISuspenseInventoryUIBridgeWidget::GetInventoryUIBridge(GetWorld());
        if (GlobalInventoryBridge)
        {
            InventoryBridge = TWeakInterfacePtr<ISuspenseInventoryUIBridgeWidget>(GlobalInventoryBridge);
        }
    }
    
    if (!EquipmentBridge.IsValid())
    {
        ISuspenseEquipmentUIBridgeWidget* GlobalEquipmentBridge = 
            ISuspenseEquipmentUIBridgeWidget::GetEquipmentUIBridge(GetWorld());
        if (GlobalEquipmentBridge)
        {
            EquipmentBridge = TWeakInterfacePtr<ISuspenseEquipmentUIBridgeWidget>(GlobalEquipmentBridge);
        }
    }
    
    LastCacheValidationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

FVector2D USuspenseDragDropHandler::CalculateDragOffsetForSlot(
    USuspenseBaseSlotWidget* Slot,
    const FGeometry& Geometry,
    const FPointerEvent& MouseEvent) const
{
    if (!Slot)
    {
        return FVector2D(0.5f, 0.5f);
    }
    
    FVector2D LocalMousePos = Geometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    FVector2D LocalSize = Geometry.GetLocalSize();
    
    FVector2D NormalizedOffset;
    NormalizedOffset.X = LocalSize.X > 0 ? FMath::Clamp(LocalMousePos.X / LocalSize.X, 0.0f, 1.0f) : 0.5f;
    NormalizedOffset.Y = LocalSize.Y > 0 ? FMath::Clamp(LocalMousePos.Y / LocalSize.Y, 0.0f, 1.0f) : 0.5f;
    
    return NormalizedOffset;
}