// SuspenseCoreFileSaveRepository.cpp
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Save/SuspenseCoreFileSaveRepository.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonReader.h"
#include "Async/Async.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreSave, Log, All);

USuspenseCoreFileSaveRepository::USuspenseCoreFileSaveRepository()
{
	// Default path will be set in Initialize
}

void USuspenseCoreFileSaveRepository::Initialize(const FString& InBasePath)
{
	if (InBasePath.IsEmpty())
	{
		BasePath = FPaths::ProjectSavedDir() / TEXT("SaveGames");
	}
	else
	{
		BasePath = InBasePath;
	}

	// Ensure base directory exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*BasePath))
	{
		PlatformFile.CreateDirectoryTree(*BasePath);
	}

	UE_LOG(LogSuspenseCoreSave, Log, TEXT("FileSaveRepository initialized at: %s"), *BasePath);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SYNC OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════════

ESuspenseCoreSaveResult USuspenseCoreFileSaveRepository::SaveToSlot(
	const FString& PlayerId,
	int32 SlotIndex,
	const FSuspenseCoreSaveData& SaveData)
{
	FScopeLock Lock(&RepositoryLock);

	if (!EnsurePlayerDirectory(PlayerId))
	{
		return ESuspenseCoreSaveResult::PermissionDenied;
	}

	// Serialize to JSON
	FString JsonString;
	if (!SerializeToJson(SaveData, JsonString))
	{
		UE_LOG(LogSuspenseCoreSave, Error, TEXT("Failed to serialize save data for slot %d"), SlotIndex);
		return ESuspenseCoreSaveResult::Failed;
	}

	// Write to file
	FString FilePath = GetSlotFilePath(PlayerId, SlotIndex);
	if (!FFileHelper::SaveStringToFile(JsonString, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8))
	{
		UE_LOG(LogSuspenseCoreSave, Error, TEXT("Failed to write save file: %s"), *FilePath);
		return ESuspenseCoreSaveResult::DiskFull;
	}

	// Update cache
	UpdateHeaderCache(PlayerId, SlotIndex, SaveData.Header);

	UE_LOG(LogSuspenseCoreSave, Log, TEXT("Saved to slot %d for player %s"), SlotIndex, *PlayerId);
	return ESuspenseCoreSaveResult::Success;
}

ESuspenseCoreSaveResult USuspenseCoreFileSaveRepository::LoadFromSlot(
	const FString& PlayerId,
	int32 SlotIndex,
	FSuspenseCoreSaveData& OutSaveData)
{
	FScopeLock Lock(&RepositoryLock);

	FString FilePath = GetSlotFilePath(PlayerId, SlotIndex);

	// Check if file exists
	if (!FPaths::FileExists(FilePath))
	{
		UE_LOG(LogSuspenseCoreSave, Warning, TEXT("Save slot not found: %s"), *FilePath);
		return ESuspenseCoreSaveResult::SlotNotFound;
	}

	// Read file
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogSuspenseCoreSave, Error, TEXT("Failed to read save file: %s"), *FilePath);
		return ESuspenseCoreSaveResult::Failed;
	}

	// Deserialize
	if (!DeserializeFromJson(JsonString, OutSaveData))
	{
		UE_LOG(LogSuspenseCoreSave, Error, TEXT("Failed to deserialize save data from slot %d"), SlotIndex);
		return ESuspenseCoreSaveResult::CorruptedData;
	}

	// Check version
	if (OutSaveData.Header.SaveVersion > FSuspenseCoreSaveData::CURRENT_VERSION)
	{
		UE_LOG(LogSuspenseCoreSave, Warning, TEXT("Save version mismatch: file=%d, current=%d"),
			OutSaveData.Header.SaveVersion, FSuspenseCoreSaveData::CURRENT_VERSION);
		return ESuspenseCoreSaveResult::VersionMismatch;
	}

	UE_LOG(LogSuspenseCoreSave, Log, TEXT("Loaded from slot %d for player %s"), SlotIndex, *PlayerId);
	return ESuspenseCoreSaveResult::Success;
}

