// Copyright Suspense Team. All Rights Reserved.

#include "Subsystems/WeaponAnimationSubsystem.h"
#include "Types/Animation/SuspenseAnimationState.h"
#include "Engine/DataTable.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameplayTagsManager.h"

// Определяем категорию логирования
DEFINE_LOG_CATEGORY_STATIC(LogWeaponAnimation, Log, All);

UWeaponAnimationSubsystem::UWeaponAnimationSubsystem()
{
    AnimationDataTable = nullptr;
    bIsInitialized = false;
    bEnableDetailedLogging = false;

    // Инициализация статистики
    CacheHits = 0;
    CacheMisses = 0;
    CacheEvictions = 0;
}

void UWeaponAnimationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogWeaponAnimation, Log, TEXT("WeaponAnimationSubsystem: Initializing..."));

    // НЕ загружаем никакие дефолтные DataTable!
    // GameInstance загрузит правильную DataTable через LoadAnimationDataTable()

    UE_LOG(LogWeaponAnimation, Log, TEXT("WeaponAnimationSubsystem: Waiting for DataTable from GameInstance"));
}

void UWeaponAnimationSubsystem::Deinitialize()
{
    UE_LOG(LogWeaponAnimation, Log, TEXT("WeaponAnimationSubsystem: Deinitializing..."));

    // Выводим финальную статистику кэша
    if (CacheHits > 0 || CacheMisses > 0)
    {
        float HitRate = 0.0f;
        int32 MemoryUsage = 0;
        int32 CacheEntries = 0;
        GetCacheMetrics(HitRate, MemoryUsage, CacheEntries);

        UE_LOG(LogWeaponAnimation, Log,
            TEXT("WeaponAnimationSubsystem: Final cache stats - Hit Rate: %.2f%%, Memory: %d bytes, Entries: %d, Evictions: %d"),
            HitRate, MemoryUsage, CacheEntries, CacheEvictions);
    }

    // Очищаем все данные
    ClearAnimationCache();
    LoadedAnimationData.Empty();
    AnimationDataTable = nullptr;
    bIsInitialized = false;

    Super::Deinitialize();
}

bool UWeaponAnimationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // Всегда создаем subsystem для игр
    return true;
}

//==================================================================
// Primary Data Loading
//==================================================================

bool UWeaponAnimationSubsystem::LoadAnimationDataTable(UDataTable* InDataTable)
{
    if (!InDataTable)
    {
        UE_LOG(LogWeaponAnimation, Error, TEXT("LoadAnimationDataTable: Cannot load null DataTable"));
        return false;
    }

    // Проверяем структуру строк
    const UScriptStruct* RowStruct = InDataTable->GetRowStruct();
    if (!RowStruct)
    {
        UE_LOG(LogWeaponAnimation, Error, TEXT("LoadAnimationDataTable: DataTable has no row structure"));
        return false;
    }

    // Проверяем что это правильная структура
    if (!RowStruct->IsChildOf(FAnimationStateData::StaticStruct()))
    {
        UE_LOG(LogWeaponAnimation, Error,
            TEXT("LoadAnimationDataTable: Invalid row structure. Expected: FAnimationStateData, Got: %s"),
            *RowStruct->GetName());
        return false;
    }

    // Очищаем старые данные
    LoadedAnimationData.Empty();
    ClearAnimationCache();

    AnimationDataTable = InDataTable;

    // Загружаем все строки в память
    UE_LOG(LogWeaponAnimation, Log, TEXT("LoadAnimationDataTable: Loading animation data from %s"),
        *InDataTable->GetName());

    InDataTable->ForeachRow<FAnimationStateData>(TEXT("LoadAnimationData"),
        [this](const FName& Key, const FAnimationStateData& Value)
        {
            // Сохраняем указатель на данные
            LoadedAnimationData.Add(Key, &Value);

            // Проверяем наличие критических анимаций
            bool bHasCriticalAnims = (Value.Draw != nullptr &&
                                      Value.Holster != nullptr &&
                                      Value.Idle != nullptr &&
                                      Value.Stance != nullptr);

            if (bHasCriticalAnims)
            {
                UE_LOG(LogWeaponAnimation, Log,
                    TEXT("  Loaded animations for %s [OK]"), *Key.ToString());
            }
            else
            {
                UE_LOG(LogWeaponAnimation, Warning,
                    TEXT("  Loaded animations for %s [INCOMPLETE - missing critical animations]"), *Key.ToString());

                if (!Value.Draw) UE_LOG(LogWeaponAnimation, Warning, TEXT("    - Missing Draw montage"));
                if (!Value.Holster) UE_LOG(LogWeaponAnimation, Warning, TEXT("    - Missing Holster montage"));
                if (!Value.Idle) UE_LOG(LogWeaponAnimation, Warning, TEXT("    - Missing Idle sequence"));
                if (!Value.Stance) UE_LOG(LogWeaponAnimation, Warning, TEXT("    - Missing Stance blendspace"));
            }

            // Оповещаем о загрузке
            FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag(Key, false);
            if (WeaponTag.IsValid())
            {
                OnAnimationDataLoaded.Broadcast(WeaponTag);
            }
        });

    bIsInitialized = LoadedAnimationData.Num() > 0;

    UE_LOG(LogWeaponAnimation, Log,
        TEXT("LoadAnimationDataTable: Successfully loaded %d weapon animation sets"),
        LoadedAnimationData.Num());

    // Выводим сводку по типам оружия
    if (bIsInitialized && bEnableDetailedLogging)
    {
        LogSystemState();
    }

    return bIsInitialized;
}

