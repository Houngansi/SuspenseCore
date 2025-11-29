# Clean Architecture Migration Plan

## Обзор

Данный документ описывает стратегию миграции проекта SuspenseCore на чистую архитектуру с изолированными модулями и новой системой событий на базе EventBus.

---

## 1. Анализ текущих зависимостей

### Текущая структура (проблемная)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ТЕКУЩИЙ ГРАФ ЗАВИСИМОСТЕЙ                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   PlayerCore ───┬──► BridgeSystem                                       │
│                 ├──► InventorySystem  ◄── ЛИШНЯЯ ЗАВИСИМОСТЬ            │
│                 ├──► EquipmentSystem  ◄── ЛИШНЯЯ ЗАВИСИМОСТЬ            │
│                 ├──► UISystem         ◄── ЛИШНЯЯ ЗАВИСИМОСТЬ (x2!)      │
│                 └──► GAS                                                 │
│                                                                          │
│   GAS ──────────┬──► BridgeSystem                                       │
│                 └──► SuspenseCore     ◄── НУЖНО УБРАТЬ                  │
│                                                                          │
│   BridgeSystem ────► (UE модули - OK)                                   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### PlayerCore.Build.cs - Проблемы

```csharp
PublicDependencyModuleNames.AddRange(
    new string[]
    {
        "Core",
        "CinematicCamera",
        "GameplayAbilities",
        "GameplayTags",
        "UMG",
        "BridgeSystem",        // ✓ OK - базовая инфраструктура
        "InventorySystem",     // ✗ УБРАТЬ - не нужно в ядре игрока
        "EquipmentSystem",     // ✗ УБРАТЬ - не нужно в ядре игрока
        "UISystem",            // ✗ УБРАТЬ - UI не должен быть в PlayerCore
        "GAS"                  // ✓ OK - система способностей
    }
);

PrivateDependencyModuleNames.AddRange(
    new string[]
    {
        "CoreUObject",
        "Engine",
        "Slate",
        "SlateCore",
        "UISystem",            // ✗ ДУБЛИКАТ! Указан дважды
        "EnhancedInput"
    }
);
```

### GAS.Build.cs - Проблемы

```csharp
PublicDependencyModuleNames.AddRange(
    new string[]
    {
        "Core",
        "SuspenseCore",        // ✗ УБРАТЬ - создаёт циклическую зависимость
        "GameplayAbilities",
        "GameplayTags",
        "GameplayTasks",
        "BridgeSystem"         // ✓ OK
    }
);
```

---

## 2. Целевая архитектура

### Чистый граф зависимостей

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    ЦЕЛЕВОЙ ГРАФ ЗАВИСИМОСТЕЙ                            │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│                         ┌──────────────┐                                │
│                         │ BridgeSystem │ ◄── ФУНДАМЕНТ                  │
│                         │  (EventBus)  │                                │
│                         │(ServiceLocator)│                              │
│                         └───────┬──────┘                                │
│                                 │                                        │
│               ┌─────────────────┴─────────────────┐                     │
│               │                                   │                     │
│               ▼                                   ▼                     │
│        ┌──────────┐                       ┌─────────────┐               │
│        │   GAS    │                       │ PlayerCore  │               │
│        │(Abilities)│                       │ (Character) │               │
│        └──────────┘                       └─────────────┘               │
│               │                                   │                     │
│               └─────────────────┬─────────────────┘                     │
│                                 │                                        │
│                    (Коммуникация через EventBus)                        │
│                                 │                                        │
│         ┌───────────────────────┼───────────────────────┐               │
│         │                       │                       │               │
│         ▼                       ▼                       ▼               │
│  ┌─────────────┐      ┌─────────────────┐      ┌─────────────┐         │
│  │ Interaction │      │ InventorySystem │      │  UISystem   │         │
│  │   System    │      │                 │      │             │         │
│  └─────────────┘      └─────────────────┘      └─────────────┘         │
│         │                       │                       │               │
│         └───────────────────────┴───────────────────────┘               │
│                                 │                                        │
│                                 ▼                                        │
│                      ┌─────────────────┐                                │
│                      │ EquipmentSystem │ ◄── ПОСЛЕДНИЙ МОДУЛЬ           │
│                      │   (Weapons)     │                                │
│                      └─────────────────┘                                │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Naming Convention (Конвенция именования)

### Принцип: Суффикс `V2` для новых классов

