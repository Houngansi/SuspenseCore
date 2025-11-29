// SuspenseCoreTypes.h
// SuspenseCore - Clean Architecture Foundation
// Copyright (c) 2025. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreTypes.generated.h"

// ═══════════════════════════════════════════════════════════════════════════════
// FORWARD DECLARATIONS
// ═══════════════════════════════════════════════════════════════════════════════

class USuspenseCoreEventBus;

// ═══════════════════════════════════════════════════════════════════════════════
// ENUMS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * ESuspenseCoreEventPriority
 *
 * Приоритет обработки событий в EventBus.
 * Меньшее значение = выше приоритет.
 */
UENUM(BlueprintType)
enum class ESuspenseCoreEventPriority : uint8
{
	/** Системные события - обрабатываются первыми */
	System = 0		UMETA(DisplayName = "System"),

	/** Высокий приоритет - GAS, боевая система */
	High = 50		UMETA(DisplayName = "High"),

	/** Нормальный приоритет - большинство событий */
	Normal = 100	UMETA(DisplayName = "Normal"),

	/** Низкий приоритет - UI, визуальные эффекты */
	Low = 150		UMETA(DisplayName = "Low"),

	/** Самый низкий - логирование, аналитика */
	Lowest = 200	UMETA(DisplayName = "Lowest")
};

// ═══════════════════════════════════════════════════════════════════════════════
// STRUCTS - SUBSCRIPTION
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCoreSubscriptionHandle
 *
 * Handle для управления подпиской на события.
 * Используется для отписки.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreSubscriptionHandle
{
	GENERATED_BODY()

	FSuspenseCoreSubscriptionHandle() : Id(0) {}
	explicit FSuspenseCoreSubscriptionHandle(uint64 InId) : Id(InId) {}

	/** Валиден ли handle */
	bool IsValid() const { return Id != 0; }

	/** Инвалидировать handle */
	void Invalidate() { Id = 0; }

	/** Получить ID */
	uint64 GetId() const { return Id; }

	bool operator==(const FSuspenseCoreSubscriptionHandle& Other) const { return Id == Other.Id; }
	bool operator!=(const FSuspenseCoreSubscriptionHandle& Other) const { return Id != Other.Id; }

	friend uint32 GetTypeHash(const FSuspenseCoreSubscriptionHandle& Handle)
	{
		return GetTypeHash(Handle.Id);
	}

private:
	UPROPERTY()
	uint64 Id;
};