//==================================================================
// C++ Performance Implementation
//==================================================================

const FAnimationStateData* UWeaponAnimationSubsystem::GetAnimationStateDataPtr(const FGameplayTag& WeaponType) const
{
    if (!bIsInitialized)
    {
        UE_LOG(LogWeaponAnimation, Warning, TEXT("GetAnimationStateDataPtr: Subsystem not initialized"));
        return nullptr;
    }

    // Строим ключ кэша
    FString CacheKey = BuildCacheKey(WeaponType);

    // Проверяем кэш с потокобезопасностью
    {
        FScopeLock Lock(&CacheCriticalSection);

        if (FWeaponAnimationCacheEntry* CachedEntry = AnimationCache.Find(CacheKey))
        {
            if (CachedEntry->bIsValid)
            {
                float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

                // Проверяем TTL кэша
                if ((CurrentTime - CachedEntry->CacheTime) < CacheLifetime)
                {
                    // Cache hit
                    CachedEntry->HitCount++;
                    CachedEntry->LastAccessTime = CurrentTime;
                    CacheHits++;

                    if (bEnableDetailedLogging)
                    {
                        UE_LOG(LogWeaponAnimation, VeryVerbose,
                            TEXT("Cache HIT for %s (hits: %d)"), *WeaponType.ToString(), CachedEntry->HitCount);
                    }

                    return CachedEntry->AnimationData;
                }
                else
                {
                    // Кэш устарел
                    CachedEntry->bIsValid = false;
                }
            }
        }
    }

    // Cache miss
    CacheMisses++;

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogWeaponAnimation, VeryVerbose, TEXT("Cache MISS for %s"), *WeaponType.ToString());
    }

    // Ищем данные в загруженной таблице
    const FAnimationStateData* FoundData = FindAnimationData(WeaponType);

    if (FoundData)
    {
        // Обновляем кэш
        UpdateCache(CacheKey, FoundData);
    }

    return FoundData;
}

void UWeaponAnimationSubsystem::PreloadAnimationDataBatch(const TArray<FGameplayTag>& WeaponTypes)
{
    UE_LOG(LogWeaponAnimation, Log, TEXT("PreloadAnimationDataBatch: Preloading %d weapon types"), WeaponTypes.Num());

    int32 SuccessCount = 0;
    for (const FGameplayTag& WeaponType : WeaponTypes)
    {
        if (GetAnimationStateDataPtr(WeaponType))
        {
            SuccessCount++;
        }
    }

    UE_LOG(LogWeaponAnimation, Log, TEXT("PreloadAnimationDataBatch: Successfully preloaded %d/%d weapon types"),
        SuccessCount, WeaponTypes.Num());
}

