# Weapon Ability Development Guide

## Содержание
1. [Архитектура](#архитектура)
2. [Модульная структура](#модульная-структура)
3. [Компоненты оружия](#компоненты-оружия)
4. [EventBus и UI](#eventbus-и-ui)
5. [Создание новой Ability](#создание-новой-ability)
6. [Чеклист разработчика](#чеклист-разработчика)

---

## Архитектура

### Dependency Inversion (DI) Principle

```
┌─────────────┐     ┌──────────────────┐     ┌─────────────────┐
│  GAS Module │ ──► │  BridgeSystem    │ ◄── │ EquipmentSystem │
│  (Abilities)│     │  (Tags/Events/   │     │ (Weapon/Ammo/   │
│             │     │   Interfaces)    │     │  Magazine)      │
└─────────────┘     └──────────────────┘     └─────────────────┘
```

**КРИТИЧЕСКОЕ ПРАВИЛО**: GAS модуль НЕ МОЖЕТ напрямую зависеть от EquipmentSystem!

- GAS → BridgeSystem: ✅ Разрешено
- GAS → EquipmentSystem: ❌ ЗАПРЕЩЕНО
- EquipmentSystem → BridgeSystem: ✅ Разрешено

### Правильные теги для GAS

```cpp
// ✅ ПРАВИЛЬНО - используем BridgeSystem теги
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
EventBus->Publish(SuspenseCoreTags::Event::Equipment::WeaponSlotSwitched, EventData);

// ❌ НЕПРАВИЛЬНО - нарушение DI
#include "SuspenseCore/Tags/SuspenseCoreEquipmentNativeTags.h"
EventBus->Publish(SuspenseCoreEquipmentTags::Event::TAG_Equipment_Event_WeaponSlot_Switched, EventData);
```

---

## Модульная структура

### Расположение файлов

```
Source/
├── GAS/                              # Gameplay Ability System
│   ├── Public/SuspenseCore/Abilities/Weapon/
│   │   ├── SuspenseCoreWeaponAbilityBase.h      # Базовый класс
│   │   ├── SuspenseCoreFireAbility.h            # Стрельба
│   │   ├── SuspenseCoreReloadAbility.h          # Перезарядка
│   │   └── SuspenseCoreAimDownSightAbility.h    # Прицеливание
│   └── Private/SuspenseCore/Abilities/Weapon/
│       └── *.cpp
│
├── BridgeSystem/                     # Мост между модулями
│   ├── Public/SuspenseCore/
│   │   ├── Tags/SuspenseCoreGameplayTags.h      # Теги для GAS
│   │   ├── Interfaces/Weapon/                   # Интерфейсы оружия
│   │   │   ├── ISuspenseCoreWeapon.h
│   │   │   ├── ISuspenseCoreWeaponCombatState.h
│   │   │   └── ISuspenseCoreAmmoProvider.h
│   │   └── Types/SuspenseCoreWeaponTypes.h      # Типы данных
│   └── Private/...
│
├── EquipmentSystem/                  # Система экипировки
│   ├── Public/SuspenseCore/
│   │   ├── Components/
│   │   │   ├── SuspenseCoreAmmoComponent.h
│   │   │   ├── SuspenseCoreMagazineComponent.h
│   │   │   └── SuspenseCoreFireModeComponent.h
│   │   └── Base/SuspenseCoreWeaponActor.h
│   └── Private/...
│
└── InventorySystem/                  # Инвентарь (SSOT)
    └── Public/SuspenseCore/Data/
        └── SuspenseCoreDataManager.h            # DataTable доступ
```

---

## Компоненты оружия

### WeaponActor - Главный актор оружия

```cpp
// SuspenseCoreWeaponActor содержит все компоненты:
ASuspenseCoreWeaponActor
├── MagazineComponent    // Магазин и калибр
├── AmmoComponent        // Счётчик патронов
├── FireModeComponent    // Режимы стрельбы
└── WeaponMeshComponent  // Визуал
```

### MagazineComponent - Магазины и калибры

**Файл**: `Source/EquipmentSystem/Public/SuspenseCore/Components/SuspenseCoreMagazineComponent.h`

```cpp
// Ключевые методы:
void InitializeFromWeapon(ISuspenseCoreWeapon* Weapon);  // Устанавливает CachedWeaponCaliber
bool IsMagazineCompatible(const FSuspenseCoreMagazineInstance& Magazine) const;
FGameplayTag GetWeaponCaliber() const { return CachedWeaponCaliber; }

// Проверка совместимости калибра:
// Magazine.Caliber должен совпадать с Weapon.AmmoType (из DataTable)
```

**КРИТИЧНО**: Без вызова `InitializeFromWeapon()` калибр = Invalid, и любой магазин считается совместимым!

### AmmoComponent - Патроны

**Файл**: `Source/EquipmentSystem/Public/SuspenseCore/Components/SuspenseCoreAmmoComponent.h`

```cpp
// Ключевые методы:
int32 GetCurrentAmmo() const;
int32 GetMaxAmmo() const;
bool HasAmmo() const;
bool ConsumeAmmo(int32 Amount = 1);
void SetAmmo(int32 NewAmmo);

// События (через EventBus):
// - AmmoChanged: при изменении количества
// - AmmoEmpty: когда патроны закончились
```

### FireModeComponent - Режимы стрельбы

**Файл**: `Source/EquipmentSystem/Public/SuspenseCore/Components/SuspenseCoreFireModeComponent.h`

```cpp
// Режимы:
enum class ESuspenseCoreFireMode : uint8
{
    Single,      // Одиночный
    Burst,       // Очередь (3 патрона)
    Auto         // Автоматический
};

// Ключевые методы:
ESuspenseCoreFireMode GetCurrentFireMode() const;
void CycleFireMode();
float GetFireRate() const;  // Выстрелов в минуту
float GetTimeBetweenShots() const;  // Секунды между выстрелами
```

---

## EventBus и UI

### Архитектура обновления UI

```
┌─────────────┐    EventBus     ┌─────────────┐
│   Ability   │ ──────────────► │   Widget    │
│  (Publisher)│   Publish()     │ (Subscriber)│
└─────────────┘                 └─────────────┘
```

### Теги событий для UI (BridgeSystem)

```cpp
// Файл: Source/BridgeSystem/Public/SuspenseCore/Tags/SuspenseCoreGameplayTags.h

namespace SuspenseCoreTags::Event::Equipment
{
    // Оружие
    extern BRIDGESYSTEM_API FGameplayTag WeaponSlotSwitched;  // Смена слота оружия
    extern BRIDGESYSTEM_API FGameplayTag WeaponFired;         // Выстрел
    extern BRIDGESYSTEM_API FGameplayTag WeaponReloaded;      // Перезарядка завершена

    // Патроны
    extern BRIDGESYSTEM_API FGameplayTag AmmoChanged;         // Изменение патронов
    extern BRIDGESYSTEM_API FGameplayTag AmmoEmpty;           // Патроны закончились
    extern BRIDGESYSTEM_API FGameplayTag MagazineChanged;     // Смена магазина
}
```

### Пример публикации события из Ability

```cpp
void USuspenseCoreFireAbility::OnShotFired()
{
    USuspenseCoreEventBus* EventBus = GetEventBus();
    if (!EventBus) return;

    FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetAvatarActorFromActorInfo());

    // Данные о выстреле
    EventData.SetInt(FName("CurrentAmmo"), AmmoComponent->GetCurrentAmmo());
    EventData.SetInt(FName("MaxAmmo"), AmmoComponent->GetMaxAmmo());
    EventData.SetInt(FName("SlotIndex"), CurrentSlotIndex);

    // Публикуем событие - UI автоматически обновится
    EventBus->Publish(SuspenseCoreTags::Event::Equipment::WeaponFired, EventData);
}
```

### Пример подписки в Widget

```cpp
// В SuspenseCoreAmmoCounterWidget.cpp

void USuspenseCoreAmmoCounterWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        // Подписка на изменение патронов
        AmmoChangedHandle = EventBus->SubscribeNative(
            SuspenseCoreTags::Event::Equipment::AmmoChanged,
            this,
            FSuspenseCoreNativeEventCallback::CreateUObject(
                this, &USuspenseCoreAmmoCounterWidget::OnAmmoChanged),
            ESuspenseCoreEventPriority::High
        );

        // Подписка на смену оружия
        WeaponChangedHandle = EventBus->SubscribeNative(
            SuspenseCoreTags::Event::Equipment::WeaponSlotSwitched,
            this,
            FSuspenseCoreNativeEventCallback::CreateUObject(
                this, &USuspenseCoreAmmoCounterWidget::OnWeaponChanged),
            ESuspenseCoreEventPriority::High
        );
    }
}

void USuspenseCoreAmmoCounterWidget::OnAmmoChanged(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
    int32 CurrentAmmo = EventData.GetInt(FName("CurrentAmmo"));
    int32 MaxAmmo = EventData.GetInt(FName("MaxAmmo"));

    // Обновляем текст
    if (AmmoText)
    {
        AmmoText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), CurrentAmmo, MaxAmmo)));
    }
}
```

---

## Создание новой Ability

### Базовый класс для Weapon Abilities

```cpp
// SuspenseCoreWeaponAbilityBase.h

UCLASS(Abstract)
class GAS_API USuspenseCoreWeaponAbilityBase : public UGameplayAbility
{
    GENERATED_BODY()

protected:
    // ========================================
    // Получение компонентов оружия
    // ========================================

    /** Получить WeaponActor через ISuspenseCoreCharacterInterface */
    AActor* GetCurrentWeaponActor() const;

    /** Получить AmmoComponent с текущего оружия */
    USuspenseCoreAmmoComponent* GetAmmoComponent() const;

    /** Получить MagazineComponent с текущего оружия */
    USuspenseCoreMagazineComponent* GetMagazineComponent() const;

    /** Получить FireModeComponent с текущего оружия */
    USuspenseCoreFireModeComponent* GetFireModeComponent() const;

    // ========================================
    // Состояние оружия через интерфейс
    // ========================================

    /** Получить ISuspenseCoreWeaponCombatState (на Character) */
    ISuspenseCoreWeaponCombatState* GetWeaponCombatState() const;

    /** Получить ISuspenseCoreWeapon (на WeaponActor) */
    ISuspenseCoreWeapon* GetWeaponInterface() const;

    // ========================================
    // EventBus
    // ========================================

    /** Получить EventBus для публикации событий */
    USuspenseCoreEventBus* GetEventBus() const;

    /** Опубликовать событие с данными об оружии */
    void PublishWeaponEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData);
};
```

### Пример: Fire Ability (структура)

```cpp
// SuspenseCoreFireAbility.h

UCLASS()
class GAS_API USuspenseCoreFireAbility : public USuspenseCoreWeaponAbilityBase
{
    GENERATED_BODY()

public:
    USuspenseCoreFireAbility();

    // ========================================
    // UGameplayAbility Interface
    // ========================================

    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags = nullptr,
        const FGameplayTagContainer* TargetTags = nullptr,
        FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

protected:
    // ========================================
    // Fire Logic
    // ========================================

    /** Выполнить выстрел */
    void FireShot();

    /** Обработать попадание */
    void ProcessHit(const FHitResult& HitResult);

    /** Применить отдачу */
    void ApplyRecoil();

    /** Проверить возможность стрельбы */
    bool CanFire() const;

    // ========================================
    // Fire Mode Handling
    // ========================================

    /** Начать автоматическую стрельбу */
    void StartAutoFire();

    /** Остановить автоматическую стрельбу */
    void StopAutoFire();

    /** Таймер для авто-режима */
    FTimerHandle AutoFireTimerHandle;

    // ========================================
    // Configurable Properties
    // ========================================

    /** GameplayEffect для урона */
    UPROPERTY(EditDefaultsOnly, Category = "Fire|Effects")
    TSubclassOf<UGameplayEffect> DamageEffectClass;

    /** Множитель урона от типа патронов */
    UPROPERTY(EditDefaultsOnly, Category = "Fire|Damage")
    float AmmoTypeDamageMultiplier = 1.0f;
};
```

### Теги для Fire Ability

```cpp
// В конструкторе:
USuspenseCoreFireAbility::USuspenseCoreFireAbility()
{
    // Input binding
    AbilityInputID = ESuspenseCoreAbilityInputID::Fire;

    // Ability identification - use SetAssetTags() (UE5.5+ compliant)
    // NEVER use deprecated AbilityTags.AddTag()
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(SuspenseCoreTags::Ability::Weapon::Fire);
    SetAssetTags(AssetTags);

    // Tags added when ability is active
    ActivationOwnedTags.AddTag(SuspenseCoreTags::State::Firing);

    // Tags that block this ability (use native tags from SuspenseCoreTags)
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Reloading);
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Stunned);
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Disabled);
    ActivationBlockedTags.AddTag(SuspenseCoreTags::State::WeaponBlocked);

    // Network
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}
```

> **⚠️ ВАЖНО**: Никогда не используйте `FGameplayTag::RequestGameplayTag()` в конструкторах!
> Вместо этого используйте нативные теги из `SuspenseCoreTags::` namespace для лучшей производительности.

---

## Чеклист разработчика

### Перед написанием кода

- [ ] Изучить существующие Weapon Abilities (ADS, Reload, WeaponSwitch)
- [ ] Определить какие компоненты нужны (Ammo, Magazine, FireMode)
- [ ] Определить какие события нужно публиковать для UI
- [ ] Проверить что все нужные теги существуют в BridgeSystem

### Структура Ability

- [ ] Наследоваться от `USuspenseCoreWeaponAbilityBase` или `UGameplayAbility`
- [ ] Настроить `AbilityInputID` для привязки к input
- [ ] Настроить `AssetTags` через `SetAssetTags()` (НЕ используйте deprecated `AbilityTags.AddTag()`)
- [ ] Настроить `ActivationOwnedTags` (теги при активной ability)
- [ ] Настроить `ActivationBlockedTags` (блокирующие теги)
- [ ] Настроить `CancelAbilitiesWithTag` (отмена других abilities)

### DI Compliance

- [ ] НЕ включать хедеры из EquipmentSystem напрямую
- [ ] Использовать интерфейсы из BridgeSystem
- [ ] Использовать теги из `SuspenseCoreTags::` (BridgeSystem)
- [ ] НЕ использовать теги из `SuspenseCoreEquipmentTags::` (EquipmentSystem)

### Доступ к компонентам оружия

```cpp
// ✅ ПРАВИЛЬНО - через интерфейс на Character
AActor* WeaponActor = ISuspenseCoreCharacterInterface::Execute_GetCurrentWeaponActor(AvatarActor);
USuspenseCoreAmmoComponent* AmmoComp = WeaponActor->FindComponentByClass<USuspenseCoreAmmoComponent>();

// ❌ НЕПРАВИЛЬНО - поиск компонента на Character
USuspenseCoreAmmoComponent* AmmoComp = AvatarActor->FindComponentByClass<USuspenseCoreAmmoComponent>();
```

### EventBus публикация

- [ ] Создать `FSuspenseCoreEventData` с `Create(SourceActor)`
- [ ] Добавить все нужные данные через `SetInt/SetFloat/SetString/SetObject`
- [ ] Использовать теги из `SuspenseCoreTags::Event::Equipment::*`
- [ ] Документировать какие данные содержит событие

### UI обновление

- [ ] Виджеты подписываются на события в `NativeConstruct()`
- [ ] Виджеты отписываются в `NativeDestruct()`
- [ ] Использовать `ESuspenseCoreEventPriority::High` для UI
- [ ] Проверять `EventData.GetObject<AActor>("Target")` == локальный игрок

### Сетевая репликация

- [ ] `NetExecutionPolicy = LocalPredicted` для отзывчивости
- [ ] Серверная валидация в `ServerRPC` методах
- [ ] Состояние оружия реплицируется через `WeaponStanceComponent`
- [ ] GameplayEffects для урона применяются на сервере

---

## Связанные документы

- [EventBus Architecture](../EventBus/EventBusArchitecture.md)
- [Equipment System Overview](../EquipmentSystem/Overview.md)
- [SSOT DataTable Guide](../Data/SSOTDataTableGuide.md)
- [Weapon Types Reference](../EquipmentSystem/WeaponTypes.md)

---

## Примеры кода

### Получение компонентов оружия

```cpp
// В методе Ability:
AActor* GetWeaponActor() const
{
    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
        return nullptr;

    AActor* AvatarActor = ActorInfo->AvatarActor.Get();

    // Через интерфейс Character
    if (AvatarActor->GetClass()->ImplementsInterface(USuspenseCoreCharacterInterface::StaticClass()))
    {
        return ISuspenseCoreCharacterInterface::Execute_GetCurrentWeaponActor(AvatarActor);
    }

    return nullptr;
}

USuspenseCoreAmmoComponent* GetAmmoComponent() const
{
    AActor* WeaponActor = GetWeaponActor();
    return WeaponActor ? WeaponActor->FindComponentByClass<USuspenseCoreAmmoComponent>() : nullptr;
}
```

### Проверка возможности стрельбы

```cpp
bool USuspenseCoreFireAbility::CanFire() const
{
    // 1. Проверить наличие оружия
    AActor* WeaponActor = GetWeaponActor();
    if (!WeaponActor) return false;

    // 2. Проверить наличие патронов
    USuspenseCoreAmmoComponent* AmmoComp = GetAmmoComponent();
    if (!AmmoComp || !AmmoComp->HasAmmo()) return false;

    // 3. Проверить состояние оружия (не перезаряжается, не заблокировано)
    ISuspenseCoreWeaponCombatState* CombatState = GetWeaponCombatState();
    if (CombatState)
    {
        if (CombatState->IsReloading()) return false;
        if (CombatState->IsWeaponBlocked()) return false;
    }

    return true;
}
```

### Публикация события выстрела

```cpp
void USuspenseCoreFireAbility::FireShot()
{
    if (!CanFire()) return;

    // 1. Потребить патрон
    USuspenseCoreAmmoComponent* AmmoComp = GetAmmoComponent();
    AmmoComp->ConsumeAmmo(1);

    // 2. Выполнить трассировку/проджектайл
    PerformLineTrace();

    // 3. Применить отдачу
    ApplyRecoil();

    // 4. Опубликовать событие для UI
    if (USuspenseCoreEventBus* EventBus = GetEventBus())
    {
        FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetAvatarActorFromActorInfo());
        EventData.SetInt(FName("CurrentAmmo"), AmmoComp->GetCurrentAmmo());
        EventData.SetInt(FName("MaxAmmo"), AmmoComp->GetMaxAmmo());
        EventData.SetBool(FName("IsEmpty"), !AmmoComp->HasAmmo());

        EventBus->Publish(SuspenseCoreTags::Event::Equipment::WeaponFired, EventData);

        // Если патроны закончились - отдельное событие
        if (!AmmoComp->HasAmmo())
        {
            EventBus->Publish(SuspenseCoreTags::Event::Equipment::AmmoEmpty, EventData);
        }
    }
}
```
