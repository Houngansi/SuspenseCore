// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/Weapon/IMedComWeaponAnimationInterface.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "MedComWeaponAnimationSubsystem.generated.h"

// Forward declarations
struct FAnimationStateData;
class UDataTable;

/**
 * Weapon animation data cache entry
 * 
 * Структура для кэширования данных анимаций с метаданными для управления кэшем.
 * Хранит сырые указатели для производительности, но они никогда не экспонируются в Blueprint.
 */
USTRUCT()
struct FWeaponAnimationCacheEntry
{
    GENERATED_BODY()
    
    /** Cached animation state data pointer (not exposed to Blueprint for safety) */
    const FAnimationStateData* AnimationData = nullptr;
    
    /** Cache timestamp for TTL management */
    UPROPERTY()
    float CacheTime = 0.0f;
    
    /** Is this cache entry still valid */
    UPROPERTY()
    bool bIsValid = false;
    
    /** Hit count for LRU cache management */
    int32 HitCount = 0;
    
    /** Last access time for advanced cache strategies */
    float LastAccessTime = 0.0f;
};

/**
 * GameInstance Subsystem for centralized weapon animation data management
 * 
 * АРХИТЕКТУРНАЯ ФИЛОСОФИЯ:
 * Этот subsystem служит единственным источником правды для всех анимационных данных оружия.
 * Он спроектирован для загрузки данных один раз при старте из DataTable, предоставленной
 * GameInstance, и обеспечивает быстрый, потокобезопасный доступ на протяжении всей игры.
 * 
 * КЛЮЧЕВЫЕ ОСОБЕННОСТИ:
 * - Нет хардкодных путей - все данные приходят из GameInstance
 * - Поддержка наследования тегов (fallback к родительским тегам)
 * - Двухуровневый доступ: указатели для C++, копии для Blueprint
 * - LRU кэш с метриками производительности
 * - Потокобезопасность через critical sections
 * 
 * ИСПОЛЬЗОВАНИЕ:
 * 1. GameInstance конфигурирует DataTable в Blueprint
 * 2. GameInstance вызывает LoadAnimationDataTable() при инициализации
 * 3. Компоненты и AnimBP получают данные через интерфейс
 */
