// SuspenseCoreUIEvents.h
// SuspenseCore - UI EventBus Events and Tags
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUITypes.h"
#include "SuspenseCore/Types/UI/SuspenseCoreUIContainerTypes.h"
#include "SuspenseCoreUIEvents.generated.h"

/**
 * SuspenseCore UI Event Tags
 *
 * All UI-related gameplay tags for EventBus communication.
 * Tags follow the pattern: SuspenseCore.Event.UI.*
 *
 * CATEGORIES:
 * - UIProvider: Data provider registration and changes
 * - UIContainer: Container widget events
 * - UIFeedback: User feedback notifications
 * - UIRequest: Operation requests from UI
 */

//==================================================================
// Native Gameplay Tags Declaration
//==================================================================

// UI Provider Events
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIProvider);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIProvider_Registered);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIProvider_Unregistered);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIProvider_DataChanged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIProvider_BindRequested);

// UI Provider Types
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIProvider_Type);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIProvider_Type_Inventory);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIProvider_Type_Equipment);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIProvider_Type_Stash);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIProvider_Type_Trader);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIProvider_Type_Loot);

// UI Container Events
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_Opened);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_Closed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_SlotClicked);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_SlotHovered);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_SlotRightClicked);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_DragStarted);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_DragEnded);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_DropCompleted);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIContainer_DropFailed);

// UI Feedback Events
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIFeedback);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIFeedback_Success);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIFeedback_Error);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIFeedback_Warning);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIFeedback_ItemPickedUp);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIFeedback_InventoryFull);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIFeedback_WeightExceeded);

// UI Request Events (from UI to game logic)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest_MoveItem);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest_RotateItem);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest_UseItem);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest_DropItem);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest_SplitStack);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest_TransferItem);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest_EquipItem);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIRequest_UnequipItem);

// UI Panel Events
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIPanel);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIPanel_Inventory);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIPanel_Equipment);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIPanel_Stash);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Event_UIPanel_Trader);

// Container Type Tags (for Drag-Drop source/target identification)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Container);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Container_Inventory);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Container_Equipment);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Container_Stash);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Container_Trader);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_Container_Loot);

// Context Menu Actions
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIAction);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIAction_Use);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIAction_Equip);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIAction_Unequip);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIAction_Drop);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIAction_Split);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIAction_Examine);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIAction_Discard);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIAction_Sell);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_SuspenseCore_UIAction_Buy);

