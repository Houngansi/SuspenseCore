// SuspenseCorePlayerData.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCorePlayerData.generated.h"

// ═══════════════════════════════════════════════════════════════════════════════
// PLAYER STATS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCorePlayerStats
 *
 * Статистика игрока.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCorePlayerStats
{
	GENERATED_BODY()

	/** Убийства */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	int32 Kills = 0;

	/** Смерти */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	int32 Deaths = 0;

	/** Помощь */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	int32 Assists = 0;

	/** Нанесённый урон */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	float DamageDealt = 0.0f;

	/** Полученный урон */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	float DamageTaken = 0.0f;

	/** Выстрелов сделано */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	int64 ShotsFired = 0;

	/** Попаданий */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	int64 ShotsHit = 0;

	/** Хедшотов */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	int32 Headshots = 0;

	/** Сыграно матчей */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	int32 MatchesPlayed = 0;

	/** Побед */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	int32 Wins = 0;

	/** Время в игре (секунды) */
	UPROPERTY(BlueprintReadWrite, Category = "Stats")
	int64 PlayTimeSeconds = 0;

	// ═══════════════════════════════════════════════════════════════════════════
	// CALCULATED
	// ═══════════════════════════════════════════════════════════════════════════

	float GetKDRatio() const
	{
		return Deaths > 0 ? static_cast<float>(Kills) / Deaths : static_cast<float>(Kills);
	}

	float GetKDARatio() const
	{
		return Deaths > 0 ? static_cast<float>(Kills + Assists) / Deaths : static_cast<float>(Kills + Assists);
	}

	float GetAccuracy() const
	{
		return ShotsFired > 0 ? static_cast<float>(ShotsHit) / ShotsFired * 100.0f : 0.0f;
	}

	float GetWinRate() const
	{
		return MatchesPlayed > 0 ? static_cast<float>(Wins) / MatchesPlayed * 100.0f : 0.0f;
	}

	float GetHeadshotRate() const
	{
		return Kills > 0 ? static_cast<float>(Headshots) / Kills * 100.0f : 0.0f;
	}
};

// ═══════════════════════════════════════════════════════════════════════════════
// PLAYER SETTINGS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCorePlayerSettings
 *
 * Настройки игрока.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCorePlayerSettings
{
	GENERATED_BODY()

	// ═══════════════════════════════════════════════════════════════════════════
	// CONTROLS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Чувствительность мыши */
	UPROPERTY(BlueprintReadWrite, Category = "Controls")
	float MouseSensitivity = 1.0f;

	/** Чувствительность ADS */
	UPROPERTY(BlueprintReadWrite, Category = "Controls")
	float ADSSensitivity = 0.7f;

	/** Инверсия Y */
	UPROPERTY(BlueprintReadWrite, Category = "Controls")
	bool bInvertY = false;

	// ═══════════════════════════════════════════════════════════════════════════
	// GRAPHICS
	// ═══════════════════════════════════════════════════════════════════════════

	/** FOV */
	UPROPERTY(BlueprintReadWrite, Category = "Graphics")
	float FieldOfView = 90.0f;

	/** Яркость */
	UPROPERTY(BlueprintReadWrite, Category = "Graphics")
	float Brightness = 1.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// AUDIO
	// ═══════════════════════════════════════════════════════════════════════════

	/** Громкость общая */
	UPROPERTY(BlueprintReadWrite, Category = "Audio")
	float MasterVolume = 1.0f;

	/** Громкость музыки */
	UPROPERTY(BlueprintReadWrite, Category = "Audio")
	float MusicVolume = 0.8f;

	/** Громкость эффектов */
	UPROPERTY(BlueprintReadWrite, Category = "Audio")
	float SFXVolume = 1.0f;

	/** Громкость голосового чата */
	UPROPERTY(BlueprintReadWrite, Category = "Audio")
	float VoiceChatVolume = 1.0f;

	// ═══════════════════════════════════════════════════════════════════════════
	// GAMEPLAY
	// ═══════════════════════════════════════════════════════════════════════════

	/** Автоматическая перезарядка */
	UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
	bool bAutoReload = true;

	/** Показывать подсказки */
	UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
	bool bShowHints = true;

	/** Crosshair цвет (hex) */
	UPROPERTY(BlueprintReadWrite, Category = "Gameplay")
	FString CrosshairColor = TEXT("#FFFFFF");
};

// ═══════════════════════════════════════════════════════════════════════════════
// LOADOUT
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCoreLoadoutSlot
 *
 * Слот в loadout.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreLoadoutSlot
{
	GENERATED_BODY()

	/** ID предмета */
	UPROPERTY(BlueprintReadWrite, Category = "Loadout")
	FString ItemId;

	/** ID скина */
	UPROPERTY(BlueprintReadWrite, Category = "Loadout")
	FString SkinId;

	/** Прикрепления (attachments) */
	UPROPERTY(BlueprintReadWrite, Category = "Loadout")
	TArray<FString> AttachmentIds;
};

