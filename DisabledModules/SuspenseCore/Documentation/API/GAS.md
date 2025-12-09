# GAS (Gameplay Ability System) API Reference

**Модуль:** GAS
**Версия:** 2.0.0
**Статус:** Полностью мигрирован, компилируется
**LOC:** ~8,003
**Последнее обновление:** 2025-11-28

---

## Обзор модуля

GAS — модуль интеграции Unreal Engine Gameplay Ability System для SuspenseCore. Предоставляет базовые классы для способностей, атрибутов и эффектов, специфичных для хардкорного тактического шутера.

### Архитектурная роль

Модуль GAS выступает как тонкая надстройка над стандартным UE GAS, добавляя:

1. **Расширенные AttributeSets** — специализированные наборы атрибутов для персонажей, оружия и боеприпасов
2. **Базовые Abilities** — управление персонажем (спринт, прыжок, приседание)
3. **Input Binding** — интеграция с системой ввода через ESuspenseAbilityInputID
4. **Готовые Effects** — набор GameplayEffect для регенерации, бафов, дебафов

### Зависимости модуля

```
GAS
├── Core
├── SuspenseCore
├── GameplayAbilities
├── GameplayTags
├── GameplayTasks
└── BridgeSystem
```

**Private зависимости:** CoreUObject, Engine, NetCore, Slate, SlateCore

---

## Ключевые компоненты

### 1. UGASAbilitySystemComponent

**Файл:** `Public/Components/GASAbilitySystemComponent.h`
**Тип:** UAbilitySystemComponent
**Назначение:** Кастомный ASC с оптимизациями

```cpp
UCLASS()
class GAS_API UGASAbilitySystemComponent : public UAbilitySystemComponent
```

#### Особенности

| Метод | Описание |
|-------|----------|
| `InitAbilityActorInfo()` | Переопределение для кастомной инициализации |
| `ShouldDoServerAbilityRPCBatch()` | Всегда возвращает `true` для batch RPC оптимизации |

#### Паттерн использования

```cpp
// На персонаже
void APlayerCharacterBase::SetupAbilitySystem()
{
    AbilitySystemComponent = CreateDefaultSubobject<UGASAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}
```

---

### 2. AttributeSets

#### UGASAttributeSet (Базовый)

**Файл:** `Public/Attributes/GASAttributeSet.h`
**Назначение:** Базовые атрибуты персонажа

```cpp
UCLASS()
class GAS_API UGASAttributeSet : public UAttributeSet
```

**Атрибуты:**

| Атрибут | Категория | Репликация | Описание |
|---------|-----------|------------|----------|
| `Health` | Core | OnRep | Текущее здоровье |
| `MaxHealth` | Core | OnRep | Максимальное здоровье |
| `HealthRegen` | Core | OnRep | Регенерация здоровья/сек |
| `Armor` | Defense | OnRep | Броня |
| `AttackPower` | Combat | OnRep | Сила атаки |
| `MovementSpeed` | Movement | OnRep | Скорость передвижения |
| `Stamina` | Resource | OnRep | Текущая выносливость |
| `MaxStamina` | Resource | OnRep | Максимальная выносливость |
| `StaminaRegen` | Resource | OnRep | Регенерация выносливости/сек |

**Макрос ATTRIBUTE_ACCESSORS:**
```cpp
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)   \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)                 \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)                 \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
```

**Ключевые методы:**
```cpp
// Клэмпинг значений перед изменением
virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

// Обработка после применения эффекта
virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

// Синхронизация скорости с CharacterMovementComponent
void UpdateCharacterMovementSpeed();
```

---

#### UWeaponAttributeSet

**Файл:** `Public/Attributes/WeaponAttributeSet.h`
**Назначение:** Комплексные характеристики огнестрельного оружия (стиль Tarkov/STALKER)

**Категории атрибутов:**

##### Combat (Боевые)
| Атрибут | Описание |
|---------|----------|
| `BaseDamage` | Базовый урон оружия |
| `RateOfFire` | Скорострельность (выстр./мин) |
| `EffectiveRange` | Эффективная дальность (метры) |
| `MaxRange` | Максимальная дальность |
| `MagazineSize` | Размер магазина |
| `TacticalReloadTime` | Время тактической перезарядки |
| `FullReloadTime` | Время полной перезарядки |

##### Accuracy (Точность)
| Атрибут | Описание |
|---------|----------|
| `MOA` | Механическая точность (MOA) |
| `HipFireSpread` | Разброс от бедра (градусы) |
| `AimSpread` | Разброс в прицеле |
| `VerticalRecoil` | Вертикальная отдача |
| `HorizontalRecoil` | Горизонтальная отдача |
| `RecoilRecoverySpeed` | Скорость восстановления |
| `SpreadIncreasePerShot` | Увеличение разброса за выстрел |
| `MaxSpread` | Максимальный разброс |

