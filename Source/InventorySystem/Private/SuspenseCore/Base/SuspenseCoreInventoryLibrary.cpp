// SuspenseCoreInventoryLibrary.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Base/SuspenseCoreInventoryLibrary.h"
#include "SuspenseCore/Base/SuspenseCoreInventoryManager.h"
#include "SuspenseCore/Components/SuspenseCoreInventoryComponent.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Texture2D.h"

USuspenseCoreInventoryManager* USuspenseCoreInventoryLibrary::GetInventoryManager(UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}

	return GI->GetSubsystem<USuspenseCoreInventoryManager>();
}

USuspenseCoreInventoryComponent* USuspenseCoreInventoryLibrary::GetInventoryComponent(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	return Actor->FindComponentByClass<USuspenseCoreInventoryComponent>();
}

TArray<USuspenseCoreInventoryComponent*> USuspenseCoreInventoryLibrary::GetAllInventoryComponents(AActor* Actor)
{
	TArray<USuspenseCoreInventoryComponent*> Result;
	if (!Actor)
	{
		return Result;
	}

	Actor->GetComponents<USuspenseCoreInventoryComponent>(Result);
	return Result;
}

bool USuspenseCoreInventoryLibrary::IsItemInstanceValid(const FSuspenseCoreItemInstance& Instance)
{
	return Instance.IsValid();
}

bool USuspenseCoreInventoryLibrary::CanItemsStack(const FSuspenseCoreItemInstance& Instance1, const FSuspenseCoreItemInstance& Instance2)
{
	return Instance1.CanStackWith(Instance2);
}

FText USuspenseCoreInventoryLibrary::GetItemDisplayName(UObject* WorldContextObject, FName ItemID)
{
	if (!WorldContextObject)
	{
		return FText::FromName(ItemID);
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return FText::FromName(ItemID);
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return FText::FromName(ItemID);
	}

	USuspenseCoreDataManager* DataMgr = GI->GetSubsystem<USuspenseCoreDataManager>();
	if (!DataMgr)
	{
		return FText::FromName(ItemID);
	}

	FSuspenseCoreItemData ItemData;
	if (DataMgr->GetItemData(ItemID, ItemData))
	{
		return ItemData.Identity.DisplayName;
	}

	return FText::FromName(ItemID);
}

FText USuspenseCoreInventoryLibrary::GetItemDescription(UObject* WorldContextObject, FName ItemID)
{
	if (!WorldContextObject)
	{
		return FText::GetEmpty();
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return FText::GetEmpty();
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return FText::GetEmpty();
	}

	USuspenseCoreDataManager* DataMgr = GI->GetSubsystem<USuspenseCoreDataManager>();
	if (!DataMgr)
	{
		return FText::GetEmpty();
	}

	FSuspenseCoreItemData ItemData;
	if (DataMgr->GetItemData(ItemID, ItemData))
	{
		return ItemData.Identity.Description;
	}

	return FText::GetEmpty();
}

UTexture2D* USuspenseCoreInventoryLibrary::GetItemIcon(UObject* WorldContextObject, FName ItemID)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}

	USuspenseCoreDataManager* DataMgr = GI->GetSubsystem<USuspenseCoreDataManager>();
	if (!DataMgr)
	{
		return nullptr;
	}

	FSuspenseCoreItemData ItemData;
	if (DataMgr->GetItemData(ItemID, ItemData))
	{
		return ItemData.Identity.Icon.LoadSynchronous();
	}

	return nullptr;
}

FGameplayTag USuspenseCoreInventoryLibrary::GetItemRarity(UObject* WorldContextObject, FName ItemID)
{
	if (!WorldContextObject)
	{
		return FGameplayTag();
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return FGameplayTag();
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return FGameplayTag();
	}

	USuspenseCoreDataManager* DataMgr = GI->GetSubsystem<USuspenseCoreDataManager>();
	if (!DataMgr)
	{
		return FGameplayTag();
	}

	FSuspenseCoreItemData ItemData;
	if (DataMgr->GetItemData(ItemID, ItemData))
	{
		return ItemData.Classification.Rarity;
	}

	return FGameplayTag();
}

FGameplayTag USuspenseCoreInventoryLibrary::GetItemType(UObject* WorldContextObject, FName ItemID)
{
	if (!WorldContextObject)
	{
		return FGameplayTag();
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return FGameplayTag();
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return FGameplayTag();
	}

	USuspenseCoreDataManager* DataMgr = GI->GetSubsystem<USuspenseCoreDataManager>();
	if (!DataMgr)
	{
		return FGameplayTag();
	}

	FSuspenseCoreItemData ItemData;
	if (DataMgr->GetItemData(ItemID, ItemData))
	{
		return ItemData.Classification.ItemType;
	}

	return FGameplayTag();
}

