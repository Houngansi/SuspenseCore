// Copyright MedCom Team. All Rights Reserved.

#include "DragDrop/MedComDragDropHandler.h"
#include "Widgets/DragDrop/MedComDragDropOperation.h"
#include "Widgets/DragDrop/MedComDragVisualWidget.h"
#include "Widgets/Base/MedComBaseSlotWidget.h"
#include "Widgets/Base/MedComBaseContainerWidget.h"
#include "Delegates/EventDelegateManager.h"
#include "Interfaces/UI/IMedComInventoryUIBridgeWidget.h"
#include "Interfaces/UI/IMedComEquipmentUIBridgeWidget.h"
#include "Widgets/Layout/MedComBaseLayoutWidget.h"
#include "Engine/World.h"
#include "ObjectArray.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/Widget.h"

// =====================================================
// Subsystem Interface
// =====================================================

void UMedComDragDropHandler::Initialize(FSubsystemCollectionBase& Collection)
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

void UMedComDragDropHandler::Deinitialize()
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

UMedComDragDropHandler* UMedComDragDropHandler::Get(const UObject* WorldContext)
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
    
    return GameInstance->GetSubsystem<UMedComDragDropHandler>();
}

// =====================================================
// Core Drag & Drop Operations
// =====================================================

UMedComDragDropOperation* UMedComDragDropHandler::StartDragOperation(
    UMedComBaseSlotWidget* SourceSlot,
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
    if (!SourceSlot->GetClass()->ImplementsInterface(UMedComDraggableInterface::StaticClass()))
    {
        return nullptr;
    }
    
    FDragDropUIData DragData = IMedComDraggableInterface::Execute_GetDragData(SourceSlot);
    if (!DragData.IsValidDragData())
    {
        return nullptr;
    }
    
    // Create drag operation
    UMedComDragDropOperation* DragOp = NewObject<UMedComDragDropOperation>();
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
    if (UMedComBaseContainerWidget* OwningContainer = SourceSlot->GetOwningContainer())
    {
        if (UMedComDragVisualWidget* DragVisual = OwningContainer->CreateDragVisualWidget(DragData))
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

FInventoryOperationResult UMedComDragDropHandler::ProcessDrop(
    UMedComDragDropOperation* DragOperation,
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

FInventoryOperationResult UMedComDragDropHandler::ProcessDropRequest(const FDropRequest& Request)
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

FDropTargetInfo UMedComDragDropHandler::CalculateDropTarget(
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
    if (Result.SlotWidget->GetClass()->ImplementsInterface(UMedComSlotUIInterface::StaticClass()))
    {
        Result.SlotIndex = IMedComSlotUIInterface::Execute_GetSlotIndex(Result.SlotWidget);
        Result.ContainerType = IMedComContainerUIInterface::Execute_GetContainerType(Result.Container);
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
            FSlotValidationResult ValidationResult = IMedComContainerUIInterface::Execute_CanAcceptDrop(
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

void UMedComDragDropHandler::OnDraggedUpdate(UMedComDragDropOperation* DragOperation, const FVector2D& ScreenPosition)
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

bool UMedComDragDropHandler::ProcessContainerDrop(
    UMedComBaseContainerWidget* Container,
    UMedComDragDropOperation* DragOperation,
    UMedComBaseSlotWidget* SlotWidget,
    const FVector2D& ScreenPosition)
{
    if (!Container || !DragOperation || !SlotWidget)
    {
        return false;
    }
    
    // Get target slot index
    int32 TargetSlot = IMedComSlotUIInterface::Execute_GetSlotIndex(SlotWidget);
    
    // Create drop request
    FDropRequest Request;
    Request.DragData = DragOperation->GetDragData();
    Request.SourceContainer = Request.DragData.SourceContainerType;
    Request.TargetContainer = IMedComContainerUIInterface::Execute_GetContainerType(Container);
    Request.TargetSlot = TargetSlot;
    Request.ScreenPosition = ScreenPosition;
    
    // Process drop
    FInventoryOperationResult Result = ProcessDropRequest(Request);
    
    return Result.IsSuccess();
}

// =====================================================
// Visual Feedback (Optimized)
// =====================================================

void UMedComDragDropHandler::UpdateDragVisual(
    UMedComDragDropOperation* DragOperation,
    bool bIsValidTarget)
{
    if (!DragOperation || !DragOperation->DefaultDragVisual)
    {
        return;
    }
    
    if (UMedComDragVisualWidget* DragVisual = Cast<UMedComDragVisualWidget>(DragOperation->DefaultDragVisual))
    {
        DragVisual->UpdateValidState(bIsValidTarget);
    }
}

void UMedComDragDropHandler::HighlightSlots(
    UMedComBaseContainerWidget* Container,
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

void UMedComDragDropHandler::ProcessHighlightUpdate(UMedComBaseContainerWidget* Container, const FLinearColor& HighlightColor)
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
        if (UMedComBaseSlotWidget* Slot = Container->GetSlotWidget(SlotIdx))
        {
            if (Slot->GetClass()->ImplementsInterface(UMedComSlotUIInterface::StaticClass()))
            {
                IMedComSlotUIInterface::Execute_SetHighlighted(Slot, false, FLinearColor::White);
            }
        }
    }
    
    // Apply new highlights to ALL pending slots
    for (int32 SlotIdx : PendingHighlightSlots)
    {
        if (UMedComBaseSlotWidget* Slot = Container->GetSlotWidget(SlotIdx))
        {
            if (Slot->GetClass()->ImplementsInterface(UMedComSlotUIInterface::StaticClass()))
            {
                IMedComSlotUIInterface::Execute_SetHighlighted(Slot, true, HighlightColor);
                
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

void UMedComDragDropHandler::ClearAllVisualFeedback()
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
            if (UMedComBaseSlotWidget* Slot = HighlightedContainer->GetSlotWidget(SlotIdx))
            {
                IMedComSlotUIInterface::Execute_SetHighlighted(Slot, false, FLinearColor::White);
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

FDropTargetInfo UMedComDragDropHandler::FindContainerAtPosition(const FVector2D& ScreenPosition) const
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
            UMedComBaseContainerWidget* Container = CachedHoveredContainer.Get();
            if (Container && Container->IsVisible() && 
                Container->GetCachedGeometry().IsUnderLocation(ScreenPosition))
            {
                Result.Container = Container;
                Result.ContainerType = IMedComContainerUIInterface::Execute_GetContainerType(Container);
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
            UMedComBaseContainerWidget* Container = CachePair.Value.Get();
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
        const_cast<UMedComDragDropHandler*>(this)->UpdateContainerCache();
    }
    
    return Result;
}

FDropTargetInfo UMedComDragDropHandler::FindContainerInLayout(
    UMedComBaseLayoutWidget* LayoutWidget, 
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
        if (UMedComBaseContainerWidget* Container = Cast<UMedComBaseContainerWidget>(ChildWidget))
        {
            const FGeometry& ContainerGeometry = Container->GetCachedGeometry();
            
            if (ContainerGeometry.IsUnderLocation(ScreenPosition))
            {
                Result.Container = Container;
                Result.ContainerType = IMedComContainerUIInterface::Execute_GetContainerType(Container);
                Result.bIsValid = true;
                
                // Cache this container
                const_cast<UMedComDragDropHandler*>(this)->CacheContainer(Container);
                
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
            if (UMedComBaseContainerWidget* Container = Cast<UMedComBaseContainerWidget>(TaggedWidget))
            {
                if (Container->IsVisible() && Container->GetCachedGeometry().IsUnderLocation(ScreenPosition))
                {
                    Result.Container = Container;
                    Result.ContainerType = IMedComContainerUIInterface::Execute_GetContainerType(Container);
                    Result.bIsValid = true;
                    
                    const_cast<UMedComDragDropHandler*>(this)->CacheContainer(Container);
                    
                    return Result;
                }
            }
        }
    }
    
    return Result;
}

FDropTargetInfo UMedComDragDropHandler::FindNearestContainer(
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
            
        UMedComBaseContainerWidget* Container = CachePair.Value.Get();
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

UMedComBaseSlotWidget* UMedComDragDropHandler::FindNearestSlot(
    UMedComBaseContainerWidget* Container, 
    const FVector2D& ScreenPosition) const
{
    if (!Container)
        return nullptr;
    
    UMedComBaseSlotWidget* NearestSlot = nullptr;
    float NearestDistance = MAX_FLT;
    
    TArray<UMedComBaseSlotWidget*> AllSlots = Container->GetAllSlotWidgets();
    
    for (UMedComBaseSlotWidget* Slot : AllSlots)
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

void UMedComDragDropHandler::ForceUpdateAllContainers()
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
        UMedComBaseContainerWidget::StaticClass(),
        false
    );
    
    for (UUserWidget* Widget : AllWidgets)
    {
        if (UMedComBaseContainerWidget* Container = Cast<UMedComBaseContainerWidget>(Widget))
        {
            if (Container->IsVisible())
            {
                FGameplayTag ContainerType = IMedComContainerUIInterface::Execute_GetContainerType(Container);
                ContainerCache.Add(ContainerType, Container);
            }
        }
    }
    
    // Also search in layouts
    TArray<UUserWidget*> LayoutWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
        GetWorld(),
        LayoutWidgets,
        UMedComBaseLayoutWidget::StaticClass(),
        false
    );
    
    for (UUserWidget* Widget : LayoutWidgets)
    {
        if (UMedComBaseLayoutWidget* Layout = Cast<UMedComBaseLayoutWidget>(Widget))
        {
            if (!Layout->IsVisible())
                continue;
            
            TArray<UUserWidget*> LayoutChildren = Layout->GetLayoutWidgets_Implementation();
            
            for (UUserWidget* Child : LayoutChildren)
            {
                if (UMedComBaseContainerWidget* Container = Cast<UMedComBaseContainerWidget>(Child))
                {
                    if (Container->IsVisible())
                    {
                        FGameplayTag ContainerType = IMedComContainerUIInterface::Execute_GetContainerType(Container);
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

FSlotValidationResult UMedComDragDropHandler::ValidateDropPlacement(
    UMedComBaseContainerWidget* Container,
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

FInventoryOperationResult UMedComDragDropHandler::ExecuteDrop(const FDropRequest& Request)
{
    // Send drop event
    if (CachedEventManager)
    {
        // Find target container widget
        UMedComBaseContainerWidget* TargetContainer = nullptr;
        
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

FInventoryOperationResult UMedComDragDropHandler::RouteDropOperation(const FDropRequest& Request)
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

FInventoryOperationResult UMedComDragDropHandler::HandleInventoryToInventory(const FDropRequest& Request)
{
    // Get inventory bridge
    TScriptInterface<IMedComInventoryUIBridgeWidget> Bridge = GetBridgeForContainer(Request.TargetContainer);
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

FInventoryOperationResult UMedComDragDropHandler::HandleEquipmentToInventory(const FDropRequest& Request)
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

FInventoryOperationResult UMedComDragDropHandler::HandleInventoryToEquipment(const FDropRequest& Request)
{
    // Execute through bridge/event system
    return ExecuteDrop(Request);
}

bool UMedComDragDropHandler::CalculateOccupiedSlots(
    UMedComBaseContainerWidget* Container,
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

TScriptInterface<IMedComInventoryUIBridgeWidget> UMedComDragDropHandler::GetBridgeForContainer(
    const FGameplayTag& ContainerType) const
{
    // Check if it's inventory container
    if (ContainerType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Container.Inventory"))))
    {
        if (InventoryBridge.IsValid())
        {
            return IMedComInventoryUIBridgeWidget::MakeScriptInterface(InventoryBridge.Get());
        }
        
        return IMedComInventoryUIBridgeWidget::GetGlobalBridge(GetWorld());
    }
    
    // Return empty TScriptInterface for unsupported container types
    return TScriptInterface<IMedComInventoryUIBridgeWidget>();
}

// =====================================================
// Cache Management
// =====================================================

void UMedComDragDropHandler::CacheContainer(UMedComBaseContainerWidget* Container)
{
    if (!Container)
    {
        return;
    }
    
    FGameplayTag ContainerType = IMedComContainerUIInterface::Execute_GetContainerType(Container);
    ContainerCache.Add(ContainerType, Container);
}

void UMedComDragDropHandler::ClearInvalidCaches()
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

void UMedComDragDropHandler::UpdateContainerCache()
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
        IMedComInventoryUIBridgeWidget* GlobalInventoryBridge = 
            IMedComInventoryUIBridgeWidget::GetInventoryUIBridge(GetWorld());
        if (GlobalInventoryBridge)
        {
            InventoryBridge = TWeakInterfacePtr<IMedComInventoryUIBridgeWidget>(GlobalInventoryBridge);
        }
    }
    
    if (!EquipmentBridge.IsValid())
    {
        IMedComEquipmentUIBridgeWidget* GlobalEquipmentBridge = 
            IMedComEquipmentUIBridgeWidget::GetEquipmentUIBridge(GetWorld());
        if (GlobalEquipmentBridge)
        {
            EquipmentBridge = TWeakInterfacePtr<IMedComEquipmentUIBridgeWidget>(GlobalEquipmentBridge);
        }
    }
    
    LastCacheValidationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

FVector2D UMedComDragDropHandler::CalculateDragOffsetForSlot(
    UMedComBaseSlotWidget* Slot,
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