ESuspenseCoreSaveResult USuspenseCoreFileSaveRepository::DeleteSlot(
	const FString& PlayerId,
	int32 SlotIndex)
{
	FScopeLock Lock(&RepositoryLock);

	FString FilePath = GetSlotFilePath(PlayerId, SlotIndex);

	if (!FPaths::FileExists(FilePath))
	{
		return ESuspenseCoreSaveResult::SlotNotFound;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DeleteFile(*FilePath))
	{
		UE_LOG(LogSuspenseCoreSave, Error, TEXT("Failed to delete save file: %s"), *FilePath);
		return ESuspenseCoreSaveResult::Failed;
	}

	// Remove from cache
	RemoveFromHeaderCache(PlayerId, SlotIndex);

	UE_LOG(LogSuspenseCoreSave, Log, TEXT("Deleted slot %d for player %s"), SlotIndex, *PlayerId);
	return ESuspenseCoreSaveResult::Success;
}

bool USuspenseCoreFileSaveRepository::SlotExists(
	const FString& PlayerId,
	int32 SlotIndex)
{
	FString FilePath = GetSlotFilePath(PlayerId, SlotIndex);
	return FPaths::FileExists(FilePath);
}

// ═══════════════════════════════════════════════════════════════════════════════
// METADATA
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreFileSaveRepository::GetSaveHeaders(
	const FString& PlayerId,
	TArray<FSuspenseCoreSaveHeader>& OutHeaders)
{
	OutHeaders.Empty();

	FString PlayerDir = GetPlayerDirectory(PlayerId);

	// Check for regular slots
	for (int32 i = 0; i < MaxSaveSlots; ++i)
	{
		FSuspenseCoreSaveHeader Header;
		if (GetSlotHeader(PlayerId, i, Header))
		{
			Header.SlotIndex = i;
			OutHeaders.Add(Header);
		}
		else
		{
			// Add empty slot placeholder
			Header.SlotIndex = i;
			Header.SlotName = TEXT("");
			OutHeaders.Add(Header);
		}
	}

	// Check auto-save
	FSuspenseCoreSaveHeader AutoSaveHeader;
	if (GetSlotHeader(PlayerId, AUTOSAVE_SLOT, AutoSaveHeader))
	{
		AutoSaveHeader.SlotIndex = AUTOSAVE_SLOT;
		AutoSaveHeader.bIsAutoSave = true;
		OutHeaders.Add(AutoSaveHeader);
	}

	// Check quick-save
	FSuspenseCoreSaveHeader QuickSaveHeader;
	if (GetSlotHeader(PlayerId, QUICKSAVE_SLOT, QuickSaveHeader))
	{
		QuickSaveHeader.SlotIndex = QUICKSAVE_SLOT;
		OutHeaders.Add(QuickSaveHeader);
	}
}

bool USuspenseCoreFileSaveRepository::GetSlotHeader(
	const FString& PlayerId,
	int32 SlotIndex,
	FSuspenseCoreSaveHeader& OutHeader)
{
	// Check cache first
	if (TMap<int32, FSuspenseCoreSaveHeader>* PlayerCache = HeaderCache.Find(PlayerId))
	{
		if (FSuspenseCoreSaveHeader* CachedHeader = PlayerCache->Find(SlotIndex))
		{
			OutHeader = *CachedHeader;
			return true;
		}
	}

	// Load from file
	FSuspenseCoreSaveData SaveData;
	ESuspenseCoreSaveResult Result = LoadFromSlot(PlayerId, SlotIndex, SaveData);
	if (Result == ESuspenseCoreSaveResult::Success)
	{
		OutHeader = SaveData.Header;
		UpdateHeaderCache(PlayerId, SlotIndex, OutHeader);
		return true;
	}

	return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ASYNC OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════════

void USuspenseCoreFileSaveRepository::SaveToSlotAsync(
	const FString& PlayerId,
	int32 SlotIndex,
	const FSuspenseCoreSaveData& SaveData,
	FOnSuspenseCoreSaveComplete OnComplete)
{
	// Capture data for async task
	TWeakObjectPtr<USuspenseCoreFileSaveRepository> WeakThis(this);
	FSuspenseCoreSaveData SaveDataCopy = SaveData;
	FString PlayerIdCopy = PlayerId;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakThis, PlayerIdCopy, SlotIndex, SaveDataCopy, OnComplete]()
	{
		ESuspenseCoreSaveResult Result = ESuspenseCoreSaveResult::Failed;
		FString ErrorMessage;

		if (USuspenseCoreFileSaveRepository* This = WeakThis.Get())
		{
			Result = This->SaveToSlot(PlayerIdCopy, SlotIndex, SaveDataCopy);
			if (Result != ESuspenseCoreSaveResult::Success)
			{
				ErrorMessage = TEXT("Save operation failed");
			}
		}
		else
		{
			ErrorMessage = TEXT("Repository was destroyed");
		}

		// Callback on game thread
		AsyncTask(ENamedThreads::GameThread, [OnComplete, Result, ErrorMessage]()
		{
			OnComplete.ExecuteIfBound(Result, ErrorMessage);
		});
	});
}

