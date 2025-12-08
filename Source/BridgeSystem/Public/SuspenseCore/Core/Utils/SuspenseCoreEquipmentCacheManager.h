// FSuspenseEquipmentCacheManager.h
// Copyright Suspense Team. All Rights Reserved.

#ifndef SUSPENSECORE_CORE_UTILS_SUSPENSECOREEQUIPMENTCACHEMANAGER_H
#define SUSPENSECORE_CORE_UTILS_SUSPENSECOREEQUIPMENTCACHEMANAGER_H

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Misc/DateTime.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformTime.h"
#include "Templates/SharedPointer.h"
#include "Templates/Function.h"
#include "Misc/Crc.h"
#include <utility>
#include <atomic>
#include <limits>

// ВАЖНО: подключаем только объявление реестра (без его повторного определения здесь)
#include "SuspenseCore/Core/Utils/SuspenseCoreGlobalCacheRegistry.h"

// Эти инклюды у вас уже были — оставляю без изменений
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"

/**
 * Delegate for external cache entry validation
 * @param Key The cache key being validated
 * @param Value The value being validated
 * @return True if the entry is valid and safe to cache
 */
template<typename KeyType, typename ValueType>
using FValidateEntryFunc = TFunction<bool(const KeyType&, const ValueType&)>;

/**
 * Update frequency tracking for cache poisoning detection
 */
struct FUpdateFrequencyData
{
	int32 UpdateCount;
	double LastUpdateTime;
	double WindowStartTime;

	FUpdateFrequencyData()
		: UpdateCount(0)
		, LastUpdateTime(0.0)
		, WindowStartTime(0.0)
	{}

	void RecordUpdate(double CurrentTime)
	{
		// Reset window every second
		if (CurrentTime - WindowStartTime > 1.0)
		{
			UpdateCount = 1;
			WindowStartTime = CurrentTime;
		}
		else
		{
			++UpdateCount;
		}

		LastUpdateTime = CurrentTime;
	}

	bool IsExcessiveUpdateRate(int32 MaxUpdatesPerSecond) const
	{
		if (MaxUpdatesPerSecond <= 0) return false;

		const double Elapsed = LastUpdateTime - WindowStartTime;
		if (Elapsed <= 0.0) return false;

		const double Rate = (double)UpdateCount / FMath::Max(Elapsed, 0.0001);
		return Rate > (double)MaxUpdatesPerSecond;
	}
};

namespace EquipmentCacheHash
{
	// Улучшенное SFINAE для детектирования GetTypeHash
	template<typename T>
	struct THasGetTypeHash
	{
	private:
		template<typename U>
		static auto Test(int) -> decltype(
			static_cast<uint32>(GetTypeHash(std::declval<const U&>())),
			std::true_type{}
		);

		template<typename>
		static std::false_type Test(...);

	public:
		static constexpr bool value = decltype(Test<T>(0))::value;
	};

	// Безопасный вызов GetTypeHash
	template<typename T>
	FORCEINLINE typename TEnableIf<THasGetTypeHash<T>::value, uint32>::Type
	ComputeGetTypeHash(const T& V)
	{
		using ::GetTypeHash;
		return GetTypeHash(V);
	}

	template<typename T>
	FORCEINLINE typename TEnableIf<!THasGetTypeHash<T>::value, uint32>::Type
	ComputeGetTypeHash(const T& V)
	{
		if constexpr (TIsTriviallyCopyConstructible<T>::Value)
		{
			return FCrc::MemCrc32(&V, sizeof(T));
		}
		else
		{
			return 0u;
		}
	}

	template<typename T>
	FORCEINLINE uint32 Compute(const T& V)
	{
		return ComputeGetTypeHash(V);
	}
}

/**
 * Cache entry with integrity and usage tracking
 */
template<typename T>
struct FCacheEntry
{
	T Value;
	FDateTime Timestamp;
	float TTL;             // Time-to-live in seconds
	int32 HitCount;        // For LRU/aging
	int32 UpdateCount;     // For poisoning detection
	uint32 DataHash;       // Integrity check

	FCacheEntry()
		: TTL(0.0f)
		, HitCount(0)
		, UpdateCount(0)
		, DataHash(0)
	{}

	FCacheEntry(const T& InValue, float InTTL)
		: Value(InValue)
		, Timestamp(FDateTime::Now())
		, TTL(InTTL)
		, HitCount(0)
		, UpdateCount(0)
		, DataHash(EquipmentCacheHash::Compute(InValue))
	{}

