# ЭТАП 2: GAS (Gameplay Ability System) — Детальный план

> **Статус:** Не начат
> **Приоритет:** P0 (КРИТИЧЕСКИЙ)
> **Зависимости:** BridgeSystem (Этап 1)
> **Предыдущий этап:** [BridgeSystem](../BridgeSystem/README.md)

---

## 1. Обзор

GAS модуль содержит Gameplay Ability System компоненты.
Создаём с нуля следующие классы:

| Класс | Legacy референс | Описание |
|-------|-----------------|----------|
| `USuspenseCoreAbilitySystemComponent` | `USuspenseAbilitySystemComponent` | ASC с интеграцией EventBus |
| `USuspenseCoreAttributeSet` | — | Базовый класс для всех атрибутов |
| `USuspenseCoreHealthAttributeSet` | — | Атрибуты здоровья |
| `USuspenseCoreWeaponAttributeSet` | `USuspenseWeaponAttributeSet` | Атрибуты оружия |
| `ISuspenseCoreAbilityInterface` | — | Интерфейс для владельца ASC |

---

## 2. Архитектура GAS модуля

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         GAS MODULE ARCHITECTURE                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│                    ┌─────────────────────────────────┐                       │
│                    │  USuspenseCoreEventManager     │                       │
│                    │      (from BridgeSystem)       │                       │
│                    └───────────────┬─────────────────┘                       │
│                                    │                                         │
│                                    ▼                                         │
│    ┌───────────────────────────────────────────────────────────────────┐    │
│    │              USuspenseCoreAbilitySystemComponent                  │    │
│    │  ═══════════════════════════════════════════════════════════════  │    │
│    │  • Владеет AttributeSets                                          │    │
│    │  • Публикует события через EventBus                               │    │
│    │  • НЕ использует делегаты напрямую                                │    │
│    └───────────────────────────────┬───────────────────────────────────┘    │
│                                    │                                         │
│              ┌─────────────────────┼─────────────────────┐                  │
│              │                     │                     │                  │
│              ▼                     ▼                     ▼                  │
│   ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐         │
│   │ USuspenseCore    │  │ USuspenseCore    │  │ USuspenseCore    │         │
│   │ HealthAttribute  │  │ WeaponAttribute  │  │ StaminaAttribute │         │
│   │ Set              │  │ Set              │  │ Set              │         │
│   └──────────────────┘  └──────────────────┘  └──────────────────┘         │
│              │                     │                     │                  │
│              └─────────────────────┼─────────────────────┘                  │
│                                    │                                         │
│                         ┌──────────▼──────────┐                              │
│                         │  USuspenseCore      │                              │
│                         │  AttributeSet       │                              │
│                         │  (базовый класс)    │                              │
│                         └─────────────────────┘                              │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Классы к созданию

### 3.1 USuspenseCoreAbilitySystemComponent