FIntPoint USuspenseCoreInventoryLibrary::SlotToGridPosition(int32 SlotIndex, int32 GridWidth)
{
	if (GridWidth <= 0 || SlotIndex < 0)
	{
		return FIntPoint::NoneValue;
	}

	return FIntPoint(SlotIndex % GridWidth, SlotIndex / GridWidth);
}

int32 USuspenseCoreInventoryLibrary::GridPositionToSlot(FIntPoint GridPosition, int32 GridWidth)
{
	if (GridWidth <= 0 || GridPosition.X < 0 || GridPosition.Y < 0)
	{
		return INDEX_NONE;
	}

	return GridPosition.Y * GridWidth + GridPosition.X;
}

bool USuspenseCoreInventoryLibrary::IsValidGridPosition(FIntPoint Position, FIntPoint GridSize)
{
	return Position.X >= 0 && Position.Y >= 0 &&
		   Position.X < GridSize.X && Position.Y < GridSize.Y;
}

FIntPoint USuspenseCoreInventoryLibrary::GetRotatedSize(FIntPoint OriginalSize, int32 Rotation)
{
	// Normalize rotation to 0, 90, 180, 270
	int32 NormalizedRotation = ((Rotation % 360) + 360) % 360;

	// 90 or 270 degrees swaps width and height
	if (NormalizedRotation == 90 || NormalizedRotation == 270)
	{
		return FIntPoint(OriginalSize.Y, OriginalSize.X);
	}

	return OriginalSize;
}

TArray<int32> USuspenseCoreInventoryLibrary::GetOccupiedSlots(int32 AnchorSlot, FIntPoint ItemSize, int32 GridWidth)
{
	TArray<int32> Slots;
	if (AnchorSlot < 0 || GridWidth <= 0 || ItemSize.X <= 0 || ItemSize.Y <= 0)
	{
		return Slots;
	}

	FIntPoint AnchorPos = SlotToGridPosition(AnchorSlot, GridWidth);

	for (int32 Y = 0; Y < ItemSize.Y; ++Y)
	{
		for (int32 X = 0; X < ItemSize.X; ++X)
		{
			int32 Slot = GridPositionToSlot(FIntPoint(AnchorPos.X + X, AnchorPos.Y + Y), GridWidth);
			if (Slot != INDEX_NONE)
			{
				Slots.Add(Slot);
			}
		}
	}

	return Slots;
}

float USuspenseCoreInventoryLibrary::CalculateTotalWeight(UObject* WorldContextObject, const TArray<FSuspenseCoreItemInstance>& Items)
{
	float TotalWeight = 0.0f;

	for (const FSuspenseCoreItemInstance& Item : Items)
	{
		float UnitWeight = GetItemWeight(WorldContextObject, Item.ItemID);
		TotalWeight += UnitWeight * Item.Quantity;
	}

	return TotalWeight;
}

float USuspenseCoreInventoryLibrary::GetItemWeight(UObject* WorldContextObject, FName ItemID)
{
	if (!WorldContextObject)
	{
		return 0.0f;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!World)
	{
		return 0.0f;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return 0.0f;
	}

	USuspenseCoreDataManager* DataMgr = GI->GetSubsystem<USuspenseCoreDataManager>();
	if (!DataMgr)
	{
		return 0.0f;
	}

	FSuspenseCoreItemData ItemData;
	if (DataMgr->GetItemData(ItemID, ItemData))
	{
		return ItemData.InventoryProps.Weight;
	}

	return 0.0f;
}

FString USuspenseCoreInventoryLibrary::FormatWeight(float Weight)
{
	if (Weight >= 1.0f)
	{
		return FString::Printf(TEXT("%.1f kg"), Weight);
	}
	else
	{
		return FString::Printf(TEXT("%.0f g"), Weight * 1000.0f);
	}
}

bool USuspenseCoreInventoryLibrary::IsOperationSuccess(const FSuspenseCoreInventorySimpleResult& Result)
{
	return Result.bSuccess;
}

FString USuspenseCoreInventoryLibrary::GetResultMessage(const FSuspenseCoreInventorySimpleResult& Result)
{
	if (Result.bSuccess)
	{
		return TEXT("Operation completed successfully");
	}

	if (!Result.ErrorMessage.IsEmpty())
	{
		return Result.ErrorMessage;
	}

	return GetResultCodeDisplayName(Result.ResultCode).ToString();
}