	void Touch()
	{
		Timestamp = FDateTime::Now();
	}

	bool IsExpired() const
	{
		if (TTL <= 0.0f) return false;
		const FTimespan Age = FDateTime::Now() - Timestamp;
		return Age.GetTotalSeconds() > TTL;
	}

	void RecordHit() { ++HitCount; }
	void IncrementUpdateCount() { ++UpdateCount; }

	bool VerifyIntegrity() const
	{
		if (DataHash == 0u)
		{
			return true;
		}
		return DataHash == EquipmentCacheHash::Compute(Value);
	}
};


/**
 * Equipment cache manager (thread-safe).
 *
 * Architecture:
 * - Supports two initialization modes: basic (MaxEntries only) and advanced (DefaultTTL + MaxEntries)
 * - DefaultTTL applies to all entries unless overridden per-entry in Set()
 * - Thread-safe via FScopeLock on all public methods
 *
 * Locking:
 * - All public methods acquire CacheLock (FCriticalSection) via FScopeLock.
 */
template<typename KeyType, typename ValueType>
class FSuspenseEquipmentCacheManager
{
public:
	/**
	 * Basic constructor with max entries only
	 * @param MaxEntries Maximum number of cache entries before LRU eviction
	 */
	FSuspenseEquipmentCacheManager(int32 MaxEntries = 100)
		: MaxCacheEntries(MaxEntries)
		, DefaultTTL(0.0f)  // No expiration by default
		, MaxValueSize(1024 * 1024)
		, MaxUpdateRatePerSecond(10)
		, EnablePoisoningProtection(true)
		, TotalHits(0)
		, TotalMisses(0)
		, TotalEvictions(0)
		, RejectedEntries(0)
		, SuspiciousPatterns(0)
	{
		CacheLock = MakeShareable(new FCriticalSection());
	}

	/**
	 * Advanced constructor with default TTL support
	 * @param InDefaultTTL Default time-to-live for all entries (in seconds, 0 = no expiration)
	 * @param MaxEntries Maximum number of cache entries before LRU eviction
	 */
	FSuspenseEquipmentCacheManager(float InDefaultTTL, int32 MaxEntries = 100)
		: MaxCacheEntries(MaxEntries)
		, DefaultTTL(FMath::Max(0.0f, InDefaultTTL))
		, MaxValueSize(1024 * 1024)
		, MaxUpdateRatePerSecond(10)
		, EnablePoisoningProtection(true)
		, TotalHits(0)
		, TotalMisses(0)
		, TotalEvictions(0)
		, RejectedEntries(0)
		, SuspiciousPatterns(0)
	{
		CacheLock = MakeShareable(new FCriticalSection());
	}

	/**
	 * Set cache entry with optional TTL override
	 * @param Key Cache key
	 * @param Value Value to cache
	 * @param TTLSeconds TTL override (-1 = use DefaultTTL, 0+ = specific TTL)
	 * @return True if entry was cached successfully
	 */
	bool Set(const KeyType& Key, const ValueType& Value, float TTLSeconds = -1.0f)
	{
		FScopeLock Lock(CacheLock.Get());

		// Determine effective TTL: use explicit if >= 0, otherwise use default
		const float EffectiveTTL = (TTLSeconds >= 0.0f) ? TTLSeconds : DefaultTTL;

		if (!CheckValueSize(Value))
		{
			RejectedEntries.fetch_add(1);
			UE_LOG(LogTemp, Warning,
				TEXT("FSuspenseEquipmentCacheManager: Rejected cache set due to size constraint (KeyHash=%u)"),
				EquipmentCacheHash::Compute(Key));
			return false;
		}

		if (EnablePoisoningProtection)
		{
			const double Now = FPlatformTime::Seconds();

			FUpdateFrequencyData& FreqData = UpdateFrequency.FindOrAdd(Key);
			FreqData.RecordUpdate(Now);

			if (FreqData.IsExcessiveUpdateRate(MaxUpdateRatePerSecond))
			{
				RejectedEntries.fetch_add(1);
				SuspiciousPatterns.fetch_add(1);

				UE_LOG(LogTemp, Warning,
					TEXT("FSuspenseEquipmentCacheManager: Rejected update due to excessive rate (KeyHash=%u, Rate=%d/s, Limit=%d/s)"),
					EquipmentCacheHash::Compute(Key), FreqData.UpdateCount, MaxUpdateRatePerSecond);

				return false;
			}

			if (ValidationFunc && !ValidationFunc(Key, Value))
			{
				RejectedEntries.fetch_add(1);
				UE_LOG(LogTemp, Warning,
					TEXT("FSuspenseEquipmentCacheManager: External validation failed for cache entry (KeyHash=%u)"),
					EquipmentCacheHash::Compute(Key));
				return false;
			}

			if (IsAnomalousNumericValue(Value))
			{
				RejectedEntries.fetch_add(1);
				SuspiciousPatterns.fetch_add(1);
				UE_LOG(LogTemp, Warning,
					TEXT("FSuspenseEquipmentCacheManager: Anomalous numeric value detected (KeyHash=%u)"),
					EquipmentCacheHash::Compute(Key));
				return false;
			}
		}

		if (FCacheEntry<ValueType>* Existing = CacheMap.Find(Key))
		{
			Existing->Value = Value;
			Existing->TTL = EffectiveTTL;
			Existing->Timestamp = FDateTime::Now();
			Existing->IncrementUpdateCount();
			Existing->DataHash = EquipmentCacheHash::Compute(Value);
			TouchAccess_NoLock(Key);
		}
		else
		{
			if (CacheMap.Num() >= MaxCacheEntries)
			{
				EvictLRU_NoLock();
			}

			CacheMap.Add(Key, FCacheEntry<ValueType>(Value, EffectiveTTL));
			AccessOrder.Add(Key);
			UpdateFrequency.FindOrAdd(Key);
		}

		return true;
	}

