// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/Inventory/ISuspenseInventory.h"
#include "Interfaces/Inventory/ISuspenseInventoryItem.h"
#include "Delegates/SuspenseEventManager.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Types/Inventory/SuspenseInventoryTypes.h"
#include "Types/Loadout/SuspenseItemDataTable.h"
#include "GameplayTagContainer.h"

// Создаем log категорию для единообразного логирования во всем модуле
DEFINE_LOG_CATEGORY_STATIC(LogMedComShared, Log, All);

//==================================================================
// Реализация статических вспомогательных методов
// 
// ВАЖНОЕ ЗАМЕЧАНИЕ: Здесь НЕТ ни одного _Implementation метода!
// Все _Implementation методы будут реализованы в конкретных классах,
// которые наследуют этот интерфейс. Это критически важно для
// правильной работы BlueprintNativeEvent системы.
//==================================================================

USuspenseEventManager* ISuspenseInventory::GetDelegateManagerStatic(const UObject* WorldContextObject)
{
    /**
     * Этот метод служит "мостом" к централизованной системе событий.
     * Он работает как "справочная служба" - дает доступ к менеджеру событий
     * из любого места в коде, где есть доступ к миру игры.
     * 
     * Архитектурная важность: В больших проектах разные модули должны
     * общаться друг с другом, не создавая жестких зависимостей.
     * EventDelegateManager служит "почтовой службой" для такого общения.
     */
    
    if (!WorldContextObject)
    {
        UE_LOG(LogMedComShared, Warning, TEXT("GetDelegateManagerStatic: Не предоставлен WorldContextObject"));
        return nullptr;
    }
    
    // Получаем World - это "вселенная" игры, содержащая все игровые объекты
    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        UE_LOG(LogMedComShared, Warning, TEXT("GetDelegateManagerStatic: Не удалось получить мир из контекста"));
        return nullptr;
    }
    
    // GameInstance - это "глобальный менеджер" игры, который живет на протяжении всей сессии
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        UE_LOG(LogMedComShared, Warning, TEXT("GetDelegateManagerStatic: GameInstance не найден"));
        return nullptr;
    }
    
    // Получаем subsystem - это "специализированный отдел" в рамках GameInstance
    USuspenseEventManager* DelegateManager = GameInstance->GetSubsystem<USuspenseEventManager>();
    if (!DelegateManager)
    {
        UE_LOG(LogMedComShared, Error, TEXT("GetDelegateManagerStatic: EventDelegateManager subsystem не найден"));
        UE_LOG(LogMedComShared, Error, TEXT("Убедитесь, что EventDelegateManager правильно зарегистрирован как subsystem"));
    }
    
    return DelegateManager;
}

bool ISuspenseInventory::GetUnifiedDataForBroadcast(
    const FSuspenseInventoryItemInstance& ItemInstance, 
    FSuspenseUnifiedItemData& OutUnifiedData)
{
    /**
     * Этот метод решает важную архитектурную задачу: как получить доступ
     * к DataTable из статического контекста? Статические методы не имеют
     * доступа к "this" указателю, но нам нужны данные из DataTable для
     * создания информативных событий.
     * 
     * Решение: Используем Engine singleton для поиска активного мира,
     * а через него получаем доступ к ItemManager subsystem.
     * 
     * Аналогия: Это как спросить у администратора здания, где находится
     * библиотека, когда вы не знаете планировку здания.
     */
    
    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogMedComShared, Warning, TEXT("GetUnifiedDataForBroadcast: Предоставлен невалидный ItemInstance"));
        return false;
    }
    
    // Проверяем доступность Engine и активных world contexts
    // Engine - это "операционная система" Unreal Engine
    if (GEngine && GEngine->GetWorldContexts().Num() > 0)
    {
        // Получаем первый доступный world context
        // В большинстве игр есть только один мир, но серверы могут иметь несколько
        UWorld* World = GEngine->GetWorldContexts()[0].World();
        if (World && World->GetGameInstance())
        {
            // Получаем ItemManager - "библиотекарь" системы предметов
            USuspenseItemManager* ItemManager = World->GetGameInstance()->GetSubsystem<USuspenseItemManager>();
            if (ItemManager)
            {
                // Пытаемся получить данные предмета из DataTable
                bool bResult = ItemManager->GetUnifiedItemData(ItemInstance.ItemID, OutUnifiedData);
                if (!bResult)
                {
                    UE_LOG(LogMedComShared, Warning, 
                        TEXT("GetUnifiedDataForBroadcast: ItemID '%s' не найден в DataTable"), 
                        *ItemInstance.ItemID.ToString());
                }
                return bResult;
            }
            else
            {
                UE_LOG(LogMedComShared, Error, 
                    TEXT("GetUnifiedDataForBroadcast: ItemManager subsystem недоступен"));
            }
        }
        else
        {
            UE_LOG(LogMedComShared, Warning, 
                TEXT("GetUnifiedDataForBroadcast: Мир или GameInstance недоступны"));
        }
    }
    else
    {
        UE_LOG(LogMedComShared, Warning, 
            TEXT("GetUnifiedDataForBroadcast: Engine или world contexts недоступны"));
    }
    
    return false;
}

