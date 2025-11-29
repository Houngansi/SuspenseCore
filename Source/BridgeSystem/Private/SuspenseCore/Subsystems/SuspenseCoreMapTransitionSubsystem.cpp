// SuspenseCoreMapTransitionSubsystem.cpp
// SuspenseCore - Clean Architecture BridgeSystem
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Subsystems/SuspenseCoreMapTransitionSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void USuspenseCoreMapTransitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMapTransitionSubsystem: Initialized"));
}

void USuspenseCoreMapTransitionSubsystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMapTransitionSubsystem: Deinitialized"));

	Super::Deinitialize();
}

USuspenseCoreMapTransitionSubsystem* USuspenseCoreMapTransitionSubsystem::Get(const UObject* WorldContextObject)
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

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		return nullptr;
	}

	return GameInstance->GetSubsystem<USuspenseCoreMapTransitionSubsystem>();
}

void USuspenseCoreMapTransitionSubsystem::SetTransitionData(const FSuspenseCoreTransitionData& Data)
{
	TransitionData = Data;
	TransitionData.TransitionTime = FDateTime::Now();

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMapTransitionSubsystem: Transition data set - PlayerId: %s, Target: %s"),
		*TransitionData.PlayerId, *TransitionData.TargetMapName.ToString());

	OnTransitionDataSet.Broadcast(TransitionData);
}

void USuspenseCoreMapTransitionSubsystem::ClearTransitionData()
{
	TransitionData = FSuspenseCoreTransitionData();

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMapTransitionSubsystem: Transition data cleared"));
}

void USuspenseCoreMapTransitionSubsystem::SetCurrentPlayerId(const FString& PlayerId)
{
	TransitionData.PlayerId = PlayerId;

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMapTransitionSubsystem: PlayerId set to %s"), *PlayerId);
}

void USuspenseCoreMapTransitionSubsystem::TransitionToGameMap(const FString& PlayerId, FName GameMapName)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("SuspenseCoreMapTransitionSubsystem: No world for transition"));
		return;
	}

	// Get current map name
	FName CurrentMapName = *World->GetMapName();

	// Setup transition data
	FSuspenseCoreTransitionData Data;
	Data.PlayerId = PlayerId;
	Data.SourceMapName = CurrentMapName;
	Data.TargetMapName = GameMapName;
	Data.TransitionReason = TEXT("PlayGame");

	SetTransitionData(Data);

	// Broadcast transition begin
	OnMapTransitionBegin.Broadcast(CurrentMapName, GameMapName);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMapTransitionSubsystem: Transitioning to game map %s for player %s"),
		*GameMapName.ToString(), *PlayerId);

	// Open the level with player ID in options
	FString Options = FString::Printf(TEXT("?PlayerId=%s"), *PlayerId);
	UGameplayStatics::OpenLevel(World, GameMapName, true, Options);
}

void USuspenseCoreMapTransitionSubsystem::TransitionToMainMenu(FName MainMenuMapName)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("SuspenseCoreMapTransitionSubsystem: No world for transition"));
		return;
	}

	// Get current map name
	FName CurrentMapName = *World->GetMapName();

	// Setup transition data (keep player ID for auto-selection in menu)
	FSuspenseCoreTransitionData Data;
	Data.PlayerId = TransitionData.PlayerId; // Keep the player ID
	Data.SourceMapName = CurrentMapName;
	Data.TargetMapName = MainMenuMapName;
	Data.TransitionReason = TEXT("ReturnToMenu");

	SetTransitionData(Data);

	// Broadcast transition begin
	OnMapTransitionBegin.Broadcast(CurrentMapName, MainMenuMapName);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMapTransitionSubsystem: Transitioning to main menu %s"),
		*MainMenuMapName.ToString());

	// Open the level
	UGameplayStatics::OpenLevel(World, MainMenuMapName);
}
