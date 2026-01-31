# EnemySystem AnimInstance Setup Guide

## Overview

Документация по настройке анимаций для AI врагов в модуле EnemySystem.

**Версия:** 1.0
**Дата:** 2026-01-31
**Статус:** Работает ✅

---

## Архитектура

```
┌─────────────────────────────────────────────────────────────┐
│                    BP_Enemy_Scav                            │
│  ┌─────────────────────────────────────────────────────┐   │
│  │           SkeletalMeshComponent                      │   │
│  │  Anim Class: ABP_Enemy_AnimInstance                  │   │
│  └─────────────────────────────────────────────────────┘   │
│                           │                                 │
│                           ▼                                 │
│  ┌─────────────────────────────────────────────────────┐   │
│  │      USuspenseCoreEnemyAnimInstance (C++)           │   │
│  │  • GroundSpeed (обновляется автоматически)          │   │
│  │  • bHasMovementInput (GroundSpeed > 10)             │   │
│  │  • bIsFalling                                        │   │
│  │  • MovementState (enum)                              │   │
│  └─────────────────────────────────────────────────────┘   │
│                           │                                 │
│                           ▼                                 │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              AnimBP (AnimGraph)                      │   │
│  │  State Machine → BlendSpace → Output Pose            │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## C++ AnimInstance

### Файлы
- `Source/EnemySystem/Public/SuspenseCore/Animation/SuspenseCoreEnemyAnimInstance.h`
- `Source/EnemySystem/Private/SuspenseCore/Animation/SuspenseCoreEnemyAnimInstance.cpp`

### Ключевые переменные (автоматически обновляются)

| Переменная | Тип | Описание | Использование в AnimBP |
|------------|-----|----------|------------------------|
| `GroundSpeed` | float | Скорость по XY (0-500+) | BlendSpace Speed axis |
| `bHasMovementInput` | bool | true если GroundSpeed > 10 | **Transition Rules** |
| `bIsFalling` | bool | Персонаж падает | State transitions |
| `bIsInAir` | bool | Персонаж в воздухе | State transitions |
| `MovementState` | Enum | Idle/Walking/Sprinting/etc | State Machine |
| `Speed` | float | Полная скорость (включая Z) | Альтернатива |
| `NormalizedSpeed` | float | 0-2 (относительно MaxWalkSpeed) | BlendSpace |

### Порядок обновления (КРИТИЧНО!)

```cpp
void NativeUpdateAnimation(float DeltaSeconds)
{
    // 1. СНАЧАЛА вычисляем velocity
    UpdateVelocityData(DeltaSeconds);  // ← GroundSpeed здесь

    // 2. ПОТОМ используем velocity для states
    UpdateMovementData(DeltaSeconds);  // ← bHasMovementInput использует GroundSpeed

    // 3. Aim данные
    UpdateAimData(DeltaSeconds);
}
```

---

## Настройка AnimBP

### 1. Parent Class

**КРИТИЧНО:** AnimBP должен наследоваться от `SuspenseCoreEnemyAnimInstance`:

```
AnimBP → Class Settings → Parent Class = SuspenseCoreEnemyAnimInstance
```

### 2. Event Graph

**НЕ НУЖЕН** дополнительный Blueprint код для обновления переменных!

C++ класс автоматически обновляет:
- GroundSpeed
- bHasMovementInput
- bIsFalling
- и все остальные переменные

❌ **НЕ ДЕЛАЙ:**
```
Event Blueprint Update Animation
    → Movement Component → Get Velocity → SET Ground Speed  // НЕ НУЖНО!
    → Get Current Acceleration → SET Should Move            // НЕ РАБОТАЕТ для AI!
```

✅ **ПРАВИЛЬНО:**
```
Event Blueprint Update Animation
    → (пусто, или только debug print)
```

### 3. AnimGraph Structure

```
┌──────────────────┐     ┌─────────────────┐     ┌──────────────┐
│   Main States    │────▶│  DefaultSlot    │────▶│  Output Pose │
│  State Machine   │     │                 │     │              │
└──────────────────┘     └─────────────────┘     └──────────────┘
```

### 4. State Machine: Main States

```
         ┌──────────────────────────┐
         │         Entry            │
         └────────────┬─────────────┘
                      ▼
         ┌──────────────────────────┐
         │          Idle            │
         └────────────┬─────────────┘
                      │ bHasMovementInput == true
                      ▼
         ┌──────────────────────────┐
         │      Walk / Run          │◀──┐
         │  (BlendSpace внутри)     │   │
         └────────────┬─────────────┘   │
                      │ bHasMovementInput == false
                      └─────────────────┘