void ISuspenseInventory::BroadcastItemAdded(
    const UObject* Inventory,
    const FSuspenseInventoryItemInstance& ItemInstance,
    int32 SlotIndex)
{
    /**
     * Этот метод демонстрирует новую архитектуру событий инвентаря.
     * Вместо передачи всех данных предмета в событии (что создавало бы
     * дублирование данных), мы передаем runtime instance и динамически
     * получаем дополнительную информацию из DataTable при необходимости.
     * 
     * Преимущества этого подхода:
     * 1. Consistency - данные всегда актуальны из единого источника
     * 2. Performance - меньше копирования данных
     * 3. Maintainability - изменения в DataTable автоматически отражаются в событиях
     * 
     * Аналогия: Вместо того чтобы рассказывать всю биографию человека,
     * мы даем его ID, и заинтересованные стороны могут посмотреть детали в базе данных.
     */
    
    if (!Inventory)
    {
        UE_LOG(LogMedComShared, Warning, TEXT("BroadcastItemAdded: Объект инвентаря равен null"));
        return;
    }
    
    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogMedComShared, Warning, TEXT("BroadcastItemAdded: Невалидный ItemInstance"));
        return;
    }
    
    // Получаем доступ к системе событий
    USuspenseEventManager* Manager = GetDelegateManagerStatic(Inventory);
    if (!Manager)
    {
        UE_LOG(LogMedComShared, Warning, TEXT("BroadcastItemAdded: Менеджер событий недоступен"));
        return;
    }
    
    // Получаем обогащенные данные из DataTable для создания информативного события
    FSuspenseUnifiedItemData UnifiedData;
    bool bHasUnifiedData = GetUnifiedDataForBroadcast(ItemInstance, UnifiedData);
    
    // Отправляем общее уведомление об обновлении инвентаря
    // Это "быстрое" уведомление для UI, которому просто нужно обновиться
    Manager->NotifyEquipmentUpdated();
    
    // Создаем детальное событие для систем, которым нужна подробная информация
    FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.ItemAdded"));
    
    FString EventData;
    if (bHasUnifiedData)
    {
        // Создаем богатое событие с данными из DataTable
        EventData = FString::Printf(
            TEXT("Item:%s,DisplayName:%s,Quantity:%d,Slot:%d,Type:%s,Weight:%.2f,InstanceID:%s"), 
            *ItemInstance.ItemID.ToString(),
            *UnifiedData.DisplayName.ToString(),
            ItemInstance.Quantity,
            SlotIndex,
            *UnifiedData.ItemType.ToString(),
            UnifiedData.Weight,
            *ItemInstance.InstanceID.ToString()
        );
    }
    else
    {
        // Fallback версия события, если DataTable недоступна
        EventData = FString::Printf(
            TEXT("Item:%s,Quantity:%d,Slot:%d,InstanceID:%s"), 
            *ItemInstance.ItemID.ToString(),
            ItemInstance.Quantity,
            SlotIndex,
            *ItemInstance.InstanceID.ToString()
        );
    }
    
    // Отправляем событие через централизованную систему
    Manager->NotifyEquipmentEvent(const_cast<UObject*>(Inventory), EventTag, EventData);
    
    // Логируем для отладки (краткий GUID для читаемости)
    UE_LOG(LogMedComShared, Verbose, 
        TEXT("BroadcastItemAdded: %s (x%d) добавлен в слот %d [%s]"),
        *ItemInstance.ItemID.ToString(), 
        ItemInstance.Quantity, 
        SlotIndex,
        *ItemInstance.InstanceID.ToString().Left(8));
}