##### Reliability (Надёжность)
| Атрибут | Описание |
|---------|----------|
| `Durability` | Текущая прочность (0-100) |
| `MaxDurability` | Максимальная прочность |
| `DurabilityLossPerShot` | Износ за выстрел |
| `MisfireChance` | Базовый шанс осечки (%) |
| `JamChance` | Шанс заклинивания (%) |
| `MisfireClearTime` | Время устранения осечки (сек) |
| `JamClearTime` | Время устранения заклинивания |

##### Ergonomics (Эргономика)
| Атрибут | Описание |
|---------|----------|
| `Ergonomics` | Эргономика (0-100) |
| `AimDownSightTime` | Время прицеливания (сек) |
| `AimSensitivityMultiplier` | Множитель чувствительности ADS |
| `WeaponWeight` | Вес (кг) |
| `StaminaDrainRate` | Штраф к выносливости при ADS |
| `WeaponSwitchTime` | Время смены оружия |

##### Modifications (Модификации)
| Атрибут | Описание |
|---------|----------|
| `ModSlotCount` | Количество слотов для модов |
| `ModAccuracyBonus` | Бонус точности от модов (%) |
| `ModErgonomicsBonus` | Бонус эргономики от модов |

##### Special (Специальные)
| Атрибут | Описание |
|---------|----------|
| `NoiseLevel` | Уровень шума (дБ) |
| `SuppressorEfficiency` | Эффективность глушителя (%) |
| `FireModeSwitchTime` | Время смены режима огня |

---

#### UAmmoAttributeSet

**Файл:** `Public/Attributes/AmmoAttributeSet.h`
**Назначение:** Характеристики боеприпасов (реалистичная баллистика)

**Категории атрибутов:**

##### Damage (Урон)
| Атрибут | Описание |
|---------|----------|
| `BaseDamage` | Базовый урон по незащищенным целям |
| `ArmorPenetration` | Пробитие брони (0-100%) |
| `StoppingPower` | Останавливающее действие |
| `FragmentationChance` | Шанс фрагментации (0-100%) |
| `FragmentationDamageMultiplier` | Множитель урона при фрагментации |

##### Ballistics (Баллистика)
| Атрибут | Описание |
|---------|----------|
| `MuzzleVelocity` | Начальная скорость (м/с) |
| `DragCoefficient` | Коэффициент сопротивления |
| `BulletMass` | Масса пули (граммы) |
| `EffectiveRange` | Эффективная дальность (метры) |
| `MaxRange` | Максимальная дальность |

##### Special Effects (Специальные эффекты)
| Атрибут | Описание |
|---------|----------|
| `RicochetChance` | Шанс рикошета (0-100%) |
| `TracerVisibility` | Видимость трассера (0-100%) |
| `IncendiaryDamagePerSecond` | Зажигательный урон/сек |
| `IncendiaryDuration` | Длительность горения |

##### Weapon Impact (Влияние на оружие)
| Атрибут | Описание |
|---------|----------|
| `WeaponDegradationRate` | Износ оружия за выстрел |
| `MisfireChance` | Шанс осечки (некачественные патроны) |
| `JamChance` | Шанс заклинивания |

---

### 3. Abilities

#### UGASAbility (Базовый)

**Файл:** `Public/Abilities/GASAbility.h`
**Назначение:** Базовый класс для всех способностей проекта

```cpp
UCLASS(Abstract, Blueprintable)
class GAS_API UGASAbility : public UGameplayAbility
{
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GAS|Ability")
    ESuspenseAbilityInputID AbilityInputID = ESuspenseAbilityInputID::None;
};
```

#### UCharacterSprintAbility

**Файл:** `Public/Abilities/CharacterSprintAbility.h`
**Назначение:** Способность спринта с затратой выносливости

**Конфигурация:**
```cpp
// GameplayEffect для бафа скорости
TSubclassOf<UGameplayEffect> SprintBuffEffectClass;

// GameplayEffect для траты выносливости
TSubclassOf<UGameplayEffect> SprintCostEffectClass;

// Множитель скорости спринта
float SprintSpeedMultiplier;

// Трата выносливости в секунду
float StaminaCostPerSecond;

// Минимальная выносливость для начала спринта
float MinimumStaminaToSprint;
```

**Lifecycle:**
```
CanActivateAbility() → ActivateAbility() → InputReleased()/OnStaminaBelowThreshold() → EndAbility()
```