/**
 * FSuspenseCoreUIEventPayload
 * Standard payload for UI events via EventBus
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreUIEventPayload
{
	GENERATED_BODY()

	//==================================================================
	// Source Identification
	//==================================================================

	/** Source container type */
	UPROPERTY(BlueprintReadWrite, Category = "Source")
	ESuspenseCoreContainerType SourceContainerType;

	/** Source container ID */
	UPROPERTY(BlueprintReadWrite, Category = "Source")
	FGuid SourceContainerID;

	/** Source slot index */
	UPROPERTY(BlueprintReadWrite, Category = "Source")
	int32 SourceSlot;

	//==================================================================
	// Target Identification (for transfers)
	//==================================================================

	/** Target container type */
	UPROPERTY(BlueprintReadWrite, Category = "Target")
	ESuspenseCoreContainerType TargetContainerType;

	/** Target container ID */
	UPROPERTY(BlueprintReadWrite, Category = "Target")
	FGuid TargetContainerID;

	/** Target slot index */
	UPROPERTY(BlueprintReadWrite, Category = "Target")
	int32 TargetSlot;

	//==================================================================
	// Item Data
	//==================================================================

	/** Item instance ID */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	FGuid ItemInstanceID;

	/** Item ID (DataTable row name) */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	FName ItemID;

	/** Quantity for stack operations */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	int32 Quantity;

	/** Is item rotated */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	bool bIsRotated;

	//==================================================================
	// Operation Info
	//==================================================================

	/** Success/failure flag */
	UPROPERTY(BlueprintReadWrite, Category = "Operation")
	bool bSuccess;

	/** Error/info message */
	UPROPERTY(BlueprintReadWrite, Category = "Operation")
	FText Message;

	/** Action tag (for context menu) */
	UPROPERTY(BlueprintReadWrite, Category = "Operation")
	FGameplayTag ActionTag;

	//==================================================================
	// UI State
	//==================================================================

	/** Screen position for context menus/tooltips */
	UPROPERTY(BlueprintReadWrite, Category = "UI")
	FVector2D ScreenPosition;

	/** Owning player controller */
	UPROPERTY(BlueprintReadWrite, Category = "UI")
	TWeakObjectPtr<APlayerController> OwningPlayer;

	FSuspenseCoreUIEventPayload()
		: SourceContainerType(ESuspenseCoreContainerType::None)
		, SourceSlot(INDEX_NONE)
		, TargetContainerType(ESuspenseCoreContainerType::None)
		, TargetSlot(INDEX_NONE)
		, Quantity(0)
		, bIsRotated(false)
		, bSuccess(false)
		, Message(FText::GetEmpty())
		, ScreenPosition(FVector2D::ZeroVector)
	{
	}

	//==================================================================
	// Factory Methods
	//==================================================================

	/** Create payload for move item request */
	static FSuspenseCoreUIEventPayload CreateMoveRequest(
		const FGuid& InContainerID,
		ESuspenseCoreContainerType InContainerType,
		int32 InFromSlot,
		int32 InToSlot,
		bool bInRotate = false)
	{
		FSuspenseCoreUIEventPayload Payload;
		Payload.SourceContainerID = InContainerID;
		Payload.SourceContainerType = InContainerType;
		Payload.SourceSlot = InFromSlot;
		Payload.TargetContainerID = InContainerID; // Same container
		Payload.TargetContainerType = InContainerType;
		Payload.TargetSlot = InToSlot;
		Payload.bIsRotated = bInRotate;
		return Payload;
	}

	/** Create payload for transfer between containers */
	static FSuspenseCoreUIEventPayload CreateTransferRequest(
		const FGuid& InSourceContainerID,
		ESuspenseCoreContainerType InSourceType,
		int32 InSourceSlot,
		const FGuid& InTargetContainerID,
		ESuspenseCoreContainerType InTargetType,
		int32 InTargetSlot,
		int32 InQuantity = 0)
	{
		FSuspenseCoreUIEventPayload Payload;
		Payload.SourceContainerID = InSourceContainerID;
		Payload.SourceContainerType = InSourceType;
		Payload.SourceSlot = InSourceSlot;
		Payload.TargetContainerID = InTargetContainerID;
		Payload.TargetContainerType = InTargetType;
		Payload.TargetSlot = InTargetSlot;
		Payload.Quantity = InQuantity;
		return Payload;
	}

	/** Create payload for drop request */
	static FSuspenseCoreUIEventPayload CreateDropRequest(
		const FGuid& InContainerID,
		ESuspenseCoreContainerType InContainerType,
		int32 InSlot,
		int32 InQuantity = 0)
	{
		FSuspenseCoreUIEventPayload Payload;
		Payload.SourceContainerID = InContainerID;
		Payload.SourceContainerType = InContainerType;
		Payload.SourceSlot = InSlot;
		Payload.Quantity = InQuantity;
		return Payload;
	}

	/** Create payload for context menu action */
	static FSuspenseCoreUIEventPayload CreateActionRequest(
		const FGuid& InContainerID,
		ESuspenseCoreContainerType InContainerType,
		int32 InSlot,
		const FGuid& InItemID,
		const FGameplayTag& InActionTag)
	{
		FSuspenseCoreUIEventPayload Payload;
		Payload.SourceContainerID = InContainerID;
		Payload.SourceContainerType = InContainerType;
		Payload.SourceSlot = InSlot;
		Payload.ItemInstanceID = InItemID;
		Payload.ActionTag = InActionTag;
		return Payload;
	}

	/** Create payload for feedback notification */
	static FSuspenseCoreUIEventPayload CreateFeedback(
		bool bInSuccess,
		const FText& InMessage,
		const FGuid& InItemID = FGuid(),
		int32 InQuantity = 0)
	{
		FSuspenseCoreUIEventPayload Payload;
		Payload.bSuccess = bInSuccess;
		Payload.Message = InMessage;
		Payload.ItemInstanceID = InItemID;
		Payload.Quantity = InQuantity;
		return Payload;
	}
};

/**
 * USuspenseCoreUIEventHelpers
 * Helper functions for UI events
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseCoreUIEventHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Get container type tag from enum
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Events")
	static FGameplayTag GetContainerTypeTag(ESuspenseCoreContainerType ContainerType);

	/**
	 * Get container type from tag
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Events")
	static ESuspenseCoreContainerType GetContainerTypeFromTag(const FGameplayTag& Tag);

	/**
	 * Get feedback type tag from enum
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|UI|Events")
	static FGameplayTag GetFeedbackTypeTag(ESuspenseCoreUIFeedbackType FeedbackType);

	/**
	 * Create and broadcast UI request event
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Events", meta = (WorldContext = "WorldContext"))
	static void BroadcastUIRequest(
		const UObject* WorldContext,
		const FGameplayTag& RequestTag,
		const FSuspenseCoreUIEventPayload& Payload);

	/**
	 * Create and broadcast UI feedback event
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|UI|Events", meta = (WorldContext = "WorldContext"))
	static void BroadcastUIFeedback(
		const UObject* WorldContext,
		ESuspenseCoreUIFeedbackType FeedbackType,
		const FText& Message,
		const FGuid& RelatedItemID = FGuid());
};