void UWeaponAnimationSubsystem::GetCacheMetrics(float& OutHitRate, int32& OutMemoryUsageBytes, int32& OutCacheEntries) const
{
    FScopeLock Lock(&CacheCriticalSection);

    // Вычисляем hit rate
    int32 TotalAccesses = CacheHits + CacheMisses;
    OutHitRate = TotalAccesses > 0 ? (float(CacheHits) / float(TotalAccesses)) * 100.0f : 0.0f;

    // Вычисляем использование памяти
    OutMemoryUsageBytes = CalculateMemoryUsage();

    // Количество записей в кэше
    OutCacheEntries = AnimationCache.Num();

    // Оповещаем об обновлении метрик
    const_cast<UWeaponAnimationSubsystem*>(this)->OnCacheMetricsUpdated.Broadcast(OutHitRate, OutCacheEntries);
}

//==================================================================
// Blueprint-Safe Implementation
//==================================================================

bool UWeaponAnimationSubsystem::GetAnimationStateData_Implementation(const FGameplayTag& WeaponType, FAnimationStateData& OutAnimationData) const
{
    const FAnimationStateData* DataPtr = GetAnimationStateDataPtr(WeaponType);

    if (DataPtr)
    {
        // Копируем данные для безопасности Blueprint
        OutAnimationData = *DataPtr;
        return true;
    }

    return false;
}

UAnimMontage* UWeaponAnimationSubsystem::GetDrawMontage_Implementation(const FGameplayTag& WeaponType, bool bFirstDraw) const
{
    const FAnimationStateData* AnimData = GetAnimationStateDataPtr(WeaponType);
    if (AnimData)
    {
        // Возвращаем FirstDraw если есть и запрошен, иначе обычный Draw
        if (bFirstDraw && AnimData->FirstDraw)
        {
            return AnimData->FirstDraw.Get();
        }
        return AnimData->Draw.Get();
    }
    return nullptr;
}

UAnimMontage* UWeaponAnimationSubsystem::GetHolsterMontage_Implementation(const FGameplayTag& WeaponType) const
{
    const FAnimationStateData* AnimData = GetAnimationStateDataPtr(WeaponType);
    return AnimData ? AnimData->Holster.Get() : nullptr;
}

UBlendSpace* UWeaponAnimationSubsystem::GetStanceBlendSpace_Implementation(const FGameplayTag& WeaponType) const
{
    const FAnimationStateData* AnimData = GetAnimationStateDataPtr(WeaponType);
    return AnimData ? AnimData->Stance.Get() : nullptr;
}

UAnimSequence* UWeaponAnimationSubsystem::GetIdleAnimation_Implementation(const FGameplayTag& WeaponType) const
{
    const FAnimationStateData* AnimData = GetAnimationStateDataPtr(WeaponType);
    return AnimData ? AnimData->Idle.Get() : nullptr;
}

UAnimMontage* UWeaponAnimationSubsystem::GetSwitchMontage_Implementation(const FGameplayTag& FromWeaponType, const FGameplayTag& ToWeaponType) const
{
    // В будущем здесь можно реализовать матрицу переходов
    // Пока возвращаем nullptr для использования стандартной последовательности holster + draw
    return nullptr;
}

UAnimMontage* UWeaponAnimationSubsystem::GetReloadMontage_Implementation(const FGameplayTag& WeaponType, bool bIsEmpty) const
{
    const FAnimationStateData* AnimData = GetAnimationStateDataPtr(WeaponType);
    if (AnimData)
    {
        return bIsEmpty ? AnimData->ReloadLong.Get() : AnimData->ReloadShort.Get();
    }
    return nullptr;
}