/**
 * FSuspenseCoreLoadout
 *
 * Полный loadout.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreLoadout
{
	GENERATED_BODY()

	/** Название loadout */
	UPROPERTY(BlueprintReadWrite, Category = "Loadout")
	FString Name = TEXT("Default");

	/** Основное оружие */
	UPROPERTY(BlueprintReadWrite, Category = "Loadout")
	FSuspenseCoreLoadoutSlot PrimaryWeapon;

	/** Вторичное оружие */
	UPROPERTY(BlueprintReadWrite, Category = "Loadout")
	FSuspenseCoreLoadoutSlot SecondaryWeapon;

	/** Снаряжение (гранаты и т.д.) */
	UPROPERTY(BlueprintReadWrite, Category = "Loadout")
	TArray<FSuspenseCoreLoadoutSlot> Equipment;

	/** Перки */
	UPROPERTY(BlueprintReadWrite, Category = "Loadout")
	TArray<FString> PerkIds;
};

// ═══════════════════════════════════════════════════════════════════════════════
// ACHIEVEMENT
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCoreAchievementProgress
 *
 * Прогресс достижения.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreAchievementProgress
{
	GENERATED_BODY()

	/** Текущий прогресс */
	UPROPERTY(BlueprintReadWrite, Category = "Achievement")
	int32 CurrentProgress = 0;

	/** Требуемый прогресс */
	UPROPERTY(BlueprintReadWrite, Category = "Achievement")
	int32 RequiredProgress = 1;

	/** Выполнено */
	UPROPERTY(BlueprintReadWrite, Category = "Achievement")
	bool bCompleted = false;

	/** Дата выполнения */
	UPROPERTY(BlueprintReadWrite, Category = "Achievement")
	FDateTime CompletedAt;

	float GetProgressPercent() const
	{
		return RequiredProgress > 0 ? static_cast<float>(CurrentProgress) / RequiredProgress * 100.0f : 0.0f;
	}
};

// ═══════════════════════════════════════════════════════════════════════════════
// INVENTORY ITEM
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCoreInventoryItem
 *
 * Предмет в инвентаре игрока.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInventoryItem
{
	GENERATED_BODY()

	/** Уникальный ID экземпляра */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	FString InstanceId;

	/** ID определения предмета */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	FString ItemDefinitionId;

	/** Количество */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	int32 Quantity = 1;

	/** Дата получения */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	FDateTime AcquiredAt;

	/** Дополнительные данные (JSON) */
	UPROPERTY(BlueprintReadWrite, Category = "Item")
	FString CustomData;
};