**Файл:** `GAS/Public/Core/SuspenseCoreAbilitySystemComponent.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreAbilitySystemComponent.generated.h"

class USuspenseCoreEventBus;
class USuspenseCoreEventManager;
struct FSuspenseCoreEventData;

/**
 * USuspenseCoreAbilitySystemComponent
 *
 * Новый ASC с интеграцией EventBus.
 *
 * Ключевые отличия от legacy:
 * - Все изменения атрибутов публикуются через EventBus
 * - Активация/завершение способностей = события
 * - Эффекты = события
 * - НЕТ прямых делегатов для внешних систем
 *
 * Legacy референс: USuspenseAbilitySystemComponent
 */
UCLASS(ClassGroup = "SuspenseCore", meta = (BlueprintSpawnableComponent))
class GAS_API USuspenseCoreAbilitySystemComponent : public UAbilitySystemComponent
{
    GENERATED_BODY()

public:
    USuspenseCoreAbilitySystemComponent();

    // ═══════════════════════════════════════════════════════════════════════
    // UActorComponent Interface
    // ═══════════════════════════════════════════════════════════════════════

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ═══════════════════════════════════════════════════════════════════════
    // UAbilitySystemComponent Interface
    // ═══════════════════════════════════════════════════════════════════════

    virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

    // ═══════════════════════════════════════════════════════════════════════
    // EVENT PUBLISHING
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Публикует событие изменения атрибута
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS|Events")
    void PublishAttributeChangeEvent(
        const FGameplayAttribute& Attribute,
        float OldValue,
        float NewValue,
        AActor* Instigator
    );

    /**
     * Публикует событие активации способности
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS|Events")
    void PublishAbilityActivatedEvent(
        const FGameplayAbilitySpecHandle& Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo& ActivationInfo
    );

    /**
     * Публикует событие завершения способности
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS|Events")
    void PublishAbilityEndedEvent(
        const FGameplayAbilitySpecHandle& Handle,
        bool bWasCancelled
    );

    /**
     * Публикует событие применения эффекта
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS|Events")
    void PublishEffectAppliedEvent(
        const FActiveGameplayEffectHandle& Handle,
        const FGameplayEffectSpec& Spec
    );

    /**
     * Публикует событие удаления эффекта
     */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|GAS|Events")
    void PublishEffectRemovedEvent(
        const FActiveGameplayEffectHandle& Handle
    );

    // ═══════════════════════════════════════════════════════════════════════
    // CONFIGURATION
    // ═══════════════════════════════════════════════════════════════════════

    /** Автоматически публиковать события изменения атрибутов */
    UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
    bool bAutoPublishAttributeChanges = true;

    /** Автоматически публиковать события способностей */
    UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
    bool bAutoPublishAbilityEvents = true;

    /** Автоматически публиковать события эффектов */
    UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
    bool bAutoPublishEffectEvents = true;

    /** Список атрибутов для публикации (пустой = все) */
    UPROPERTY(EditDefaultsOnly, Category = "SuspenseCore|Config")
    TArray<FGameplayAttribute> AttributesToPublish;

protected:
    // ═══════════════════════════════════════════════════════════════════════
    // INTERNAL
    // ═══════════════════════════════════════════════════════════════════════

    /** Кэшированный EventBus */
    UPROPERTY()
    TWeakObjectPtr<USuspenseCoreEventBus> CachedEventBus;

    /** Получить EventBus */
    USuspenseCoreEventBus* GetEventBus() const;

    /** Настроить callbacks на изменения */
    void SetupAttributeChangeCallbacks();

    /** Отписаться от callbacks */
    void TeardownAttributeChangeCallbacks();

    /** Callback при изменении атрибута */
    void OnAttributeChanged(const FOnAttributeChangeData& Data);

    /** Handle для отслеживания изменений */
    TMap<FGameplayAttribute, FDelegateHandle> AttributeChangeDelegates;

private:
    /** Проверка нужно ли публиковать атрибут */
    bool ShouldPublishAttribute(const FGameplayAttribute& Attribute) const;

    /** Формирование GameplayTag для атрибута */
    FGameplayTag GetAttributeEventTag(const FGameplayAttribute& Attribute) const;
};
```

### 3.2 USuspenseCoreAttributeSet (Базовый класс)

**Файл:** `GAS/Public/Core/SuspenseCoreAttributeSet.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "SuspenseCoreAttributeSet.generated.h"

/**
 * USuspenseCoreAttributeSet
 *
 * Базовый класс для всех AttributeSet в проекте.
 *
 * Особенности:
 * - Интеграция с EventBus (через владеющий ASC)
 * - Стандартные макросы для атрибутов
 * - Валидация значений
 */
UCLASS(Abstract)
class GAS_API USuspenseCoreAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    USuspenseCoreAttributeSet();

    // ═══════════════════════════════════════════════════════════════════════
    // UAttributeSet Interface
    // ═══════════════════════════════════════════════════════════════════════

    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    // ═══════════════════════════════════════════════════════════════════════
    // HELPERS
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Получить владеющий ASC
     */
    USuspenseCoreAbilitySystemComponent* GetOwningASC() const;

    /**
     * Получить владеющего Actor
     */
    AActor* GetOwningActor() const;

    /**
     * Clamp значение атрибута
     */
    void ClampAttribute(const FGameplayAttribute& Attribute, float& Value, float Min, float Max) const;

    /**
     * Проверить и применить clamp к CurrentValue
     */
    void AdjustAttributeForMaxChange(
        const FGameplayAttribute& AffectedAttribute,
        const FGameplayAttribute& MaxAttribute,
        float NewMaxValue,
        const FGameplayAttribute& AffectedAttributeProperty
    );
};

// ═══════════════════════════════════════════════════════════════════════════
// ATTRIBUTE MACROS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * Макрос для объявления атрибута с accessor функциями
 * Использование: SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreHealthAttributeSet, Health)
 */
#define SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
```