void ISuspenseInventory::BroadcastItemRemoved(
    const UObject* Inventory,
    const FName& ItemID,
    int32 Quantity,
    int32 SlotIndex)
{
    /**
     * События удаления более простые по структуре, поскольку нам не нужны
     * runtime свойства удаленного предмета. Однако мы все еще можем получить
     * статическую информацию из DataTable для обогащения события.
     * 
     * Это как уведомление об отправке посылки - нам важно знать что отправили,
     * но не обязательно все детали содержимого.
     */
    
    if (!Inventory)
    {
        UE_LOG(LogMedComShared, Warning, TEXT("BroadcastItemRemoved: Объект инвентаря равен null"));
        return;
    }
    
    if (ItemID.IsNone())
    {
        UE_LOG(LogMedComShared, Warning, TEXT("BroadcastItemRemoved: Невалидный ItemID"));
        return;
    }
    
    USuspenseEventManager* Manager = GetDelegateManagerStatic(Inventory);
    if (!Manager)
    {
        UE_LOG(LogMedComShared, Warning, TEXT("BroadcastItemRemoved: Менеджер событий недоступен"));
        return;
    }
    
    // Уведомляем о общем обновлении
    Manager->NotifyEquipmentUpdated();
    
    // Создаем событие удаления
    FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.ItemRemoved"));
    FString EventData = FString::Printf(
        TEXT("Item:%s,Quantity:%d,Slot:%d"), 
        *ItemID.ToString(),
        Quantity,
        SlotIndex
    );
    
    Manager->NotifyEquipmentEvent(const_cast<UObject*>(Inventory), EventTag, EventData);
    
    UE_LOG(LogMedComShared, Verbose, 
        TEXT("BroadcastItemRemoved: %s (x%d) удален из слота %d"),
        *ItemID.ToString(), Quantity, SlotIndex);
}

void ISuspenseInventory::BroadcastItemMoved(
    const UObject* Inventory,
    const FGuid& InstanceID,
    int32 OldSlotIndex,
    int32 NewSlotIndex,
    bool bWasRotated)
{
    /**
     * События перемещения теперь используют FGuid для идентификации
     * Это обеспечивает точное отслеживание в multiplayer среде
     */
    
    if (!Inventory || !InstanceID.IsValid())
    {
        UE_LOG(LogMedComShared, Warning, TEXT("BroadcastItemMoved: Недействительные параметры"));
        return;
    }
    
    USuspenseEventManager* Manager = GetDelegateManagerStatic(Inventory);
    if (!Manager)
    {
        UE_LOG(LogMedComShared, Warning, TEXT("BroadcastItemMoved: Менеджер событий недоступен"));
        return;
    }
    
    Manager->NotifyEquipmentUpdated();
    
    FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.ItemMoved"));
    FString EventData = FString::Printf(
        TEXT("InstanceID:%s,OldSlot:%d,NewSlot:%d,Rotated:%s"), 
        *InstanceID.ToString(),
        OldSlotIndex,
        NewSlotIndex,
        bWasRotated ? TEXT("true") : TEXT("false")
    );
    
    Manager->NotifyEquipmentEvent(const_cast<UObject*>(Inventory), EventTag, EventData);
    
    UE_LOG(LogMedComShared, Verbose, 
        TEXT("BroadcastItemMoved: Экземпляр %s перемещен из слота %d в %d (Повернут: %s)"),
        *InstanceID.ToString().Left(8), OldSlotIndex, NewSlotIndex, bWasRotated ? TEXT("Да") : TEXT("Нет"));
}

void ISuspenseInventory::BroadcastInventoryError(
    const UObject* Inventory,
    ESuspenseInventoryErrorCode ErrorCode,
    const FString& Context)
{
    /**
     * Система уведомлений об ошибках критически важна для пользовательского
     * опыта и отладки. Она создает как machine-readable события для
     * автоматической обработки, так и human-readable сообщения для логирования.
     * 
     * Архитектурная ценность: Централизованная обработка ошибок позволяет
     * легко добавить новые типы реакций на ошибки (UI уведомления, логирование,
     * метрики) без изменения основного кода.
     */
    
    if (!Inventory)
    {
        UE_LOG(LogMedComShared, Warning, TEXT("BroadcastInventoryError: Объект инвентаря равен null"));
        return;
    }
    
    USuspenseEventManager* Manager = GetDelegateManagerStatic(Inventory);
    if (!Manager)
    {
        UE_LOG(LogMedComShared, Warning, TEXT("BroadcastInventoryError: Менеджер событий недоступен"));
        return;
    }
    
    // Используем helper из FSuspenseInventoryOperationResult для конвертации enum в строку
    // Это обеспечивает consistency в представлении ошибок по всей системе
    FString ErrorString = FSuspenseInventoryOperationResult::GetErrorCodeString(ErrorCode);
    
    FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.Error"));
    FString EventData = FString::Printf(
        TEXT("Error:%s,Context:%s"), 
        *ErrorString,
        *Context
    );
    
    Manager->NotifyEquipmentEvent(const_cast<UObject*>(Inventory), EventTag, EventData);
    
    // Логируем с соответствующим уровнем важности
    // Критические ошибки (сеть, инициализация) логируются как Error
    // Обычные ошибки (нет места, превышен вес) как Warning
    if (ErrorCode == ESuspenseInventoryErrorCode::NetworkError || ErrorCode == ESuspenseInventoryErrorCode::NotInitialized)
    {
        UE_LOG(LogMedComShared, Error, 
            TEXT("BroadcastInventoryError: КРИТИЧЕСКАЯ - %s - %s"), *ErrorString, *Context);
    }
    else
    {
        UE_LOG(LogMedComShared, Warning, 
            TEXT("BroadcastInventoryError: %s - %s"), *ErrorString, *Context);
    }
}