// ═══════════════════════════════════════════════════════════════════════════════
// MAIN PLAYER DATA
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCorePlayerData
 *
 * Полные данные игрока для сохранения/загрузки.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCorePlayerData
{
	GENERATED_BODY()

	// ═══════════════════════════════════════════════════════════════════════════
	// ИДЕНТИФИКАЦИЯ
	// ═══════════════════════════════════════════════════════════════════════════

	/** Уникальный ID игрока (GUID) */
	UPROPERTY(BlueprintReadWrite, Category = "Identity")
	FString PlayerId;

	/** Отображаемое имя */
	UPROPERTY(BlueprintReadWrite, Category = "Identity")
	FString DisplayName;

	/** ID аватара */
	UPROPERTY(BlueprintReadWrite, Category = "Identity")
	FString AvatarId;

	/** Дата создания аккаунта */
	UPROPERTY(BlueprintReadWrite, Category = "Identity")
	FDateTime CreatedAt;

	/** Последний вход */
	UPROPERTY(BlueprintReadWrite, Category = "Identity")
	FDateTime LastLoginAt;

	// ═══════════════════════════════════════════════════════════════════════════
	// CHARACTER CLASS
	// ═══════════════════════════════════════════════════════════════════════════

	/** ID класса персонажа (Assault, Medic, Sniper, etc.) */
	UPROPERTY(BlueprintReadWrite, Category = "Class")
	FString CharacterClassId;

	// ═══════════════════════════════════════════════════════════════════════════
	// ПРОГРЕСС
	// ═══════════════════════════════════════════════════════════════════════════

	/** Уровень */
	UPROPERTY(BlueprintReadWrite, Category = "Progress")
	int32 Level = 1;

	/** Очки опыта */
	UPROPERTY(BlueprintReadWrite, Category = "Progress")
	int64 ExperiencePoints = 0;

	/** Уровень престижа */
	UPROPERTY(BlueprintReadWrite, Category = "Progress")
	int32 PrestigeLevel = 0;

	// ═══════════════════════════════════════════════════════════════════════════
	// ВАЛЮТА
	// ═══════════════════════════════════════════════════════════════════════════

	/** Мягкая валюта (зарабатывается в игре) */
	UPROPERTY(BlueprintReadWrite, Category = "Currency")
	int64 SoftCurrency = 0;

	/** Твёрдая валюта (premium) */
	UPROPERTY(BlueprintReadWrite, Category = "Currency")
	int64 HardCurrency = 0;

	// ═══════════════════════════════════════════════════════════════════════════
	// ДАННЫЕ
	// ═══════════════════════════════════════════════════════════════════════════

	/** Статистика */
	UPROPERTY(BlueprintReadWrite, Category = "Data")
	FSuspenseCorePlayerStats Stats;

	/** Настройки */
	UPROPERTY(BlueprintReadWrite, Category = "Data")
	FSuspenseCorePlayerSettings Settings;

	/** Loadouts */
	UPROPERTY(BlueprintReadWrite, Category = "Data")
	TArray<FSuspenseCoreLoadout> Loadouts;

	/** Активный loadout */
	UPROPERTY(BlueprintReadWrite, Category = "Data")
	int32 ActiveLoadoutIndex = 0;

	// ═══════════════════════════════════════════════════════════════════════════
	// ИНВЕНТАРЬ
	// ═══════════════════════════════════════════════════════════════════════════

	/** Разблокированное оружие */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory")
	TArray<FString> UnlockedWeapons;

	/** Разблокированные скины */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory")
	TArray<FString> UnlockedSkins;

	/** Инвентарь предметов */
	UPROPERTY(BlueprintReadWrite, Category = "Inventory")
	TArray<FSuspenseCoreInventoryItem> Inventory;

	// ═══════════════════════════════════════════════════════════════════════════
	// ДОСТИЖЕНИЯ
	// ═══════════════════════════════════════════════════════════════════════════

	/** Прогресс достижений: AchievementId -> Progress */
	UPROPERTY(BlueprintReadWrite, Category = "Achievements")
	TMap<FString, FSuspenseCoreAchievementProgress> Achievements;

	// ═══════════════════════════════════════════════════════════════════════════
	// ХЕЛПЕРЫ
	// ═══════════════════════════════════════════════════════════════════════════

	/** Создать нового игрока */
	static FSuspenseCorePlayerData CreateNew(const FString& InDisplayName, const FString& InCharacterClassId = TEXT("Assault"))
	{
		FSuspenseCorePlayerData Data;
		Data.PlayerId = FGuid::NewGuid().ToString();
		Data.DisplayName = InDisplayName;
		Data.CharacterClassId = InCharacterClassId;
		Data.CreatedAt = FDateTime::UtcNow();
		Data.LastLoginAt = FDateTime::UtcNow();

		// Starter currency for new players
		Data.SoftCurrency = 1000;
		Data.HardCurrency = 100;

		// Дефолтный loadout
		FSuspenseCoreLoadout DefaultLoadout;
		DefaultLoadout.Name = TEXT("Default");
		Data.Loadouts.Add(DefaultLoadout);

		return Data;
	}

	/** Создать тестового игрока с данными для отладки */
	static FSuspenseCorePlayerData CreateTestPlayer(const FString& InDisplayName)
	{
		FSuspenseCorePlayerData Data = CreateNew(InDisplayName);

		// Test progression data
		Data.Level = 25;
		Data.ExperiencePoints = 15000;
		Data.PrestigeLevel = 1;

		// Test currency
		Data.SoftCurrency = 50000;
		Data.HardCurrency = 500;

		// Test stats
		Data.Stats.Kills = 342;
		Data.Stats.Deaths = 198;
		Data.Stats.Assists = 156;
		Data.Stats.Headshots = 87;
		Data.Stats.MatchesPlayed = 48;
		Data.Stats.Wins = 22;
		Data.Stats.PlayTimeSeconds = 72000; // 20 hours
		Data.Stats.DamageDealt = 125000.0f;
		Data.Stats.DamageTaken = 85000.0f;
		Data.Stats.ShotsFired = 15000;
		Data.Stats.ShotsHit = 4500;

		// Test unlocks
		Data.UnlockedWeapons.Add(TEXT("WPN_AssaultRifle"));
		Data.UnlockedWeapons.Add(TEXT("WPN_SMG"));
		Data.UnlockedWeapons.Add(TEXT("WPN_Shotgun"));
		Data.UnlockedSkins.Add(TEXT("SKIN_Default"));
		Data.UnlockedSkins.Add(TEXT("SKIN_Tactical"));

		return Data;
	}

	/** Валидны ли данные */
	bool IsValid() const
	{
		return !PlayerId.IsEmpty() && !DisplayName.IsEmpty();
	}
};
