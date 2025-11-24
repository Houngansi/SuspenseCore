// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Types/Loadout/MedComItemDataTable.h"
#include "Types/Inventory/InventoryTypes.h"
#include "IMedComInventoryItemInterface.generated.h"

// Forward declarations
class UTexture2D;
class UMedComItemManager;
struct FMedComUnifiedItemData;

/**
 * Интерфейс для предметов инвентаря в архитектуре DataTable
 * 
 * АРХИТЕКТУРНЫЕ ПРИНЦИПЫ:
 * - DataTable как единственный источник статических данных
 * - Runtime properties для динамического состояния
 * - Полная интеграция с FInventoryItemInstance
 * - Отказ от legacy UObject подхода
 */
UINTERFACE(MinimalAPI, BlueprintType, meta=(NotBlueprintable))
class UMedComInventoryItemInterface : public UInterface
{
    GENERATED_BODY()
};

class MEDCOMSHARED_API IMedComInventoryItemInterface
{
    GENERATED_BODY()

public:
    //==================================================================
    // Основная идентификация (DataTable-based)
    //==================================================================
    
    /**
     * Получает ID предмета из DataTable
     * Основной идентификатор для связи с статическими данными
     */
    virtual FName GetItemID() const = 0;
    
    /**
     * Получает все статические данные из DataTable
     * Единая точка доступа к конфигурации предмета
     */
    virtual bool GetItemData(FMedComUnifiedItemData& OutItemData) const = 0;
    
    /**
     * Доступ к менеджеру предметов
     * Необходим для работы с DataTable
     */
    virtual UMedComItemManager* GetItemManager() const = 0;
    
    //==================================================================
    // Runtime состояние
    //==================================================================
    
    /**
     * Текущее количество в стеке
     */
    virtual int32 GetAmount() const = 0;
    
    /**
     * Устанавливает количество с валидацией
     * Проверяет MaxStackSize из DataTable
     */
    virtual bool TrySetAmount(int32 NewAmount) = 0;
    
    /**
     * Универсальная система runtime свойств
     * Хранит динамические данные (прочность, патроны и т.д.)
     */
    virtual float GetRuntimeProperty(const FName& PropertyName, float DefaultValue = 0.0f) const = 0;
    virtual void SetRuntimeProperty(const FName& PropertyName, float Value) = 0;
    virtual bool HasRuntimeProperty(const FName& PropertyName) const = 0;
    virtual void ClearRuntimeProperty(const FName& PropertyName) = 0;
    
    //==================================================================
    // Позиционирование в сетке
    //==================================================================
    
    /**
     * Эффективный размер с учетом поворота
     * Базовый размер берется из DataTable
     */
    virtual FVector2D GetEffectiveGridSize() const = 0;
    
    /**
     * Базовый размер из DataTable без учета поворота
     */
    virtual FVector2D GetBaseGridSize() const = 0;
    
    /**
     * Позиция в линейном массиве сетки
     */
    virtual int32 GetAnchorIndex() const = 0;
    virtual void SetAnchorIndex(int32 InAnchorIndex) = 0;
    
    /**
     * Состояние поворота
     */
    virtual bool IsRotated() const = 0;
    virtual void SetRotated(bool bRotated) = 0;
    
    //==================================================================
    // Инициализация
    //==================================================================
    
    /**
     * Основной способ создания предмета
     * Инициализирует из DataTable по ID
     */
    virtual bool InitializeFromID(const FName& InItemID, int32 InAmount) = 0;
    
    /**
     * Проверка состояния инициализации
     */
    virtual bool IsInitialized() const = 0;
    
    //==================================================================
    // Управление состоянием оружия
    //==================================================================
    
    /**
     * Сохраненное состояние патронов
     * Используется при снятии/экипировке оружия
     */
    virtual float GetSavedCurrentAmmo() const = 0;
    virtual float GetSavedRemainingAmmo() const = 0;
    virtual bool HasSavedAmmoState() const = 0;
    virtual void SetSavedAmmoState(float CurrentAmmo, float RemainingAmmo) = 0;
    virtual void ClearSavedAmmoState() = 0;
    
    //==================================================================
    // Удобные методы для частых свойств
    //==================================================================
    