```

### 5. Transition Rules

#### Idle → Walk/Run
```
Condition: bHasMovementInput == true
```
или
```
Condition: GroundSpeed > 10.0
```

#### Walk/Run → Idle
```
Condition: bHasMovementInput == false
```
или
```
Condition: GroundSpeed < 10.0
```

⚠️ **НЕ ИСПОЛЬЗУЙ** `ShouldMove` или `Get Current Acceleration` — они не работают для AI!

### 6. BlendSpace Setup

В состоянии Walk/Run используй BlendSpace:

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   GroundSpeed   │────▶│  BS_MM_WalkRun  │────▶│   Output Pose   │
│   (из C++)      │     │  Speed axis     │     │                 │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

**BlendSpace настройки:**
- Horizontal Axis: Speed (0 - 500 или по твоим анимациям)
- Animations: Idle → Walk → Run по оси Speed

---

## FSM States и Movement

### ConfigureMovement()

Каждое FSM состояние настраивает движение при входе:

```cpp
void USuspenseCoreEnemyPatrolState::ConfigureMovement(Enemy)
{
    MovementComp->MaxWalkSpeed = PatrolSpeed;           // 200-300
    MovementComp->bOrientRotationToMovement = true;     // КРИТИЧНО для анимации!
    MovementComp->RotationRate = FRotator(0, 300, 0);
    Enemy->bUseControllerRotationYaw = false;
}

void USuspenseCoreEnemyChaseState::ConfigureMovement(Enemy)
{
    MovementComp->MaxWalkSpeed = ChaseSpeed;            // 400-500
    MovementComp->bOrientRotationToMovement = true;     // КРИТИЧНО для анимации!
    MovementComp->RotationRate = FRotator(0, 360, 0);
    Enemy->bUseControllerRotationYaw = false;
}
```

### Критичные настройки

| Параметр | Значение | Зачем |
|----------|----------|-------|
| `bOrientRotationToMovement` | `true` | Поворот модели в направлении движения |
| `bUseControllerRotationYaw` | `false` | Отключить поворот от контроллера |
| `RotationRate` | 300-360 | Скорость поворота |

❌ **НЕ ВЫЗЫВАЙ** `SetMovementMode(MOVE_Walking)` — ломает pathfinding!

---

## Troubleshooting

### Проблема: Анимация не переключается с Idle

**Причина:** Transition Rule использует неправильную переменную

**Решение:**
- Используй `bHasMovementInput` или `GroundSpeed > 10`
- НЕ используй `ShouldMove` или `Get Current Acceleration`

### Проблема: GroundSpeed = 0 хотя враг двигается

**Причина:** AnimBP перезаписывает значение из C++

**Решение:**
- Удали Blueprint код который делает `SET Ground Speed`
- C++ автоматически обновляет переменную

### Проблема: MoveToLocation FAILED

**Причина:** `SetMovementMode()` ломает pathfinding

**Решение:**
- НЕ вызывай `SetMovementMode(MOVE_Walking)` в ConfigureMovement
- Устанавливай только `MaxWalkSpeed` и `bOrientRotationToMovement`

### Проблема: Модель не поворачивается при движении

**Причина:** Не включён `bOrientRotationToMovement`

**Решение:**
```cpp
MovementComp->bOrientRotationToMovement = true;
Enemy->bUseControllerRotationYaw = false;
```

---

## Debug

### Включить логи AnimInstance

В `SuspenseCoreEnemyAnimInstance.cpp` есть debug лог:
```
[AnimInstance] BP_Enemy_Scav - GroundSpeed=500.0, bHasMovementInput=true
```

### Print String в AnimBP

Для отладки добавь в Event Graph:
```
Event Blueprint Update Animation
    → Print String: GroundSpeed
```

---

## Связанные файлы

- `Source/EnemySystem/Public/SuspenseCore/Animation/SuspenseCoreEnemyAnimInstance.h`
- `Source/EnemySystem/Private/SuspenseCore/Animation/SuspenseCoreEnemyAnimInstance.cpp`
- `Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyPatrolState.cpp`
- `Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyChaseState.cpp`
- `Documentation/Plans/EnemySystem_Mover_Migration_Plan.md`

---

*Документ создан: 2026-01-31*