FTransform UWeaponAnimationSubsystem::GetWeaponTransform_Implementation(const FGameplayTag& WeaponType) const
{
    const FAnimationStateData* AnimData = GetAnimationStateDataPtr(WeaponType);
    return AnimData ? AnimData->WTransform : FTransform::Identity;
}

FTransform UWeaponAnimationSubsystem::GetLeftHandGripTransform_Implementation(const FGameplayTag& WeaponType, int32 GripIndex) const
{
    const FAnimationStateData* AnimData = GetAnimationStateDataPtr(WeaponType);
    if (AnimData)
    {
        return AnimData->GetLeftHandGripTransform(GripIndex);
    }
    return FTransform::Identity;
}

FTransform UWeaponAnimationSubsystem::GetRightHandTransform_Implementation(const FGameplayTag& WeaponType) const
{
    const FAnimationStateData* AnimData = GetAnimationStateDataPtr(WeaponType);
    return AnimData ? AnimData->RHTransform : FTransform::Identity;
}

float UWeaponAnimationSubsystem::GetDrawDuration_Implementation(const FGameplayTag& WeaponType, bool bFirstDraw) const
{
    UAnimMontage* DrawMontage = GetDrawMontage_Implementation(WeaponType, bFirstDraw);
    if (DrawMontage)
    {
        return DrawMontage->GetPlayLength();
    }
    return 0.5f; // Дефолтная длительность для безопасности
}

float UWeaponAnimationSubsystem::GetHolsterDuration_Implementation(const FGameplayTag& WeaponType) const
{
    UAnimMontage* HolsterMontage = GetHolsterMontage_Implementation(WeaponType);
    if (HolsterMontage)
    {
        return HolsterMontage->GetPlayLength();
    }
    return 0.5f; // Дефолтная длительность для безопасности
}

float UWeaponAnimationSubsystem::GetSwitchDuration_Implementation(const FGameplayTag& FromWeaponType, const FGameplayTag& ToWeaponType) const
{
    // Проверяем специализированный переход
    UAnimMontage* SwitchMontage = GetSwitchMontage_Implementation(FromWeaponType, ToWeaponType);
    if (SwitchMontage)
    {
        return SwitchMontage->GetPlayLength();
    }

    // Стандартная последовательность: holster + draw
    float HolsterDuration = GetHolsterDuration_Implementation(FromWeaponType);
    float DrawDuration = GetDrawDuration_Implementation(ToWeaponType, false);
    return HolsterDuration + DrawDuration;
}

float UWeaponAnimationSubsystem::GetReloadDuration_Implementation(const FGameplayTag& WeaponType, bool bIsEmpty) const
{
    UAnimMontage* ReloadMontage = GetReloadMontage_Implementation(WeaponType, bIsEmpty);
    if (ReloadMontage)
    {
        return ReloadMontage->GetPlayLength();
    }
    return bIsEmpty ? 3.0f : 2.0f; // Дефолтные длительности перезарядки
}

bool UWeaponAnimationSubsystem::HasAnimationData_Implementation(const FGameplayTag& WeaponType) const
{
    return GetAnimationStateDataPtr(WeaponType) != nullptr;
}

