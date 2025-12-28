# Attribute System Developer Guide

> **Version:** 1.0
> **Last Updated:** December 2025
> **Module:** GAS

---

## Table of Contents

1. [Overview](#overview)
2. [SuspenseCoreAttributeSet](#suspensecoreattributeset)
3. [Attribute Clamping](#attribute-clamping)
4. [Stamina Regeneration](#stamina-regeneration)
5. [Passive Effects](#passive-effects)
6. [Common Pitfalls](#common-pitfalls)
7. [Troubleshooting](#troubleshooting)

---

## Overview

SuspenseCore использует Gameplay Ability System (GAS) для управления атрибутами персонажа. Атрибуты определены в `USuspenseCoreAttributeSet` и модифицируются через `GameplayEffects`.

### Основные атрибуты

| Attribute | Default | Description |
|-----------|---------|-------------|
| `Health` | 100 | Текущее здоровье |
| `MaxHealth` | 100 | Максимальное здоровье |
| `HealthRegen` | 1.0 | Регенерация HP/сек |
| `Stamina` | 100 | Текущая стамина |
| `MaxStamina` | 100 | Максимальная стамина |
| `StaminaRegen` | 10.0 | Регенерация STA/сек |
| `Armor` | 0 | Броня (уменьшает урон) |
| `AttackPower` | 1.0 | Множитель урона |
| `MovementSpeed` | 1.0 | Множитель скорости |

---

## SuspenseCoreAttributeSet

### Lifecycle

```
┌─────────────────────────────────────────────────────────────────┐
│                    ATTRIBUTE CHANGE FLOW                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  GameplayEffect Applied                                          │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────────────────────────────────┐                   │
│  │ PreAttributeChange()                      │                   │
│  │  - Clamps DISPLAYED value                 │                   │
│  │  - Does NOT modify base value             │                   │
│  │  - Called for EVERY Get() operation       │                   │
│  └──────────────────────────────────────────┘                   │
│         │                                                        │
│         ▼                                                        │
│  ┌──────────────────────────────────────────┐                   │
│  │ PostGameplayEffectExecute()               │                   │
│  │  - Called AFTER effect applied            │                   │
│  │  - Can modify BASE value                  │                   │
│  │  - Broadcasts EventBus events             │                   │
│  └──────────────────────────────────────────┘                   │
│         │                                                        │
│         ▼                                                        │
│  EventBus: SuspenseCore.Event.GAS.Attribute.<Name>              │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Ключевые методы

| Method | Purpose |
|--------|---------|
| `PreAttributeChange()` | Clamping перед Get() |
| `PostGameplayEffectExecute()` | Обработка после применения эффекта |
| `ClampAttribute()` | Логика clamping для каждого атрибута |
| `BroadcastAttributeChange()` | Публикация события в EventBus |

---

## Attribute Clamping

### Проблема

GAS имеет важную особенность:
- `PreAttributeChange()` клампит **возвращаемое значение** при Get()
- Но **base value** может продолжать расти/падать бесконтрольно

```
┌─────────────────────────────────────────────────────────────────┐
│ ПРОБЛЕМА: Накопление base value                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Infinite Effect: +10 STA/sec (Period=0.1, Additive +1/tick)    │
│                                                                  │
│  Time 0.0s:  BaseValue = 100, MaxStamina = 100                  │
│              GetStamina() returns 100 (clamped) ✓               │
│                                                                  │
│  Time 0.1s:  BaseValue = 101 (!), MaxStamina = 100              │
│              GetStamina() returns 100 (clamped) ✓               │
│              BUT base is already > max!                          │
│                                                                  │
│  Time 1.0s:  BaseValue = 110 (!!!), MaxStamina = 100            │
│              GetStamina() returns 100 (clamped) ✓               │
│              Потеряно 10 единиц "скрытой" стамины               │
│                                                                  │
│  Проблема: Когда игрок тратит стамину, base уменьшается        │
│  с 110, а не со 100. Эффективно игрок имеет 110 стамины!       │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Решение

В `PostGameplayEffectExecute()` при **положительном изменении** (рег) мы принудительно устанавливаем base value = clamped value:

```cpp
else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
{
    const float MaxST = GetMaxStamina();
    const float StaminaDelta = Data.EvaluatedData.Magnitude;
    float CurrentStamina = GetStamina(); // Already clamped

    // КРИТИЧНО: Для положительных изменений (реген) всегда
    // "сплющиваем" base value чтобы предотвратить накопление
    if (StaminaDelta > 0.0f)
    {
        const float ClampedValue = FMath::Clamp(CurrentStamina, 0.0f, MaxST);
        if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
        {
            ASC->SetNumericAttributeBase(GetStaminaAttribute(), ClampedValue);
        }
        CurrentStamina = ClampedValue;
    }

    // Broadcast event...
}
```

### Почему проверка `GetStamina() > MaxStamina` НЕ работает

```cpp
// НЕПРАВИЛЬНО - никогда не сработает!
if (GetStamina() > MaxST)  // GetStamina() УЖЕ clamped!
{
    // Этот код никогда не выполнится
}

// ПРАВИЛЬНО - проверяем Magnitude эффекта
if (StaminaDelta > 0.0f)  // Это был реген
{
    // Принудительно устанавливаем base value
    ASC->SetNumericAttributeBase(GetStaminaAttribute(), ClampedValue);
}
```

---

## Stamina Regeneration

### Архитектура регенерации

```
┌─────────────────────────────────────────────────────────────────┐
│               STAMINA REGENERATION SYSTEM                        │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  SuspenseCoreCharacterClassData                                  │
│  └── PassiveEffects[]                                           │
│       └── SuspenseCoreEffect_StaminaRegen                       │
│                                                                  │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ USuspenseCoreEffect_StaminaRegen                           │ │
│  │                                                             │ │
│  │   DurationPolicy: Infinite                                  │ │
│  │   Period: 0.1f (10 ticks/sec)                              │ │
│  │                                                             │ │
│  │   Modifier:                                                 │ │
│  │     Attribute: Stamina                                      │ │
│  │     ModifierOp: Additive                                    │ │
│  │     Magnitude: +1.0 per tick = +10.0/sec                   │ │
│  │                                                             │ │
│  │   BlockingTags: State.Sprinting, State.Dead                │ │
│  └────────────────────────────────────────────────────────────┘ │
│                                                                  │
│  Применяется при выборе класса через:                           │
│  USuspenseCoreCharacterClassSubsystem::ApplyClassToPlayer()     │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Как это работает

1. **Применение эффекта** - При выборе класса, все `PassiveEffects` из `SuspenseCoreCharacterClassData` применяются к ASC

2. **Periodic Tick** - Каждые 0.1 секунды GameplayEffect добавляет +1 к Stamina

3. **Tag Blocking** - Если на персонаже есть тег `State.Sprinting` или `State.Dead`, реген не применяется

4. **Clamping** - `PostGameplayEffectExecute()` "сплющивает" base value если StaminaDelta > 0

### Настройка скорости регенерации

Скорость регенерации определяется в двух местах:

1. **SuspenseCoreEffect_StaminaRegen** - базовая скорость (+1/tick = +10/sec)

2. **SuspenseCoreCharacterClassData.AttributeModifiers.StaminaRegenMultiplier** - множитель для класса

```
Итоговая скорость = BaseRegen * StaminaRegenMultiplier
                  = 10.0 * 1.5 (для класса с 1.5x)
                  = 15.0 STA/sec
```

---

## Passive Effects

### Что такое Passive Effects

Passive Effects - это GameplayEffects которые применяются постоянно при выборе класса персонажа.

```cpp
// SuspenseCoreCharacterClassData.h
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effects")
TArray<TSubclassOf<UGameplayEffect>> PassiveEffects;
```

### Типичные Passive Effects

| Effect | Purpose | Duration |
|--------|---------|----------|
| `SuspenseCoreEffect_StaminaRegen` | Регенерация стамины | Infinite |
| `SuspenseCoreEffect_HealthRegen` | Регенерация здоровья | Infinite |
| `SuspenseCoreEffect_ClassStats` | Статы класса | Infinite |

### Применение Passive Effects

```cpp
// В SuspenseCoreCharacterClassSubsystem::ApplyClassToPlayer()
for (TSubclassOf<UGameplayEffect> EffectClass : ClassData->PassiveEffects)
{
    if (EffectClass)
    {
        FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
        FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.0f, Context);

        if (Spec.IsValid())
        {
            ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
        }
    }
}
```

---

## Common Pitfalls

### 1. Infinite + Period + Additive Overflow

**Проблема:** Additive модификатор на Infinite эффекте накапливает base value бесконечно.

**Симптомы:**
- Стамина визуально на максимуме, но можно потратить "больше 100"
- При отладке base value > MaxStamina

**Решение:** Всегда устанавливать `SetNumericAttributeBase()` после положительных изменений.

---

### 2. PreAttributeChange не сохраняет clamp

**Проблема:** `PreAttributeChange()` только клампит возвращаемое значение, не base.

**Неправильное ожидание:**
```cpp
// Это НЕ меняет base value!
void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
    // Base value остаётся неизменным!
}
```

**Правильное понимание:**
- `PreAttributeChange` = фильтр для Get()
- Чтобы изменить base, используйте `SetNumericAttributeBase()` в `PostGameplayEffectExecute()`

---

### 3. Проверка clamped значения

**Проблема:** Проверка `GetAttribute() > Max` всегда false после clamping.

**Неправильно:**
```cpp
if (GetStamina() > GetMaxStamina())  // Всегда false!
{
    // Никогда не выполнится
}
```

**Правильно:**
```cpp
// Используйте Magnitude из эффекта
if (Data.EvaluatedData.Magnitude > 0.0f)
{
    // Это был положительный модификатор (реген)
    // Нужно сплющить base value
}
```

---

### 4. ASC Timer вместо GameplayEffect

**Проблема:** Попытка реализовать реген через таймер в ASC.

**Почему плохо:**
- Не учитывает blocking tags
- Не интегрируется с GAS prediction
- Сложнее отслеживать и дебажить
- Дублирование логики

**Правильный подход:** Используйте Infinite + Period GameplayEffect с правильным clamping в AttributeSet.

---

## Troubleshooting

### Стамина превышает MaxStamina

**Диагностика:**
```cpp
// Добавьте лог в PostGameplayEffectExecute
UE_LOG(LogTemp, Warning, TEXT("Stamina: Base=%.2f, Current=%.2f, Max=%.2f"),
    Stamina.GetBaseValue(),
    Stamina.GetCurrentValue(),
    GetMaxStamina());
```

**Решение:** Проверьте что `SetNumericAttributeBase()` вызывается при `StaminaDelta > 0.0f`.

---

### Реген не работает

**Проверки:**
1. Passive Effect применён? Проверьте `ASC->GetActiveEffects()`
2. Blocking tags? Проверьте `State.Sprinting`, `State.Dead`
3. Effect настроен? DurationPolicy=Infinite, Period=0.1f
4. Modifier правильный? Additive, Stamina attribute

**Лог для отладки:**
```cpp
UE_LOG(LogTemp, Warning, TEXT("[StaminaRegen] Delta: %.2f, Current: %.2f"),
    StaminaDelta, CurrentStamina);
```

---

### События не публикуются

**Проверки:**
1. `bPublishAttributeEvents` = true в ASC?
2. EventBus существует?
3. GameplayTag зарегистрирован?

**Лог для отладки:**
```cpp
UE_LOG(LogTemp, Warning, TEXT("[AttributeSet] Broadcasting: %s (%.2f -> %.2f)"),
    *Attribute.GetName(), OldValue, NewValue);
```

---

## Best Practices

1. **Всегда используйте `SetNumericAttributeBase()`** для коррекции base value при регене

2. **Проверяйте `Magnitude`**, а не `GetAttribute() > Max`

3. **Passive Effects** - используйте для постоянных эффектов вместо таймеров

4. **Логируйте** изменения атрибутов в Debug билдах

5. **Тестируйте** edge cases:
   - Стамина на максимуме
   - Стамина на нуле
   - Быстрое чередование расхода и регена

---

## Related Documentation

- [MovementAbilities_SetupGuide.md](MovementAbilities_SetupGuide.md) - Настройка Sprint/Jump/Crouch
- [SuspenseCoreCharacterClassData](../../Source/GAS/Public/SuspenseCore/Data/SuspenseCoreCharacterClassData.h) - Данные класса персонажа

---

*Last Updated: 2025-12-28*
