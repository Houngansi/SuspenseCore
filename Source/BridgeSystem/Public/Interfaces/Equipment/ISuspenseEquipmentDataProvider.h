// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"

#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "Types/Equipment/SuspenseEquipmentTypes.h" // Единый источник истины для снапшотов и конфигов

#include "ISuspenseEquipmentDataProvider.generated.h"

// Делегаты
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSlotDataChanged, int32 /*SlotIndex*/, const FSuspenseInventoryItemInstance& /*NewData*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSlotConfigurationChanged, int32 /*SlotIndex*/);
DECLARE_MULTICAST_DELEGATE(FOnDataStoreReset);

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseEquipmentDataProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for equipment data storage and access
 *
 * Архитектура:
 * - Контракт на доступ/модификацию данных экипировки
 * - Снимки состояния для транзакций/отката
 * - Наблюдаемые события на изменения
 *
 * Важно:
 * - Ключевые методы (получение/установка данных) — чисто виртуальные
 * - Удобные запросы (поиск слотов, вес и т.п.) имеют дефолтные реализации
 *   поверх базовых методов и могут быть переопределены в конкретном сторе
 */
class BRIDGESYSTEM_API ISuspenseEquipmentDataProvider
{
    GENERATED_BODY()

public:
    //========================================
    // Slot Data Access (обязательные к реализации)
    //========================================

    /** Текущий предмет в слоте (или невалидный экземпляр) */
    virtual FSuspenseInventoryItemInstance GetSlotItem(int32 SlotIndex) const = 0;

    /** Конфигурация указанного слота */
    virtual FEquipmentSlotConfig GetSlotConfiguration(int32 SlotIndex) const = 0;

    /** Все конфигурации слотов */
    virtual TArray<FEquipmentSlotConfig> GetAllSlotConfigurations() const = 0;

    /** Все экипированные предметы: SlotIndex -> Instance */
    virtual TMap<int32, FSuspenseInventoryItemInstance> GetAllEquippedItems() const = 0;

    /** Кол-во слотов */
    virtual int32 GetSlotCount() const = 0;

    /** Валиден ли индекс слота */
    virtual bool IsValidSlotIndex(int32 SlotIndex) const = 0;

    /** Занят ли слот предметом */
    virtual bool IsSlotOccupied(int32 SlotIndex) const = 0;

    //========================================
    // Data Modification (обязательные к реализации)
    //========================================

    /** Установить предмет в слот */
    virtual bool SetSlotItem(int32 SlotIndex, const FSuspenseInventoryItemInstance& ItemInstance, bool bNotifyObservers = true) = 0;

    /** Очистить слот и вернуть удалённый предмет */
    virtual FSuspenseInventoryItemInstance ClearSlot(int32 SlotIndex, bool bNotifyObservers = true) = 0;

    /** Инициализировать конфигурации слотов */
    virtual bool InitializeSlots(const TArray<FEquipmentSlotConfig>& Configurations) = 0;

    //========================================
    // State Management (обязательные к реализации)
    //========================================

    virtual int32        GetActiveWeaponSlot() const = 0;
    virtual bool         SetActiveWeaponSlot(int32 SlotIndex) = 0;
    virtual FGameplayTag GetCurrentEquipmentState() const = 0;
    virtual bool         SetEquipmentState(const FGameplayTag& NewState) = 0;

    //========================================
    // Snapshot Management (обязательные к реализации)
    //========================================

    /** Снимок всего состояния экипировки */
    virtual FEquipmentStateSnapshot CreateSnapshot() const = 0;

    /** Восстановление состояния из снимка */
    virtual bool RestoreSnapshot(const FEquipmentStateSnapshot& Snapshot) = 0;

    /** Снимок конкретного слота */
    virtual FEquipmentSlotSnapshot CreateSlotSnapshot(int32 SlotIndex) const = 0;

    //========================================
    // Events (обязательные к реализации)
    //========================================

