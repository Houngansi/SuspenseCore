// SuspenseCoreDragDropHandler.h
// SuspenseCore - Centralized Drag-Drop Handler
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUITypes.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "SuspenseCoreDragDropHandler.generated.h"

// Forward declarations
class USuspenseCoreDragDropOperation;
class ISuspenseCoreUIContainer;
class ISuspenseCoreUIDataProvider;
class USuspenseCoreEventBus;
class UWidget;

/**
 * Drop target information with validation result
 */
USTRUCT(BlueprintType)
struct UISYSTEM_API FSuspenseCoreDropTargetInfo
{
	GENERATED_BODY()

	/** Container widget at target */
	UPROPERTY(BlueprintReadOnly, Category = "DropTarget")
	TScriptInterface<ISuspenseCoreUIContainer> Container;

	/** Target slot index */
	UPROPERTY(BlueprintReadOnly, Category = "DropTarget")
	int32 SlotIndex = INDEX_NONE;

	/** Is the drop valid at this location */
	UPROPERTY(BlueprintReadOnly, Category = "DropTarget")
	bool bIsValid = false;

	/** Validation message for feedback */
	UPROPERTY(BlueprintReadOnly, Category = "DropTarget")
	FText ValidationMessage;

	/** Container type tag */
	UPROPERTY(BlueprintReadOnly, Category = "DropTarget")
	FGameplayTag ContainerTypeTag;

	/** All slots that would be affected by drop */
	UPROPERTY(BlueprintReadOnly, Category = "DropTarget")
	TArray<int32> AffectedSlots;

	/** Is the item rotated */
	UPROPERTY(BlueprintReadOnly, Category = "DropTarget")
	bool bIsRotated = false;
};

/**
 * Smart drop configuration for snapping and auto-placement
 */
USTRUCT(BlueprintType)
struct UISYSTEM_API FSuspenseCoreSmartDropConfig
{
	GENERATED_BODY()

	/** Enable smart drop (find nearest valid slot) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartDrop")
	bool bEnableSmartDrop = true;

	/** Detection radius for nearby slots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartDrop",
		meta = (ClampMin = "50", ClampMax = "200"))
	float DetectionRadius = 100.0f;

	/** Snap strength (0-1) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartDrop",
		meta = (ClampMin = "0", ClampMax = "1"))
	float SnapStrength = 0.8f;

	/** Animation speed for visual feedback */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SmartDrop",
		meta = (ClampMin = "1", ClampMax = "20"))
	float AnimationSpeed = 10.0f;
};

/**
 * Drop request data for centralized processing
 */
USTRUCT(BlueprintType)
struct UISYSTEM_API FSuspenseCoreDropRequest
{
	GENERATED_BODY()

	/** Source container type */
	UPROPERTY(BlueprintReadOnly, Category = "DropRequest")
	FGameplayTag SourceContainerTag;

	/** Target container type */
	UPROPERTY(BlueprintReadOnly, Category = "DropRequest")
	FGameplayTag TargetContainerTag;

	/** Source provider ID */
	UPROPERTY(BlueprintReadOnly, Category = "DropRequest")
	FGuid SourceProviderID;

	/** Target provider ID */
	UPROPERTY(BlueprintReadOnly, Category = "DropRequest")
	FGuid TargetProviderID;

	/** Source slot */
	UPROPERTY(BlueprintReadOnly, Category = "DropRequest")
	int32 SourceSlot = INDEX_NONE;

	/** Target slot */
	UPROPERTY(BlueprintReadOnly, Category = "DropRequest")
	int32 TargetSlot = INDEX_NONE;

	/** Drag data */
	UPROPERTY(BlueprintReadOnly, Category = "DropRequest")
	FSuspenseCoreDragData DragData;

	/** Screen position of drop */
	UPROPERTY(BlueprintReadOnly, Category = "DropRequest")
	FVector2D ScreenPosition = FVector2D::ZeroVector;
};

/**
 * Drop operation result
 */
USTRUCT(BlueprintType)
struct UISYSTEM_API FSuspenseCoreDropResult
{
	GENERATED_BODY()

	/** Did the drop succeed */
	UPROPERTY(BlueprintReadOnly, Category = "DropResult")
	bool bSuccess = false;

	/** Result message for feedback */
	UPROPERTY(BlueprintReadOnly, Category = "DropResult")
	FText ResultMessage;

	/** Error code if failed */
	UPROPERTY(BlueprintReadOnly, Category = "DropResult")
	FGameplayTag ErrorTag;

	/** Create success result */
	static FSuspenseCoreDropResult Success(const FText& Message = FText::GetEmpty())
	{
		FSuspenseCoreDropResult Result;
		Result.bSuccess = true;
		Result.ResultMessage = Message;
		return Result;
	}

	/** Create failure result */
	static FSuspenseCoreDropResult Failure(const FText& Message, const FGameplayTag& ErrorTagIn = FGameplayTag())
	{
		FSuspenseCoreDropResult Result;
		Result.bSuccess = false;
		Result.ResultMessage = Message;
		Result.ErrorTag = ErrorTagIn;
		return Result;
	}
};