### 3.3 USuspenseCoreHealthAttributeSet

**Файл:** `GAS/Public/Core/Attributes/SuspenseCoreHealthAttributeSet.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Core/SuspenseCoreAttributeSet.h"
#include "SuspenseCoreHealthAttributeSet.generated.h"

/**
 * USuspenseCoreHealthAttributeSet
 *
 * Атрибуты здоровья персонажа.
 *
 * Атрибуты:
 * - Health: Текущее здоровье
 * - MaxHealth: Максимальное здоровье
 * - HealthRegenRate: Скорость регенерации
 * - Shield: Щит (поглощает урон перед Health)
 * - MaxShield: Максимальный щит
 */
UCLASS()
class GAS_API USuspenseCoreHealthAttributeSet : public USuspenseCoreAttributeSet
{
    GENERATED_BODY()

public:
    USuspenseCoreHealthAttributeSet();

    // ═══════════════════════════════════════════════════════════════════════
    // ATTRIBUTES
    // ═══════════════════════════════════════════════════════════════════════

    /** Текущее здоровье */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Health")
    FGameplayAttributeData Health;
    SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreHealthAttributeSet, Health)

    /** Максимальное здоровье */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Health")
    FGameplayAttributeData MaxHealth;
    SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreHealthAttributeSet, MaxHealth)

    /** Скорость регенерации здоровья (в секунду) */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_HealthRegenRate, Category = "Health")
    FGameplayAttributeData HealthRegenRate;
    SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreHealthAttributeSet, HealthRegenRate)

    /** Текущий щит */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Shield, Category = "Shield")
    FGameplayAttributeData Shield;
    SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreHealthAttributeSet, Shield)

    /** Максимальный щит */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxShield, Category = "Shield")
    FGameplayAttributeData MaxShield;
    SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreHealthAttributeSet, MaxShield)

    // ═══════════════════════════════════════════════════════════════════════
    // META ATTRIBUTES (не реплицируются, для расчётов)
    // ═══════════════════════════════════════════════════════════════════════

    /** Входящий урон (для расчёта в PostGameplayEffectExecute) */
    UPROPERTY(BlueprintReadOnly, Category = "Meta")
    FGameplayAttributeData IncomingDamage;
    SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreHealthAttributeSet, IncomingDamage)

    /** Входящее лечение */
    UPROPERTY(BlueprintReadOnly, Category = "Meta")
    FGameplayAttributeData IncomingHealing;
    SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreHealthAttributeSet, IncomingHealing)

    // ═══════════════════════════════════════════════════════════════════════
    // UAttributeSet Interface
    // ═══════════════════════════════════════════════════════════════════════

    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    // ═══════════════════════════════════════════════════════════════════════
    // REPLICATION
    // ═══════════════════════════════════════════════════════════════════════

    UFUNCTION()
    virtual void OnRep_Health(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    virtual void OnRep_HealthRegenRate(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    virtual void OnRep_Shield(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    virtual void OnRep_MaxShield(const FGameplayAttributeData& OldValue);

private:
    /** Обработка входящего урона */
    void HandleIncomingDamage(const FGameplayEffectModCallbackData& Data);

    /** Обработка входящего лечения */
    void HandleIncomingHealing(const FGameplayEffectModCallbackData& Data);
};
```

### 3.4 USuspenseCoreWeaponAttributeSet