FText USuspenseCoreInventoryLibrary::GetResultCodeDisplayName(ESuspenseCoreInventoryResult ResultCode)
{
	switch (ResultCode)
	{
	case ESuspenseCoreInventoryResult::Success:
		return NSLOCTEXT("SuspenseCore", "Result_Success", "Success");
	case ESuspenseCoreInventoryResult::NoSpace:
		return NSLOCTEXT("SuspenseCore", "Result_NoSpace", "No Space Available");
	case ESuspenseCoreInventoryResult::WeightLimitExceeded:
		return NSLOCTEXT("SuspenseCore", "Result_WeightLimit", "Weight Limit Exceeded");
	case ESuspenseCoreInventoryResult::InvalidItem:
		return NSLOCTEXT("SuspenseCore", "Result_InvalidItem", "Invalid Item");
	case ESuspenseCoreInventoryResult::ItemNotFound:
		return NSLOCTEXT("SuspenseCore", "Result_ItemNotFound", "Item Not Found");
	case ESuspenseCoreInventoryResult::InsufficientQuantity:
		return NSLOCTEXT("SuspenseCore", "Result_InsufficientQty", "Insufficient Quantity");
	case ESuspenseCoreInventoryResult::InvalidSlot:
		return NSLOCTEXT("SuspenseCore", "Result_InvalidSlot", "Invalid Slot");
	case ESuspenseCoreInventoryResult::SlotOccupied:
		return NSLOCTEXT("SuspenseCore", "Result_SlotOccupied", "Slot Already Occupied");
	case ESuspenseCoreInventoryResult::TypeNotAllowed:
		return NSLOCTEXT("SuspenseCore", "Result_TypeNotAllowed", "Item Type Not Allowed");
	case ESuspenseCoreInventoryResult::TransactionActive:
		return NSLOCTEXT("SuspenseCore", "Result_TxnActive", "Transaction Already Active");
	case ESuspenseCoreInventoryResult::NotInitialized:
		return NSLOCTEXT("SuspenseCore", "Result_NotInit", "Inventory Not Initialized");
	case ESuspenseCoreInventoryResult::NetworkError:
		return NSLOCTEXT("SuspenseCore", "Result_NetworkError", "Network Error");
	default:
		return NSLOCTEXT("SuspenseCore", "Result_Unknown", "Unknown Error");
	}
}

int32 USuspenseCoreInventoryLibrary::CompareItemsByName(const FSuspenseCoreItemInstance& A, const FSuspenseCoreItemInstance& B)
{
	return A.ItemID.Compare(B.ItemID);
}

int32 USuspenseCoreInventoryLibrary::CompareItemsByQuantity(const FSuspenseCoreItemInstance& A, const FSuspenseCoreItemInstance& B)
{
	return A.Quantity - B.Quantity;
}

FString USuspenseCoreInventoryLibrary::GetItemInstanceDebugString(const FSuspenseCoreItemInstance& Instance)
{
	return FString::Printf(TEXT("[%s] %s x%d @ Slot %d (Rot: %d)"),
		*Instance.UniqueInstanceID.ToString().Left(8),
		*Instance.ItemID.ToString(),
		Instance.Quantity,
		Instance.SlotIndex,
		Instance.Rotation);
}

FString USuspenseCoreInventoryLibrary::GetOperationRecordDebugString(const FSuspenseCoreOperationRecord& Record)
{
	FString TypeStr;
	switch (Record.OperationType)
	{
	case ESuspenseCoreOperationType::Add: TypeStr = TEXT("Add"); break;
	case ESuspenseCoreOperationType::Remove: TypeStr = TEXT("Remove"); break;
	case ESuspenseCoreOperationType::Move: TypeStr = TEXT("Move"); break;
	case ESuspenseCoreOperationType::Swap: TypeStr = TEXT("Swap"); break;
	case ESuspenseCoreOperationType::Rotate: TypeStr = TEXT("Rotate"); break;
	case ESuspenseCoreOperationType::SplitStack: TypeStr = TEXT("Split"); break;
	case ESuspenseCoreOperationType::MergeStack: TypeStr = TEXT("Merge"); break;
	default: TypeStr = TEXT("Unknown"); break;
	}

	return FString::Printf(TEXT("[%s] %s: %s -> %d (Qty: %d) %s"),
		*Record.OperationID.ToString().Left(8),
		*TypeStr,
		*Record.ItemID.ToString(),
		Record.NewSlot,
		Record.Quantity,
		Record.bSuccess ? TEXT("SUCCESS") : TEXT("FAILED"));
}
