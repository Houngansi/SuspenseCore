// SuspenseCoreInventoryStorage.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Storage/SuspenseCoreInventoryStorage.h"
#include "SuspenseCore/Base/SuspenseCoreInventoryLogs.h"

USuspenseCoreInventoryStorage::USuspenseCoreInventoryStorage()
	: GridWidth(0)
	, GridHeight(0)
	, bIsInitialized(false)
{
}

void USuspenseCoreInventoryStorage::Initialize(int32 InGridWidth, int32 InGridHeight)
{
	check(IsInGameThread());

	GridWidth = FMath::Clamp(InGridWidth, 1, 50);
	GridHeight = FMath::Clamp(InGridHeight, 1, 50);

	int32 TotalSlots = GridWidth * GridHeight;
	Slots.SetNum(TotalSlots);
	FreeSlotBitmap.Init(true, TotalSlots);

	for (FSuspenseCoreInventorySlot& Slot : Slots)
	{
		Slot.Clear();
	}

	bIsInitialized = true;

	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("Storage initialized: %dx%d (%d slots)"),
		GridWidth, GridHeight, TotalSlots);
}

void USuspenseCoreInventoryStorage::Clear()
{
	for (FSuspenseCoreInventorySlot& Slot : Slots)
	{
		Slot.Clear();
	}
	UpdateFreeBitmap();
}

bool USuspenseCoreInventoryStorage::IsSlotOccupied(int32 SlotIndex) const
{
	if (!IsValidSlot(SlotIndex))
	{
		return false;
	}
	return !Slots[SlotIndex].IsEmpty();
}

bool USuspenseCoreInventoryStorage::IsValidSlot(int32 SlotIndex) const
{
	return SlotIndex >= 0 && SlotIndex < Slots.Num();
}

FSuspenseCoreInventorySlot USuspenseCoreInventoryStorage::GetSlot(int32 SlotIndex) const
{
	if (!IsValidSlot(SlotIndex))
	{
		return FSuspenseCoreInventorySlot();
	}
	return Slots[SlotIndex];
}

FGuid USuspenseCoreInventoryStorage::GetInstanceIDAtSlot(int32 SlotIndex) const
{
	if (!IsValidSlot(SlotIndex))
	{
		return FGuid();
	}
	return Slots[SlotIndex].InstanceID;
}

int32 USuspenseCoreInventoryStorage::GetAnchorSlot(int32 SlotIndex) const
{
	if (!IsSlotOccupied(SlotIndex))
	{
		return INDEX_NONE;
	}

	const FSuspenseCoreInventorySlot& Slot = Slots[SlotIndex];
	if (Slot.bIsAnchor)
	{
		return SlotIndex;
	}

	// Calculate anchor from offset
	FIntPoint CurrentCoords = SlotToCoords(SlotIndex);
	FIntPoint AnchorCoords = CurrentCoords - Slot.OffsetFromAnchor;
	return CoordsToSlot(AnchorCoords);
}

bool USuspenseCoreInventoryStorage::CanPlaceItem(FIntPoint ItemSize, int32 SlotIndex, bool bRotated, FGuid IgnoreInstanceID) const
{
	if (!bIsInitialized || !IsValidSlot(SlotIndex))
	{
		return false;
	}

	FIntPoint EffectiveSize = GetEffectiveSize(ItemSize, bRotated);
	FIntPoint StartCoords = SlotToCoords(SlotIndex);

	// Check bounds
	if (StartCoords.X + EffectiveSize.X > GridWidth ||
		StartCoords.Y + EffectiveSize.Y > GridHeight)
	{
		return false;
	}

	// Check each cell
	for (int32 Y = 0; Y < EffectiveSize.Y; ++Y)
	{
		for (int32 X = 0; X < EffectiveSize.X; ++X)
		{
			int32 CheckSlot = CoordsToSlot(FIntPoint(StartCoords.X + X, StartCoords.Y + Y));
			if (CheckSlot == INDEX_NONE)
			{
				return false;
			}

			if (IsSlotOccupied(CheckSlot))
			{
				// Check if this is the item we're ignoring (for moves)
				if (IgnoreInstanceID.IsValid() && Slots[CheckSlot].InstanceID == IgnoreInstanceID)
				{
					continue;
				}
				return false;
			}
		}
	}

	return true;
}