    virtual FOnSlotDataChanged&           OnSlotDataChanged() = 0;
    virtual FOnSlotConfigurationChanged&  OnSlotConfigurationChanged() = 0;
    virtual FOnDataStoreReset&            OnDataStoreReset() = 0;

    //========================================
    // Queries (дефолтная реализация; можно переопределить)
    //========================================

    /**
     * Найти слоты, которые допускают данный тип предмета (по конфигу).
     * Замечание: по умолчанию НЕ проверяет занятость слота.
     */
    virtual TArray<int32> FindCompatibleSlots(const FGameplayTag& ItemType) const
    {
        TArray<int32> Out;
        const int32 Num = GetSlotCount();
        Out.Reserve(Num);

        for (int32 i = 0; i < Num; ++i)
        {
            if (!IsValidSlotIndex(i))
            {
                continue;
            }

            const FEquipmentSlotConfig Config = GetSlotConfiguration(i);
            if (Config.IsValid() && Config.CanEquipItemType(ItemType))
            {
                Out.Add(i);
            }
        }
        return Out;
    }

    /**
     * Получить индексы слотов по их типу.
     */
    virtual TArray<int32> GetSlotsByType(EEquipmentSlotType EquipmentType) const
    {
        TArray<int32> Out;
        const int32 Num = GetSlotCount();
        Out.Reserve(Num);

        for (int32 i = 0; i < Num; ++i)
        {
            if (!IsValidSlotIndex(i))
            {
                continue;
            }

            const FEquipmentSlotConfig Config = GetSlotConfiguration(i);
            if (Config.SlotType == EquipmentType)
            {
                Out.Add(i);
            }
        }
        return Out;
    }

    /**
     * Найти первый ПУСТОЙ слот указанного типа.
     */
    virtual int32 GetFirstEmptySlotOfType(EEquipmentSlotType EquipmentType) const
    {
        const int32 Num = GetSlotCount();
        for (int32 i = 0; i < Num; ++i)
        {
            if (!IsValidSlotIndex(i))
            {
                continue;
            }

            const FEquipmentSlotConfig Config = GetSlotConfiguration(i);
            if (Config.SlotType == EquipmentType && !IsSlotOccupied(i))
            {
                return i;
            }
        }
        return INDEX_NONE;
    }

    //========================================
    // Utility Methods (дефолтная реализация; можно переопределить)
    //========================================

    /**
     * Суммарный вес экипированных предметов.
     * По умолчанию читает RuntimeProperty "Weight" из экземпляров.
     */
    virtual float GetTotalEquippedWeight() const
    {
        float Total = 0.0f;

        const int32 Num = GetSlotCount();
        for (int32 i = 0; i < Num; ++i)
        {
            if (!IsValidSlotIndex(i) || !IsSlotOccupied(i))
            {
                continue;
            }

            const FSuspenseInventoryItemInstance Item = GetSlotItem(i);
            // Стандартный ключ для веса — "Weight"
            Total += Item.GetRuntimeProperty(TEXT("Weight"), 0.0f);
        }

        return Total;
    }

    /**
     * Проверка требований к предмету для указанного слота.
     * Базовая реализация «разрешает всё» — переопределите под свою логику.
     */
    virtual bool MeetsItemRequirements(const FSuspenseInventoryItemInstance& /*ItemInstance*/, int32 /*SlotIndex*/) const
    {
        return true;
    }

    /**
     * Короткая отладочная сводка по состоянию стора.
     */
    virtual FString GetDebugInfo() const
    {
        int32 Occupied = 0;
        const int32 Num = GetSlotCount();

        for (int32 i = 0; i < Num; ++i)
        {
            if (IsValidSlotIndex(i) && IsSlotOccupied(i))
            {
                ++Occupied;
            }
        }

        const FGameplayTag State = GetCurrentEquipmentState();

        return FString::Printf(
            TEXT("EquipmentDataProvider: Slots=%d, Occupied=%d, Weight=%.2f, State=%s"),
            Num,
            Occupied,
            GetTotalEquippedWeight(),
            *State.ToString()
        );
    }
};
