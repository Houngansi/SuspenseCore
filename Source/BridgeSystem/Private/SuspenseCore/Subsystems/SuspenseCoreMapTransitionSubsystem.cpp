// SuspenseCoreMapTransitionSubsystem.cpp
// SuspenseCore - Clean Architecture BridgeSystem
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Subsystems/SuspenseCoreMapTransitionSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

namespace
{
	/**
	 * Converts a Blueprint SoftObjectPath to a proper class path for ?game= URL parameter.
	 * Input formats:
	 *   - "/Script/Engine.Blueprint'/Game/Path/BP_GameMode.BP_GameMode'" (SoftObjectPath)
	 *   - "/Game/Path/BP_GameMode.BP_GameMode" (partial path)
	 *   - "/Game/Path/BP_GameMode.BP_GameMode_C" (correct format - returned as-is)
	 *
	 * Output format:
	 *   - "/Game/Path/BP_GameMode.BP_GameMode_C" (class path with _C suffix)
	 */
	FString NormalizeGameModeClassPath(const FString& InputPath)
	{
		if (InputPath.IsEmpty())
		{
			return InputPath;
		}

		FString Result = InputPath;

		// Check if it's in SoftObjectPath format: /Script/Engine.Blueprint'/Game/...'
		// Extract the actual path from between quotes
		if (Result.Contains(TEXT("'/")) && Result.EndsWith(TEXT("'")))
		{
			int32 StartIndex = Result.Find(TEXT("'/"));
			if (StartIndex != INDEX_NONE)
			{
				// Extract path between '/ and '
				Result = Result.Mid(StartIndex + 1); // Skip the '
				Result = Result.Left(Result.Len() - 1); // Remove trailing '
				UE_LOG(LogTemp, Log, TEXT("NormalizeGameModeClassPath: Extracted from SoftObjectPath: %s"), *Result);
			}
		}

		// Ensure it ends with _C (class suffix for Blueprints)
		if (!Result.EndsWith(TEXT("_C")))
		{
			Result += TEXT("_C");
			UE_LOG(LogTemp, Log, TEXT("NormalizeGameModeClassPath: Added _C suffix: %s"), *Result);
		}

		return Result;
	}
}

void USuspenseCoreMapTransitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMapTransitionSubsystem: Initialized"));
	UE_LOG(LogTemp, Log, TEXT("  IMPORTANT: Set GameGameModePath and MenuGameModePath for proper GameMode switching!"));
	UE_LOG(LogTemp, Log, TEXT("  Example: /Game/Blueprints/GameModes/BP_SuspenseCoreGameMode.BP_SuspenseCoreGameMode_C"));
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

	// Build options string with PlayerId
	FString Options = FString::Printf(TEXT("?PlayerId=%s"), *PlayerId);

	// CRITICAL: Force GameMode via URL options
	// OpenLevel does NOT respect World Settings GameMode Override!
	// Must pass ?game=/Path/To/GameMode.GameMode_C
	if (!GameGameModePath.IsEmpty())
	{
		FString NormalizedPath = NormalizeGameModeClassPath(GameGameModePath);
		Options += FString::Printf(TEXT("?game=%s"), *NormalizedPath);
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreMapTransitionSubsystem: Forcing GameMode: %s (original: %s)"), *NormalizedPath, *GameGameModePath);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreMapTransitionSubsystem: GameGameModePath not set! GameMode may not switch correctly."));
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMapTransitionSubsystem: Transitioning to game map %s for player %s"),
		*GameMapName.ToString(), *PlayerId);
	UE_LOG(LogTemp, Log, TEXT("  Options: %s"), *Options);

	// Open the level with forced GameMode
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

	// Build options string
	FString Options;

	// CRITICAL: Force MenuGameMode via URL options
	// OpenLevel does NOT respect World Settings GameMode Override!
	// Must pass ?game=/Path/To/GameMode.GameMode_C
	if (!MenuGameModePath.IsEmpty())
	{
		FString NormalizedPath = NormalizeGameModeClassPath(MenuGameModePath);
		Options = FString::Printf(TEXT("?game=%s"), *NormalizedPath);
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreMapTransitionSubsystem: Forcing MenuGameMode: %s (original: %s)"), *NormalizedPath, *MenuGameModePath);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCoreMapTransitionSubsystem: MenuGameModePath not set! GameMode may not switch correctly."));
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCoreMapTransitionSubsystem: Transitioning to main menu %s"),
		*MainMenuMapName.ToString());
	UE_LOG(LogTemp, Log, TEXT("  Options: %s"), *Options);

	// Open the level with forced GameMode
	UGameplayStatics::OpenLevel(World, MainMenuMapName, true, Options);
}
