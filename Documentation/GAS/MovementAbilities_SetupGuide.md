# Movement Abilities Setup Guide

## Архитектура SetByCaller

Все параметры движения настраиваются через **UPROPERTY в Ability** и передаются в **GameplayEffect через SetByCaller**.

```
┌─────────────────────────────────────────────────────────────────┐
│                        EDITOR / BLUEPRINT                        │
│  ┌─────────────────────────────────────────────────────────────┐ │
│  │ BP_SprintAbility (Blueprint Child)                          │ │
│  │   SprintSpeedMultiplier = 1.5    ◄── Настраивается в Editor │ │
│  │   StaminaCostPerSecond = 15.0    ◄── Настраивается в Editor │ │
│  └─────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                        C++ ABILITY                               │
│  ApplySprintEffects() {                                         │
│    EffectSpec.SetSetByCallerMagnitude(                          │
│      Data.Cost.SpeedMultiplier,     ◄── Native Tag              │
│      SprintSpeedMultiplier - 1.0    ◄── Значение из UPROPERTY   │
│    );                                                            │
│  }                                                               │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      GAMEPLAY EFFECT                             │
│  USuspenseCoreEffect_SprintBuff                                  │
│    Modifier: MultiplyAdditive                                    │
│    Magnitude: SetByCaller(Data.Cost.SpeedMultiplier)            │
│                        ▲                                         │
│                        │                                         │
│            Читает значение из EffectSpec                         │
└─────────────────────────────────────────────────────────────────┘
```

---

## Настройка в Unreal Editor

### 1. Sprint Ability

**Создание Blueprint:**
1. Content Browser → Right Click → Blueprint Class
2. Parent: `USuspenseCoreCharacterSprintAbility`
3. Name: `BP_SprintAbility`

**Настройка параметров (Details Panel):**

| Property | Default | Description |
|----------|---------|-------------|
| `Sprint Speed Multiplier` | 1.5 | Множитель скорости (1.5 = +50%) |
| `Stamina Cost Per Second` | 15.0 | Расход стамины в секунду |
| `Minimum Stamina To Sprint` | 10.0 | Минимум для начала спринта |
| `Stamina Exhaustion Threshold` | 1.0 | Порог для остановки |
| `Sprint Buff Effect Class` | GE_SprintBuff | GameplayEffect для скорости |
| `Sprint Cost Effect Class` | GE_SprintCost | GameplayEffect для стамины |

```
┌─────────────────────────────────────────────────────────────┐
│ Details - BP_SprintAbility                                   │
├─────────────────────────────────────────────────────────────┤
│ ▼ SuspenseCore|Sprint|Speed                                 │
│   Sprint Speed Multiplier     [1.5        ]  ◄── Изменить   │
│                                                              │
│ ▼ SuspenseCore|Sprint|Stamina                               │
│   Stamina Cost Per Second     [15.0       ]  ◄── Изменить   │
│   Minimum Stamina To Sprint   [10.0       ]                 │
│   Stamina Exhaustion Threshold [1.0        ]                │
│                                                              │
│ ▼ SuspenseCore|Sprint|Effects                               │
│   Sprint Buff Effect Class    [GE_SprintBuff  ▼]            │
│   Sprint Cost Effect Class    [GE_SprintCost  ▼]            │
└─────────────────────────────────────────────────────────────┘
```

---

### 2. Jump Ability

**Создание Blueprint:**
1. Parent: `USuspenseCoreCharacterJumpAbility`
2. Name: `BP_JumpAbility`

**Настройка параметров:**

| Property | Default | Description |
|----------|---------|-------------|
| `Stamina Cost Per Jump` | 10.0 | Расход стамины за прыжок |
| `Minimum Stamina To Jump` | 5.0 | Минимум для прыжка |
| `Jump Power Multiplier` | 1.0 | Множитель силы прыжка |
| `Jump Stamina Cost Effect Class` | GE_JumpCost | GameplayEffect |

---

### 3. Crouch Ability

**Создание Blueprint:**
1. Parent: `USuspenseCoreCharacterCrouchAbility`
2. Name: `BP_CrouchAbility`

**Настройка параметров:**

| Property | Default | Description |
|----------|---------|-------------|
| `Crouch Speed Multiplier` | 0.5 | Множитель скорости (0.5 = 50%) |
| `Toggle Mode` | false | Переключение вместо удержания |
| `Crouch Debuff Effect Class` | GE_CrouchDebuff | GameplayEffect |

---

## SetByCaller Tags Reference