/**
 * USuspenseCoreDragDropHandler
 *
 * Centralized drag-drop handler following legacy pattern.
 * GameInstanceSubsystem that manages all drag-drop operations.
 *
 * ARCHITECTURE:
 * - Single point of control for all drag-drop operations
 * - Widgets delegate drag-drop logic to this handler
 * - Routes operations based on source/target container types
 * - Provides visual feedback coordination
 *
 * FEATURES:
 * - Smart drop (find nearest valid slot)
 * - Cross-container transfers
 * - Visual feedback management
 * - Rotation during drag
 * - Drop validation with caching
 *
 * USAGE:
 * ```cpp
 * // In widget's NativeOnDragDetected:
 * USuspenseCoreDragDropHandler* Handler = USuspenseCoreDragDropHandler::Get(this);
 * if (Handler)
 * {
 *     OutOperation = Handler->StartDragOperation(SourceContainer, SourceSlot, MouseEvent);
 * }
 *
 * // In widget's NativeOnDrop:
 * if (Handler)
 * {
 *     Result = Handler->ProcessDrop(DragOperation, TargetContainer, TargetSlot);
 * }
 * ```
 *
 * @see USuspenseDragDropHandler - Legacy version for reference
 * @see USuspenseCoreDragDropOperation
 */
UCLASS()
class UISYSTEM_API USuspenseCoreDragDropHandler : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//==================================================================
	// Static Access
	//==================================================================

	/**
	 * Get DragDropHandler from world context
	 * @param WorldContext Object with world context
	 * @return Handler or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DragDrop", meta = (WorldContext = "WorldContext"))
	static USuspenseCoreDragDropHandler* Get(const UObject* WorldContext);

	//==================================================================
	// Lifecycle
	//==================================================================

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	//==================================================================
	// Core Drag-Drop Operations
	//==================================================================

	/**
	 * Start a drag operation from a container
	 * Creates and returns a UDragDropOperation for UE's drag-drop system.
	 *
	 * @param SourceContainer Container widget where drag started
	 * @param SourceSlot Slot index being dragged from
	 * @param MouseEvent The mouse event that triggered the drag
	 * @return DragDropOperation or nullptr if failed
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DragDrop")
	USuspenseCoreDragDropOperation* StartDragOperation(
		TScriptInterface<ISuspenseCoreUIContainer> SourceContainer,
		int32 SourceSlot,
		const FPointerEvent& MouseEvent);

	/**
	 * Process a drop operation on a target container
	 * Routes to appropriate handler based on source/target types.
	 *
	 * @param DragOperation The drag operation being dropped
	 * @param TargetContainer Target container widget
	 * @param TargetSlot Target slot index
	 * @return Drop result with success/failure info
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DragDrop")
	FSuspenseCoreDropResult ProcessDrop(
		USuspenseCoreDragDropOperation* DragOperation,
		TScriptInterface<ISuspenseCoreUIContainer> TargetContainer,
		int32 TargetSlot);

	/**
	 * Process a drop request (data-only version)
	 * @param Request Drop request data
	 * @return Drop result
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DragDrop")
	FSuspenseCoreDropResult ProcessDropRequest(const FSuspenseCoreDropRequest& Request);

	/**
	 * Cancel current drag operation
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DragDrop")
	void CancelDragOperation();

	//==================================================================
	// Drop Target Calculation
	//==================================================================

	/**
	 * Calculate drop target at screen position
	 * Finds container and slot, validates placement.
	 *
	 * @param ScreenPosition Current cursor screen position
	 * @param ItemSize Size of item being dragged
	 * @param bIsRotated Is item currently rotated
	 * @return Drop target info with validation
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DragDrop")
	FSuspenseCoreDropTargetInfo CalculateDropTarget(
		const FVector2D& ScreenPosition,
		const FIntPoint& ItemSize,
		bool bIsRotated) const;

	/**
	 * Find best drop target using smart drop algorithm
	 * Searches nearby slots for valid placement.
	 *
	 * @param ScreenPosition Cursor position
	 * @param ItemSize Item size
	 * @param bIsRotated Is rotated
	 * @return Best drop target info
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DragDrop")
	FSuspenseCoreDropTargetInfo FindBestDropTarget(
		const FVector2D& ScreenPosition,
		const FIntPoint& ItemSize,
		bool bIsRotated) const;

	//==================================================================
	// Visual Feedback
	//==================================================================

	/**
	 * Update drag visual based on validity
	 * @param DragOperation Current operation
	 * @param bIsValidTarget Is hovering over valid target
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DragDrop|Visual")
	void UpdateDragVisual(USuspenseCoreDragDropOperation* DragOperation, bool bIsValidTarget);

	/**
	 * Highlight slots in a container for drop preview
	 * @param Container Container to highlight in
	 * @param Slots Slot indices to highlight
	 * @param bIsValid Use valid (green) or invalid (red) color
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DragDrop|Visual")
	void HighlightDropSlots(
		TScriptInterface<ISuspenseCoreUIContainer> Container,
		const TArray<int32>& Slots,
		bool bIsValid);

	/**
	 * Clear all visual feedback
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DragDrop|Visual")
	void ClearAllHighlights();

	//==================================================================
	// Rotation Support
	//==================================================================

	/**
	 * Toggle rotation on current drag operation
	 * @return New rotation state
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DragDrop")
	bool ToggleRotation();

	/**
	 * Get current rotation state
	 * @return true if item is rotated
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DragDrop")
	bool IsCurrentDragRotated() const;

	//==================================================================
	// State Queries
	//==================================================================

	/**
	 * Check if drag operation is active
	 * @return true if dragging
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DragDrop")
	bool IsDragOperationActive() const { return ActiveOperation.IsValid(); }

	/**
	 * Get current active drag operation
	 * @return Active operation or nullptr
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DragDrop")
	USuspenseCoreDragDropOperation* GetActiveOperation() const { return ActiveOperation.Get(); }

	//==================================================================
	// Configuration
	//==================================================================

	/**
	 * Get smart drop configuration
	 * @return Current config
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|DragDrop|Config")
	const FSuspenseCoreSmartDropConfig& GetSmartDropConfig() const { return SmartDropConfig; }

	/**
	 * Set smart drop configuration
	 * @param NewConfig New configuration
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|DragDrop|Config")
	void SetSmartDropConfig(const FSuspenseCoreSmartDropConfig& NewConfig) { SmartDropConfig = NewConfig; }

protected:
	//==================================================================
	// Drop Routing
	//==================================================================

	/**
	 * Route drop operation to appropriate handler
	 * @param Request Drop request
	 * @return Result
	 */
	FSuspenseCoreDropResult RouteDropOperation(const FSuspenseCoreDropRequest& Request);

	/**
	 * Handle inventory-to-inventory drop (same or different)
	 * @param Request Drop request
	 * @return Result
	 */
	FSuspenseCoreDropResult HandleInventoryToInventory(const FSuspenseCoreDropRequest& Request);

	/**
	 * Handle inventory-to-equipment drop
	 * @param Request Drop request
	 * @return Result
	 */
	FSuspenseCoreDropResult HandleInventoryToEquipment(const FSuspenseCoreDropRequest& Request);

	/**
	 * Handle equipment-to-inventory drop
	 * @param Request Drop request
	 * @return Result
	 */
	FSuspenseCoreDropResult HandleEquipmentToInventory(const FSuspenseCoreDropRequest& Request);

	/**
	 * Handle equipment-to-equipment drop (swap or transfer between slots)
	 * @param Request Drop request
	 * @return Result
	 */
	FSuspenseCoreDropResult HandleEquipmentToEquipment(const FSuspenseCoreDropRequest& Request);

	/**
	 * Execute the actual drop via provider
	 * @param Request Drop request
	 * @return Result
	 */
	FSuspenseCoreDropResult ExecuteDrop(const FSuspenseCoreDropRequest& Request);

	//==================================================================
	// Internal Helpers
	//==================================================================

	/**
	 * Find provider by ID
	 * @param ProviderID Provider GUID
	 * @return Provider interface or nullptr
	 */
	TScriptInterface<ISuspenseCoreUIDataProvider> FindProviderByID(const FGuid& ProviderID) const;

	/**
	 * Get EventBus for broadcasting
	 * @return EventBus or nullptr
	 */
	USuspenseCoreEventBus* GetEventBus() const;

	/**
	 * Calculate occupied slots for item at position
	 * @param Container Container widget
	 * @param AnchorSlot Anchor slot
	 * @param ItemSize Item size
	 * @param bIsRotated Is rotated
	 * @param OutSlots Output array of slots
	 * @return true if calculation succeeded
	 */
	bool CalculateOccupiedSlots(
		TScriptInterface<ISuspenseCoreUIContainer> Container,
		int32 AnchorSlot,
		const FIntPoint& ItemSize,
		bool bIsRotated,
		TArray<int32>& OutSlots) const;

private:
	//==================================================================
	// Configuration
	//==================================================================

	/** Smart drop configuration */
	UPROPERTY()
	FSuspenseCoreSmartDropConfig SmartDropConfig;

	//==================================================================
	// State
	//==================================================================

	/** Currently active drag operation */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreDragDropOperation> ActiveOperation;

	/** Currently highlighted container */
	TWeakObjectPtr<UObject> HighlightedContainer;

	/** Currently highlighted slots */
	TSet<int32> CurrentHighlightedSlots;

	//==================================================================
	// Caching
	//==================================================================

	/** Cached hover position for optimization */
	mutable FVector2D CachedHoverPosition;

	/** Cached hover time */
	mutable float CachedHoverTime;

	/** Cached EventBus */
	mutable TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

	//==================================================================
	// Thresholds
	//==================================================================

	/** Position update threshold */
	static constexpr float HOVER_UPDATE_THRESHOLD = 30.0f;

	/** Cache lifetime */
	static constexpr float HOVER_CACHE_LIFETIME = 0.3f;
};