void USuspenseCoreFileSaveRepository::LoadFromSlotAsync(
	const FString& PlayerId,
	int32 SlotIndex,
	FOnSuspenseCoreLoadComplete OnComplete)
{
	TWeakObjectPtr<USuspenseCoreFileSaveRepository> WeakThis(this);
	FString PlayerIdCopy = PlayerId;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakThis, PlayerIdCopy, SlotIndex, OnComplete]()
	{
		ESuspenseCoreSaveResult Result = ESuspenseCoreSaveResult::Failed;
		FSuspenseCoreSaveData LoadedData;
		FString ErrorMessage;

		if (USuspenseCoreFileSaveRepository* This = WeakThis.Get())
		{
			Result = This->LoadFromSlot(PlayerIdCopy, SlotIndex, LoadedData);
			if (Result != ESuspenseCoreSaveResult::Success)
			{
				ErrorMessage = TEXT("Load operation failed");
			}
		}
		else
		{
			ErrorMessage = TEXT("Repository was destroyed");
		}

		// Callback on game thread
		AsyncTask(ENamedThreads::GameThread, [OnComplete, Result, LoadedData, ErrorMessage]()
		{
			OnComplete.ExecuteIfBound(Result, LoadedData, ErrorMessage);
		});
	});
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL
// ═══════════════════════════════════════════════════════════════════════════════

FString USuspenseCoreFileSaveRepository::GetSlotFilePath(const FString& PlayerId, int32 SlotIndex) const
{
	FString FileName;
	if (SlotIndex == AUTOSAVE_SLOT)
	{
		FileName = TEXT("AutoSave.sav");
	}
	else if (SlotIndex == QUICKSAVE_SLOT)
	{
		FileName = TEXT("QuickSave.sav");
	}
	else
	{
		FileName = FString::Printf(TEXT("Slot_%d.sav"), SlotIndex);
	}

	return GetPlayerDirectory(PlayerId) / FileName;
}

FString USuspenseCoreFileSaveRepository::GetPlayerDirectory(const FString& PlayerId) const
{
	return BasePath / PlayerId;
}