**Файл:** `GAS/Public/Core/Attributes/SuspenseCoreWeaponAttributeSet.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Core/SuspenseCoreAttributeSet.h"
#include "SuspenseCoreWeaponAttributeSet.generated.h"

/**
 * USuspenseCoreWeaponAttributeSet
 *
 * Атрибуты оружия.
 *
 * Legacy референс: USuspenseWeaponAttributeSet
 *
 * Атрибуты:
 * - CurrentAmmo: Патроны в магазине
 * - MaxAmmo: Размер магазина
 * - ReserveAmmo: Запас патронов
 * - MaxReserveAmmo: Максимальный запас
 * - FireRate: Скорострельность
 * - ReloadSpeed: Скорость перезарядки
 * - Damage: Базовый урон
 * - Spread: Разброс
 */
UCLASS()
class GAS_API USuspenseCoreWeaponAttributeSet : public USuspenseCoreAttributeSet
{
    GENERATED_BODY()

public:
    USuspenseCoreWeaponAttributeSet();

    // ═══════════════════════════════════════════════════════════════════════
    // AMMO ATTRIBUTES
    // ═══════════════════════════════════════════════════════════════════════

    /** Текущие патроны в магазине */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentAmmo, Category = "Ammo")
    FGameplayAttributeData CurrentAmmo;
    SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, CurrentAmmo)

    /** Максимум патронов в магазине */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxAmmo, Category = "Ammo")
    FGameplayAttributeData MaxAmmo;
    SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, MaxAmmo)

    /** Запас патронов */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ReserveAmmo, Category = "Ammo")
    FGameplayAttributeData ReserveAmmo;
    SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, ReserveAmmo)

    /** Максимальный запас */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxReserveAmmo, Category = "Ammo")
    FGameplayAttributeData MaxReserveAmmo;
    SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, MaxReserveAmmo)

    // ═══════════════════════════════════════════════════════════════════════
    // WEAPON STATS
    // ═══════════════════════════════════════════════════════════════════════

    /** Скорострельность (выстрелов в минуту) */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FireRate, Category = "Stats")
    FGameplayAttributeData FireRate;
    SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, FireRate)

    /** Скорость перезарядки (множитель) */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ReloadSpeed, Category = "Stats")
    FGameplayAttributeData ReloadSpeed;
    SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, ReloadSpeed)

    /** Базовый урон */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Damage, Category = "Stats")
    FGameplayAttributeData Damage;
    SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, Damage)

    /** Разброс (в градусах) */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Spread, Category = "Stats")
    FGameplayAttributeData Spread;
    SUSPENSE_CORE_ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, Spread)

    // ═══════════════════════════════════════════════════════════════════════
    // UAttributeSet Interface
    // ═══════════════════════════════════════════════════════════════════════

    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ═══════════════════════════════════════════════════════════════════════
    // UTILITY
    // ═══════════════════════════════════════════════════════════════════════

    /** Проверить можно ли стрелять */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Weapon")
    bool CanFire() const;

    /** Проверить нужна ли перезарядка */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Weapon")
    bool NeedsReload() const;

    /** Проверить есть ли патроны в запасе */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Weapon")
    bool HasReserveAmmo() const;

    /** Получить время между выстрелами (в секундах) */
    UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Weapon")
    float GetTimeBetweenShots() const;

protected:
    // ═══════════════════════════════════════════════════════════════════════
    // REPLICATION
    // ═══════════════════════════════════════════════════════════════════════

    UFUNCTION()
    virtual void OnRep_CurrentAmmo(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    virtual void OnRep_MaxAmmo(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    virtual void OnRep_ReserveAmmo(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    virtual void OnRep_MaxReserveAmmo(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    virtual void OnRep_FireRate(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    virtual void OnRep_ReloadSpeed(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    virtual void OnRep_Damage(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    virtual void OnRep_Spread(const FGameplayAttributeData& OldValue);
};
```

### 3.5 ISuspenseCoreAbilityInterface

**Файл:** `GAS/Public/Core/Interfaces/SuspenseCoreAbilityInterface.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SuspenseCoreAbilityInterface.generated.h"

class USuspenseCoreAbilitySystemComponent;

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreAbilityInterface : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreAbilityInterface
 *
 * Интерфейс для Actor'ов, владеющих AbilitySystemComponent.
 * Стандартный способ получения ASC.
 */
class GAS_API ISuspenseCoreAbilityInterface
{
    GENERATED_BODY()

public:
    /**
     * Получить AbilitySystemComponent
     */
    virtual USuspenseCoreAbilitySystemComponent* GetSuspenseCoreAbilitySystemComponent() const = 0;

    /**
     * Проверить жив ли Actor (для GAS)
     */
    virtual bool IsAlive() const { return true; }

    /**
     * Получить уровень для GAS (для скейлинга эффектов)
     */
    virtual int32 GetAbilityLevel() const { return 1; }
};
```

---

## 4. Интеграция с EventBus

