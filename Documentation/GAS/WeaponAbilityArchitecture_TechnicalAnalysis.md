# SuspenseCore Weapon Ability Architecture
## Technical Analysis - AAA Level (Tarkov-style)

**Version:** 1.0
**Author:** Technical Audit
**Date:** 2026-01-12

---

## Executive Summary

Данный документ описывает архитектуру системы способностей оружия в SuspenseCore, её соответствие AAA стандартам (Escape from Tarkov), и текущие проблемы/рекомендации.

---

## 1. Архитектура хранения ASC (AbilitySystemComponent)

### 1.1 Принцип: ASC на PlayerState

```
┌─────────────────────────────────────────────────────────────────┐
│                      PLAYER STATE                                │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  USuspenseCoreAbilitySystemComponent* ASC                 │   │
│  │  - Реплицируется на всех клиентах                        │   │
│  │  - Персистирует между респаунами                         │   │
│  │  - Server-authoritative (Mixed replication mode)         │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  USuspenseCoreEquipmentDataStore                         │   │
│  │  - Хранит данные об экипировке                           │   │
│  │  - ActiveWeaponSlot tracking                             │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │  USuspenseCoreEquipmentAbilityConnector                   │   │
│  │  - Грантит abilities при экипировке                       │   │
│  │  - Отслеживает выданные abilities по слотам               │   │
│  └──────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ GetAbilitySystemComponent()
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      CHARACTER                                   │
│  - Не владеет ASC напрямую                                      │
│  - Получает ASC через PlayerState                               │
│  - К нему attach'ится активное оружие                           │
└─────────────────────────────────────────────────────────────────┘
```

**Файлы:**
- `Source/PlayerCore/Public/SuspenseCore/Core/SuspenseCorePlayerState.h:252-253`
- `Source/GAS/Public/SuspenseCore/Components/SuspenseCoreAbilitySystemComponent.h`

### 1.2 Почему ASC на PlayerState?

| Причина | Описание |
|---------|----------|
| **Персистентность** | Abilities сохраняются между смертями/респаунами |
| **Репликация** | PlayerState реплицируется на всех клиентах |
| **MMO Pattern** | Стандарт для сетевых игр с длительными сессиями |
| **Tarkov-style** | Экипировка персистирует в рейде |

---

## 2. Flow грантинга Weapon Abilities

### 2.1 Полная последовательность

```
┌─────────────────┐
│ Item помещен    │
│ в слот          │
└────────┬────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────────┐
│ EquipmentAbilityService::OnEquipmentSpawned()                   │
│ Source/EquipmentSystem/Private/.../SuspenseCoreEquipmentAbility │
│ Service.cpp:706                                                  │
└────────┬────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────────┐
│ EquipmentAbilityConnector::GrantAbilitiesForSlot()              │
│ Source/EquipmentSystem/Private/.../SuspenseCoreEquipmentAbility │
│ Connector.cpp:425                                               │
│                                                                  │
│ 1. Получает FSuspenseCoreUnifiedItemData из ItemManager         │
│ 2. Вызывает GrantAbilitiesFromItemData()                        │
└────────┬────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────────┐
│ GrantAbilitiesFromItemData() (cpp:953-1067)                     │
│                                                                  │
│ ДВЕ КАТЕГОРИИ ABILITIES:                                        │
│                                                                  │
│ 1. Base Abilities (из ItemData.GrantedAbilities):               │
│    - ADS Ability                                                 │
│    - Reload Ability                                              │
│    - Inspect Ability                                             │
│                                                                  │
│ 2. Fire Mode Abilities (из ItemData.FireModes):                 │
│    - USuspenseCoreSingleShotAbility                             │
│    - USuspenseCoreBurstFireAbility                              │
│    - USuspenseCoreAutoFireAbility                               │
└────────┬────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────────┐
│ GrantSingleAbility() (cpp:1227-1260)                            │
│                                                                  │
│ FGameplayAbilitySpec AbilitySpec(AbilityClass, Level, ...);    │
│ AbilitySpec.GetDynamicSpecSourceTags().AddTag(InputTag);        │
│ ASC->GiveAbility(AbilitySpec);                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Структура данных в SSOT (DataTable)

```cpp
// Source/BridgeSystem/Public/SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h