bool USuspenseCoreFileSaveRepository::SerializeToJson(const FSuspenseCoreSaveData& Data, FString& OutJson) const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject());

	// Header
	TSharedPtr<FJsonObject> HeaderObj = MakeShareable(new FJsonObject());
	HeaderObj->SetNumberField(TEXT("SaveVersion"), Data.Header.SaveVersion);
	HeaderObj->SetStringField(TEXT("SaveTimestamp"), Data.Header.SaveTimestamp.ToString());
	HeaderObj->SetNumberField(TEXT("TotalPlayTimeSeconds"), Data.Header.TotalPlayTimeSeconds);
	HeaderObj->SetStringField(TEXT("SlotName"), Data.Header.SlotName);
	HeaderObj->SetStringField(TEXT("Description"), Data.Header.Description);
	HeaderObj->SetStringField(TEXT("CharacterName"), Data.Header.CharacterName);
	HeaderObj->SetNumberField(TEXT("CharacterLevel"), Data.Header.CharacterLevel);
	HeaderObj->SetStringField(TEXT("LocationName"), Data.Header.LocationName);
	HeaderObj->SetBoolField(TEXT("bIsAutoSave"), Data.Header.bIsAutoSave);
	JsonObject->SetObjectField(TEXT("Header"), HeaderObj);

	// Profile Data
	TSharedPtr<FJsonObject> ProfileObj = MakeShareable(new FJsonObject());
	ProfileObj->SetStringField(TEXT("PlayerId"), Data.ProfileData.PlayerId);
	ProfileObj->SetStringField(TEXT("DisplayName"), Data.ProfileData.DisplayName);
	ProfileObj->SetNumberField(TEXT("Level"), Data.ProfileData.Level);
	ProfileObj->SetNumberField(TEXT("ExperiencePoints"), Data.ProfileData.ExperiencePoints);
	ProfileObj->SetNumberField(TEXT("SoftCurrency"), Data.ProfileData.SoftCurrency);
	ProfileObj->SetNumberField(TEXT("HardCurrency"), Data.ProfileData.HardCurrency);
	ProfileObj->SetStringField(TEXT("CreatedAt"), Data.ProfileData.CreatedAt.ToString());
	ProfileObj->SetStringField(TEXT("LastLoginAt"), Data.ProfileData.LastLoginAt.ToString());
	JsonObject->SetObjectField(TEXT("ProfileData"), ProfileObj);

	// Character State
	TSharedPtr<FJsonObject> CharStateObj = MakeShareable(new FJsonObject());
	CharStateObj->SetNumberField(TEXT("CurrentHealth"), Data.CharacterState.CurrentHealth);
	CharStateObj->SetNumberField(TEXT("MaxHealth"), Data.CharacterState.MaxHealth);
	CharStateObj->SetNumberField(TEXT("CurrentStamina"), Data.CharacterState.CurrentStamina);
	CharStateObj->SetNumberField(TEXT("CurrentMana"), Data.CharacterState.CurrentMana);

	// Position
	TSharedPtr<FJsonObject> PosObj = MakeShareable(new FJsonObject());
	PosObj->SetNumberField(TEXT("X"), Data.CharacterState.WorldPosition.X);
	PosObj->SetNumberField(TEXT("Y"), Data.CharacterState.WorldPosition.Y);
	PosObj->SetNumberField(TEXT("Z"), Data.CharacterState.WorldPosition.Z);
	CharStateObj->SetObjectField(TEXT("WorldPosition"), PosObj);

	TSharedPtr<FJsonObject> RotObj = MakeShareable(new FJsonObject());
	RotObj->SetNumberField(TEXT("Pitch"), Data.CharacterState.WorldRotation.Pitch);
	RotObj->SetNumberField(TEXT("Yaw"), Data.CharacterState.WorldRotation.Yaw);
	RotObj->SetNumberField(TEXT("Roll"), Data.CharacterState.WorldRotation.Roll);
	CharStateObj->SetObjectField(TEXT("WorldRotation"), RotObj);

	CharStateObj->SetStringField(TEXT("CurrentMapName"), Data.CharacterState.CurrentMapName.ToString());
	CharStateObj->SetBoolField(TEXT("bIsDead"), Data.CharacterState.bIsDead);
	CharStateObj->SetBoolField(TEXT("bIsInCombat"), Data.CharacterState.bIsInCombat);
	JsonObject->SetObjectField(TEXT("CharacterState"), CharStateObj);

	// Serialize to string
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
	{
		return false;
	}

	return true;
}