| Legacy класс | Новый класс | Описание |
|--------------|-------------|----------|
| `USuspenseEventManager` | `USuspenseEventManagerV2` | Новый менеджер на базе EventBus |
| `ASuspenseCharacter` | `ASuspenseCharacterV2` | Чистый персонаж без лишних зависимостей |
| `ASuspensePlayerController` | `ASuspensePlayerControllerV2` | Контроллер с EventBus |
| `USuspensePlayerState` | `USuspensePlayerStateV2` | State без прямых ссылок на UI |
| `USuspenseAbilitySystemComponent` | `USuspenseAbilitySystemComponentV2` | ASC с интеграцией EventBus |
| `USuspenseWeaponAttributeSet` | `USuspenseWeaponAttributeSetV2` | Атрибуты с событиями через теги |

### Почему V2?

1. **Ясность** — сразу видно что это новая версия
2. **Сосуществование** — legacy и новый код могут работать параллельно
3. **Поиск** — легко найти все новые/старые классы
4. **Стандарт** — UE использует V2 (например `UCharacterMovementComponentV2`)
5. **Рефакторинг** — после завершения миграции можно удалить суффикс

### Структура папок

```
Source/
├── BridgeSystem/
│   ├── Public/
│   │   ├── Core/
│   │   │   ├── Services/
│   │   │   │   ├── SuspenseServiceLocator.h      (переименован из Equipment*)
│   │   │   │   └── SuspenseEventBus.h            (переименован из Equipment*)
│   │   │   └── Utils/
│   │   │       ├── SuspenseCacheManager.h
│   │   │       └── SuspenseThreadGuard.h
│   │   └── V2/                                    ◄── НОВЫЕ КЛАССЫ
│   │       └── SuspenseEventManagerV2.h
│   └── Private/
│       └── V2/
│           └── SuspenseEventManagerV2.cpp
│
├── GAS/
│   ├── Public/
│   │   ├── AbilitySystem/
│   │   │   └── SuspenseAbilitySystemComponent.h  (legacy)
│   │   └── V2/                                    ◄── НОВЫЕ КЛАССЫ
│   │       ├── SuspenseAbilitySystemComponentV2.h
│   │       └── AttributeSets/
│   │           └── SuspenseWeaponAttributeSetV2.h
│   └── Private/
│       └── V2/
│
├── PlayerCore/
│   ├── Public/
│   │   ├── Character/
│   │   │   └── SuspenseCharacter.h               (legacy)
│   │   └── V2/                                    ◄── НОВЫЕ КЛАССЫ
│   │       ├── SuspenseCharacterV2.h
│   │       ├── SuspensePlayerControllerV2.h
│   │       └── SuspensePlayerStateV2.h
│   └── Private/
│       └── V2/
```

---

## 4. Порядок миграции модулей

### Рекомендация: Начать с BridgeSystem

```
┌────────────────────────────────────────────────────────────────────────┐
│                    ПОРЯДОК МИГРАЦИИ                                     │
├────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   ЭТАП 1: BridgeSystem (ФУНДАМЕНТ)                                     │
│   ├── Переименование Equipment* → Suspense*                            │
│   ├── Создание SuspenseEventManagerV2                                  │
│   ├── Регистрация GameplayTags                                         │
│   └── Тестирование инфраструктуры изолированно                         │
│                                                                         │
│   ЭТАП 2: GAS                                                          │
│   ├── Убрать зависимость от SuspenseCore                               │
│   ├── SuspenseAbilitySystemComponentV2                                 │
│   ├── SuspenseWeaponAttributeSetV2 с EventBus                          │
│   └── Интеграция GAS событий с тегами                                  │
│                                                                         │
│   ЭТАП 3: PlayerCore                                                   │
│   ├── Убрать зависимости: InventorySystem, EquipmentSystem, UISystem   │
│   ├── SuspenseCharacterV2 (минимальный)                                │
│   ├── SuspensePlayerControllerV2                                       │
│   ├── SuspensePlayerStateV2                                            │
│   └── Подключение к GAS через EventBus                                 │
│                                                                         │
│   ═══════════════════════════════════════════════════════════════════  │
│   │ КОНТРОЛЬНАЯ ТОЧКА: Компиляция и тест базовых модулей             │ │
│   ═══════════════════════════════════════════════════════════════════  │
│                                                                         │
│   ЭТАП 4: InteractionSystem                                            │
│   ├── Подписки на EventBus                                             │
│   └── Публикация событий взаимодействия                                │
│                                                                         │
│   ЭТАП 5: InventorySystem                                              │
│   ├── Замена делегатов на EventBus                                     │
│   └── Транзакции через события                                         │
│                                                                         │
│   ЭТАП 6: UISystem                                                     │
│   ├── Виджеты подписываются на EventBus                                │
│   ├── Фильтрация по владельцу                                          │
│   └── Batch updates                                                     │
│                                                                         │
│   ЭТАП 7: EquipmentSystem                                              │
│   ├── Полная интеграция с EventBus                                     │
│   ├── Удаление SuspenseEventManager (legacy)                           │
│   └── Финальная очистка                                                │
│                                                                         │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Детальный план Этапа 1: BridgeSystem

### 5.1 Переименование классов

| Текущее имя | Новое имя |
|-------------|-----------|
| `USuspenseEquipmentServiceLocator` | `USuspenseServiceLocator` |
| `USuspenseEquipmentEventBus` | `USuspenseEventBus` |
| `USuspenseEquipmentCacheManager` | `USuspenseCacheManager` |
| `USuspenseEquipmentThreadGuard` | `USuspenseThreadGuard` |
| `USuspenseGlobalCacheRegistry` | `USuspenseGlobalCacheRegistry` (OK) |
| `FSuspenseEquipmentEventData` | `FSuspenseEventData` |
| `ESuspenseEquipmentEventPriority` | `ESuspenseEventPriority` |

### 5.2 Создание GameplayTags иерархии

```ini
; Config/DefaultGameplayTags.ini

