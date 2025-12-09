// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "Abilities/GameplayAbility.h"
#include "ISuspenseFireModeProvider.generated.h"

// Forward declarations
class USuspenseCoreEventManager;
struct FWeaponFireModeData;
struct FSuspenseUnifiedItemData;

/**
 * Данные о режиме огня, загруженные из DataTable
 * Расширяет FWeaponFireModeData дополнительной runtime информацией
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FFireModeRuntimeData
{
    GENERATED_BODY()
    
    /** Тег режима огня */
    UPROPERTY(BlueprintReadOnly)
    FGameplayTag FireModeTag;
    
    /** Отображаемое название */
    UPROPERTY(BlueprintReadOnly)
    FText DisplayName;
    
    /** Класс способности режима огня */
    UPROPERTY(BlueprintReadOnly)
    TSubclassOf<UGameplayAbility> FireModeAbility;
    
    /** ID привязки ввода */
    UPROPERTY(BlueprintReadOnly)
    int32 InputID = 0;
    
    /** Включен ли режим */
    UPROPERTY(BlueprintReadOnly)
    bool bEnabled = true;
    
    /** Активен ли в данный момент */
    UPROPERTY(BlueprintReadOnly)
    bool bIsActive = false;
    
    /** Доступен ли для переключения */
    UPROPERTY(BlueprintReadOnly)
    bool bIsAvailable = true;
    
    /** Индекс в массиве режимов */
    UPROPERTY(BlueprintReadOnly)
    int32 Index = -1;
    
    FFireModeRuntimeData() = default;
    
    /** Конструктор из данных DataTable */
    FFireModeRuntimeData(const FWeaponFireModeData& DataTableData, int32 InIndex)
        : FireModeTag(DataTableData.FireModeTag)
        , DisplayName(DataTableData.DisplayName)
        , FireModeAbility(DataTableData.FireModeAbility)
        , InputID(DataTableData.InputID)
        , bEnabled(DataTableData.bEnabled)
        , bIsActive(false)
        , bIsAvailable(DataTableData.bEnabled)
        , Index(InIndex)
    {
    }
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseFireModeProvider : public UInterface
{
    GENERATED_BODY()
};

/**
 * Интерфейс провайдера режимов огня
 * 
 * Управляет режимами огня оружия, загруженными из DataTable
 * Работает в связке с ISuspenseWeapon для полной функциональности
 * 
 * Архитектура:
 * - Режимы огня определяются в FSuspenseUnifiedItemData.FireModes
 * - Runtime состояние отслеживается через FFireModeRuntimeData
 * - Переключение режимов транслируется через EventDelegateManager
 */
class BRIDGESYSTEM_API ISuspenseFireModeProvider
{
    GENERATED_BODY()

public:
    //================================================
    // Инициализация из DataTable
    //================================================
    
