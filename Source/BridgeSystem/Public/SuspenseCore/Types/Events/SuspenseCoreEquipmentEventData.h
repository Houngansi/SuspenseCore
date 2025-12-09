// Types/Events/SuspenseEquipmentEventData.h
// Copyright Suspense Team. All Rights Reserved.
// SuspenseCore - Clean Architecture Foundation

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"

/**
 * MIGRATION NOTICE:
 *
 * Legacy FSuspenseEquipmentEventBus has been REMOVED.
 * All event handling now uses the Clean Architecture USuspenseCoreEventBus.
 *
 * Migration guide:
 *
 * OLD (FSuspenseEquipmentEventBus):
 *   FSuspenseEquipmentEventData EventData;
 *   EventData.EventType = MyTag;
 *   EventData.Source = this;
 *   FSuspenseEquipmentEventBus::Get()->Broadcast(EventData);
 *
 * NEW (USuspenseCoreEventBus):
 *   FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
 *   EventData.SetString(NAME_Payload, TEXT("MyValue"));
 *   EventBus->Publish(MyTag, EventData);
 *
 * Key changes:
 * - FSuspenseEquipmentEventData -> FSuspenseCoreEventData
 * - FSuspenseEquipmentEventBus::Get() -> Get EventBus via ServiceLocator or EventManager
 * - Broadcast() -> Publish()
 * - EventData.Payload (FString) -> EventData.StringPayload (TMap<FName, FString>)
 * - EventData.Metadata (TMap<FString,FString>) -> Use typed payload maps
 *
 * Include this header for backward compatibility type alias:
 */

// Backward compatibility type alias
using FSuspenseEquipmentEventData = FSuspenseCoreEventData;

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
