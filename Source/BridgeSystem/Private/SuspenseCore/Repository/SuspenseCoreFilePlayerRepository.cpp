// SuspenseCoreFilePlayerRepository.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Repository/SuspenseCoreFilePlayerRepository.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "JsonObjectConverter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCorePlayerRepository, Log, All);

USuspenseCoreFilePlayerRepository::USuspenseCoreFilePlayerRepository()
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreFilePlayerRepository::Initialize(const FString& InBasePath)
{
	if (InBasePath.IsEmpty())
	{
		// Default path: [Project]/Saved/Players/
		BasePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Players"));
	}
	else
	{
		BasePath = InBasePath;
	}

	// Create directory if it doesn't exist
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*BasePath))
	{
		PlatformFile.CreateDirectoryTree(*BasePath);
		UE_LOG(LogSuspenseCorePlayerRepository, Log, TEXT("Created player data directory: %s"), *BasePath);
	}

	UE_LOG(LogSuspenseCorePlayerRepository, Log, TEXT("FilePlayerRepository initialized. Path: %s"), *BasePath);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CRUD
// ═══════════════════════════════════════════════════════════════════════════════

bool USuspenseCoreFilePlayerRepository::LoadPlayer(const FString& PlayerId, FSuspenseCorePlayerData& OutPlayerData)
{
	if (PlayerId.IsEmpty())
	{
		UE_LOG(LogSuspenseCorePlayerRepository, Warning, TEXT("LoadPlayer: Empty PlayerId"));
		return false;
	}

	return LoadPlayerInternal(PlayerId, OutPlayerData);
}

bool USuspenseCoreFilePlayerRepository::SavePlayer(const FSuspenseCorePlayerData& PlayerData)
{
	if (!PlayerData.IsValid())
	{
		UE_LOG(LogSuspenseCorePlayerRepository, Warning, TEXT("SavePlayer: Invalid PlayerData"));
		return false;
	}

	return SavePlayerInternal(PlayerData);
}

bool USuspenseCoreFilePlayerRepository::DeletePlayer(const FString& PlayerId)
{
	if (PlayerId.IsEmpty())
	{
		return false;
	}

	FScopeLock Lock(&RepositoryLock);

	// Remove from cache
	PlayerCache.Remove(PlayerId);

	// Delete file
	FString FilePath = GetPlayerFilePath(PlayerId);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (PlatformFile.FileExists(*FilePath))
	{
		if (PlatformFile.DeleteFile(*FilePath))
		{
			UE_LOG(LogSuspenseCorePlayerRepository, Log, TEXT("Deleted player: %s"), *PlayerId);
			return true;
		}
		else
		{
			UE_LOG(LogSuspenseCorePlayerRepository, Error, TEXT("Failed to delete player file: %s"), *FilePath);
			return false;
		}
	}

	return true; // File didn't exist - consider success
}

bool USuspenseCoreFilePlayerRepository::PlayerExists(const FString& PlayerId)
{
	if (PlayerId.IsEmpty())
	{
		return false;
	}

	FScopeLock Lock(&RepositoryLock);

	// Check cache
	if (PlayerCache.Contains(PlayerId))
	{
		return true;
	}

	// Check file
	FString FilePath = GetPlayerFilePath(PlayerId);
	return FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath);
}

bool USuspenseCoreFilePlayerRepository::CreatePlayer(const FString& DisplayName, FSuspenseCorePlayerData& OutPlayerData)
{
	OutPlayerData = FSuspenseCorePlayerData::CreateNew(DisplayName);

	if (SavePlayerInternal(OutPlayerData))
	{
		UE_LOG(LogSuspenseCorePlayerRepository, Log, TEXT("Created new player: %s (%s)"),
			*DisplayName, *OutPlayerData.PlayerId);
		return true;
	}

	return false;
}

void USuspenseCoreFilePlayerRepository::GetAllPlayerIds(TArray<FString>& OutPlayerIds)
{
	OutPlayerIds.Empty();

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Find all .json files in directory
	TArray<FString> FoundFiles;
	PlatformFile.FindFiles(FoundFiles, *BasePath, TEXT(".json"));

	for (const FString& FilePath : FoundFiles)
	{
		FString FileName = FPaths::GetBaseFilename(FilePath);
		OutPlayerIds.Add(FileName);
	}

	UE_LOG(LogSuspenseCorePlayerRepository, Verbose, TEXT("Found %d players"), OutPlayerIds.Num());
}