    /**
     * Инициализировать провайдер данными оружия из DataTable
     * @param WeaponData - данные оружия из таблицы
     * @return true если инициализация успешна
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Initialization")
    bool InitializeFromWeaponData(const FSuspenseUnifiedItemData& WeaponData);
    
    /**
     * Очистить все режимы огня
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Initialization")
    void ClearFireModes();

    //================================================
    // Управление режимами огня
    //================================================
    
    /**
     * Переключить на следующий доступный режим огня
     * @return true если переключение успешно
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Control")
    bool CycleToNextFireMode();
    
    /**
     * Переключить на предыдущий доступный режим огня
     * @return true если переключение успешно
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Control")
    bool CycleToPreviousFireMode();
    
    /**
     * Установить конкретный режим огня
     * @param FireModeTag - тег режима огня для активации
     * @return true если режим успешно установлен
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Control")
    bool SetFireMode(const FGameplayTag& FireModeTag);
    
    /**
     * Установить режим огня по индексу
     * @param Index - индекс режима в массиве
     * @return true если режим установлен
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Control")
    bool SetFireModeByIndex(int32 Index);

    //================================================
    // Запросы состояния
    //================================================
    
    /**
     * Получить текущий активный режим огня
     * @return Тег текущего режима
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Query")
    FGameplayTag GetCurrentFireMode() const;
    
    /**
     * Получить данные текущего режима огня
     * @return Runtime данные активного режима
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Query")
    FFireModeRuntimeData GetCurrentFireModeData() const;
    
    /**
     * Проверить доступность режима огня
     * @param FireModeTag - проверяемый режим
     * @return true если режим доступен для переключения
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Query")
    bool IsFireModeAvailable(const FGameplayTag& FireModeTag) const;
    
    /**
     * Получить все загруженные режимы огня
     * @return Массив всех режимов с runtime данными
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Query")
    TArray<FFireModeRuntimeData> GetAllFireModes() const;
    
    /**
     * Получить только доступные режимы огня
     * @return Массив тегов доступных режимов
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Query")
    TArray<FGameplayTag> GetAvailableFireModes() const;
    
    /**
     * Получить количество доступных режимов
     * @return Количество режимов, доступных для переключения
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Query")
    int32 GetAvailableFireModeCount() const;

    //================================================
    // Динамическое управление доступностью
    //================================================
    
    /**
     * Включить или выключить режим огня
     * @param FireModeTag - режим для изменения
     * @param bEnabled - новое состояние доступности
     * @return true если состояние изменено
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Control")
    bool SetFireModeEnabled(const FGameplayTag& FireModeTag, bool bEnabled);
    
    /**
     * Временно заблокировать режим огня
     * @param FireModeTag - режим для блокировки
     * @param bBlocked - заблокировать или разблокировать
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Control")
    void SetFireModeBlocked(const FGameplayTag& FireModeTag, bool bBlocked);
    
    /**
     * Проверить, заблокирован ли режим
     * @param FireModeTag - проверяемый режим
     * @return true если режим временно заблокирован
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Query")
    bool IsFireModeBlocked(const FGameplayTag& FireModeTag) const;

    //================================================
    // Получение данных режима
    //================================================
    
    /**
     * Получить runtime данные конкретного режима огня
     * @param FireModeTag - запрашиваемый режим
     * @param OutData - выходные данные режима
     * @return true если режим найден
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Query")
    bool GetFireModeData(const FGameplayTag& FireModeTag, FFireModeRuntimeData& OutData) const;
    
    /**
     * Получить способность для режима огня
     * @param FireModeTag - режим огня
     * @return Класс способности или nullptr
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Query")
    TSubclassOf<UGameplayAbility> GetFireModeAbility(const FGameplayTag& FireModeTag) const;
    
    /**
     * Получить ID ввода для режима огня
     * @param FireModeTag - режим огня
     * @return ID привязки ввода
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "FireMode|Query")
    int32 GetFireModeInputID(const FGameplayTag& FireModeTag) const;

    //================================================
    // Интеграция с системой событий
    //================================================
    
    /**
     * Получить центральный менеджер делегатов для событий режимов огня
     * @return Указатель на менеджер делегатов или nullptr
     */
    virtual USuspenseCoreEventManager* GetDelegateManager() const = 0;

    /**
     * Статический хелпер для получения менеджера делегатов
     * @param WorldContextObject - любой объект с валидным контекстом мира
     * @return Менеджер делегатов или nullptr
     */
    static USuspenseCoreEventManager* GetDelegateManagerStatic(const UObject* WorldContextObject);
    
    /**
     * Хелпер для безопасной трансляции изменений режима огня
     * @param FireModeProvider - объект провайдера
     * @param NewFireMode - новый режим огня
     * @param CurrentSpread - текущий разброс оружия
     */
    static void BroadcastFireModeChanged(
        const UObject* FireModeProvider,
        const FGameplayTag& NewFireMode,
        float CurrentSpread);
    
    /**
     * Транслировать изменение доступности режима
     * @param FireModeProvider - объект провайдера
     * @param FireModeTag - затронутый режим
     * @param bEnabled - новое состояние доступности
     */
    static void BroadcastFireModeAvailabilityChanged(
        const UObject* FireModeProvider,
        const FGameplayTag& FireModeTag,
        bool bEnabled);
};