**Важно:** Способность автоматически завершается когда:
- Игрок отпускает кнопку спринта
- Выносливость опускается ниже порогового значения

---

### 4. GameplayEffects

#### UGASEffect (Базовый)

**Файл:** `Public/Effects/GASEffect.h`
**Назначение:** Базовый класс для всех эффектов проекта

```cpp
UCLASS(Abstract, Blueprintable)
class GAS_API UGASEffect : public UGameplayEffect
```

#### Готовые эффекты

| Эффект | Файл | Назначение |
|--------|------|------------|
| `GameplayEffect_SprintBuff` | Effects/ | Увеличение скорости во время спринта |
| `GameplayEffect_SprintCost` | Effects/ | Трата выносливости во время спринта |
| `GameplayEffect_StaminaRegen` | Effects/ | Регенерация выносливости |
| `GameplayEffect_HealthRegen` | Effects/ | Регенерация здоровья |
| `GameplayEffect_JumpCost` | Effects/ | Трата выносливости на прыжок |
| `GameplayEffect_CrouchDebuff` | Effects/ | Замедление при приседании |
| `InitialAttributesEffect` | Effects/ | Инициализация атрибутов персонажа |

---

## Input Integration

### ESuspenseAbilityInputID

**Файл:** `BridgeSystem/Public/Input/SuspenseAbilityInputID.h`

```cpp
UENUM(BlueprintType)
enum class ESuspenseAbilityInputID : uint8
{
    None,
    Confirm,
    Cancel,
    Sprint,
    Jump,
    Crouch,
    Interact,
    PrimaryFire,
    SecondaryFire,
    Reload,
    WeaponSwitch,
    // ...
};
```

### Привязка способности к вводу

```cpp
// В UGASAbility
AbilityInputID = ESuspenseAbilityInputID::Sprint;

// В PlayerController при инициализации ASC
AbilitySystemComponent->BindAbilityActivationToInputComponent(
    InputComponent,
    FGameplayAbilityInputBinds(
        "Confirm",
        "Cancel",
        "ESuspenseAbilityInputID"
    )
);
```

---

## Паттерны использования

### Создание персонажа с GAS

```cpp
// APlayerCharacterBase::SetupAbilitySystem()
void APlayerCharacterBase::SetupAbilitySystem()
{
    // Создание ASC
    AbilitySystemComponent = CreateDefaultSubobject<UGASAbilitySystemComponent>(TEXT("AbilitySystem"));

    // Создание AttributeSet
    AttributeSet = CreateDefaultSubobject<UGASAttributeSet>(TEXT("AttributeSet"));

    // Регистрация в ASC
    AbilitySystemComponent->GetSpawnedAttributes_Mutable().AddUnique(AttributeSet);

    // Настройка репликации
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}
```

### Применение начальных атрибутов

```cpp
void APlayerCharacterBase::InitializeAttributes()
{
    if (HasAuthority() && InitialAttributesEffectClass)
    {
        FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
        FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
            InitialAttributesEffectClass, 1, EffectContext
        );

        if (SpecHandle.IsValid())
        {
            AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        }
    }
}
```

### Выдача способностей

```cpp
void APlayerCharacterBase::GiveDefaultAbilities()
{
    if (!HasAuthority()) return;

    for (TSubclassOf<UGASAbility>& AbilityClass : DefaultAbilities)
    {
        if (AbilityClass)
        {
            FGameplayAbilitySpec Spec(AbilityClass, 1,
                static_cast<int32>(AbilityClass.GetDefaultObject()->AbilityInputID), this);
            AbilitySystemComponent->GiveAbility(Spec);
        }
    }
}
```

### Изменение атрибута через GameplayEffect

```cpp
// Динамическое создание GE для изменения здоровья
void ApplyDamage(float DamageAmount)
{
    UGameplayEffect* DamageEffect = NewObject<UGameplayEffect>(GetTransientPackage(), FName(TEXT("DamageEffect")));
    DamageEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

    FGameplayModifierInfo ModInfo;
    ModInfo.Attribute = UGASAttributeSet::GetHealthAttribute();
    ModInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(-DamageAmount));
    ModInfo.ModifierOp = EGameplayModOp::Additive;
    DamageEffect->Modifiers.Add(ModInfo);

    FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
    AbilitySystemComponent->ApplyGameplayEffectToSelf(DamageEffect, 1.0f, Context);
}
```

---

## Репликация

### Стратегия репликации атрибутов

```cpp
void UGASAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Все атрибуты реплицируются всем клиентам
    DOREPLIFETIME_CONDITION_NOTIFY(UGASAttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UGASAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    // ... остальные атрибуты
}
```