bool UWeaponAnimationSubsystem::ValidateAnimationData_Implementation(const FGameplayTag& WeaponType, TArray<FString>& OutErrors) const
{
    OutErrors.Empty();

    const FAnimationStateData* AnimData = GetAnimationStateDataPtr(WeaponType);
    if (!AnimData)
    {
        OutErrors.Add(FString::Printf(TEXT("No animation data found for weapon type: %s"), *WeaponType.ToString()));
        return false;
    }

    bool bIsValid = true;

    // Проверяем критические анимации
    if (!ValidateMontage(AnimData->Draw.Get(), TEXT("Draw"), OutErrors))
    {
        bIsValid = false;
    }

    if (!ValidateMontage(AnimData->Holster.Get(), TEXT("Holster"), OutErrors))
    {
        bIsValid = false;
    }

    if (!AnimData->Idle)
    {
        OutErrors.Add(TEXT("Missing Idle animation sequence"));
        bIsValid = false;
    }

    if (!AnimData->Stance)
    {
        OutErrors.Add(TEXT("Missing Stance blend space"));
        bIsValid = false;
    }

    // Проверяем опциональные анимации (только предупреждения)
    if (!AnimData->FirstDraw)
    {
        OutErrors.Add(TEXT("Warning: Missing FirstDraw animation (will use regular Draw)"));
    }

    if (!AnimData->ReloadShort)
    {
        OutErrors.Add(TEXT("Warning: Missing ReloadShort animation"));
    }

    if (!AnimData->ReloadLong)
    {
        OutErrors.Add(TEXT("Warning: Missing ReloadLong animation"));
    }

    if (!AnimData->AimPose)
    {
        OutErrors.Add(TEXT("Warning: Missing AimPose animation"));
    }

    return bIsValid;
}

TArray<FGameplayTag> UWeaponAnimationSubsystem::GetAvailableWeaponTypes_Implementation() const
{
    TArray<FGameplayTag> AvailableTypes;

    for (const auto& DataPair : LoadedAnimationData)
    {
        FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag(DataPair.Key, false);
        if (WeaponTag.IsValid())
        {
            AvailableTypes.Add(WeaponTag);
        }
    }

    return AvailableTypes;
}

bool UWeaponAnimationSubsystem::HasSwitchAnimation_Implementation(const FGameplayTag& FromWeaponType, const FGameplayTag& ToWeaponType) const
{
    // Проверяем наличие специализированной анимации перехода
    UAnimMontage* SwitchMontage = GetSwitchMontage_Implementation(FromWeaponType, ToWeaponType);
    return SwitchMontage != nullptr;
}

//==================================================================
// Public Methods
//==================================================================

void UWeaponAnimationSubsystem::ClearAnimationCache()
{
    FScopeLock Lock(&CacheCriticalSection);

    // Оповещаем об очистке
    for (const auto& CacheEntry : AnimationCache)
    {
        FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag(FName(*CacheEntry.Key), false);
        if (WeaponTag.IsValid())
        {
            OnAnimationDataCleared.Broadcast(WeaponTag);
        }
    }

    AnimationCache.Empty();

    // Сбрасываем статистику
    CacheHits = 0;
    CacheMisses = 0;
    CacheEvictions = 0;

    UE_LOG(LogWeaponAnimation, Log, TEXT("ClearAnimationCache: Animation cache cleared"));
}

void UWeaponAnimationSubsystem::GetCacheStatistics(int32& OutCacheSize, int32& OutMemoryUsage) const
{
    FScopeLock Lock(&CacheCriticalSection);

    OutCacheSize = AnimationCache.Num();
    OutMemoryUsage = CalculateMemoryUsage();
}

void UWeaponAnimationSubsystem::PreloadWeaponAnimations(const TArray<FGameplayTag>& WeaponTypes)
{
    PreloadAnimationDataBatch(WeaponTypes);
}

FString UWeaponAnimationSubsystem::GetDebugInfo() const
{
    FScopeLock Lock(&CacheCriticalSection);

    float HitRate = 0.0f;
    int32 MemoryUsage = 0;
    int32 CacheEntries = 0;
    GetCacheMetrics(HitRate, MemoryUsage, CacheEntries);

    FString DebugInfo = FString::Printf(
        TEXT("WeaponAnimationSubsystem Debug Info:\n")
        TEXT("  Initialized: %s\n")
        TEXT("  DataTable: %s\n")
        TEXT("  Loaded Animations: %d\n")
        TEXT("  Cache Entries: %d/%d\n")
        TEXT("  Cache Hit Rate: %.2f%%\n")
        TEXT("  Cache Hits/Misses: %d/%d\n")
        TEXT("  Cache Evictions: %d\n")
        TEXT("  Memory Usage: %d bytes\n"),
        bIsInitialized ? TEXT("Yes") : TEXT("No"),
        AnimationDataTable ? *AnimationDataTable->GetName() : TEXT("None"),
        LoadedAnimationData.Num(),
        CacheEntries, MaxCacheSize,
        HitRate,
        CacheHits, CacheMisses,
        CacheEvictions,
        MemoryUsage
    );

    return DebugInfo;
}