; Корневой тег событий
+GameplayTags=(Tag="Suspense.Event")

; События GAS
+GameplayTags=(Tag="Suspense.Event.GAS")
+GameplayTags=(Tag="Suspense.Event.GAS.Ability.Activated")
+GameplayTags=(Tag="Suspense.Event.GAS.Ability.Ended")
+GameplayTags=(Tag="Suspense.Event.GAS.Attribute.Changed")
+GameplayTags=(Tag="Suspense.Event.GAS.Attribute.Health")
+GameplayTags=(Tag="Suspense.Event.GAS.Attribute.Ammo")

; События игрока
+GameplayTags=(Tag="Suspense.Event.Player")
+GameplayTags=(Tag="Suspense.Event.Player.Spawned")
+GameplayTags=(Tag="Suspense.Event.Player.Died")
+GameplayTags=(Tag="Suspense.Event.Player.State.Changed")

; События оружия
+GameplayTags=(Tag="Suspense.Event.Weapon")
+GameplayTags=(Tag="Suspense.Event.Weapon.Equipped")
+GameplayTags=(Tag="Suspense.Event.Weapon.Unequipped")
+GameplayTags=(Tag="Suspense.Event.Weapon.Fired")
+GameplayTags=(Tag="Suspense.Event.Weapon.Reloaded")

; События инвентаря
+GameplayTags=(Tag="Suspense.Event.Inventory")
+GameplayTags=(Tag="Suspense.Event.Inventory.Item.Added")
+GameplayTags=(Tag="Suspense.Event.Inventory.Item.Removed")
+GameplayTags=(Tag="Suspense.Event.Inventory.Slot.Changed")

; События экипировки
+GameplayTags=(Tag="Suspense.Event.Equipment")
+GameplayTags=(Tag="Suspense.Event.Equipment.Slot.Changed")
+GameplayTags=(Tag="Suspense.Event.Equipment.Loadout.Changed")

; События UI
+GameplayTags=(Tag="Suspense.Event.UI")
+GameplayTags=(Tag="Suspense.Event.UI.Widget.Opened")
+GameplayTags=(Tag="Suspense.Event.UI.Widget.Closed")
+GameplayTags=(Tag="Suspense.Event.UI.Notification")

; События взаимодействия
+GameplayTags=(Tag="Suspense.Event.Interaction")
+GameplayTags=(Tag="Suspense.Event.Interaction.Started")
+GameplayTags=(Tag="Suspense.Event.Interaction.Completed")
+GameplayTags=(Tag="Suspense.Event.Interaction.Cancelled")
```

### 5.3 SuspenseEventManagerV2

```cpp
// BridgeSystem/Public/V2/SuspenseEventManagerV2.h

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Core/Services/SuspenseEventBus.h"
#include "Core/Services/SuspenseServiceLocator.h"
#include "SuspenseEventManagerV2.generated.h"

/**
 * USuspenseEventManagerV2
 *
 * Новый менеджер событий на базе EventBus.
 * Заменяет legacy USuspenseEventManager с делегатами.
 *
 * Преимущества:
 * - Развязанность модулей через GameplayTags
 * - Приоритеты и контексты выполнения
 * - Фильтрация событий по источнику
 * - Поддержка отложенных событий
 */