### OnRep для визуального обновления

```cpp
void UGASAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGASAttributeSet, Health, OldHealth);

    // Оповещение UI через EventManager
    if (USuspenseEventManager* EventMgr = USuspenseEventManager::Get(GetOwningActor()))
    {
        float HealthPercent = MaxHealth.GetCurrentValue() > 0
            ? Health.GetCurrentValue() / MaxHealth.GetCurrentValue()
            : 0.0f;
        EventMgr->NotifyHealthUpdated(Health.GetCurrentValue(), MaxHealth.GetCurrentValue(), HealthPercent);
    }
}
```

---

## Риски и потенциальные проблемы

### 1. Избыточное количество атрибутов

**Проблема:** UWeaponAttributeSet содержит 30+ атрибутов, UAmmoAttributeSet — 20+ атрибутов.

**Последствия:**
- Высокий сетевой трафик при репликации
- Сложность балансировки
- Overhead на OnRep функции

**Рекомендации:**
- Профилировать сетевой трафик при использовании всех атрибутов
- Рассмотреть conditional replication для редко изменяемых атрибутов
- Группировать редко используемые атрибуты в отдельный AttributeSet

### 2. Coupling с CharacterMovementComponent

**Проблема:** `UpdateCharacterMovementSpeed()` напрямую изменяет MaxWalkSpeed.

**Последствия:**
- Жесткая связь AttributeSet с конкретным компонентом
- Потенциальные race conditions при репликации
- Невозможность использовать AttributeSet без CharacterMovementComponent

**Рекомендации:**
- Использовать делегаты для уведомления о изменении скорости
- Вынести логику синхронизации в отдельный компонент-медиатор

### 3. Дублирование атрибутов

**Проблема:** Некоторые атрибуты дублируются между AttributeSet'ами (MisfireChance, JamChance в Weapon и Ammo).

**Последствия:**
- Неясно какой источник истины
- Возможные конфликты при вычислении финального значения

**Рекомендации:**
- Определить четкую иерархию: Ammo модифицирует базу Weapon
- Документировать формулы комбинирования значений

### 4. Отсутствие GameplayTags структуры

**Проблема:** В модуле GAS нет документированной GameplayTags иерархии.

**Последствия:**
- Риск конфликтов при добавлении новых тегов
- Неочевидные зависимости между эффектами

**Рекомендации:**
- Создать централизованный файл определения тегов (DefaultGameplayTags.ini)
- Документировать иерархию тегов

### 5. Минималистичный UGASAbilitySystemComponent

**Проблема:** Кастомный ASC практически не расширяет базовый функционал.

**Последствия:**
- Отсутствие централизованного места для общих хуков
- Потенциальная необходимость переписывать при расширении

**Рекомендации:**
- Добавить виртуальные методы для common operations
- Рассмотреть добавление кеширования часто используемых AttributeSet

### 6. Отсутствие Gameplay Cues

**Проблема:** В модуле нет Gameplay Cues для визуального/звукового feedback.

**Последствия:**
- Визуальные эффекты придется обрабатывать вручную
- Потеря возможностей client-side prediction для VFX

**Рекомендации:**
- Добавить базовые GameplayCue классы
- Определить нейминг конвенцию для Cue тегов

---

## Метрики модуля

| Метрика | Значение |
|---------|----------|
| Файлов (.h) | 23 |
| AttributeSets | 5 (Base, Weapon, Ammo, Armor, Default) |
| Abilities | 7 |
| Effects | 8 |
| Атрибутов в WeaponAttributeSet | 32 |
| Атрибутов в AmmoAttributeSet | 22 |
| LOC (приблизительно) | 8,003 |

---

## Связь с другими модулями

```
┌─────────────┐       ┌───────────────┐
│ PlayerCore  │◀──────│      GAS      │
│ (Character) │       │  (Abilities)  │
└─────────────┘       └───────┬───────┘
                              │
                              ▼
                      ┌───────────────┐
                      │ BridgeSystem  │
                      │ (EventManager)│
                      └───────────────┘
                              ▲
                              │
                      ┌───────────────┐
                      │EquipmentSys  │
                      │(Weapon Stats)│
                      └───────────────┘
```

**GAS интегрируется с:**
- **PlayerCore** — персонажи используют ASC и AttributeSets
- **BridgeSystem** — EventManager для UI оповещений
- **EquipmentSystem** — WeaponAttributeSet для данных оружия

---

**Дата создания:** 2025-11-28
**Автор:** Tech Lead
**Версия документа:** 1.0
