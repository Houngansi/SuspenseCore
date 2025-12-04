// SuspenseCoreInteractionSettings.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Utils/SuspenseCoreInteractionSettings.h"
#include "SuspenseCore/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

USuspenseCoreInteractionSettings::USuspenseCoreInteractionSettings()
	: DefaultTraceDistance(300.0f)
	, DefaultTraceChannel(ECC_Visibility)
	, TraceSphereRadius(0.0f)
	, DefaultInteractionCooldown(0.5f)
	, FocusUpdateRate(10.0f)
	, bBroadcastFocusEvents(true)
	, bEnableDebugDraw(false)
	, DebugLineDuration(0.1f)
	, bEnableVerboseLogging(false)
{
}

const USuspenseCoreInteractionSettings* USuspenseCoreInteractionSettings::Get()
{
	return GetDefault<USuspenseCoreInteractionSettings>();
}

USuspenseCoreInteractionSettings* USuspenseCoreInteractionSettings::GetMutable()
{
	return GetMutableDefault<USuspenseCoreInteractionSettings>();
}

#if WITH_EDITOR
void USuspenseCoreInteractionSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Get the name of the changed property
	FName PropertyName = PropertyChangedEvent.Property
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;

	if (PropertyName != NAME_None)
	{
		// Broadcast change through EventBus for hot-reload support
		BroadcastSettingsChanged(PropertyName);

		UE_LOG(LogTemp, Log, TEXT("SuspenseCoreInteractionSettings: Property '%s' changed"),
			*PropertyName.ToString());
	}
}
#endif

void USuspenseCoreInteractionSettings::BroadcastSettingsChanged(FName ChangedPropertyName)
{
	USuspenseCoreEventBus* EventBus = GetEventBus();
	if (!EventBus)
	{
		return;
	}

	// Create event data with settings information
	FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(
		const_cast<USuspenseCoreInteractionSettings*>(this),
		ESuspenseCoreEventPriority::High
	);

	// Add property name
	EventData.SetString(TEXT("PropertyName"), ChangedPropertyName.ToString());

	// Add current values for common properties
	EventData.SetFloat(TEXT("TraceDistance"), DefaultTraceDistance);
	EventData.SetFloat(TEXT("TraceSphereRadius"), TraceSphereRadius);
	EventData.SetFloat(TEXT("InteractionCooldown"), DefaultInteractionCooldown);
	EventData.SetFloat(TEXT("FocusUpdateRate"), FocusUpdateRate);
	EventData.SetBool(TEXT("BroadcastFocusEvents"), bBroadcastFocusEvents);
	EventData.SetBool(TEXT("DebugDraw"), bEnableDebugDraw);
	EventData.SetBool(TEXT("VerboseLogging"), bEnableVerboseLogging);

	// Broadcast using static tag
	static const FGameplayTag SettingsChangedTag =
		FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Settings.InteractionChanged"));

	EventBus->Publish(SettingsChangedTag, EventData);
}

USuspenseCoreEventBus* USuspenseCoreInteractionSettings::GetEventBus() const
{
	// Check cached reference first
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	// Try to get from GameInstance
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(GEngine, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	USuspenseCoreEventBus* EventBus = GameInstance->GetSubsystem<USuspenseCoreEventBus>();
	if (EventBus)
	{
		CachedEventBus = EventBus;
	}

	return EventBus;
}