bool USuspenseCoreFileSaveRepository::DeserializeFromJson(const FString& Json, FSuspenseCoreSaveData& OutData) const
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}

	// Header
	if (TSharedPtr<FJsonObject> HeaderObj = JsonObject->GetObjectField(TEXT("Header")))
	{
		OutData.Header.SaveVersion = HeaderObj->GetIntegerField(TEXT("SaveVersion"));
		FDateTime::Parse(HeaderObj->GetStringField(TEXT("SaveTimestamp")), OutData.Header.SaveTimestamp);
		OutData.Header.TotalPlayTimeSeconds = HeaderObj->GetNumberField(TEXT("TotalPlayTimeSeconds"));
		OutData.Header.SlotName = HeaderObj->GetStringField(TEXT("SlotName"));
		OutData.Header.Description = HeaderObj->GetStringField(TEXT("Description"));
		OutData.Header.CharacterName = HeaderObj->GetStringField(TEXT("CharacterName"));
		OutData.Header.CharacterLevel = HeaderObj->GetIntegerField(TEXT("CharacterLevel"));
		OutData.Header.LocationName = HeaderObj->GetStringField(TEXT("LocationName"));
		OutData.Header.bIsAutoSave = HeaderObj->GetBoolField(TEXT("bIsAutoSave"));
	}

	// Profile Data
	if (TSharedPtr<FJsonObject> ProfileObj = JsonObject->GetObjectField(TEXT("ProfileData")))
	{
		OutData.ProfileData.PlayerId = ProfileObj->GetStringField(TEXT("PlayerId"));
		OutData.ProfileData.DisplayName = ProfileObj->GetStringField(TEXT("DisplayName"));
		OutData.ProfileData.Level = ProfileObj->GetIntegerField(TEXT("Level"));
		OutData.ProfileData.ExperiencePoints = ProfileObj->GetNumberField(TEXT("ExperiencePoints"));
		OutData.ProfileData.SoftCurrency = ProfileObj->GetNumberField(TEXT("SoftCurrency"));
		OutData.ProfileData.HardCurrency = ProfileObj->GetNumberField(TEXT("HardCurrency"));
		FDateTime::Parse(ProfileObj->GetStringField(TEXT("CreatedAt")), OutData.ProfileData.CreatedAt);
		FDateTime::Parse(ProfileObj->GetStringField(TEXT("LastLoginAt")), OutData.ProfileData.LastLoginAt);
	}

	// Character State
	if (TSharedPtr<FJsonObject> CharStateObj = JsonObject->GetObjectField(TEXT("CharacterState")))
	{
		OutData.CharacterState.CurrentHealth = CharStateObj->GetNumberField(TEXT("CurrentHealth"));
		OutData.CharacterState.MaxHealth = CharStateObj->GetNumberField(TEXT("MaxHealth"));
		OutData.CharacterState.CurrentStamina = CharStateObj->GetNumberField(TEXT("CurrentStamina"));
		OutData.CharacterState.CurrentMana = CharStateObj->GetNumberField(TEXT("CurrentMana"));

		if (TSharedPtr<FJsonObject> PosObj = CharStateObj->GetObjectField(TEXT("WorldPosition")))
		{
			OutData.CharacterState.WorldPosition.X = PosObj->GetNumberField(TEXT("X"));
			OutData.CharacterState.WorldPosition.Y = PosObj->GetNumberField(TEXT("Y"));
			OutData.CharacterState.WorldPosition.Z = PosObj->GetNumberField(TEXT("Z"));
		}

		if (TSharedPtr<FJsonObject> RotObj = CharStateObj->GetObjectField(TEXT("WorldRotation")))
		{
			OutData.CharacterState.WorldRotation.Pitch = RotObj->GetNumberField(TEXT("Pitch"));
			OutData.CharacterState.WorldRotation.Yaw = RotObj->GetNumberField(TEXT("Yaw"));
			OutData.CharacterState.WorldRotation.Roll = RotObj->GetNumberField(TEXT("Roll"));
		}

		OutData.CharacterState.CurrentMapName = FName(*CharStateObj->GetStringField(TEXT("CurrentMapName")));
		OutData.CharacterState.bIsDead = CharStateObj->GetBoolField(TEXT("bIsDead"));
		OutData.CharacterState.bIsInCombat = CharStateObj->GetBoolField(TEXT("bIsInCombat"));
	}

	return true;
}

bool USuspenseCoreFileSaveRepository::EnsurePlayerDirectory(const FString& PlayerId) const
{
	FString PlayerDir = GetPlayerDirectory(PlayerId);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (!PlatformFile.DirectoryExists(*PlayerDir))
	{
		return PlatformFile.CreateDirectoryTree(*PlayerDir);
	}

	return true;
}

void USuspenseCoreFileSaveRepository::UpdateHeaderCache(const FString& PlayerId, int32 SlotIndex, const FSuspenseCoreSaveHeader& Header)
{
	if (!HeaderCache.Contains(PlayerId))
	{
		HeaderCache.Add(PlayerId, TMap<int32, FSuspenseCoreSaveHeader>());
	}
	HeaderCache[PlayerId].Add(SlotIndex, Header);
}

void USuspenseCoreFileSaveRepository::RemoveFromHeaderCache(const FString& PlayerId, int32 SlotIndex)
{
	if (TMap<int32, FSuspenseCoreSaveHeader>* PlayerCache = HeaderCache.Find(PlayerId))
	{
		PlayerCache->Remove(SlotIndex);
	}
}