USTRUCT(BlueprintType)
struct FSuspenseCoreUnifiedItemData
{
    // === Identity ===
    FName ItemID;
    FText DisplayName;

    // === GAS Integration ===

    // Abilities грантящиеся при экипировке
    UPROPERTY(EditAnywhere, Category = "Item|GAS")
    TArray<FGrantedAbilityData> GrantedAbilities;

    // Passive effects (всегда активны пока экипировано)
    UPROPERTY(EditAnywhere, Category = "Item|GAS")
    TArray<TSubclassOf<UGameplayEffect>> PassiveEffects;

    // === Weapon-specific ===

    // Fire modes с их ability классами
    UPROPERTY(EditAnywhere, Category = "Item|Weapon")
    TArray<FWeaponFireModeData> FireModes;

    // Default fire mode при экипировке
    UPROPERTY(EditAnywhere, Category = "Item|Weapon")
    FGameplayTag DefaultFireMode;
};

// Структура для Fire Mode
USTRUCT(BlueprintType)
struct FWeaponFireModeData
{
    FGameplayTag FireModeTag;              // e.g., Weapon.FireMode.Single
    FText DisplayName;                      // "Semi-Auto"
    TSubclassOf<UGameplayAbility> FireModeAbility;  // USuspenseCoreSingleShotAbility
    int32 InputID = 0;
    bool bEnabled = true;
};

// Структура для грантинга ability
USTRUCT(BlueprintType)
struct FGrantedAbilityData
{
    TSubclassOf<UGameplayAbility> AbilityClass;  // USuspenseCoreReloadAbility
    int32 AbilityLevel = 1;
    FGameplayTag InputTag;                        // SuspenseCore.Ability.Weapon.Reload
    FGameplayTagContainer ActivationRequiredTags;
};
```

---

## 3. Input → Ability Activation Flow

### 3.1 Полная цепочка вызовов

```
┌─────────────────────────────────────────────────────────────────┐
│ PLAYER INPUT (Enhanced Input System)                            │
│                                                                  │
│ PlayerController::SetupInputComponent()                         │
│ Source/PlayerCore/Private/.../SuspenseCorePlayerController.cpp  │
└────────┬────────────────────────────────────────────────────────┘
         │ IA_Fire triggered
         ▼
┌─────────────────────────────────────────────────────────────────┐
│ HandleFirePressed(const FInputActionValue& Value)               │
│ (cpp:394-397)                                                   │
│                                                                  │
│ ActivateAbilityByTag(                                           │
│     FGameplayTag::RequestGameplayTag(                           │
│         FName("SuspenseCore.Ability.Weapon.Fire")),             │
│     true);                                                       │
└────────┬────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────────┐
│ ActivateAbilityByTag(AbilityTag, bPressed) (cpp:469-489)        │
│                                                                  │
│ if (bPressed) {                                                  │
│     FGameplayTagContainer TagContainer;                          │
│     TagContainer.AddTag(AbilityTag);                             │
│     ASC->TryActivateAbilitiesByTag(TagContainer);  ◄── KEY!     │
│ } else {                                                         │
│     ASC->CancelAbilities(&TagContainer);                        │
│ }                                                                │
└────────┬────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────────┐
│ ASC->TryActivateAbilitiesByTag()                                │
│                                                                  │
│ ПОИСК MATCHING ABILITY:                                         │
│ - Ищет все granted abilities                                    │
│ - Сравнивает AssetTags с переданным TagContainer                │
│ - Находит ability с тегом SuspenseCore.Ability.Weapon.Fire      │
│ - Проверяет CanActivateAbility()                                │
│ - Вызывает ActivateAbility()                                    │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Как Ability связывается с тегом

