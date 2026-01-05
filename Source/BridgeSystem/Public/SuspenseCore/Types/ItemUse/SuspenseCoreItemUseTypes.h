// SuspenseCoreItemUseTypes.h
// Item Use System - Core Types
// Copyright Suspense Team. All Rights Reserved.
//
// LOCATION: BridgeSystem (shared types - no module dependencies)
// USAGE: Used by GAS abilities and EquipmentSystem handlers
//
// This file is in BridgeSystem to avoid circular dependencies:
//   - GAS needs these types to define abilities
//   - EquipmentSystem needs these types for handlers
//   - Neither module depends on the other
//
// RELATED: ISuspenseCoreItemUseHandler.h, ISuspenseCoreItemUseService.h
//          SuspenseCoreItemUseNativeTags.h

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Items/SuspenseCoreItemTypes.h"
#include "SuspenseCoreItemUseTypes.generated.h"

/**
 * Result of item use operation
 */
UENUM(BlueprintType)
enum class ESuspenseCoreItemUseResult : uint8
{
	/** Operation completed successfully */
	Success                     UMETA(DisplayName = "Success"),

	/** Operation started, will complete async (time-based) */
	InProgress                  UMETA(DisplayName = "In Progress"),

	/** Items are not compatible for this operation */
	Failed_IncompatibleItems    UMETA(DisplayName = "Incompatible Items"),

	/** Target is full (magazine, container) */
	Failed_TargetFull           UMETA(DisplayName = "Target Full"),

	/** Item/slot is on cooldown */
	Failed_OnCooldown           UMETA(DisplayName = "On Cooldown"),

	/** Item cannot be used (wrong context) */
	Failed_NotUsable            UMETA(DisplayName = "Not Usable"),

	/** Missing requirement (ammo type, health condition) */
	Failed_MissingRequirement   UMETA(DisplayName = "Missing Requirement"),

	/** Operation was cancelled by user or damage */
	Cancelled                   UMETA(DisplayName = "Cancelled"),

	/** No handler found for this item combination */
	Failed_NoHandler            UMETA(DisplayName = "No Handler"),

	/** Security validation failed */
	Failed_SecurityDenied       UMETA(DisplayName = "Security Denied"),

	/** System error */
	Failed_SystemError          UMETA(DisplayName = "System Error")
};

/**
 * Context that triggered the item use
 */
UENUM(BlueprintType)
enum class ESuspenseCoreItemUseContext : uint8
{
	/** Double-click on item (consume, open) */
	DoubleClick     UMETA(DisplayName = "Double Click"),

	/** Drag item A onto item B */
	DragDrop        UMETA(DisplayName = "Drag & Drop"),

	/** Use from QuickSlot (keys 4-7) */
	QuickSlot       UMETA(DisplayName = "QuickSlot Hotkey"),

	/** Direct hotkey (F for interact) */
	Hotkey          UMETA(DisplayName = "Direct Hotkey"),

	/** Right-click context menu */
	ContextMenu     UMETA(DisplayName = "Context Menu"),

	/** Programmatic use (script/AI) */
	Programmatic    UMETA(DisplayName = "Programmatic")
};

/**
 * Handler priority for resolving conflicts
 * Higher priority handlers are checked first
 */
UENUM(BlueprintType)
enum class ESuspenseCoreHandlerPriority : uint8
{
	Low = 0         UMETA(DisplayName = "Low (0)"),
	Normal = 50     UMETA(DisplayName = "Normal (50)"),
	High = 100      UMETA(DisplayName = "High (100)"),
	Critical = 200  UMETA(DisplayName = "Critical (200)")
};