// ═══════════════════════════════════════════════════════════════════════════════
// STRUCTS - EVENT DATA
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCoreEventData
 *
 * Данные события. Содержит источник, временную метку и гибкий payload.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreEventData
{
	GENERATED_BODY()

	FSuspenseCoreEventData()
		: Source(nullptr)
		, Timestamp(0.0)
		, Priority(ESuspenseCoreEventPriority::Normal)
	{}

	// ═══════════════════════════════════════════════════════════════════════════
	// CORE FIELDS
	// ═══════════════════════════════════════════════════════════════════════════

	/** Источник события (Actor, Component, etc.) */
	UPROPERTY(BlueprintReadWrite, Category = "Event")
	TObjectPtr<UObject> Source;

	/** Временная метка */
	UPROPERTY(BlueprintReadOnly, Category = "Event")
	double Timestamp;

	/** Приоритет обработки */
	UPROPERTY(BlueprintReadWrite, Category = "Event")
	ESuspenseCoreEventPriority Priority;

	// ═══════════════════════════════════════════════════════════════════════════
	// PAYLOAD
	// ═══════════════════════════════════════════════════════════════════════════

	/** Строковые данные */
	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	TMap<FName, FString> StringPayload;

	/** Числовые данные (float) */
	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	TMap<FName, float> FloatPayload;

	/** Целочисленные данные */
	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	TMap<FName, int32> IntPayload;

	/** Булевые данные */
	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	TMap<FName, bool> BoolPayload;

	/** Объекты */
	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	TMap<FName, TObjectPtr<UObject>> ObjectPayload;

	/** Векторы */
	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	TMap<FName, FVector> VectorPayload;

	/** Дополнительные теги */
	UPROPERTY(BlueprintReadWrite, Category = "Payload")
	FGameplayTagContainer Tags;

	// ═══════════════════════════════════════════════════════════════════════════
	// GETTERS
	// ═══════════════════════════════════════════════════════════════════════════

	FString GetString(FName Key, const FString& Default = TEXT("")) const
	{
		const FString* Value = StringPayload.Find(Key);
		return Value ? *Value : Default;
	}

	float GetFloat(FName Key, float Default = 0.0f) const
	{
		const float* Value = FloatPayload.Find(Key);
		return Value ? *Value : Default;
	}

	int32 GetInt(FName Key, int32 Default = 0) const
	{
		const int32* Value = IntPayload.Find(Key);
		return Value ? *Value : Default;
	}

	bool GetBool(FName Key, bool Default = false) const
	{
		const bool* Value = BoolPayload.Find(Key);
		return Value ? *Value : Default;
	}

	FVector GetVector(FName Key, const FVector& Default = FVector::ZeroVector) const
	{
		const FVector* Value = VectorPayload.Find(Key);
		return Value ? *Value : Default;
	}

	template<typename T>
	T* GetObject(FName Key) const
	{
		const TObjectPtr<UObject>* Value = ObjectPayload.Find(Key);
		return Value ? Cast<T>(Value->Get()) : nullptr;
	}

	bool HasKey(FName Key) const
	{
		return StringPayload.Contains(Key)
			|| FloatPayload.Contains(Key)
			|| IntPayload.Contains(Key)
			|| BoolPayload.Contains(Key)
			|| ObjectPayload.Contains(Key)
			|| VectorPayload.Contains(Key);
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// SETTERS (Fluent API)
	// ═══════════════════════════════════════════════════════════════════════════

	FSuspenseCoreEventData& SetString(FName Key, const FString& Value)
	{
		StringPayload.Add(Key, Value);
		return *this;
	}

	FSuspenseCoreEventData& SetFloat(FName Key, float Value)
	{
		FloatPayload.Add(Key, Value);
		return *this;
	}

	FSuspenseCoreEventData& SetInt(FName Key, int32 Value)
	{
		IntPayload.Add(Key, Value);
		return *this;
	}

	FSuspenseCoreEventData& SetBool(FName Key, bool Value)
	{
		BoolPayload.Add(Key, Value);
		return *this;
	}

	FSuspenseCoreEventData& SetVector(FName Key, const FVector& Value)
	{
		VectorPayload.Add(Key, Value);
		return *this;
	}

	FSuspenseCoreEventData& SetObject(FName Key, UObject* Value)
	{
		ObjectPayload.Add(Key, Value);
		return *this;
	}

	FSuspenseCoreEventData& AddTag(FGameplayTag Tag)
	{
		Tags.AddTag(Tag);
		return *this;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// FACTORY
	// ═══════════════════════════════════════════════════════════════════════════

	static FSuspenseCoreEventData Create(UObject* InSource)
	{
		FSuspenseCoreEventData Data;
		Data.Source = InSource;
		Data.Timestamp = FPlatformTime::Seconds();
		return Data;
	}

	static FSuspenseCoreEventData Create(UObject* InSource, ESuspenseCoreEventPriority InPriority)
	{
		FSuspenseCoreEventData Data;
		Data.Source = InSource;
		Data.Timestamp = FPlatformTime::Seconds();
		Data.Priority = InPriority;
		return Data;
	}
};

// ═══════════════════════════════════════════════════════════════════════════════
// STRUCTS - INTERNAL
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCoreQueuedEvent
 *
 * Событие в очереди отложенных.
 */
USTRUCT()
struct FSuspenseCoreQueuedEvent
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag EventTag;

	UPROPERTY()
	FSuspenseCoreEventData EventData;

	double QueuedTime = 0.0;
};

/**
 * FSuspenseCoreEventBusStats
 *
 * Статистика EventBus для мониторинга.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreEventBusStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 ActiveSubscriptions = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 UniqueEventTags = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int64 TotalEventsPublished = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Stats")
	int32 DeferredEventsQueued = 0;
};

// ═══════════════════════════════════════════════════════════════════════════════
// DELEGATES
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * Callback при получении события (Dynamic для Blueprint)
 */
DECLARE_DYNAMIC_DELEGATE_TwoParams(
	FSuspenseCoreEventCallback,
	FGameplayTag, EventTag,
	const FSuspenseCoreEventData&, EventData
);

/**
 * Native callback для C++ (более эффективный)
 */
DECLARE_DELEGATE_TwoParams(
	FSuspenseCoreNativeEventCallback,
	FGameplayTag /*EventTag*/,
	const FSuspenseCoreEventData& /*EventData*/
);

// ═══════════════════════════════════════════════════════════════════════════════
// EVENT TAG MACROS
// ═══════════════════════════════════════════════════════════════════════════════

/** Получить тег события (с кэшированием) */
#define SUSPENSE_CORE_TAG(Path) \
	FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore." #Path)))

/** Получить тег события (static, более эффективный) */
#define SUSPENSE_CORE_TAG_STATIC(Path) \
	[]() -> FGameplayTag { \
		static FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TEXT("SuspenseCore." #Path))); \
		return Tag; \
	}()

// Быстрые макросы для частых событий
#define SUSPENSE_EVENT_PLAYER_SPAWNED		SUSPENSE_CORE_TAG_STATIC(Event.Player.Spawned)
#define SUSPENSE_EVENT_PLAYER_DIED			SUSPENSE_CORE_TAG_STATIC(Event.Player.Died)
#define SUSPENSE_EVENT_PLAYER_RESPAWNED		SUSPENSE_CORE_TAG_STATIC(Event.Player.Respawned)
#define SUSPENSE_EVENT_GAS_ATTRIBUTE		SUSPENSE_CORE_TAG_STATIC(Event.GAS.Attribute.Changed)
#define SUSPENSE_EVENT_GAS_HEALTH			SUSPENSE_CORE_TAG_STATIC(Event.GAS.Attribute.Health)
#define SUSPENSE_EVENT_WEAPON_FIRED			SUSPENSE_CORE_TAG_STATIC(Event.Weapon.Fired)
#define SUSPENSE_EVENT_WEAPON_RELOADED		SUSPENSE_CORE_TAG_STATIC(Event.Weapon.Reloaded)
#define SUSPENSE_EVENT_DATABASE_LOADED		SUSPENSE_CORE_TAG_STATIC(Event.Database.PlayerLoaded)
#define SUSPENSE_EVENT_DATABASE_SAVED		SUSPENSE_CORE_TAG_STATIC(Event.Database.PlayerSaved)