```cpp
// В конструкторе Fire Ability:
USuspenseCoreSingleShotAbility::USuspenseCoreSingleShotAbility()
{
    // Устанавливаем тег через SetAssetTags() (UE5.5+ compliant)
    FGameplayTagContainer AssetTags;
    AssetTags.AddTag(SuspenseCoreTags::Ability::Weapon::Fire);
    SetAssetTags(AssetTags);

    // ActivationRequiredTags - требуем тег fire mode на ASC
    ActivationRequiredTags.AddTag(SuspenseCoreTags::Weapon::FireMode::Single);
}
```

---

## 4. Weapon Ability Activation Context

### 4.1 Как Fire Ability находит текущее оружие

```cpp
// Source/GAS/Private/.../SuspenseCoreBaseFireAbility.cpp:723-744

ISuspenseCoreWeapon* USuspenseCoreBaseFireAbility::GetWeaponInterface() const
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if (!Avatar) return nullptr;

    // Ищем attached actors на персонаже
    TArray<AActor*> AttachedActors;
    Avatar->GetAttachedActors(AttachedActors);

    for (AActor* Attached : AttachedActors)
    {
        // Находим первый актор с ISuspenseCoreWeapon интерфейсом
        if (ISuspenseCoreWeapon* Weapon = Cast<ISuspenseCoreWeapon>(Attached))
        {
            return Weapon;  // ◄── Это АКТИВНОЕ оружие
        }
    }

    return nullptr;
}
```

### 4.2 Критический момент: Attach/Detach при переключении

```
WEAPON SWITCH FLOW:
═══════════════════

1. Player Press "1" (Primary Weapon)
         │
         ▼
2. GA_WeaponSwitch_Primary::ActivateAbility()
         │
         ▼
3. Provider->SetActiveWeaponSlot(0)
         │
         ▼
4. WeaponStateManager updates state
         │
         ▼
5. EquipmentDataStore notifies weapon actors
         │
         ▼
6. OLD Weapon: Detach from Character, hide
   NEW Weapon: Attach to Character, show
         │
         ▼
7. Fire Ability теперь находит NEW Weapon
   через GetAttachedActors()
```

---

## 5. Текущие проблемы и рекомендации

### 5.1 ПРОБЛЕМА: Все abilities грантятся одновременно

**Текущее поведение:**
- При экипировке оружия в слот - abilities сразу грантятся в ASC
- Все abilities всех 3-4 оружий находятся в ASC одновременно
- Fire Ability использует attached weapon для контекста

**Потенциальные issues:**
- Memory overhead от множества granted abilities
- Возможные коллизии тегов если у двух оружий одинаковый fire mode

**Рекомендация (AAA уровень):**
```
ВАРИАНТ A: Текущий подход (приемлемо для MVP)
- Abilities грантятся при экипировке
- Контекст определяется через attached weapon
- Простая реализация

ВАРИАНТ B: Grant/Revoke при переключении (Tarkov-style)
- Abilities грантятся только для АКТИВНОГО слота
- При переключении: Revoke old → Grant new
- Чище, но сложнее реализовать
```

### 5.2 ПРОБЛЕМА: RequestGameplayTag в Input Handlers

**Текущий код:**
```cpp
// cpp:394-397
void ASuspenseCorePlayerController::HandleFirePressed(...)
{
    ActivateAbilityByTag(
        FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Ability.Weapon.Fire")),
        true);
}
```

**Проблема:** `RequestGameplayTag()` создаёт тег в runtime - неэффективно.

**Рекомендация:**
```cpp
// Использовать native tags:
ActivateAbilityByTag(SuspenseCoreTags::Ability::Weapon::Fire, true);
```

### 5.3 ПРОБЛЕМА: Fire Mode Tag не добавлялся в ASC

**Исправлено в текущем коммите:**
- `SuspenseCoreWeaponFireModeComponent::UpdateFireModeTagOnASC()`
- `SuspenseCoreSwitchFireModeAbility::UpdateFireModeTagsOnASC()`