	bool Get(const KeyType& Key, ValueType& OutValue)
	{
		FScopeLock Lock(CacheLock.Get());

		if (FCacheEntry<ValueType>* Entry = CacheMap.Find(Key))
		{
			if (!Entry->VerifyIntegrity())
			{
				CacheMap.Remove(Key);
				UE_LOG(LogTemp, Error,
					TEXT("FSuspenseEquipmentCacheManager: Integrity check failed, entry removed (KeyHash=%u)"),
					EquipmentCacheHash::Compute(Key));
				RemoveFromAccess_NoLock(Key);
				UpdateFrequency.Remove(Key);
				return false;
			}

			if (Entry->IsExpired())
			{
				CacheMap.Remove(Key);
				RemoveFromAccess_NoLock(Key);
				UpdateFrequency.Remove(Key);
				TotalEvictions.fetch_add(1);
				TotalMisses.fetch_add(1);
				return false;
			}

			OutValue = Entry->Value;
			Entry->RecordHit();
			Entry->Touch();
			TouchAccess_NoLock(Key);
			TotalHits.fetch_add(1);
			return true;
		}

		TotalMisses.fetch_add(1);
		return false;
	}

	void Invalidate(const KeyType& Key)
	{
		FScopeLock Lock(CacheLock.Get());
		CacheMap.Remove(Key);
		RemoveFromAccess_NoLock(Key);
		UpdateFrequency.Remove(Key);
	}

	void Remove(const KeyType& Key)
	{
		Invalidate(Key);
	}

	void Clear()
	{
		FScopeLock Lock(CacheLock.Get());
		CacheMap.Reset();
		AccessOrder.Reset();
		UpdateFrequency.Reset();
		TotalHits.store(0);
		TotalMisses.store(0);
		TotalEvictions.store(0);
		RejectedEntries.store(0);
		// SuspiciousPatterns оставляем для отладки после чистки
	}

	void SetMaxEntries(int32 MaxEntries)
	{
		FScopeLock Lock(CacheLock.Get());

		MaxCacheEntries = FMath::Max(1, MaxEntries);
		while (CacheMap.Num() > MaxCacheEntries)
		{
			EvictLRU_NoLock();
		}
	}

	/**
	 * Get current default TTL setting
	 * @return Default TTL in seconds (0 = no expiration)
	 */
	float GetDefaultTTL() const
	{
		FScopeLock Lock(CacheLock.Get());
		return DefaultTTL;
	}

	/**
	 * Set default TTL for future entries
	 * @param InTTL New default TTL in seconds (0 = no expiration)
	 */
	void SetDefaultTTL(float InTTL)
	{
		FScopeLock Lock(CacheLock.Get());
		DefaultTTL = FMath::Max(0.0f, InTTL);
	}