void USuspenseCoreFilePlayerRepository::GetLeaderboard(const FString& Category, int32 Count, TArray<FSuspenseCorePlayerData>& OutLeaderboard)
{
	OutLeaderboard.Empty();

	// Load all players
	TArray<FString> PlayerIds;
	GetAllPlayerIds(PlayerIds);

	TArray<FSuspenseCorePlayerData> AllPlayers;
	for (const FString& PlayerId : PlayerIds)
	{
		FSuspenseCorePlayerData Data;
		if (LoadPlayerInternal(PlayerId, Data))
		{
			AllPlayers.Add(Data);
		}
	}

	// Sort by category
	if (Category == TEXT("Kills"))
	{
		AllPlayers.Sort([](const FSuspenseCorePlayerData& A, const FSuspenseCorePlayerData& B)
		{
			return A.Stats.Kills > B.Stats.Kills;
		});
	}
	else if (Category == TEXT("KD"))
	{
		AllPlayers.Sort([](const FSuspenseCorePlayerData& A, const FSuspenseCorePlayerData& B)
		{
			return A.Stats.GetKDRatio() > B.Stats.GetKDRatio();
		});
	}
	else if (Category == TEXT("Wins"))
	{
		AllPlayers.Sort([](const FSuspenseCorePlayerData& A, const FSuspenseCorePlayerData& B)
		{
			return A.Stats.Wins > B.Stats.Wins;
		});
	}
	else if (Category == TEXT("Level"))
	{
		AllPlayers.Sort([](const FSuspenseCorePlayerData& A, const FSuspenseCorePlayerData& B)
		{
			return A.Level > B.Level;
		});
	}
	else if (Category == TEXT("PlayTime"))
	{
		AllPlayers.Sort([](const FSuspenseCorePlayerData& A, const FSuspenseCorePlayerData& B)
		{
			return A.Stats.PlayTimeSeconds > B.Stats.PlayTimeSeconds;
		});
	}

	// Take top N
	int32 ResultCount = FMath::Min(Count, AllPlayers.Num());
	for (int32 i = 0; i < ResultCount; ++i)
	{
		OutLeaderboard.Add(AllPlayers[i]);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// PARTIAL UPDATES
// ═══════════════════════════════════════════════════════════════════════════════

bool USuspenseCoreFilePlayerRepository::UpdateStats(const FString& PlayerId, int32 Kills, int32 Deaths, int32 Assists)
{
	FSuspenseCorePlayerData Data;
	if (!LoadPlayerInternal(PlayerId, Data))
	{
		return false;
	}

	Data.Stats.Kills = Kills;
	Data.Stats.Deaths = Deaths;
	Data.Stats.Assists = Assists;

	return SavePlayerInternal(Data);
}

bool USuspenseCoreFilePlayerRepository::UpdateCurrency(const FString& PlayerId, int64 SoftCurrency, int64 HardCurrency)
{
	FSuspenseCorePlayerData Data;
	if (!LoadPlayerInternal(PlayerId, Data))
	{
		return false;
	}

	Data.SoftCurrency = SoftCurrency;
	Data.HardCurrency = HardCurrency;

	return SavePlayerInternal(Data);
}

bool USuspenseCoreFilePlayerRepository::UpdateProgress(const FString& PlayerId, int64 XP, int32 Level)
{
	FSuspenseCorePlayerData Data;
	if (!LoadPlayerInternal(PlayerId, Data))
	{
		return false;
	}

	Data.ExperiencePoints = XP;
	Data.Level = Level;

	return SavePlayerInternal(Data);
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL
// ═══════════════════════════════════════════════════════════════════════════════

FString USuspenseCoreFilePlayerRepository::GetPlayerFilePath(const FString& PlayerId) const
{
	return FPaths::Combine(BasePath, PlayerId + TEXT(".json"));
}

bool USuspenseCoreFilePlayerRepository::SerializeToJson(const FSuspenseCorePlayerData& Data, FString& OutJson) const
{
	TSharedPtr<FJsonObject> JsonObject = FJsonObjectConverter::UStructToJsonObject(Data);
	if (!JsonObject.IsValid())
	{
		UE_LOG(LogSuspenseCorePlayerRepository, Error, TEXT("Failed to convert PlayerData to JSON object"));
		return false;
	}

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
	{
		UE_LOG(LogSuspenseCorePlayerRepository, Error, TEXT("Failed to serialize JSON"));
		return false;
	}

	return true;
}

bool USuspenseCoreFilePlayerRepository::DeserializeFromJson(const FString& Json, FSuspenseCorePlayerData& OutData) const
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogSuspenseCorePlayerRepository, Error, TEXT("Failed to parse JSON"));
		return false;
	}

	if (!FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), &OutData))
	{
		UE_LOG(LogSuspenseCorePlayerRepository, Error, TEXT("Failed to convert JSON to PlayerData"));
		return false;
	}

	return true;
}

bool USuspenseCoreFilePlayerRepository::LoadPlayerInternal(const FString& PlayerId, FSuspenseCorePlayerData& OutData)
{
	FScopeLock Lock(&RepositoryLock);

	// Check cache
	if (FSuspenseCorePlayerData* Cached = PlayerCache.Find(PlayerId))
	{
		OutData = *Cached;
		return true;
	}

	// Load from file
	FString FilePath = GetPlayerFilePath(PlayerId);
	FString JsonContent;

	if (!FFileHelper::LoadFileToString(JsonContent, *FilePath))
	{
		UE_LOG(LogSuspenseCorePlayerRepository, Warning, TEXT("Player file not found: %s"), *FilePath);
		return false;
	}

	if (!DeserializeFromJson(JsonContent, OutData))
	{
		return false;
	}

	// Add to cache
	PlayerCache.Add(PlayerId, OutData);

	UE_LOG(LogSuspenseCorePlayerRepository, Verbose, TEXT("Loaded player: %s"), *PlayerId);
	return true;
}

bool USuspenseCoreFilePlayerRepository::SavePlayerInternal(const FSuspenseCorePlayerData& Data)
{
	FScopeLock Lock(&RepositoryLock);

	FString JsonContent;
	if (!SerializeToJson(Data, JsonContent))
	{
		return false;
	}

	FString FilePath = GetPlayerFilePath(Data.PlayerId);

	if (!FFileHelper::SaveStringToFile(JsonContent, *FilePath))
	{
		UE_LOG(LogSuspenseCorePlayerRepository, Error, TEXT("Failed to save player file: %s"), *FilePath);
		return false;
	}

	// Update cache
	PlayerCache.Add(Data.PlayerId, Data);

	UE_LOG(LogSuspenseCorePlayerRepository, Verbose, TEXT("Saved player: %s"), *Data.PlayerId);
	return true;
}