bool USuspenseCoreInventoryStorage::PlaceItem(FGuid InstanceID, FIntPoint ItemSize, int32 SlotIndex, bool bRotated)
{
	check(IsInGameThread());

	if (!CanPlaceItem(ItemSize, SlotIndex, bRotated))
	{
		return false;
	}

	FIntPoint EffectiveSize = GetEffectiveSize(ItemSize, bRotated);
	FIntPoint StartCoords = SlotToCoords(SlotIndex);

	for (int32 Y = 0; Y < EffectiveSize.Y; ++Y)
	{
		for (int32 X = 0; X < EffectiveSize.X; ++X)
		{
			int32 CellSlot = CoordsToSlot(FIntPoint(StartCoords.X + X, StartCoords.Y + Y));
			if (CellSlot != INDEX_NONE)
			{
				Slots[CellSlot].InstanceID = InstanceID;
				Slots[CellSlot].bIsAnchor = (X == 0 && Y == 0);
				Slots[CellSlot].OffsetFromAnchor = FIntPoint(X, Y);
				FreeSlotBitmap[CellSlot] = false;
			}
		}
	}

	return true;
}

bool USuspenseCoreInventoryStorage::RemoveItem(FGuid InstanceID)
{
	check(IsInGameThread());

	bool bRemoved = false;

	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (Slots[i].InstanceID == InstanceID)
		{
			Slots[i].Clear();
			FreeSlotBitmap[i] = true;
			bRemoved = true;
		}
	}

	return bRemoved;
}

bool USuspenseCoreInventoryStorage::MoveItem(FGuid InstanceID, FIntPoint ItemSize, int32 NewSlotIndex, bool bRotated)
{
	check(IsInGameThread());

	// Check if can place at new location (ignoring current placement)
	if (!CanPlaceItem(ItemSize, NewSlotIndex, bRotated, InstanceID))
	{
		return false;
	}

	// Remove from old location
	RemoveItem(InstanceID);

	// Place at new location
	return PlaceItem(InstanceID, ItemSize, NewSlotIndex, bRotated);
}

int32 USuspenseCoreInventoryStorage::FindFreeSlot(FIntPoint ItemSize, bool bAllowRotation, bool& OutRotated) const
{
	OutRotated = false;

	// Try normal orientation first
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		if (CanPlaceItem(ItemSize, SlotIndex, false))
		{
			return SlotIndex;
		}
	}

	// Try rotated if allowed
	if (bAllowRotation && ItemSize.X != ItemSize.Y)
	{
		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			if (CanPlaceItem(ItemSize, SlotIndex, true))
			{
				OutRotated = true;
				return SlotIndex;
			}
		}
	}

	return INDEX_NONE;
}

int32 USuspenseCoreInventoryStorage::GetFreeSlotCount() const
{
	int32 Count = 0;
	for (int32 i = 0; i < FreeSlotBitmap.Num(); ++i)
	{
		if (FreeSlotBitmap[i])
		{
			++Count;
		}
	}
	return Count;
}

float USuspenseCoreInventoryStorage::GetFragmentationRatio() const
{
	if (Slots.Num() == 0)
	{
		return 0.0f;
	}

	int32 FreeCount = GetFreeSlotCount();
	if (FreeCount == 0 || FreeCount == Slots.Num())
	{
		return 0.0f;
	}

	// Count transitions between occupied/free
	int32 Transitions = 0;
	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth - 1; ++X)
		{
			int32 Slot1 = CoordsToSlot(FIntPoint(X, Y));
			int32 Slot2 = CoordsToSlot(FIntPoint(X + 1, Y));
			if (FreeSlotBitmap[Slot1] != FreeSlotBitmap[Slot2])
			{
				++Transitions;
			}
		}
	}

	// Normalize by theoretical max transitions
	float MaxTransitions = static_cast<float>(GridWidth - 1) * GridHeight;
	return FMath::Clamp(Transitions / MaxTransitions, 0.0f, 1.0f);
}