    /**
     * Прочность предмета
     */
    virtual float GetCurrentDurability() const 
    {
        return GetRuntimeProperty(TEXT("Durability"), 0.0f);
    }
    
    virtual float GetMaxDurability() const 
    {
        return GetRuntimeProperty(TEXT("MaxDurability"), 100.0f);
    }
    
    virtual float GetDurabilityPercent() const 
    {
        float MaxDur = GetMaxDurability();
        return MaxDur > 0.0f ? FMath::Clamp(GetCurrentDurability() / MaxDur, 0.0f, 1.0f) : 1.0f;
    }
    
    /**
     * Патроны для оружия
     */
    virtual int32 GetCurrentAmmo() const 
    {
        return FMath::RoundToInt(GetRuntimeProperty(TEXT("Ammo"), 0.0f));
    }
    
    virtual int32 GetMaxAmmo() const 
    {
        return FMath::RoundToInt(GetRuntimeProperty(TEXT("MaxAmmo"), 30.0f));
    }
    
    //==================================================================
    // Визуальные свойства из DataTable
    //==================================================================
    
    /**
     * Иконка предмета
     */
    virtual UTexture2D* GetItemIcon() const
    {
        FMedComUnifiedItemData ItemData;
        if (GetItemData(ItemData) && !ItemData.Icon.IsNull())
        {
            return ItemData.Icon.LoadSynchronous();
        }
        return nullptr;
    }
    
    /**
     * Локализованные тексты
     */
    virtual FText GetDisplayName() const
    {
        FMedComUnifiedItemData ItemData;
        return GetItemData(ItemData) ? ItemData.DisplayName : FText::FromString(GetItemID().ToString());
    }
    
    virtual FText GetDescription() const
    {
        FMedComUnifiedItemData ItemData;
        return GetItemData(ItemData) ? ItemData.Description : FText::GetEmpty();
    }
    
    //==================================================================
    // Физические свойства из DataTable
    //==================================================================
    
    /**
     * Вес за единицу
     */
    virtual float GetWeight() const
    {
        FMedComUnifiedItemData ItemData;
        return GetItemData(ItemData) ? ItemData.Weight : 1.0f;
    }
    
    /**
     * Общий вес стека
     */
    virtual float GetTotalWeight() const
    {
        return GetWeight() * GetAmount();
    }
    
    //==================================================================
    // Типизация и категоризация
    //==================================================================
    
    /**
     * Основной тип предмета
     */
    virtual FGameplayTag GetItemType() const
    {
        FMedComUnifiedItemData ItemData;
        return GetItemData(ItemData) ? ItemData.ItemType : FGameplayTag();
    }
    
    /**
     * Проверки возможностей
     */
    virtual bool IsStackable() const
    {
        FMedComUnifiedItemData ItemData;
        return GetItemData(ItemData) ? ItemData.MaxStackSize > 1 : false;
    }
    
    virtual int32 GetMaxStackSize() const
    {
        FMedComUnifiedItemData ItemData;
        return GetItemData(ItemData) ? ItemData.MaxStackSize : 1;
    }
    
    virtual bool IsEquippable() const
    {
        FMedComUnifiedItemData ItemData;
        return GetItemData(ItemData) ? ItemData.bIsEquippable : false;
    }
    
    virtual bool IsWeapon() const
    {
        FMedComUnifiedItemData ItemData;
        return GetItemData(ItemData) ? ItemData.bIsWeapon : false;
    }
    
    virtual bool IsArmor() const
    {
        FMedComUnifiedItemData ItemData;
        return GetItemData(ItemData) ? ItemData.bIsArmor : false;
    }
    
    //==================================================================
    // Интеграция с новой архитектурой
    //==================================================================
    
    /**
     * Полный доступ к runtime экземпляру
     */
    virtual const FInventoryItemInstance& GetItemInstance() const = 0;
    
    /**
     * Замена runtime экземпляра
     */
    virtual bool SetItemInstance(const FInventoryItemInstance& InInstance) = 0;
    
    /**
     * Уникальный идентификатор экземпляра
     * Критически важен для multiplayer синхронизации
     */
    virtual FGuid GetInstanceID() const = 0;
};