//==================================================================
// Protected Methods
//==================================================================

const FAnimationStateData* UWeaponAnimationSubsystem::FindAnimationData(const FGameplayTag& WeaponType) const
{
    // Сначала ищем точное совпадение
    FName RowName = WeaponType.GetTagName();

    // Используем auto для автоматического вывода правильного типа
    // или явно указываем полный const-корректный тип
    if (const auto Found = LoadedAnimationData.Find(RowName))
    {
        if (bEnableDetailedLogging)
        {
            UE_LOG(LogWeaponAnimation, VeryVerbose, TEXT("FindAnimationData: Exact match for %s"), *WeaponType.ToString());
        }
        return *Found;
    }

    // Поддержка наследования тегов - ищем по родительским тегам
    // Например: Weapon.Type.Rifle.AK47 -> Weapon.Type.Rifle -> Weapon.Type
    FGameplayTag ParentTag = WeaponType.RequestDirectParent();
    int32 ParentCheckCount = 0;

    while (ParentTag.IsValid())
    {
        RowName = ParentTag.GetTagName();
        ParentCheckCount++;

        // Снова используем auto для правильной типизации
        if (const auto Found = LoadedAnimationData.Find(RowName))
        {
            UE_LOG(LogWeaponAnimation, Log,
                TEXT("FindAnimationData: Using parent animations %s for %s (checked %d levels up)"),
                *ParentTag.ToString(), *WeaponType.ToString(), ParentCheckCount);
            return *Found;
        }

        ParentTag = ParentTag.RequestDirectParent();
    }

    // Логируем детальную информацию о неудачном поиске
    UE_LOG(LogWeaponAnimation, Warning,
        TEXT("FindAnimationData: No animation data found for %s (checked tag and %d parent levels)"),
        *WeaponType.ToString(), ParentCheckCount);

    // В режиме отладки можем вывести список доступных анимаций
    if (bEnableDetailedLogging && LoadedAnimationData.Num() > 0)
    {
        FString AvailableTags;
        for (const auto& Pair : LoadedAnimationData)
        {
            if (!AvailableTags.IsEmpty())
            {
                AvailableTags += TEXT(", ");
            }
            AvailableTags += Pair.Key.ToString();
        }
        UE_LOG(LogWeaponAnimation, Warning,
            TEXT("  Available animation sets: [%s]"), *AvailableTags);
    }

    return nullptr;
}

FString UWeaponAnimationSubsystem::BuildCacheKey(const FGameplayTag& WeaponType) const
{
    return WeaponType.ToString();
}

void UWeaponAnimationSubsystem::UpdateCache(const FString& CacheKey, const FAnimationStateData* Data) const
{
    FScopeLock Lock(&CacheCriticalSection);

    // Проверяем нужно ли освободить место
    if (AnimationCache.Num() >= MaxCacheSize)
    {
        EvictLRUCacheEntry();
    }

    // Добавляем или обновляем запись
    FWeaponAnimationCacheEntry& NewEntry = AnimationCache.FindOrAdd(CacheKey);
    NewEntry.AnimationData = Data;
    NewEntry.CacheTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    NewEntry.LastAccessTime = NewEntry.CacheTime;
    NewEntry.bIsValid = true;
    NewEntry.HitCount = 0;

    if (bEnableDetailedLogging)
    {
        UE_LOG(LogWeaponAnimation, VeryVerbose, TEXT("UpdateCache: Cached data for %s"), *CacheKey);
    }
}