FIntPoint USuspenseCoreInventoryStorage::SlotToCoords(int32 SlotIndex) const
{
	if (GridWidth <= 0 || !IsValidSlot(SlotIndex))
	{
		return FIntPoint::NoneValue;
	}
	return FIntPoint(SlotIndex % GridWidth, SlotIndex / GridWidth);
}

int32 USuspenseCoreInventoryStorage::CoordsToSlot(FIntPoint Coords) const
{
	if (!IsValidCoords(Coords))
	{
		return INDEX_NONE;
	}
	return Coords.Y * GridWidth + Coords.X;
}

bool USuspenseCoreInventoryStorage::IsValidCoords(FIntPoint Coords) const
{
	return Coords.X >= 0 && Coords.X < GridWidth &&
		   Coords.Y >= 0 && Coords.Y < GridHeight;
}

TArray<int32> USuspenseCoreInventoryStorage::GetOccupiedSlots(FGuid InstanceID) const
{
	TArray<int32> Result;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (Slots[i].InstanceID == InstanceID)
		{
			Result.Add(i);
		}
	}
	return Result;
}

TArray<int32> USuspenseCoreInventoryStorage::GetAllAnchorSlots() const
{
	TArray<int32> Result;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (!Slots[i].IsEmpty() && Slots[i].bIsAnchor)
		{
			Result.Add(i);
		}
	}
	return Result;
}

FString USuspenseCoreInventoryStorage::GetDebugGridString() const
{
	FString Result;
	Result += FString::Printf(TEXT("Grid %dx%d:\n"), GridWidth, GridHeight);

	for (int32 Y = 0; Y < GridHeight; ++Y)
	{
		for (int32 X = 0; X < GridWidth; ++X)
		{
			int32 SlotIdx = CoordsToSlot(FIntPoint(X, Y));
			if (IsSlotOccupied(SlotIdx))
			{
				Result += Slots[SlotIdx].bIsAnchor ? TEXT("[A]") : TEXT("[X]");
			}
			else
			{
				Result += TEXT("[ ]");
			}
		}
		Result += TEXT("\n");
	}

	return Result;
}

void USuspenseCoreInventoryStorage::LogGridState() const
{
	UE_LOG(LogSuspenseCoreInventory, Log, TEXT("%s"), *GetDebugGridString());
}

FIntPoint USuspenseCoreInventoryStorage::GetEffectiveSize(FIntPoint ItemSize, bool bRotated) const
{
	return bRotated ? FIntPoint(ItemSize.Y, ItemSize.X) : ItemSize;
}

TArray<int32> USuspenseCoreInventoryStorage::CalculateOccupiedSlots(int32 AnchorSlot, FIntPoint ItemSize, bool bRotated) const
{
	TArray<int32> Result;
	FIntPoint EffectiveSize = GetEffectiveSize(ItemSize, bRotated);
	FIntPoint StartCoords = SlotToCoords(AnchorSlot);

	for (int32 Y = 0; Y < EffectiveSize.Y; ++Y)
	{
		for (int32 X = 0; X < EffectiveSize.X; ++X)
		{
			int32 Slot = CoordsToSlot(FIntPoint(StartCoords.X + X, StartCoords.Y + Y));
			if (Slot != INDEX_NONE)
			{
				Result.Add(Slot);
			}
		}
	}

	return Result;
}

void USuspenseCoreInventoryStorage::UpdateFreeBitmap()
{
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		FreeSlotBitmap[i] = Slots[i].IsEmpty();
	}
}