UCLASS()
class MEDCOMGAS_API UMedComWeaponAnimationSubsystem : public UGameInstanceSubsystem,
    public IMedComWeaponAnimationInterface
{
    GENERATED_BODY()

public:
    UMedComWeaponAnimationSubsystem();

    //==================================================================
    // USubsystem Interface
    //==================================================================
    
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

    //==================================================================
    // IMedComWeaponAnimationInterface Implementation - C++ Performance
    //==================================================================
    
    /** High-performance pointer access for C++ code */
    virtual const FAnimationStateData* GetAnimationStateDataPtr(const FGameplayTag& WeaponType) const override;
    
    /** Batch preload for optimization */
    virtual void PreloadAnimationDataBatch(const TArray<FGameplayTag>& WeaponTypes) override;
    
    /** Cache metrics for monitoring */
    virtual void GetCacheMetrics(float& OutHitRate, int32& OutMemoryUsageBytes, int32& OutCacheEntries) const override;

    //==================================================================
    // IMedComWeaponAnimationInterface Implementation - Blueprint Safe
    //==================================================================
    
    virtual bool GetAnimationStateData_Implementation(const FGameplayTag& WeaponType, FAnimationStateData& OutAnimationData) const override;
    virtual UAnimMontage* GetDrawMontage_Implementation(const FGameplayTag& WeaponType, bool bFirstDraw = false) const override;
    virtual UAnimMontage* GetHolsterMontage_Implementation(const FGameplayTag& WeaponType) const override;
    virtual UBlendSpace* GetStanceBlendSpace_Implementation(const FGameplayTag& WeaponType) const override;
    virtual UAnimSequence* GetIdleAnimation_Implementation(const FGameplayTag& WeaponType) const override;
    virtual UAnimMontage* GetSwitchMontage_Implementation(const FGameplayTag& FromWeaponType, const FGameplayTag& ToWeaponType) const override;
    virtual UAnimMontage* GetReloadMontage_Implementation(const FGameplayTag& WeaponType, bool bIsEmpty = false) const override;
    virtual FTransform GetWeaponTransform_Implementation(const FGameplayTag& WeaponType) const override;
    virtual FTransform GetLeftHandGripTransform_Implementation(const FGameplayTag& WeaponType, int32 GripIndex = 0) const override;
    virtual FTransform GetRightHandTransform_Implementation(const FGameplayTag& WeaponType) const override;
    virtual float GetDrawDuration_Implementation(const FGameplayTag& WeaponType, bool bFirstDraw = false) const override;
    virtual float GetHolsterDuration_Implementation(const FGameplayTag& WeaponType) const override;
    virtual float GetSwitchDuration_Implementation(const FGameplayTag& FromWeaponType, const FGameplayTag& ToWeaponType) const override;
    virtual float GetReloadDuration_Implementation(const FGameplayTag& WeaponType, bool bIsEmpty = false) const override;
    virtual bool HasAnimationData_Implementation(const FGameplayTag& WeaponType) const override;
    virtual bool ValidateAnimationData_Implementation(const FGameplayTag& WeaponType, TArray<FString>& OutErrors) const override;
    virtual TArray<FGameplayTag> GetAvailableWeaponTypes_Implementation() const override;
    virtual bool HasSwitchAnimation_Implementation(const FGameplayTag& FromWeaponType, const FGameplayTag& ToWeaponType) const override;

    //==================================================================
    // Public Methods
    //==================================================================
    
    /**
     * Load animation data from DataTable provided by GameInstance
     * This is the PRIMARY way to load animation data - no hardcoded paths!
     * @param InDataTable DataTable containing animation data (must have FAnimationStateData rows)
     * @return True if successfully loaded at least one animation set
     */
    UFUNCTION(BlueprintCallable, Category = "WeaponAnimation")
    bool LoadAnimationDataTable(UDataTable* InDataTable);
    
    /**
     * Clear all cached animation data
     * Useful for hot-reloading or memory management
     */
    UFUNCTION(BlueprintCallable, Category = "WeaponAnimation")
    void ClearAnimationCache();
    
    /**
     * Get cache statistics for debugging
     * @param OutCacheSize Number of cached entries
     * @param OutMemoryUsage Approximate memory usage in bytes
     */
    UFUNCTION(BlueprintCallable, Category = "WeaponAnimation|Debug")
    void GetCacheStatistics(int32& OutCacheSize, int32& OutMemoryUsage) const;
    
    /**
     * Preload animation data for specific weapon types
     * Forces data into cache for predictable performance
     * @param WeaponTypes Array of weapon type tags to preload
     */
    UFUNCTION(BlueprintCallable, Category = "WeaponAnimation")
    void PreloadWeaponAnimations(const TArray<FGameplayTag>& WeaponTypes);
    
    /**
     * Check if subsystem has been initialized with data
     * @return True if animation data has been loaded
     */
    UFUNCTION(BlueprintCallable, Category = "WeaponAnimation")
    bool IsInitialized() const { return bIsInitialized; }
    
    /**
     * Get debug information about loaded animations
     * @return Debug string with system state
     */
    UFUNCTION(BlueprintCallable, Category = "WeaponAnimation|Debug")
    FString GetDebugInfo() const;

protected:
    //==================================================================
    // Internal Methods
    //==================================================================
    
    /**
     * Find animation data by weapon type with tag inheritance support
     * @param WeaponType Weapon type tag
     * @return Animation data or nullptr if not found
     */
    const FAnimationStateData* FindAnimationData(const FGameplayTag& WeaponType) const;
    
    /**
     * Build cache key from weapon type
     * @param WeaponType Weapon type tag
     * @return Cache key string
     */
    FString BuildCacheKey(const FGameplayTag& WeaponType) const;
    
    /**
     * Update cache with new data
     * @param CacheKey Key for cache entry
     * @param Data Animation data to cache
     */
    void UpdateCache(const FString& CacheKey, const FAnimationStateData* Data) const;
    
    /**
     * Evict least recently used cache entry
     */
    void EvictLRUCacheEntry() const;
    
    /**
     * Validate single animation montage
     * @param Montage Montage to validate
     * @param AnimationName Name for error reporting
     * @param OutErrors Error array
     * @return True if valid
     */
    bool ValidateMontage(const UAnimMontage* Montage, const FString& AnimationName, TArray<FString>& OutErrors) const;
    
    /**
     * Calculate memory usage of animation data
     * @return Approximate memory usage in bytes
     */
    int32 CalculateMemoryUsage() const;
    
    /**
     * Log system state for debugging
     */
    void LogSystemState() const;

private:
    //==================================================================
    // Data Storage
    //==================================================================
    
    /** Primary animation data table loaded from GameInstance */
    UPROPERTY()
    TObjectPtr<UDataTable> AnimationDataTable;
    
    /** All loaded animation data rows indexed by weapon type tag name */
    TMap<FName, const FAnimationStateData*> LoadedAnimationData;
    
    /** Cache of animation data by weapon type (mutable for const methods) */
    mutable TMap<FString, FWeaponAnimationCacheEntry> AnimationCache;
    
    /** Critical section for thread-safe cache access */
    mutable FCriticalSection CacheCriticalSection;
    
    /** Flag indicating if data has been loaded */
    bool bIsInitialized = false;
    
    /** Enable detailed logging */
    bool bEnableDetailedLogging = false;
    
    //==================================================================
    // Cache Configuration
    //==================================================================
    
    /** Cache lifetime in seconds before entries are considered stale */
    static constexpr float CacheLifetime = 60.0f;
    
    /** Maximum cache size before LRU eviction */
    static constexpr int32 MaxCacheSize = 100;
    
    /** Minimum hit count to protect from eviction */
    static constexpr int32 MinHitCountForProtection = 5;
    
    //==================================================================
    // Cache Statistics
    //==================================================================
    
    /** Total cache hits for performance monitoring */
    mutable int32 CacheHits = 0;
    
    /** Total cache misses for performance monitoring */
    mutable int32 CacheMisses = 0;
    
    /** Total evictions for cache tuning */
    mutable int32 CacheEvictions = 0;
    
    //==================================================================
    // Delegates (NOTE: Will be moved to EventDelegateManager later)
    //==================================================================
    
    // TODO: Move these delegates to EventDelegateManager in refactor phase
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnAnimationDataLoaded, const FGameplayTag& /*WeaponType*/);
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnAnimationDataCleared, const FGameplayTag& /*WeaponType*/);
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCacheMetricsUpdated, float /*HitRate*/, int32 /*CacheSize*/);
    
public:
    /** Called when animation data is loaded for a weapon type */
    FOnAnimationDataLoaded OnAnimationDataLoaded;
    
    /** Called when animation data is cleared */
    FOnAnimationDataCleared OnAnimationDataCleared;
    
    /** Called when cache metrics are updated */
    FOnCacheMetricsUpdated OnCacheMetricsUpdated;
};