void UWeaponAnimationSubsystem::EvictLRUCacheEntry() const
{
    FString LRUKey;
    float OldestAccessTime = FLT_MAX;
    int32 MinHitCount = INT32_MAX;

    // Находим наименее используемую запись
    for (const auto& Pair : AnimationCache)
    {
        // Защищаем часто используемые записи
        if (Pair.Value.HitCount >= MinHitCountForProtection)
        {
            continue;
        }

        // Приоритет: сначала по количеству использований, потом по времени доступа
        if (Pair.Value.HitCount < MinHitCount ||
            (Pair.Value.HitCount == MinHitCount && Pair.Value.LastAccessTime < OldestAccessTime))
        {
            MinHitCount = Pair.Value.HitCount;
            OldestAccessTime = Pair.Value.LastAccessTime;
            LRUKey = Pair.Key;
        }
    }

    if (!LRUKey.IsEmpty())
    {
        AnimationCache.Remove(LRUKey);
        CacheEvictions++;

        if (bEnableDetailedLogging)
        {
            UE_LOG(LogWeaponAnimation, VeryVerbose,
                TEXT("EvictLRUCacheEntry: Evicted %s (hits: %d)"), *LRUKey, MinHitCount);
        }
    }
}

bool UWeaponAnimationSubsystem::ValidateMontage(const UAnimMontage* Montage, const FString& AnimationName, TArray<FString>& OutErrors) const
{
    if (!Montage)
    {
        OutErrors.Add(FString::Printf(TEXT("Missing %s animation montage"), *AnimationName));
        return false;
    }

    if (Montage->GetPlayLength() <= 0.0f)
    {
        OutErrors.Add(FString::Printf(TEXT("%s animation has invalid length (%.2f)"),
            *AnimationName, Montage->GetPlayLength()));
        return false;
    }

    // Дополнительная проверка на валидность ассета
    if (Montage->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
    {
        OutErrors.Add(FString::Printf(TEXT("%s animation montage is being destroyed"), *AnimationName));
        return false;
    }

    return true;
}

int32 UWeaponAnimationSubsystem::CalculateMemoryUsage() const
{
    int32 TotalMemory = 0;

    // Размер записей кэша
    TotalMemory += AnimationCache.Num() * sizeof(FWeaponAnimationCacheEntry);

    // Размер загруженных данных (приблизительно)
    TotalMemory += LoadedAnimationData.Num() * sizeof(FAnimationStateData);

    // Overhead структур данных
    TotalMemory += AnimationCache.GetAllocatedSize();
    TotalMemory += LoadedAnimationData.GetAllocatedSize();

    // Дополнительный overhead на строки в кэше
    for (const auto& Pair : AnimationCache)
    {
        TotalMemory += Pair.Key.GetAllocatedSize();
    }

    return TotalMemory;
}

void UWeaponAnimationSubsystem::LogSystemState() const
{
    UE_LOG(LogWeaponAnimation, Log, TEXT("=== WeaponAnimationSubsystem State ==="));
    UE_LOG(LogWeaponAnimation, Log, TEXT("  Loaded weapon types:"));

    for (const auto& DataPair : LoadedAnimationData)
    {
        const FAnimationStateData* Data = DataPair.Value;
        if (Data)
        {
            int32 AnimCount = 0;
            if (Data->Draw) AnimCount++;
            if (Data->Holster) AnimCount++;
            if (Data->FirstDraw) AnimCount++;
            if (Data->ReloadShort) AnimCount++;
            if (Data->ReloadLong) AnimCount++;
            if (Data->Idle) AnimCount++;
            if (Data->Stance) AnimCount++;
            if (Data->AimPose) AnimCount++;

            UE_LOG(LogWeaponAnimation, Log, TEXT("    %s - %d animations configured"),
                *DataPair.Key.ToString(), AnimCount);
        }
    }

    UE_LOG(LogWeaponAnimation, Log, TEXT("  Cache state: %d entries"), AnimationCache.Num());
    UE_LOG(LogWeaponAnimation, Log, TEXT("======================================="));
}