	void SetValidationDelegate(FValidateEntryFunc<KeyType, ValueType> InFunc)
	{
		FScopeLock Lock(CacheLock.Get());
		ValidationFunc = MoveTemp(InFunc);
		UE_LOG(LogTemp, Verbose, TEXT("FSuspenseEquipmentCacheManager: Validation function set"));
	}

	void SetPoisoningProtectionEnabled(bool bEnabled)
	{
		FScopeLock Lock(CacheLock.Get());
		EnablePoisoningProtection = bEnabled;
	}

	void SetMaxValueSize(int32 InBytes)
	{
		FScopeLock Lock(CacheLock.Get());
		MaxValueSize = FMath::Max(0, InBytes);
	}

	void SetMaxUpdateRatePerSecond(int32 InRate)
	{
		FScopeLock Lock(CacheLock.Get());
		MaxUpdateRatePerSecond = FMath::Max(0, InRate);
	}

	void ResetSecurityCounters()
	{
		FScopeLock Lock(CacheLock.Get());
		RejectedEntries.store(0);
		SuspiciousPatterns.store(0);
		UpdateFrequency.Empty();
	}

public:
	// --- Stats & Introspection ---

	int32 GetTotalHits() const { return TotalHits.load(); }
	int32 GetTotalMisses() const { return TotalMisses.load(); }
	int32 GetTotalEvictions() const { return TotalEvictions.load(); }
	int32 GetRejectedEntries() const { return RejectedEntries.load(); }
	int32 GetSuspiciousPatterns() const { return SuspiciousPatterns.load(); }

	float GetHitRate() const
	{
		const int32 Hits = TotalHits.load();
		const int32 Misses = TotalMisses.load();
		const int32 Total = Hits + Misses;
		return (Total > 0) ? (float)Hits / (float)Total : 0.0f;
	}

	FString DumpStats() const
	{
		FScopeLock Lock(CacheLock.Get());
		FString Stats;
		Stats += FString::Printf(TEXT("Entries: %d / %d\n"), CacheMap.Num(), MaxCacheEntries);
		Stats += FString::Printf(TEXT("Hits: %d, Misses: %d, HitRate: %.2f\n"),
			TotalHits.load(), TotalMisses.load(), GetHitRate());
		Stats += FString::Printf(TEXT("Evictions: %d, Rejected: %d, Suspicious: %d\n"),
			TotalEvictions.load(), RejectedEntries.load(), SuspiciousPatterns.load());
		Stats += FString::Printf(TEXT("DefaultTTL: %.1fs\n"), DefaultTTL);
		return Stats;
	}

	float ComputeIntegrityScore() const
	{
		FScopeLock Lock(CacheLock.Get());

		float Integrity = 1.0f;

		Integrity *= (0.5f + 0.5f * GetHitRate()); // 0.5..1.0

		int32 Expired = 0;
		for (const auto& Pair : CacheMap)
		{
			if (Pair.Value.IsExpired()) ++Expired;
		}
		if (CacheMap.Num() > 0)
		{
			const float ExpiredRatio = (float)Expired / (float)CacheMap.Num();
			Integrity *= (1.0f - ExpiredRatio * 0.5f); // Up to -50%
		}

		int32 Excessive = 0;
		for (const auto& UF : UpdateFrequency)
		{
			if (UF.Value.UpdateCount > MaxUpdateRatePerSecond)
			{
				++Excessive;
			}
		}
		if (CacheMap.Num() > 0 && Excessive > 0)
		{
			const float ExcessiveRate = (float)Excessive / (float)CacheMap.Num();
			Integrity *= (1.0f - ExcessiveRate * 0.3f); // Up to -30%
		}

		if (TotalEvictions.load() > CacheMap.Num())
		{
			const float Pressure = FMath::Min(1.0f, (float)TotalEvictions.load() / (float)(CacheMap.Num() * 10));
			Integrity *= (1.0f - Pressure * 0.2f); // Up to -20%
		}

		return FMath::Clamp(Integrity, 0.0f, 1.0f);
	}

	struct FCacheStatistics
	{
		int32 Entries = 0;
		int32 Capacity = 0;
		int32 Hits = 0;
		int32 Misses = 0;
		int32 Evictions = 0;
		int32 Rejected = 0;
		int32 Suspicious = 0;
		float HitRate = 0.0f;
		float Integrity = 1.0f;
		float DefaultTTL = 0.0f;

