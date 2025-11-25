// Copyright Suspense Team. All Rights Reserved.

#include "Interfaces/Equipment/ISuspenseEquipment.h"
#include "Delegates/SuspenseEventManager.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

//==================================================================
// ТОЛЬКО статические вспомогательные методы для межмодульной коммуникации
//
// КРИТИЧЕСКИ ВАЖНО: В этом файле НЕТ ни одного _Implementation метода!
// Все _Implementation методы будут автоматически сгенерированы Unreal Engine
// для BlueprintNativeEvent функций и должны быть реализованы в конкретных
// классах экипировки.
//
// Этот файл содержит ТОЛЬКО статические utility методы, которые
// предоставляют общую функциональность для всех реализаций интерфейса.
//==================================================================

USuspenseEventManager* ISuspenseEquipment::GetDelegateManagerStatic(const UObject* WorldContextObject)
{
    /**
     * Этот метод служит "универсальным ключом" к централизованной системе событий.
     * Он работает как "информационная служба" - предоставляет доступ к менеджеру
     * событий из любого места в коде, где есть доступ к игровому миру.
     * 
     * Архитектурная важность: В сложных проектах разные модули должны общаться
     * друг с другом, не создавая жестких зависимостей. EventDelegateManager
     * служит "почтовой службой" для такого общения.
     */
    
    if (!WorldContextObject)
    {
        UE_LOG(LogTemp, Warning, TEXT("ISuspenseEquipment::GetDelegateManagerStatic - Null world context"));
        return nullptr;
    }
    
    // Получаем World - это "вселенная" игры, содержащая все игровые объекты
    UWorld* World = WorldContextObject->GetWorld();
    if (!World)
    {
        UE_LOG(LogTemp, Warning, TEXT("ISuspenseEquipment::GetDelegateManagerStatic - Cannot get world from context"));
        return nullptr;
    }
    
    // GameInstance - это "глобальный координатор" игры, который живет на протяжении всей сессии
    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        UE_LOG(LogTemp, Warning, TEXT("ISuspenseEquipment::GetDelegateManagerStatic - No game instance"));
        return nullptr;
    }
    
    // Получаем subsystem - это "специализированный отдел" в рамках GameInstance
    return GameInstance->GetSubsystem<USuspenseEventManager>();
}