/**
 * Item use request - input to ItemUseService
 *
 * This struct encapsulates ALL information needed to execute an item use:
 * - Source item (what is being used)
 * - Target item (for drag&drop operations)
 * - Context (how the use was triggered)
 * - Additional metadata
 *
 * USAGE:
 *   FSuspenseCoreItemUseRequest Request;
 *   Request.SourceItem = MyItem;
 *   Request.Context = ESuspenseCoreItemUseContext::QuickSlot;
 *   Request.QuickSlotIndex = 0;
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreItemUseRequest
{
	GENERATED_BODY()

	//==================================================================
	// Source Item (required)
	//==================================================================

	/** Item being used (source) - REQUIRED */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Source")
	FSuspenseCoreItemInstance SourceItem;

	/** Source location index (inventory slot, equipment slot) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Source")
	int32 SourceSlotIndex = INDEX_NONE;

	/** Source container type tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Source",
		meta = (Categories = "Container"))
	FGameplayTag SourceContainerTag;

	//==================================================================
	// Target Item (optional - for DragDrop)
	//==================================================================

	/** Target item (for drag&drop operations) - OPTIONAL */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Target")
	FSuspenseCoreItemInstance TargetItem;

	/** Target location index */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Target")
	int32 TargetSlotIndex = INDEX_NONE;

	/** Target container type tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Target",
		meta = (Categories = "Container"))
	FGameplayTag TargetContainerTag;

	//==================================================================
	// Context
	//==================================================================

	/** How this use was triggered */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Context")
	ESuspenseCoreItemUseContext Context = ESuspenseCoreItemUseContext::DoubleClick;

	/** Quantity to use (for stackable items) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Context",
		meta = (ClampMin = "1"))
	int32 Quantity = 1;

	/** QuickSlot index if Context == QuickSlot (0-3) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ItemUse|Context")
	int32 QuickSlotIndex = INDEX_NONE;

	/** Request timestamp (for cooldown/debounce) */
	UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Context")
	float RequestTime = 0.0f;

	/** Unique request ID for tracking async operations */
	UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Context")
	FGuid RequestID;

	/** Requesting actor (player controller or pawn) */
	UPROPERTY(BlueprintReadWrite, Category = "ItemUse|Context")
	TWeakObjectPtr<AActor> RequestingActor;

	//==================================================================
	// Constructor
	//==================================================================

	FSuspenseCoreItemUseRequest()
	{
		RequestID = FGuid::NewGuid();
		RequestTime = 0.0f;
	}

	//==================================================================
	// Query Methods
	//==================================================================

	/** Is this a DragDrop operation with target? */
	bool HasTarget() const { return TargetItem.IsValid(); }

	/** Is this from QuickSlot? */
	bool IsFromQuickSlot() const { return Context == ESuspenseCoreItemUseContext::QuickSlot; }

	/** Is this from UI (double-click, drag&drop, context menu)? */
	bool IsFromUI() const
	{
		return Context == ESuspenseCoreItemUseContext::DoubleClick ||
			   Context == ESuspenseCoreItemUseContext::DragDrop ||
			   Context == ESuspenseCoreItemUseContext::ContextMenu;
	}

	/** Is request valid (has source item)? */
	bool IsValid() const { return SourceItem.IsValid(); }

	/** Debug string */
	FString ToString() const
	{
		return FString::Printf(TEXT("UseRequest[%s]: Source=%s, Target=%s, Context=%d, QuickSlot=%d"),
			*RequestID.ToString().Left(8),
			*SourceItem.ItemID.ToString(),
			HasTarget() ? *TargetItem.ItemID.ToString() : TEXT("None"),
			static_cast<int32>(Context),
			QuickSlotIndex);
	}
};

/**
 * Item use response - output from ItemUseService
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreItemUseResponse
{
	GENERATED_BODY()

	/** Result of the operation */
	UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
	ESuspenseCoreItemUseResult Result = ESuspenseCoreItemUseResult::Failed_SystemError;

	/** Request ID this response is for */
	UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
	FGuid RequestID;

	/** Human-readable error/status message */
	UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
	FText Message;

	/** Duration for time-based operations (0 = instant) */
	UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
	float Duration = 0.0f;

	/** Cooldown to apply after completion */
	UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
	float Cooldown = 0.0f;

	/** Progress (0.0 - 1.0) for in-progress operations */
	UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
	float Progress = 0.0f;

	/** Handler tag that processed this request */
	UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response",
		meta = (Categories = "ItemUse.Handler"))
	FGameplayTag HandlerTag;

	/** Modified source item (updated state after use) */
	UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
	FSuspenseCoreItemInstance ModifiedSourceItem;

	/** Modified target item (if applicable) */
	UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
	FSuspenseCoreItemInstance ModifiedTargetItem;

	/** Additional data (handler-specific) */
	UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Response")
	TMap<FString, FString> Metadata;

	//==================================================================
	// Factory Methods
	//==================================================================

	static FSuspenseCoreItemUseResponse Success(const FGuid& InRequestID, float InDuration = 0.0f)
	{
		FSuspenseCoreItemUseResponse Response;
		Response.Result = InDuration > 0.0f ?
			ESuspenseCoreItemUseResult::InProgress :
			ESuspenseCoreItemUseResult::Success;
		Response.RequestID = InRequestID;
		Response.Duration = InDuration;
		Response.Progress = InDuration > 0.0f ? 0.0f : 1.0f;
		return Response;
	}

	static FSuspenseCoreItemUseResponse Failure(
		const FGuid& InRequestID,
		ESuspenseCoreItemUseResult InResult,
		const FText& InMessage)
	{
		FSuspenseCoreItemUseResponse Response;
		Response.Result = InResult;
		Response.RequestID = InRequestID;
		Response.Message = InMessage;
		return Response;
	}

	//==================================================================
	// Query Methods
	//==================================================================

	bool IsSuccess() const
	{
		return Result == ESuspenseCoreItemUseResult::Success ||
			   Result == ESuspenseCoreItemUseResult::InProgress;
	}

	bool IsInProgress() const
	{
		return Result == ESuspenseCoreItemUseResult::InProgress;
	}

	bool IsFailed() const
	{
		return !IsSuccess();
	}
};

/**
 * EventBus event data for item use events
 * Sent via EventBus with tags from SuspenseCoreItemUseTags::Event
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreItemUseEventData
{
	GENERATED_BODY()

	/** Original request */
	UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Event")
	FSuspenseCoreItemUseRequest Request;

	/** Response from handler */
	UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Event")
	FSuspenseCoreItemUseResponse Response;

	/** Actor that owns the item use operation */
	UPROPERTY(BlueprintReadOnly, Category = "ItemUse|Event")
	TWeakObjectPtr<AActor> OwnerActor;

	/** Constructor */
	FSuspenseCoreItemUseEventData()
	{
	}

	FSuspenseCoreItemUseEventData(
		const FSuspenseCoreItemUseRequest& InRequest,
		const FSuspenseCoreItemUseResponse& InResponse,
		AActor* InOwner)
		: Request(InRequest)
		, Response(InResponse)
		, OwnerActor(InOwner)
	{
	}
};