### 5.4 Рекомендации по конфигурации в DataTable

**Пример записи для AK-74:**
```json
{
    "ItemID": "Weapon_AK74",
    "DisplayName": "АК-74",
    "bIsWeapon": true,

    "GrantedAbilities": [
        {
            "AbilityClass": "USuspenseCoreAimDownSightAbility",
            "InputTag": "SuspenseCore.Ability.Weapon.AimDownSight"
        },
        {
            "AbilityClass": "USuspenseCoreReloadAbility",
            "InputTag": "SuspenseCore.Ability.Weapon.Reload"
        }
    ],

    "FireModes": [
        {
            "FireModeTag": "SuspenseCore.Weapon.FireMode.Single",
            "DisplayName": "Одиночный",
            "FireModeAbility": "USuspenseCoreSingleShotAbility",
            "bEnabled": true
        },
        {
            "FireModeTag": "SuspenseCore.Weapon.FireMode.Auto",
            "DisplayName": "Автоматический",
            "FireModeAbility": "USuspenseCoreAutoFireAbility",
            "bEnabled": true
        }
    ],

    "DefaultFireMode": "SuspenseCore.Weapon.FireMode.Auto",

    "WeaponAttributesRowName": "AK74_Attributes"
}
```

---

## 6. Сетевая архитектура

### 6.1 Server-Authoritative Model

```
┌─────────────────────────────────────────────────────────────────┐
│                         SERVER                                   │
│                                                                  │
│  ASC на PlayerState:                                            │
│  - Владеет всеми abilities и effects                            │
│  - Принимает решения об активации                               │
│  - Реплицирует состояние на клиентов                            │
│                                                                  │
│  ReplicationMode = EGameplayEffectReplicationMode::Mixed        │
│  - Для owner: Full replication                                   │
│  - Для spectators: Minimal replication                          │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ Replicated
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                        CLIENT (Owner)                            │
│                                                                  │
│  - Local prediction для responsive gameplay                     │
│  - Server reconciliation при расхождениях                       │
│  - ShouldDoServerAbilityRPCBatch() = true для оптимизации      │
└─────────────────────────────────────────────────────────────────┘
```

### 6.2 RPC Batching

```cpp
// Source/GAS/Public/.../SuspenseCoreAbilitySystemComponent.h:45
virtual bool ShouldDoServerAbilityRPCBatch() const override { return true; }
```

Это критически важно для MMO FPS - без батчинга каждый выстрел будет отдельным RPC.

---

## 7. Чеклист для добавления нового оружия

### В DataTable (SSOT):
- [ ] Создать запись `FSuspenseCoreUnifiedItemData`
- [ ] Заполнить `GrantedAbilities` (ADS, Reload, Inspect)
- [ ] Настроить `FireModes` с правильными ability классами
- [ ] Установить `DefaultFireMode`
- [ ] Указать `WeaponAttributesRowName` для статистики

### Attributes DataTable:
- [ ] Создать запись с атрибутами (Damage, FireRate, etc.)
- [ ] Настроить TacticalReloadTime, FullReloadTime
- [ ] Установить базовый Spread

### Проверка:
- [ ] Abilities грантятся при экипировке (check logs)
- [ ] Fire mode tag добавляется в ASC
- [ ] Input активирует правильную ability
- [ ] Weapon context находится через GetAttachedActors()

---

## 8. Резюме архитектуры

| Компонент | Расположение | Ответственность |
|-----------|--------------|-----------------|
| **ASC** | PlayerState | Владение abilities, replication |
| **DataStore** | PlayerState | Equipment state, active slot |
| **AbilityConnector** | PlayerState | Grant/Revoke abilities |
| **Character** | World | Physical representation, weapon attachment |
| **WeaponActor** | Attached to Character | Weapon state, magazine, fire mode |
| **PlayerController** | Client | Input handling → ability activation |
| **ItemManager** | Subsystem | SSOT data access |

**Архитектура соответствует AAA стандартам** с небольшими оптимизациями, описанными выше.