UCLASS()
class BRIDGESYSTEM_API USuspenseEventManagerV2 : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    //~ USubsystem interface
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    //~ End USubsystem interface

    /** Получить EventBus */
    UFUNCTION(BlueprintCallable, Category = "Suspense|Events")
    USuspenseEventBus* GetEventBus() const { return EventBus; }

    /** Получить ServiceLocator */
    UFUNCTION(BlueprintCallable, Category = "Suspense|Services")
    USuspenseServiceLocator* GetServiceLocator() const { return ServiceLocator; }

    /** Быстрый доступ - публикация события */
    UFUNCTION(BlueprintCallable, Category = "Suspense|Events")
    void PublishEvent(FGameplayTag EventTag, UObject* Source, const TMap<FName, FString>& Payload);

    /** Статический хелпер для быстрого доступа */
    UFUNCTION(BlueprintCallable, Category = "Suspense|Events", meta = (WorldContext = "WorldContextObject"))
    static USuspenseEventManagerV2* Get(const UObject* WorldContextObject);

protected:
    /** EventBus для публикации/подписки */
    UPROPERTY()
    TObjectPtr<USuspenseEventBus> EventBus;

    /** ServiceLocator для DI */
    UPROPERTY()
    TObjectPtr<USuspenseServiceLocator> ServiceLocator;

private:
    /** Регистрация базовых сервисов */
    void RegisterCoreServices();

    /** Настройка GameplayTags */
    void SetupEventTags();
};
```

---

## 6. Чеклист для каждого этапа

### Чеклист Этапа 1: BridgeSystem

- [ ] Переименовать `SuspenseEquipment*` → `Suspense*`
- [ ] Создать папку `BridgeSystem/Public/V2/`
- [ ] Создать `SuspenseEventManagerV2.h/.cpp`
- [ ] Добавить GameplayTags в `DefaultGameplayTags.ini`
- [ ] Создать макросы для тегов `SUSPENSE_EVENT_TAG()`
- [ ] Написать unit тесты для EventBus
- [ ] Проверить компиляцию BridgeSystem изолированно

### Чеклист Этапа 2: GAS

- [ ] Убрать `SuspenseCore` из зависимостей
- [ ] Создать папку `GAS/Public/V2/`
- [ ] Создать `SuspenseAbilitySystemComponentV2`
- [ ] Создать `SuspenseWeaponAttributeSetV2`
- [ ] Интегрировать с EventBus
- [ ] Тесты GAS событий

### Чеклист Этапа 3: PlayerCore

- [ ] Убрать зависимости: `InventorySystem`, `EquipmentSystem`, `UISystem`
- [ ] Создать папку `PlayerCore/Public/V2/`
- [ ] Создать `SuspenseCharacterV2` (минимальный)
- [ ] Создать `SuspensePlayerControllerV2`
- [ ] Создать `SuspensePlayerStateV2`
- [ ] Подключить GAS через EventBus
- [ ] Тесты персонажа

---

## 7. Правила миграции

### DO (Делать)

1. ✅ Создавать V2 классы рядом с legacy
2. ✅ Использовать EventBus для всей коммуникации между модулями
3. ✅ Писать тесты перед удалением legacy кода
4. ✅ Документировать изменения в каждом PR
5. ✅ Компилировать модули изолированно

### DON'T (Не делать)

1. ❌ Удалять legacy классы до полной миграции
2. ❌ Смешивать делегаты и EventBus в одном классе
3. ❌ Добавлять новые зависимости в PlayerCore
4. ❌ Пропускать тесты "потому что работает"
5. ❌ Делать большие PR (максимум 1 этап = 1 PR)

---

## 8. Метрики успеха

| Метрика | До миграции | После миграции |
|---------|-------------|----------------|
| Зависимости PlayerCore | 6 модулей | 2 модуля |
| Делегаты в EventManager | 50+ | 0 |
| Compile time (полная) | ~15 мин | ~8 мин |
| Compile time (incremental) | ~5 мин | ~1 мин |
| Изолированная компиляция | Невозможна | Каждый модуль отдельно |
| Связанность (coupling) | Высокая | Низкая |

---

## 9. Timeline

| Этап | Модуль | Оценка |
|------|--------|--------|
| 1 | BridgeSystem | Первый приоритет |
| 2 | GAS | После BridgeSystem |
| 3 | PlayerCore | После GAS |
| — | **Checkpoint** | Тестирование базы |
| 4 | InteractionSystem | По готовности |
| 5 | InventorySystem | По готовности |
| 6 | UISystem | По готовности |
| 7 | EquipmentSystem | Финальный |

---

## 10. Риски и митигация

| Риск | Вероятность | Митигация |
|------|-------------|-----------|
| Breaking changes в Blueprint | Высокая | Оставить legacy до миграции BP |
| Потеря функциональности | Средняя | Тесты + код ревью |
| Регрессии производительности | Низкая | Профилирование на каждом этапе |
| Конфликты в git | Средняя | Мелкие PR + частые merge |

---

*Документ создан: 2025-11-29*
*Версия: 1.0*