void ISuspenseInventory::BroadcastWeightLimitExceeded(
    const UObject* Inventory,
    const FSuspenseInventoryItemInstance& ItemInstance,
    float RequiredWeight,
    float AvailableWeight)
{
    /**
     * Специализированные события для превышения лимита веса особенно важны
     * в играх выживания и тактических играх, где управление весом является
     * core gameplay механикой.
     * 
     * Эти события предоставляют достаточно информации для UI, чтобы показать
     * пользователю конкретные детали проблемы и возможные решения.
     */
    
    if (!Inventory)
    {
        UE_LOG(LogMedComShared, Warning, TEXT("BroadcastWeightLimitExceeded: Объект инвентаря равен null"));
        return;
    }
    
    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogMedComShared, Warning, TEXT("BroadcastWeightLimitExceeded: Невалидный ItemInstance"));
        return;
    }
    
    USuspenseEventManager* Manager = GetDelegateManagerStatic(Inventory);
    if (!Manager)
    {
        UE_LOG(LogMedComShared, Warning, TEXT("BroadcastWeightLimitExceeded: Менеджер событий недоступен"));
        return;
    }
    
    // Получаем обогащенные данные для создания информативного сообщения об ошибке
    FSuspenseUnifiedItemData UnifiedData;
    bool bHasUnifiedData = GetUnifiedDataForBroadcast(ItemInstance, UnifiedData);
    
    FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Inventory.Event.WeightLimitExceeded"));
    
    FString EventData;
    FString ItemDisplayName;
    
    if (bHasUnifiedData)
    {
        ItemDisplayName = UnifiedData.DisplayName.ToString();
        float ItemWeight = UnifiedData.Weight;
        float TotalItemWeight = ItemWeight * ItemInstance.Quantity;
        
        EventData = FString::Printf(
            TEXT("Item:%s,DisplayName:%s,Quantity:%d,ItemWeight:%.2f,TotalItemWeight:%.2f,RequiredWeight:%.2f,AvailableWeight:%.2f,ExceededBy:%.2f"), 
            *ItemInstance.ItemID.ToString(),
            *ItemDisplayName,
            ItemInstance.Quantity,
            ItemWeight,
            TotalItemWeight,
            RequiredWeight,
            AvailableWeight,
            RequiredWeight - AvailableWeight
        );
    }
    else
    {
        // Fallback версия без данных из DataTable
        ItemDisplayName = ItemInstance.ItemID.ToString();
        EventData = FString::Printf(
            TEXT("Item:%s,Quantity:%d,RequiredWeight:%.2f,AvailableWeight:%.2f,ExceededBy:%.2f"), 
            *ItemInstance.ItemID.ToString(),
            ItemInstance.Quantity,
            RequiredWeight,
            AvailableWeight,
            RequiredWeight - AvailableWeight
        );
    }
    
    Manager->NotifyEquipmentEvent(const_cast<UObject*>(Inventory), EventTag, EventData);
    
    // Также отправляем как общую ошибку для унифицированной обработки
    FString ErrorContext = FString::Printf(
        TEXT("Невозможно добавить %s (x%d) - Требуется: %.2fкг, Доступно: %.2fкг"),
        *ItemDisplayName,
        ItemInstance.Quantity,
        RequiredWeight,
        AvailableWeight
    );
    
    BroadcastInventoryError(Inventory, ESuspenseInventoryErrorCode::WeightLimit, ErrorContext);
    
    UE_LOG(LogMedComShared, Warning, TEXT("BroadcastWeightLimitExceeded: %s"), *ErrorContext);
}

//==================================================================
// КРИТИЧЕСКИ ВАЖНО: 
// В этом файле НЕТ ни одного _Implementation метода!
// 
// Все методы с суффиксом _Implementation будут автоматически
// сгенерированы Unreal Engine для BlueprintNativeEvent функций
// и должны быть реализованы в конкретных классах инвентаря.
// 
// Этот файл содержит ТОЛЬКО статические utility методы,
// которые предоставляют общую функциональность для всех
// реализаций интерфейса.
//==================================================================