		FString ToString() const
		{
			return FString::Printf(
				TEXT("Entries: %d/%d; Hits=%d; Misses=%d; Evictions=%d; Rejected=%d; Suspicious=%d; HitRate=%.1f%%; Integrity=%.2f; TTL=%.1fs"),
				Entries, Capacity, Hits, Misses, Evictions, Rejected, Suspicious, HitRate * 100.0f, Integrity, DefaultTTL
			);
		}
	};

	FCacheStatistics GetStatistics() const
	{
		FScopeLock Lock(CacheLock.Get());
		FCacheStatistics S;
		S.Entries    = CacheMap.Num();
		S.Capacity   = MaxCacheEntries;
		S.Hits       = TotalHits.load();
		S.Misses     = TotalMisses.load();
		S.Evictions  = TotalEvictions.load();
		S.Rejected   = RejectedEntries.load();
		S.Suspicious = SuspiciousPatterns.load();
		S.HitRate    = GetHitRate();
		S.Integrity  = ComputeIntegrityScore();
		S.DefaultTTL = DefaultTTL;
		return S;
	}

private:
	void EvictLRU_NoLock()
	{
		if (AccessOrder.Num() == 0) return;

		const KeyType& LRUKey = AccessOrder[0];
		CacheMap.Remove(LRUKey);
		AccessOrder.RemoveAt(0);
		UpdateFrequency.Remove(LRUKey);
		TotalEvictions.fetch_add(1);
	}

	void TouchAccess_NoLock(const KeyType& Key)
	{
		const int32 Index = AccessOrder.Find(Key);
		if (Index != INDEX_NONE)
		{
			AccessOrder.RemoveAt(Index);
		}
		AccessOrder.Add(Key);
	}

	void RemoveFromAccess_NoLock(const KeyType& Key)
	{
		const int32 Index = AccessOrder.Find(Key);
		if (Index != INDEX_NONE)
		{
			AccessOrder.RemoveAt(Index);
		}
	}

	template<typename T>
	typename TEnableIf<TIsTriviallyCopyConstructible<T>::Value, bool>::Type
	CheckValueSize(const T& /*InValue*/) const
	{
		return sizeof(T) <= (size_t)MaxValueSize;
	}

	template<typename T>
	typename TEnableIf<!TIsTriviallyCopyConstructible<T>::Value, bool>::Type
	CheckValueSize(const T& /*InValue*/) const
	{
		// Для нетривиальных типов — не считаем, отсекаем валидацией
		return true;
	}

	template<typename T>
	typename TEnableIf<TIsArithmetic<T>::Value, bool>::Type
	IsAnomalousNumericValue(const T& InValue) const
	{
		if constexpr (std::numeric_limits<T>::has_quiet_NaN)
		{
			if (InValue != InValue) return true; // NaN
		}
		if constexpr (std::numeric_limits<T>::has_infinity)
		{
			const T Inf = std::numeric_limits<T>::infinity();
			if (InValue == Inf || InValue == -Inf) return true;
		}
		return false;
	}

	template<typename T>
	typename TEnableIf<!TIsArithmetic<T>::Value, bool>::Type
	IsAnomalousNumericValue(const T& /*InValue*/) const
	{
		return false;
	}

private:
	TMap<KeyType, FCacheEntry<ValueType>> CacheMap;
	TArray<KeyType> AccessOrder;
	TMap<KeyType, FUpdateFrequencyData> UpdateFrequency;

	int32 MaxCacheEntries;
	float DefaultTTL;  // Default time-to-live for entries when not specified explicitly
	int32 MaxValueSize;
	int32 MaxUpdateRatePerSecond;
	bool  EnablePoisoningProtection;

	mutable std::atomic<int32> TotalHits;
	mutable std::atomic<int32> TotalMisses;
	mutable std::atomic<int32> TotalEvictions;
	mutable std::atomic<int32> RejectedEntries;
	mutable std::atomic<int32> SuspiciousPatterns;

	FValidateEntryFunc<KeyType, ValueType> ValidationFunc;

	mutable TSharedPtr<FCriticalSection> CacheLock;
};

// Specialized caches
using FTagQueryCache = FSuspenseEquipmentCacheManager<FGameplayTag, bool>;
using FItemDataCache = FSuspenseEquipmentCacheManager<FName, TSharedPtr<struct FSuspenseInventoryItemInstance>>;


#endif // SUSPENSECORE_CORE_UTILS_SUSPENSECOREEQUIPMENTCACHEMANAGER_H