### 4.1 Публикуемые события

| Событие | Tag | Payload |
|---------|-----|---------|
| Изменение атрибута | `SuspenseCore.Event.GAS.Attribute.Changed` | AttributeName, OldValue, NewValue, Instigator |
| Здоровье изменилось | `SuspenseCore.Event.GAS.Attribute.Health` | OldValue, NewValue, Instigator |
| Патроны изменились | `SuspenseCore.Event.GAS.Attribute.Ammo` | CurrentAmmo, MaxAmmo, ReserveAmmo |
| Способность активирована | `SuspenseCore.Event.GAS.Ability.Activated` | AbilityClass, AbilityLevel |
| Способность завершена | `SuspenseCore.Event.GAS.Ability.Ended` | AbilityClass, WasCancelled |
| Эффект применён | `SuspenseCore.Event.GAS.Effect.Applied` | EffectClass, Duration, Stacks |
| Эффект удалён | `SuspenseCore.Event.GAS.Effect.Removed` | EffectClass |

### 4.2 Пример интеграции

```cpp
void USuspenseCoreAbilitySystemComponent::PublishAttributeChangeEvent(
    const FGameplayAttribute& Attribute,
    float OldValue,
    float NewValue,
    AActor* Instigator)
{
    if (!bAutoPublishAttributeChanges) return;
    if (!ShouldPublishAttribute(Attribute)) return;

    USuspenseCoreEventBus* EventBus = GetEventBus();
    if (!EventBus) return;

    // Создаём данные события
    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetOwner());
    EventData.SetString(TEXT("AttributeName"), Attribute.GetName());
    EventData.SetFloat(TEXT("OldValue"), OldValue);
    EventData.SetFloat(TEXT("NewValue"), NewValue);
    EventData.SetObject(TEXT("Instigator"), Instigator);

    // Общее событие изменения атрибута
    EventBus->Publish(
        FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.GAS.Attribute.Changed")),
        EventData
    );

    // Специфичное событие для атрибута (например Health)
    FGameplayTag SpecificTag = GetAttributeEventTag(Attribute);
    if (SpecificTag.IsValid())
    {
        EventBus->Publish(SpecificTag, EventData);
    }
}
```

---

## 5. Зависимости модуля

### 5.1 GAS.Build.cs (ЦЕЛЕВОЙ)

```csharp
PublicDependencyModuleNames.AddRange(
    new string[]
    {
        "Core",
        "CoreUObject",
        "Engine",
        "GameplayAbilities",
        "GameplayTags",
        "GameplayTasks",
        "BridgeSystem"          // ← ЕДИНСТВЕННАЯ внутренняя зависимость
    }
);

PrivateDependencyModuleNames.AddRange(
    new string[]
    {
        "NetCore"
    }
);

// УБРАТЬ зависимость от SuspenseCore (если была)
```

---