void ISuspenseEquipment::BroadcastEquipmentStateChanged(
    const UObject* Equipment,
    const FGameplayTag& OldState, 
    const FGameplayTag& NewState, 
    bool bInterrupted)
{
    /**
     * Этот метод обеспечивает централизованное уведомление об изменениях состояния
     * экипировки. Представьте его как "диспетчера новостей" - он собирает информацию
     * о важных изменениях и рассылает её всем заинтересованным сторонам.
     * 
     * Примеры использования:
     * - UI нужно обновить индикатор состояния оружия
     * - Система достижений отслеживает использование экипировки
     * - Аналитическая система собирает метрики игрового поведения
     */
    
    if (!Equipment)
    {
        UE_LOG(LogTemp, Warning, TEXT("BroadcastEquipmentStateChanged - Null equipment object"));
        return;
    }
    
    USuspenseEventManager* Manager = GetDelegateManagerStatic(Equipment);
    if (Manager)
    {
        // Отправляем расширенное уведомление об изменении состояния с контекстом
        Manager->NotifyEquipmentStateChanged(OldState, NewState, bInterrupted);
        
        UE_LOG(LogTemp, VeryVerbose, TEXT("Equipment state changed: %s -> %s (interrupted: %s)"),
            *OldState.ToString(), *NewState.ToString(), bInterrupted ? TEXT("true") : TEXT("false"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BroadcastEquipmentStateChanged - No delegate manager available"));
    }
}

void ISuspenseEquipment::BroadcastEquipmentOperationEvent(
    const UObject* Equipment,
    const FGameplayTag& EventTag,
    const FSuspenseInventoryItemInstance* ItemInstance,
    const FString& EventData)
{
    /**
     * Этот метод обрабатывает операционные события экипировки - экипировка,
     * снятие, обмен, модификации. Он создает rich event payload с подробной
     * информацией для различных систем игры.
     * 
     * Архитектурное преимущество: Вместо того чтобы каждая система
     * экипировки изобретала свой способ уведомлений, все используют
     * единый стандартизированный подход.
     */
    
    if (!Equipment)
    {
        UE_LOG(LogTemp, Warning, TEXT("BroadcastEquipmentOperationEvent - Null equipment object"));
        return;
    }
    
    USuspenseEventManager* Manager = GetDelegateManagerStatic(Equipment);
    if (Manager)
    {
        // Строим comprehensive данные события
        FString CompleteEventData = EventData;
        
        if (ItemInstance && ItemInstance->IsValid())
        {
            if (!CompleteEventData.IsEmpty())
            {
                CompleteEventData += TEXT("; ");
            }
            CompleteEventData += FString::Printf(TEXT("ItemID=%s, Quantity=%d, InstanceID=%s"),
                *ItemInstance->ItemID.ToString(),
                ItemInstance->Quantity,
                *ItemInstance->InstanceID.ToString());
        }
        
        // Отправляем событие через централизованную систему
        Manager->NotifyEquipmentEvent(const_cast<UObject*>(Equipment), EventTag, CompleteEventData);
        
        UE_LOG(LogTemp, VeryVerbose, TEXT("Equipment operation event: %s - %s"),
            *EventTag.ToString(), *CompleteEventData);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BroadcastEquipmentOperationEvent - No delegate manager available"));
    }
}

void ISuspenseEquipment::BroadcastEquipmentPropertyChanged(
    const UObject* Equipment,
    const FName& PropertyName,
    float OldValue,
    float NewValue)
{
    /**
     * Специализированный метод для уведомлений об изменениях runtime свойств.
     * Это особенно важно для свойств, которые изменяются часто - прочность,
     * заряд батареи, состояние модификаций.
     * 
     * UI системы могут подписаться на эти события для real-time обновления
     * индикаторов без необходимости постоянного polling состояния.
     */
    
    if (!Equipment)
    {
        UE_LOG(LogTemp, Warning, TEXT("BroadcastEquipmentPropertyChanged - Null equipment object"));
        return;
    }
    
    USuspenseEventManager* Manager = GetDelegateManagerStatic(Equipment);
    if (Manager)
    {
        // Создаем данные события изменения свойства
        FString EventData = FString::Printf(TEXT("Property=%s, OldValue=%.2f, NewValue=%.2f"),
            *PropertyName.ToString(), OldValue, NewValue);
        
        // Используем стандартный тег события изменения свойства экипировки
        FGameplayTag PropertyChangedTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.PropertyChanged"));
        
        Manager->NotifyEquipmentEvent(const_cast<UObject*>(Equipment), PropertyChangedTag, EventData);
        
        UE_LOG(LogTemp, VeryVerbose, TEXT("Equipment property changed: %s = %.2f (was %.2f)"),
            *PropertyName.ToString(), NewValue, OldValue);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("BroadcastEquipmentPropertyChanged - No delegate manager available"));
    }
}

//==================================================================
// ОБРАЗОВАТЕЛЬНАЯ ЗАМЕТКА:
//
// В этом файле НЕТ ни одного метода с суффиксом _Implementation!
// Это критически важно для понимания архитектуры Unreal Engine.
//
// ПОЧЕМУ НЕТ _Implementation МЕТОДОВ:
// 1. BlueprintNativeEvent автоматически создает объявления _Implementation методов
// 2. Эти методы должны быть реализованы в КОНКРЕТНЫХ классах, а не в интерфейсе
// 3. Интерфейс определяет КОНТРАКТ, конкретные классы предоставляют РЕАЛИЗАЦИЮ
//
// ПРАВИЛЬНАЯ АРХИТЕКТУРА:
// Interface.h    -> Объявляет контракт (что должно быть сделано)
// Interface.cpp  -> Предоставляет общие utilities (помощники для всех)
// ConcreteClass.h -> Объявляет что будет реализовывать контракт
// ConcreteClass.cpp -> Фактически реализует _Implementation методы
//
// Эта архитектура обеспечивает:
// - Гибкость: разные классы могут реализовывать по-разному
// - Переиспользование: общие utilities доступны всем
// - Чистоту: четкое разделение контрактов и реализаций
// - Blueprint совместимость: методы можно переопределить в BP
//==================================================================