| Native Tag | Used By | Description |
|------------|---------|-------------|
| `Data.Cost.Stamina` | JumpCost | Мгновенный расход стамины |
| `Data.Cost.StaminaPerSecond` | SprintCost | Расход стамины за тик (0.1s) |
| `Data.Cost.SpeedMultiplier` | SprintBuff, CrouchDebuff | Изменение скорости |

---

## GameplayEffect Blueprint Setup

Если нужно создать Effects через Blueprint:

### GE_SprintBuff (Blueprint)

1. Create Blueprint: Parent `UGameplayEffect`
2. **Duration Policy:** Infinite
3. **Modifiers:**
   - Attribute: `MovementSpeed`
   - Modifier Op: `Multiply Additive`
   - Magnitude Type: `Set By Caller`
   - Set By Caller Tag: `Data.Cost.SpeedMultiplier`

4. **Target Tags:**
   - Added: `State.Sprinting`

### GE_SprintCost (Blueprint)

1. **Duration Policy:** Infinite
2. **Period:** 0.1
3. **Execute Periodic Effect On Application:** true
4. **Modifiers:**
   - Attribute: `Stamina`
   - Modifier Op: `Additive`
   - Magnitude Type: `Set By Caller`
   - Set By Caller Tag: `Data.Cost.StaminaPerSecond`

---

## Class Selection Guide

При выборе Effect Class в Ability:

```
Content/GAS/Effects/
├── Movement/
│   ├── GE_SprintBuff        ◄── C++ class: USuspenseCoreEffect_SprintBuff
│   ├── GE_SprintCost        ◄── C++ class: USuspenseCoreEffect_SprintCost
│   ├── GE_JumpCost          ◄── C++ class: USuspenseCoreEffect_JumpCost
│   └── GE_CrouchDebuff      ◄── C++ class: USuspenseCoreEffect_CrouchDebuff
```

**Рекомендация:** Используйте C++ классы напрямую (они уже настроены с SetByCaller).

---

## Balancing Examples

### Fast Sprint (High Speed, High Cost)
```
SprintSpeedMultiplier = 2.0    // +100% speed
StaminaCostPerSecond = 25.0    // High drain
```

### Efficient Sprint (Moderate Speed, Low Cost)
```
SprintSpeedMultiplier = 1.3    // +30% speed
StaminaCostPerSecond = 8.0     // Low drain
```

### Stealth Crouch (Very Slow)
```
CrouchSpeedMultiplier = 0.3    // 30% of normal speed
```

---

## Troubleshooting

### Effect не применяется

1. Проверьте что Effect Class назначен в Details панели Ability
2. Убедитесь что ASC инициализирован на персонаже
3. Проверьте логи: `LogSuspenseCoreASC`

### Значения не изменяются

1. SetByCaller требует передачи тега через `SetSetByCallerMagnitude()`
2. Проверьте что Ability вызывает этот метод перед `ApplyGameplayEffectSpecToSelf()`

### Native Tag не найден

1. Убедитесь что `SuspenseCoreGameplayTags.h` включён
2. Проверьте что модуль BridgeSystem загружается первым

### Stamina Issues

#### Стамина превышает MaxStamina

Это происходит когда Infinite+Period Additive модификатор накапливает base value.

**Решение:** См. [AttributeSystem_DeveloperGuide.md](AttributeSystem_DeveloperGuide.md#attribute-clamping)

#### Реген стамины не работает

1. Проверьте что `SuspenseCoreEffect_StaminaRegen` добавлен в `PassiveEffects` класса
2. Проверьте отсутствие blocking tags (`State.Sprinting`, `State.Dead`)
3. Проверьте логи: `LogSuspenseCoreAttributes`

#### Стамина не кончается при спринте

1. Проверьте `StaminaCostPerSecond` в BP_SprintAbility
2. Проверьте что SprintCostEffect применяется (лог: `ApplySprintEffects`)

> **Важно:** Подробная документация по атрибутам и регенерации: [AttributeSystem_DeveloperGuide.md](AttributeSystem_DeveloperGuide.md)

---

## Architecture Benefits

| Aspect | Old (Hardcoded) | New (SetByCaller) |
|--------|-----------------|-------------------|
| Value Location | In Effect C++ | In Ability UPROPERTY |
| Designer Access | Requires C++ change | Editor-configurable |
| Balance Iteration | Slow (recompile) | Fast (change in Editor) |
| Per-Class Variants | New Effect needed | Same Effect, different Ability |
| Data Driven | No | Yes |

---

## Related Documentation

- [AttributeSystem_DeveloperGuide.md](AttributeSystem_DeveloperGuide.md) - Атрибуты, стамина, регенерация
- [SuspenseCoreDeveloperGuide.md](../SuspenseCoreDeveloperGuide.md) - Общая архитектура проекта

---

*Last Updated: 2025-12-28*
