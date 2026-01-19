# Grenade System - Аудит и План Внедрения AAA Системы

> **Version:** 1.0
> **Author:** Claude Code
> **Date:** 2026-01-19
> **Status:** DRAFT - Ready for Review
> **Branch:** `claude/review-grenade-docs-SxcxF`

---

## Содержание

1. [Executive Summary](#1-executive-summary)
2. [Аудит Текущего Кода](#2-аудит-текущего-кода)
3. [Оценка Компонентов](#3-оценка-компонентов)
4. [Архитектурные Проблемы](#4-архитектурные-проблемы)
5. [План Действий](#5-план-действий)
6. [Детальный План Реализации](#6-детальный-план-реализации)
7. [Чеклист Готовности](#7-чеклист-готовности)

---

## 1. Executive Summary

### Текущее Состояние

Существующая система гранат имеет **смешанную архитектуру**:
- `GrenadeHandler` - использует legacy fallback для визуализации
- `GrenadeThrowAbility` - не использует AnimNotify паттерн
- QuickSlot система работает, но не интегрирована с GAS

### Цель Рефакторинга

Создать AAA-качества систему броска гранат:
1. **GrenadeHandler** - ТОЛЬКО активирует GAS ability (без fallback)
2. **GrenadeThrowAbility** - использует AnimNotify паттерн (как ReloadAbility)
3. **EventBus** - используется только для spawn request от Ability к Handler

### Итоговая Оценка

| Компонент | Текущая Оценка | Целевая Оценка |
|-----------|----------------|----------------|
| GrenadeHandler | 🟡 6/10 | 🟢 9/10 |
| GrenadeThrowAbility | 🟠 4/10 | 🟢 9/10 |
| QuickSlot Integration | 🟡 7/10 | 🟢 9/10 |
| EventBus Integration | 🟡 6/10 | 🟢 9/10 |
| **Общая оценка** | **🟡 5.75/10** | **🟢 9/10** |

---

## 2. Аудит Текущего Кода

### 2.1 SuspenseCoreGrenadeHandler

**Файл:** `Source/EquipmentSystem/Private/SuspenseCore/Handlers/ItemUse/SuspenseCoreGrenadeHandler.cpp`

#### Что Хорошо (✅)

```cpp
// Правильная структура Handler'а
USuspenseCoreGrenadeHandler::USuspenseCoreGrenadeHandler()
{
    HandlerTag = SuspenseCoreItemUseTags::Handler::TAG_ItemUse_Handler_Grenade;
    HandlerPriority = ESuspenseCoreHandlerPriority::High;
    bRequiresTargetItem = false;
}

// Корректная валидация
bool USuspenseCoreGrenadeHandler::CanHandle(const FSuspenseCoreItemUseRequest& Request) const
{
    // Проверяет тег Item.Type.Throwable
}
```

#### Что Плохо (❌)

1. **Legacy Fallback** (критично):
```cpp
// ПРОБЛЕМА: Fallback для визуализации без GAS
void USuspenseCoreGrenadeHandler::SpawnGrenadeActor_Implementation(...)
{
    // Спавн гранаты напрямую, минуя GAS ability
    // Это нарушает SSOT и не использует AnimNotify
}
```

2. **Дублирование логики спавна**:
   - Handler не должен спавнить гранату
   - Спавн должен происходить из Ability через AnimNotify

3. **Отсутствие интеграции с GrenadeThrowAbility**:
   - Handler активирует ability, но затем дублирует логику
   - Нужно убрать весь код визуализации

#### Оценка: 🟡 6/10

---

### 2.2 SuspenseCoreGrenadeThrowAbility

**Файл:** `Source/GAS/Private/SuspenseCore/Abilities/Throwable/SuspenseCoreGrenadeThrowAbility.cpp`

#### Что Хорошо (✅)

```cpp
// Базовая структура ability есть
void USuspenseCoreGrenadeThrowAbility::ActivateAbility(...)
{
    // Воспроизведение анимации
    // Публикация событий через EventBus
}
```

#### Что Плохо (❌)

1. **НЕ использует AnimNotify паттерн** (критично):
```cpp
// ПРОБЛЕМА: Спавн по таймеру, а не по AnimNotify
void USuspenseCoreGrenadeThrowAbility::ActivateAbility(...)
{
    // Используется FTimerHandle для спавна
    // Это антипаттерн - нужен AnimNotify как в ReloadAbility
}
```

2. **Сравнение с эталоном ReloadAbility**:
```cpp
// ReloadAbility (ЭТАЛОН - так должно быть):
void USuspenseCoreReloadAbility::OnMontageCompleted(...)
{
    // Спавн/действие происходит через AnimNotify
    // AnimNotify_MagazineSwap вызывается из Montage
}

// GrenadeThrowAbility (ТЕКУЩЕЕ - неправильно):
void USuspenseCoreGrenadeThrowAbility::ActivateAbility(...)
{
    // Таймер вместо AnimNotify
    GetWorld()->GetTimerManager().SetTimer(...)
}
```

3. **Отсутствует AnimNotify для момента броска**:
   - Нужен `AN_GrenadeRelease` AnimNotify
   - Notify вызывает метод `OnGrenadeRelease()`
   - Метод публикует событие через EventBus

4. **Нет отмены при получении урона**:
```cpp
// Нет ActivationBlockedTags или CancelAbilitiesWithTag
// Граната продолжает бросаться даже при получении урона
```

#### Оценка: 🟠 4/10

---

### 2.3 QuickSlotComponent

**Файл:** `Source/EquipmentSystem/Private/SuspenseCore/Components/SuspenseCoreQuickSlotComponent.cpp`

#### Что Хорошо (✅)

1. Правильная структура слотов (4 слота, индексы 0-3)
2. Интеграция с ItemUseService
3. Валидация типов предметов

#### Что Плохо (❌)

1. **Нет прямой интеграции с GrenadeHandler**:
```cpp
// QuickSlot вызывает ItemUseService, но не передаёт
// информацию о том что это граната из QuickSlot
```

2. **Fallback логика для гранат**:
```cpp
// Присутствует legacy код для обработки гранат
// без использования GAS системы
```

#### Оценка: 🟡 7/10

---

### 2.4 Паттерн MagazineSwapHandler (Эталон)

**Файл:** `Source/EquipmentSystem/Private/SuspenseCore/Handlers/ItemUse/SuspenseCoreMagazineSwapHandler.cpp`

#### Почему это Эталон (✅)

```cpp
FSuspenseCoreItemUseResponse USuspenseCoreMagazineSwapHandler::Execute(...)
{
    // 1. Валидация
    if (!ValidateRequest(Request)) { return FailedResponse; }

    // 2. Активация GAS Ability
    bool bActivated = ActivateAbility(ASC, Request);

    // 3. Ability делает всё остальное через AnimNotify
    // Handler НЕ содержит логику визуализации

    return InProgressResponse;
}
```

**Ключевые принципы:**
- Handler ТОЛЬКО валидирует и активирует ability
- Вся логика в Ability
- Ability использует AnimNotify для timing-critical действий
- EventBus для коммуникации

---

### 2.5 Паттерн ReloadAbility (Эталон AnimNotify)

**Файл:** `Source/GAS/Private/SuspenseCore/Abilities/Weapon/SuspenseCoreReloadAbility.cpp`

#### Почему это Эталон AnimNotify (✅)

```cpp
void USuspenseCoreReloadAbility::ActivateAbility(...)
{
    // 1. Запуск анимации
    PlayMontage(ReloadMontage);

    // 2. Привязка к AnimNotify (не таймеры!)
    // AnimNotify_MagazineSwap в Montage вызывает:
    //   OnAnimNotify_MagazineSwap()

    // 3. Отмена при получении урона
    // ActivationBlockedTags содержит State.Stunned
}

// Вызывается из AnimNotify, НЕ из таймера
void USuspenseCoreReloadAbility::OnAnimNotify_MagazineSwap()
{
    // Логика смены магазина
    // Точно синхронизирована с анимацией
}
```

---

### 2.6 Данные о Гранатах

**Файл:** `Content/Data/ItemDatabase/SuspenseCoreThrowableAttributes.json`

#### Структура Данных (✅)

```json
{
  "ThrowableID": "Grenade_F1",
  "DisplayName": "F-1 Grenade",
  "FuseTime": 3.5,
  "ExplosionRadius": 500.0,
  "Damage": 250.0,
  "ThrowForce": 1500.0,
  "GrenadeClass": "/Game/Blueprints/Weapons/Throwables/BP_Grenade_F1.BP_Grenade_F1_C"
}
```

**Оценка данных:** 🟢 8/10 - структура хорошая, можно использовать

---

## 3. Оценка Компонентов

### Сводная Таблица

| Компонент | Файл | Оценка | Критичность | Действие |
|-----------|------|--------|-------------|----------|
| GrenadeHandler | SuspenseCoreGrenadeHandler.cpp | 6/10 | 🔴 High | Рефакторинг |
| GrenadeThrowAbility | SuspenseCoreGrenadeThrowAbility.cpp | 4/10 | 🔴 Critical | Переписать |
| QuickSlotComponent | SuspenseCoreQuickSlotComponent.cpp | 7/10 | 🟡 Medium | Минимальные изменения |
| ItemUseService | SuspenseCoreItemUseService.cpp | 8/10 | 🟢 Low | Готов |
| EventBus | SuspenseCoreEventBus.h | 9/10 | 🟢 Low | Готов |
| ThrowableData | SuspenseCoreThrowableAttributes.json | 8/10 | 🟢 Low | Добавить поля |

### Детальная Оценка по Критериям

#### GrenadeHandler (6/10)

| Критерий | Оценка | Комментарий |
|----------|--------|-------------|
| Соответствие SOLID | 5/10 | Нарушен SRP - Handler содержит логику спавна |
| Использование паттернов | 6/10 | Частичное соответствие Handler паттерну |
| Интеграция с GAS | 5/10 | Есть, но с fallback |
| Читаемость кода | 7/10 | Хорошо структурирован |
| Тестируемость | 5/10 | Сложно тестировать из-за смешанной логики |

#### GrenadeThrowAbility (4/10)

| Критерий | Оценка | Комментарий |
|----------|--------|-------------|
| Соответствие GAS паттернам | 3/10 | Не использует AnimNotify |
| Синхронизация с анимацией | 2/10 | Таймеры вместо notify |
| Обработка отмены | 4/10 | Базовая, без damage interrupt |
| Сетевая репликация | 5/10 | Частичная |
| Соответствие эталону | 3/10 | Далеко от ReloadAbility |

---

## 4. Архитектурные Проблемы

### 4.1 Нарушение SSOT (Single Source of Truth)

```
ТЕКУЩЕЕ (неправильно):
┌────────────────┐      ┌────────────────┐
│ GrenadeHandler │ ──── │ Spawn Logic    │  ← ДУБЛИРОВАНИЕ
└────────────────┘      └────────────────┘
        │
        ▼
┌────────────────┐      ┌────────────────┐
│  GrenadeAbility│ ──── │ Spawn Logic    │  ← ДУБЛИРОВАНИЕ
└────────────────┘      └────────────────┘

ЦЕЛЕВОЕ (правильно):
┌────────────────┐      ┌────────────────┐
│ GrenadeHandler │ ──── │ Activate Only  │
└────────────────┘      └────────────────┘
        │
        ▼
┌────────────────┐      ┌────────────────┐      ┌─────────────┐
│  GrenadeAbility│ ──── │ AnimNotify     │ ──── │ Spawn Event │
└────────────────┘      └────────────────┘      └─────────────┘
                                                       │
                                                       ▼
                                               ┌─────────────────┐
                                               │ Handler Spawns  │ ← ЕДИНСТВЕННОЕ МЕСТО
                                               └─────────────────┘
```

### 4.2 Отсутствие AnimNotify

```cpp
// ТЕКУЩЕЕ (антипаттерн):
void ActivateAbility() {
    PlayMontage();
    GetWorld()->GetTimerManager().SetTimer(
        ThrowTimer, this, &ThisClass::SpawnGrenade, 0.5f);  // Magic number!
}

// ЦЕЛЕВОЕ (правильно):
void ActivateAbility() {
    PlayMontage();  // Montage содержит AN_GrenadeRelease notify
}

void OnAnimNotify_GrenadeRelease() {
    // Публикация события через EventBus
    EventBus->Publish(TAG_Grenade_SpawnRequest, EventData);
}
```

### 4.3 Отсутствие Granted Ability

**Проблема:** GrenadeThrowAbility не выдаётся в PlayerState при инициализации.

```cpp
// В SuspenseCorePlayerState.cpp отсутствует:
void ASuspenseCorePlayerState::GrantStartupAbilities()
{
    // GrenadeThrowAbility НЕ в списке StartupAbilities
}
```

---

## 5. План Действий

### Фаза 1: Подготовка (P0 - Critical)

| Шаг | Задача | Файлы | Оценка |
|-----|--------|-------|--------|
| 1.1 | Создать AN_GrenadeRelease AnimNotify | `Source/GAS/Public/SuspenseCore/AnimNotify/AN_GrenadeRelease.h` | 2ч |
| 1.2 | Добавить Native Tags для гранат | `Source/BridgeSystem/Public/SuspenseCore/Tags/` | 1ч |
| 1.3 | Обновить ThrowableAttributes.json | `Content/Data/` | 30м |

### Фаза 2: GrenadeThrowAbility Рефакторинг (P0 - Critical)

| Шаг | Задача | Файлы | Оценка |
|-----|--------|-------|--------|
| 2.1 | Переписать ActivateAbility | `SuspenseCoreGrenadeThrowAbility.cpp` | 4ч |
| 2.2 | Добавить AnimNotify обработчик | `SuspenseCoreGrenadeThrowAbility.cpp` | 2ч |
| 2.3 | Добавить Cancel/Interrupt логику | `SuspenseCoreGrenadeThrowAbility.cpp` | 2ч |
| 2.4 | Интегрировать EventBus events | `SuspenseCoreGrenadeThrowAbility.cpp` | 1ч |

### Фаза 3: GrenadeHandler Рефакторинг (P1 - High)

| Шаг | Задача | Файлы | Оценка |
|-----|--------|-------|--------|
| 3.1 | Удалить legacy fallback | `SuspenseCoreGrenadeHandler.cpp` | 2ч |
| 3.2 | Подписка на EventBus spawn events | `SuspenseCoreGrenadeHandler.cpp` | 2ч |
| 3.3 | Перенести логику спавна в обработчик события | `SuspenseCoreGrenadeHandler.cpp` | 3ч |

### Фаза 4: Интеграция (P1 - High)

| Шаг | Задача | Файлы | Оценка |
|-----|--------|-------|--------|
| 4.1 | Добавить GrenadeThrowAbility в StartupAbilities | `SuspenseCorePlayerState.cpp` | 30м |
| 4.2 | Обновить QuickSlotComponent | `SuspenseCoreQuickSlotComponent.cpp` | 2ч |
| 4.3 | Создать Animation Montage с notify | `Content/Animations/` | 4ч |

### Фаза 5: Тестирование и Полировка (P2)

| Шаг | Задача | Файлы | Оценка |
|-----|--------|-------|--------|
| 5.1 | Unit тесты для Handler | `Tests/` | 3ч |
| 5.2 | Integration тесты | `Tests/` | 4ч |
| 5.3 | Network replication тесты | `Tests/` | 3ч |

### Общая Оценка Времени

| Фаза | Время |
|------|-------|
| Фаза 1 | 3.5ч |
| Фаза 2 | 9ч |
| Фаза 3 | 7ч |
| Фаза 4 | 6.5ч |
| Фаза 5 | 10ч |
| **Итого** | **~36ч** |

---

## 6. Детальный План Реализации

### 6.1 Создание AN_GrenadeRelease AnimNotify

```cpp
// AN_GrenadeRelease.h
UCLASS()
class GAS_API UAN_GrenadeRelease : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(USkeletalMeshComponent* MeshComp,
                       UAnimSequenceBase* Animation,
                       const FAnimNotifyEventReference& EventReference) override;
};

// AN_GrenadeRelease.cpp
void UAN_GrenadeRelease::Notify(...)
{
    if (AActor* Owner = MeshComp->GetOwner())
    {
        // Получить AbilitySystemComponent
        if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner))
        {
            // Отправить GameplayEvent
            FGameplayEventData EventData;
            EventData.EventTag = TAG_Event_Grenade_Release;
            ASC->HandleGameplayEvent(TAG_Event_Grenade_Release, &EventData);
        }
    }
}
```

### 6.2 Переписанная GrenadeThrowAbility

```cpp
// SuspenseCoreGrenadeThrowAbility.h (обновлённая версия)
UCLASS()
class GAS_API USuspenseCoreGrenadeThrowAbility : public USuspenseCoreGameplayAbility
{
    GENERATED_BODY()

public:
    USuspenseCoreGrenadeThrowAbility();

    virtual void ActivateAbility(...) override;
    virtual void EndAbility(...) override;
    virtual void CancelAbility(...) override;

protected:
    // AnimNotify обработчики
    UFUNCTION()
    void OnGrenadeRelease(FGameplayEventData Payload);

    UFUNCTION()
    void OnMontageCancelled(FGameplayTag EventTag, FGameplayEventData EventData);

    // Публикация события спавна через EventBus
    void PublishSpawnRequest();

    // Подписки
    FDelegateHandle ReleaseNotifyHandle;
    FDelegateHandle MontageEndHandle;

    // Данные гранаты
    UPROPERTY()
    FSuspenseCoreThrowableData CachedThrowableData;

    // Montage
    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    UAnimMontage* ThrowMontage;

    // Tags
    UPROPERTY(EditDefaultsOnly, Category = "Tags")
    FGameplayTag GrenadeReleaseEventTag;
};

// SuspenseCoreGrenadeThrowAbility.cpp
USuspenseCoreGrenadeThrowAbility::USuspenseCoreGrenadeThrowAbility()
{
    // Ability Tags
    AbilityTags.AddTag(TAG_Ability_Grenade_Throw);

    // Cancel при получении урона
    ActivationBlockedTags.AddTag(TAG_State_Stunned);
    ActivationBlockedTags.AddTag(TAG_State_Dead);

    // Отмена другими abilities
    CancelAbilitiesWithTag.AddTag(TAG_State_ItemUse_InProgress);

    // Net policy
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

    // Tags для события
    GrenadeReleaseEventTag = TAG_Event_Grenade_Release;
}

void USuspenseCoreGrenadeThrowAbility::ActivateAbility(...)
{
    Super::ActivateAbility(...);

    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Получить данные о гранате из Request
    // (переданы через EventData при активации)

    // Подписка на AnimNotify событие
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (ASC)
    {
        ReleaseNotifyHandle = ASC->AbilityTargetDataSetDelegate(
            Handle,
            FAbilityTargetDataSetDelegate::CreateUObject(
                this, &USuspenseCoreGrenadeThrowAbility::OnGrenadeRelease));

        // Альтернативно: подписка на GameplayEvent
        ASC->GenericGameplayEventCallbacks.FindOrAdd(GrenadeReleaseEventTag)
            .AddUObject(this, &USuspenseCoreGrenadeThrowAbility::OnGrenadeRelease);
    }

    // Воспроизвести анимацию (Montage содержит AN_GrenadeRelease)
    if (ThrowMontage)
    {
        PlayMontageAndWait(ThrowMontage);
    }

    // Публикация события начала броска
    PublishEventBus(TAG_Event_ItemUse_Started);
}

void USuspenseCoreGrenadeThrowAbility::OnGrenadeRelease(FGameplayEventData Payload)
{
    // Вызывается из AnimNotify в момент броска

    // Публикация события спавна через EventBus
    // Handler подписан на это событие и спавнит гранату
    PublishSpawnRequest();
}

void USuspenseCoreGrenadeThrowAbility::PublishSpawnRequest()
{
    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetAvatarActorFromActorInfo());

        // Данные для спавна
        EventData.SetString(FName("ThrowableID"), CachedThrowableData.ThrowableID.ToString());
        EventData.SetVector(FName("SpawnLocation"), GetAvatarActorFromActorInfo()->GetActorLocation());
        EventData.SetVector(FName("ThrowDirection"), CalculateThrowDirection());
        EventData.SetFloat(FName("ThrowForce"), CachedThrowableData.ThrowForce);

        EventBus->Publish(TAG_Event_Grenade_SpawnRequest, EventData);
    }
}

void USuspenseCoreGrenadeThrowAbility::CancelAbility(...)
{
    // Публикация события отмены
    PublishEventBus(TAG_Event_ItemUse_Cancelled);

    Super::CancelAbility(...);
}
```

### 6.3 Обновлённый GrenadeHandler

```cpp
// SuspenseCoreGrenadeHandler.cpp (обновлённая версия)

void USuspenseCoreGrenadeHandler::Initialize()
{
    Super::Initialize();

    // Подписка на событие спавна от Ability
    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        SpawnRequestHandle = EventBus->SubscribeNative(
            TAG_Event_Grenade_SpawnRequest,
            this,
            FSuspenseCoreNativeEventCallback::CreateUObject(
                this, &USuspenseCoreGrenadeHandler::OnGrenadeSpawnRequest),
            ESuspenseCoreEventPriority::High
        );
    }
}

FSuspenseCoreItemUseResponse USuspenseCoreGrenadeHandler::Execute(
    const FSuspenseCoreItemUseRequest& Request)
{
    // 1. Валидация
    if (!CanHandle(Request))
    {
        return CreateFailedResponse(ESuspenseCoreItemUseResult::Failed_IncompatibleItems);
    }

    // 2. Получить ASC
    UAbilitySystemComponent* ASC = GetASC(Request.RequestingActor.Get());
    if (!ASC)
    {
        return CreateFailedResponse(ESuspenseCoreItemUseResult::Failed_SystemError);
    }

    // 3. Проверить что ability granted
    if (!ASC->HasAbilityWithTag(TAG_Ability_Grenade_Throw))
    {
        UE_LOG(LogTemp, Warning, TEXT("GrenadeThrowAbility not granted!"));
        return CreateFailedResponse(ESuspenseCoreItemUseResult::Failed_NoHandler);
    }

    // 4. Подготовить EventData для передачи в Ability
    FGameplayEventData EventData;
    EventData.Instigator = Request.RequestingActor.Get();
    EventData.OptionalObject = CreateThrowableDataObject(Request);

    // 5. Активировать Ability через GameplayEvent
    ASC->HandleGameplayEvent(TAG_Ability_Grenade_Throw, &EventData);

    // 6. Вернуть InProgress (спавн произойдёт через EventBus callback)
    return CreateInProgressResponse();

    // БЕЗ LEGACY FALLBACK!
    // Спавн происходит ТОЛЬКО через OnGrenadeSpawnRequest
}

void USuspenseCoreGrenadeHandler::OnGrenadeSpawnRequest(
    FGameplayTag EventTag,
    const FSuspenseCoreEventData& EventData)
{
    // Вызывается когда Ability публикует spawn request через EventBus

    FString ThrowableID = EventData.GetString(FName("ThrowableID"));
    FVector SpawnLocation = EventData.GetVector(FName("SpawnLocation"));
    FVector ThrowDirection = EventData.GetVector(FName("ThrowDirection"));
    float ThrowForce = EventData.GetFloat(FName("ThrowForce"));

    // Получить данные гранаты из базы
    FSuspenseCoreThrowableData* ThrowableData = GetThrowableData(FName(ThrowableID));
    if (!ThrowableData)
    {
        UE_LOG(LogTemp, Error, TEXT("Throwable data not found: %s"), *ThrowableID);
        return;
    }

    // Спавн гранаты
    SpawnGrenadeActor(
        EventData.Source.Get(),
        *ThrowableData,
        SpawnLocation,
        ThrowDirection,
        ThrowForce
    );

    // Публикация события успешного спавна
    PublishEventBus(TAG_Event_Grenade_Spawned);
}

void USuspenseCoreGrenadeHandler::Deinitialize()
{
    // Отписка от EventBus
    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        EventBus->Unsubscribe(SpawnRequestHandle);
    }

    Super::Deinitialize();
}
```

### 6.4 Native Tags для Гранат

```cpp
// SuspenseCoreGrenadeNativeTags.h
#pragma once

#include "NativeGameplayTags.h"

namespace SuspenseCoreGrenadeTags
{
    // Ability Tags
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Grenade_Throw);
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Grenade_Prepare);

    // Event Tags
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Grenade_Release);
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Grenade_SpawnRequest);
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Grenade_Spawned);
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Event_Grenade_Exploded);

    // State Tags
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Grenade_Preparing);
    BRIDGESYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Grenade_Throwing);
}

// SuspenseCoreGrenadeNativeTags.cpp
#include "SuspenseCore/Tags/SuspenseCoreGrenadeNativeTags.h"

namespace SuspenseCoreGrenadeTags
{
    UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Grenade_Throw, "SuspenseCore.Ability.Grenade.Throw");
    UE_DEFINE_GAMEPLAY_TAG(TAG_Ability_Grenade_Prepare, "SuspenseCore.Ability.Grenade.Prepare");

    UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Grenade_Release, "SuspenseCore.Event.Grenade.Release");
    UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Grenade_SpawnRequest, "SuspenseCore.Event.Grenade.SpawnRequest");
    UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Grenade_Spawned, "SuspenseCore.Event.Grenade.Spawned");
    UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Grenade_Exploded, "SuspenseCore.Event.Grenade.Exploded");

    UE_DEFINE_GAMEPLAY_TAG(TAG_State_Grenade_Preparing, "State.Grenade.Preparing");
    UE_DEFINE_GAMEPLAY_TAG(TAG_State_Grenade_Throwing, "State.Grenade.Throwing");
}
```

### 6.5 Добавление Ability в PlayerState

```cpp
// В SuspenseCorePlayerState.cpp или в Blueprint
void ASuspenseCorePlayerState::GrantStartupAbilities()
{
    // ... существующие abilities ...

    // Добавить GrenadeThrowAbility
    AbilitySystemComponent->GiveAbilityOfClass(
        USuspenseCoreGrenadeThrowAbility::StaticClass(),
        1  // Level
    );
}
```

---

## 7. Чеклист Готовности

### Фаза 1: Подготовка
- [ ] AN_GrenadeRelease AnimNotify создан
- [ ] Native Tags добавлены в BridgeSystem
- [ ] ThrowableAttributes.json обновлён
- [ ] Компиляция успешна

### Фаза 2: GrenadeThrowAbility
- [ ] ActivateAbility переписан без таймеров
- [ ] AnimNotify обработчик добавлен
- [ ] Отмена при уроне работает
- [ ] EventBus события публикуются
- [ ] Network replication проверена

### Фаза 3: GrenadeHandler
- [ ] Legacy fallback удалён
- [ ] Подписка на EventBus работает
- [ ] Спавн происходит только через событие
- [ ] Логирование добавлено

### Фаза 4: Интеграция
- [ ] Ability добавлена в StartupAbilities
- [ ] QuickSlotComponent обновлён
- [ ] Animation Montage создан с notify
- [ ] Тестирование в PIE

### Фаза 5: Финальная проверка
- [ ] Все unit тесты проходят
- [ ] Multiplayer тестирование
- [ ] Профилирование производительности
- [ ] Документация обновлена

---

## Заключение

Текущая система гранат требует значительного рефакторинга для достижения AAA качества. Основные проблемы:

1. **GrenadeHandler** содержит дублированную логику спавна (должен только активировать ability)
2. **GrenadeThrowAbility** не использует AnimNotify паттерн (использует таймеры)
3. **EventBus** не используется для коммуникации между Ability и Handler

После рефакторинга система будет соответствовать архитектуре проекта и паттернам, уже использованным в ReloadAbility и MagazineSwapHandler.

---

**Документ готов к ревью.**
**Оцениваемое время реализации: ~36 часов**