## 6. Чеклист выполнения

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    ЧЕКЛИСТ ЭТАПА 2: GAS                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ ПОДГОТОВКА                                                                   │
│ ☐ Создать папку GAS/Public/Core/                                            │
│ ☐ Создать папку GAS/Public/Core/Attributes/                                 │
│ ☐ Создать папку GAS/Public/Core/Interfaces/                                 │
│ ☐ Создать папку GAS/Private/Core/                                           │
│                                                                              │
│ ABILITY SYSTEM COMPONENT                                                     │
│ ☐ Создать SuspenseCoreAbilitySystemComponent.h                              │
│ ☐ Создать SuspenseCoreAbilitySystemComponent.cpp                            │
│   ☐ BeginPlay/EndPlay                                                       │
│   ☐ InitAbilityActorInfo                                                    │
│   ☐ PublishAttributeChangeEvent                                             │
│   ☐ PublishAbilityActivatedEvent                                            │
│   ☐ PublishAbilityEndedEvent                                                │
│   ☐ PublishEffectAppliedEvent                                               │
│   ☐ PublishEffectRemovedEvent                                               │
│   ☐ SetupAttributeChangeCallbacks                                           │
│   ☐ Интеграция с EventBus                                                   │
│                                                                              │
│ BASE ATTRIBUTE SET                                                           │
│ ☐ Создать SuspenseCoreAttributeSet.h                                        │
│ ☐ Создать SuspenseCoreAttributeSet.cpp                                      │
│   ☐ PreAttributeChange                                                      │
│   ☐ PostAttributeChange                                                     │
│   ☐ PostGameplayEffectExecute                                               │
│   ☐ Helper методы                                                           │
│   ☐ Макрос SUSPENSE_CORE_ATTRIBUTE_ACCESSORS                                │
│                                                                              │
│ HEALTH ATTRIBUTE SET                                                         │
│ ☐ Создать SuspenseCoreHealthAttributeSet.h                                  │
│ ☐ Создать SuspenseCoreHealthAttributeSet.cpp                                │
│   ☐ Health, MaxHealth атрибуты                                              │
│   ☐ Shield, MaxShield атрибуты                                              │
│   ☐ IncomingDamage, IncomingHealing мета-атрибуты                           │
│   ☐ Репликация                                                              │
│   ☐ Обработка урона                                                         │
│                                                                              │
│ WEAPON ATTRIBUTE SET                                                         │
│ ☐ Создать SuspenseCoreWeaponAttributeSet.h                                  │
│ ☐ Создать SuspenseCoreWeaponAttributeSet.cpp                                │
│   ☐ CurrentAmmo, MaxAmmo, ReserveAmmo атрибуты                              │
│   ☐ FireRate, ReloadSpeed, Damage, Spread атрибуты                          │
│   ☐ Репликация                                                              │
│   ☐ Utility методы (CanFire, NeedsReload, etc.)                             │
│                                                                              │
│ INTERFACE                                                                    │
│ ☐ Создать SuspenseCoreAbilityInterface.h                                    │
│   ☐ GetSuspenseCoreAbilitySystemComponent                                   │
│   ☐ IsAlive                                                                 │
│   ☐ GetAbilityLevel                                                         │
│                                                                              │
│ ЗАВИСИМОСТИ                                                                  │
│ ☐ Обновить GAS.Build.cs                                                     │
│ ☐ Убрать SuspenseCore из зависимостей                                       │
│ ☐ Добавить BridgeSystem в зависимости                                       │
│                                                                              │
│ ТЕСТИРОВАНИЕ                                                                 │
│ ☐ Unit тест: ASC публикует события                                          │
│ ☐ Unit тест: AttributeSet clamp работает                                    │
│ ☐ Unit тест: Репликация атрибутов                                           │
│ ☐ Integration тест: GAS + EventBus                                          │
│                                                                              │
│ ФИНАЛИЗАЦИЯ                                                                  │
│ ☐ Компиляция без ошибок                                                     │
│ ☐ Компиляция без warnings                                                   │
│ ☐ Изолированная компиляция модуля                                           │
│ ☐ Code review пройден                                                       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Пример использования

### 7.1 Создание персонажа с GAS

```cpp
// В SuspenseCoreCharacter
void ASuspenseCoreCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    // ASC создаётся как компонент
    AbilitySystemComponent = CreateDefaultSubobject<USuspenseCoreAbilitySystemComponent>(
        TEXT("AbilitySystemComponent")
    );

    // Инициализация
    AbilitySystemComponent->InitAbilityActorInfo(this, this);
}

USuspenseCoreAbilitySystemComponent* ASuspenseCoreCharacter::GetSuspenseCoreAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}
```

### 7.2 Подписка на изменение здоровья

```cpp
// В UI виджете
void UHealthBarWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (auto* Manager = USuspenseCoreEventManager::Get(this))
    {
        Manager->GetEventBus()->Subscribe(
            FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.GAS.Attribute.Health")),
            FSuspenseCoreEventCallback::CreateUObject(this, &UHealthBarWidget::OnHealthChanged)
        );
    }
}

void UHealthBarWidget::OnHealthChanged(FGameplayTag EventTag, const FSuspenseCoreEventData& Data)
{
    // Проверяем что это наш персонаж
    if (Data.Source != OwnerCharacter) return;

    float NewHealth = Data.GetFloat(TEXT("NewValue"));
    float MaxHealth = Data.GetFloat(TEXT("MaxValue")); // нужно добавить в публикацию

    UpdateHealthBar(NewHealth / MaxHealth);
}
```

---

*Документ создан: 2025-11-29*
*Этап: 2 из 7*
*Зависимости: BridgeSystem*
