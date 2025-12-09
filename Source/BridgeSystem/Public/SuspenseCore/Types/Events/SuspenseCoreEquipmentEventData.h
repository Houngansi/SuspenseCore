// Types/Events/SuspenseEquipmentEventData.h
// Copyright Suspense Team. All Rights Reserved.
// SuspenseCore - Clean Architecture Foundation

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"

// Common payload keys for Equipment events
namespace SuspenseEquipmentPayloadKeys
{
	const FName ItemId = TEXT("ItemId");
	const FName SlotIndex = TEXT("SlotIndex");
	const FName SocketName = TEXT("SocketName");
	const FName ActorRef = TEXT("ActorRef");
	const FName PreviousState = TEXT("PreviousState");
	const FName NewState = TEXT("NewState");
	const FName Payload = TEXT("Payload");
	const FName Target = TEXT("Target");
}
