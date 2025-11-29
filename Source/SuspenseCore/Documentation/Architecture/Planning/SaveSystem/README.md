# SaveSystem Architecture Design

> **Статус:** ЧАСТИЧНО РЕАЛИЗОВАНО (Этапы 1, 3, 5)
> **Модуль:** BridgeSystem (расширение)
> **Версия:** 1.0
> **Дата:** 2025-11-29
> **Обновлено:** 2025-11-29

## Реализованные компоненты

| Компонент | Файл | Статус |
|-----------|------|--------|
| Save Types | `SuspenseCore/Save/SuspenseCoreSaveTypes.h` | ✅ |
| Save Interfaces | `SuspenseCore/Save/SuspenseCoreSaveInterfaces.h` | ✅ |
| File Repository | `SuspenseCore/Save/SuspenseCoreFileSaveRepository.h/cpp` | ✅ |
| Save Manager | `SuspenseCore/Save/SuspenseCoreSaveManager.h/cpp` | ✅ |
| Pause Menu | `Widgets/SuspenseCorePauseMenuWidget.h/cpp` | ✅ |

---

## Содержание

1. [Обзор](#1-обзор)
2. [Текущее состояние](#2-текущее-состояние)
3. [Требования](#3-требования)
4. [Архитектура](#4-архитектура)
5. [Структура данных](#5-структура-данных)
6. [Интерфейсы](#6-интерфейсы)
7. [Бэкенды](#7-бэкенды)
8. [Этапы реализации](#8-этапы-реализации)
9. [Интеграция](#9-интеграция)
10. [UI компоненты](#10-ui-компоненты)

---

## 1. Обзор

### 1.1 Назначение

SaveSystem — единая система сохранения и загрузки всех игровых данных:
- **Профиль игрока** (аккаунт, прогресс, статистика)
- **Состояние персонажа** (здоровье, атрибуты, эффекты)
- **Инвентарь и экипировка** (предметы, слоты)
- **Состояние мира** (для persistent worlds)
- **Настройки** (controls, graphics, audio)

### 1.2 Цели

| Цель | Описание |
|------|----------|
| **Унификация** | Единый API для всех типов данных |
| **Расширяемость** | Легко добавлять новые типы данных |
| **Масштабируемость** | Поддержка MMO (тысячи игроков) |
| **Надёжность** | Защита от потери данных |
| **Производительность** | Асинхронное сохранение без лагов |

### 1.3 Принципы

```
┌─────────────────────────────────────────────────────────────┐
│                    CLEAN ARCHITECTURE                       │
│                                                             │
│   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐   │
│   │  UI Layer   │───▶│  Use Cases  │───▶│  Entities   │   │
│   │ (Widgets)   │    │  (Managers) │    │   (Data)    │   │
│   └──────┬──────┘    └──────┬──────┘    └─────────────┘   │
│          │                  │                              │
│          │                  ▼                              │
│          │           ┌─────────────┐                       │
│          └──────────▶│ Repositories│◀── Interface          │
│                      └──────┬──────┘                       │
│                             │                              │
│          ┌──────────────────┼──────────────────┐          │
│          ▼                  ▼                  ▼          │
│   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐   │
│   │    File     │    │   Steam     │    │  Database   │   │
│   │  Backend    │    │   Cloud     │    │   Backend   │   │
│   └─────────────┘    └─────────────┘    └─────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## 2. Текущее состояние

### 2.1 Что уже реализовано

| Компонент | Статус | Расположение |
|-----------|--------|--------------|
| `FSuspenseCorePlayerData` | ✅ Готово | BridgeSystem |
| `ISuspenseCorePlayerRepository` | ✅ Готово | BridgeSystem |
| `USuspenseCoreFilePlayerRepository` | ✅ Готово | BridgeSystem |
| Регистрация игрока | ✅ Готово | UISystem |
| Выбор персонажа | ✅ Готово | UISystem |

### 2.2 Что нужно добавить

| Компонент | Приоритет | Описание |
|-----------|-----------|----------|
| Runtime Save Data | 🔴 Высокий | Сохранение состояния во время игры |
| Auto-Save System | 🔴 Высокий | Автоматическое сохранение |
| Save Slots | 🟡 Средний | Несколько слотов сохранения |
| Cloud Sync | 🟡 Средний | Синхронизация Steam/Epic |
| World State | 🟢 Низкий | Для persistent worlds |

---

## 3. Требования

### 3.1 Функциональные

#### FR-1: Типы данных для сохранения

| Тип | Описание | Когда сохраняется |
|-----|----------|-------------------|
| Profile | Аккаунт, XP, валюта, статистика | При изменении |
| Character | HP, позиция, атрибуты, эффекты | Auto-save / ручное |
| Inventory | Все предметы персонажа | При изменении |
| Equipment | Текущая экипировка | При изменении |
| Settings | Настройки игрока | При выходе |
| World | Состояние объектов мира | Checkpoint / ручное |

#### FR-2: Операции

| Операция | Описание |
|----------|----------|
| Save | Сохранить данные в хранилище |
| Load | Загрузить данные из хранилища |
| Delete | Удалить сохранение |
| List | Получить список сохранений |
| Export | Экспорт в файл (backup) |
| Import | Импорт из файла |

#### FR-3: Save Slots

```
Player Account
├── Profile Data (единственный)
│
├── Save Slot 1 (Character A)
│   ├── Character State
│   ├── Inventory
│   ├── Equipment
│   └── World State
│
├── Save Slot 2 (Character B)
│   └── ...
│
└── Save Slot 3 (Auto-Save)
    └── ...
```

### 3.2 Нефункциональные

| Требование | Значение |
|------------|----------|
| Время сохранения | < 100ms (async) |
| Размер сохранения | < 5MB на слот |
| Версионирование | Миграция между версиями |
| Шифрование | Опционально для защиты |
| Сжатие | Опционально для размера |

---

## 4. Архитектура

### 4.1 Диаграмма компонентов

```
┌─────────────────────────────────────────────────────────────────────┐
│                           UISystem                                   │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐  │
│  │  SaveLoadWidget  │  │ SaveSlotWidget   │  │ AutoSaveIndicator│  │
│  └────────┬─────────┘  └────────┬─────────┘  └────────┬─────────┘  │
└───────────┼──────────────────────┼──────────────────────┼───────────┘
            │                      │                      │
            ▼                      ▼                      ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         BridgeSystem                                 │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │                   USuspenseCoreSaveManager                    │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐   │  │
│  │  │ SaveProfile  │  │ SaveSlot     │  │ AutoSaveSystem   │   │  │
│  │  └──────────────┘  └──────────────┘  └──────────────────┘   │  │
│  └──────────────────────────────────────────────────────────────┘  │
│                                │                                     │
│                                ▼                                     │
│  ┌──────────────────────────────────────────────────────────────┐  │
│  │              ISuspenseCoreSaveRepository                      │  │
│  └──────────────────────────────────────────────────────────────┘  │
│           │                    │                    │               │
│           ▼                    ▼                    ▼               │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐       │
│  │FileRepository│     │CloudRepository│     │ DBRepository │       │
│  │ (Local JSON) │     │(Steam/Epic)  │     │  (MySQL/etc) │       │
│  └──────────────┘     └──────────────┘     └──────────────┘       │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.2 Поток данных

```
┌─────────────────────────────────────────────────────────────────────┐
│                         SAVE FLOW                                    │
│                                                                      │
│  [Game State] ──► [Collect Data] ──► [Serialize] ──► [Compress]     │
│                                                            │         │
│                                                            ▼         │
│                                                      [Encrypt]       │
│                                                            │         │
│                                                            ▼         │
│                                                      [Write to       │
│                                                       Backend]       │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                         LOAD FLOW                                    │
│                                                                      │
│  [Read from   ──► [Decrypt] ──► [Decompress] ──► [Deserialize]      │
│   Backend]                                             │             │
│                                                        ▼             │
│                                              [Validate Version]      │
│                                                        │             │
│                                                        ▼             │
│                                              [Migrate if needed]     │
│                                                        │             │
│                                                        ▼             │
│                                              [Apply to Game State]   │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 5. Структура данных

### 5.1 Полная иерархия

```cpp
// Корневая структура сохранения
FSuspenseCoreSaveData
├── Header (метаданные)
│   ├── SaveVersion: int32
│   ├── SaveTimestamp: FDateTime
│   ├── PlayTime: int64
│   ├── SlotName: FString
│   └── ThumbnailData: TArray<uint8>
│
├── ProfileData (существующий FSuspenseCorePlayerData)
│   ├── PlayerId
│   ├── DisplayName
│   ├── Level, XP, Currency
│   ├── Stats
│   ├── Settings
│   ├── Loadouts
│   ├── Unlocks
│   └── Achievements
│
├── CharacterState (НОВОЕ)
│   ├── CurrentHealth: float
│   ├── CurrentStamina: float
│   ├── CurrentMana: float
│   ├── Position: FVector
│   ├── Rotation: FRotator
│   ├── CurrentMapName: FName
│   ├── ActiveAbilities: TArray<FGameplayTag>
│   ├── ActiveEffects: TArray<FSuspenseCoreActiveEffect>
│   └── AttributeOverrides: TMap<FGameplayTag, float>
│
├── InventoryState (НОВОЕ)
│   ├── Items: TArray<FSuspenseCoreRuntimeItem>
│   │   ├── DefinitionId
│   │   ├── InstanceId
│   │   ├── Quantity
│   │   ├── SlotIndex
│   │   ├── Durability
│   │   └── CustomData
│   └── Currency: TMap<FString, int64>
│
├── EquipmentState (НОВОЕ)
│   ├── EquippedSlots: TMap<EEquipmentSlot, FString>
│   ├── WeaponAttachments: TMap<FString, TArray<FString>>
│   ├── ArmorDyes: TMap<FString, FLinearColor>
│   └── ActiveLoadoutIndex: int32
│
├── QuestState (НОВОЕ)
│   ├── ActiveQuests: TArray<FSuspenseCoreQuestProgress>
│   ├── CompletedQuests: TArray<FString>
│   └── QuestFlags: TMap<FString, bool>
│
└── WorldState (НОВОЕ - для singleplayer/persistent)
    ├── DestroyedActors: TArray<FGuid>
    ├── SpawnedActors: TArray<FSuspenseCoreSpawnedActor>
    ├── ContainerStates: TMap<FGuid, FSuspenseCoreContainerState>
    ├── DoorStates: TMap<FGuid, bool>
    └── CustomWorldData: TMap<FString, FString>
```

### 5.2 Новые структуры

```cpp
// ═══════════════════════════════════════════════════════════════
// SAVE HEADER
// ═══════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct FSuspenseCoreSaveHeader
{
    GENERATED_BODY()

    /** Версия формата сохранения */
    UPROPERTY()
    int32 SaveVersion = 1;

    /** Время создания сохранения */
    UPROPERTY()
    FDateTime SaveTimestamp;

    /** Общее время игры (секунды) */
    UPROPERTY()
    int64 TotalPlayTimeSeconds = 0;

    /** Название слота */
    UPROPERTY()
    FString SlotName;

    /** Описание (для отображения) */
    UPROPERTY()
    FString Description;

    /** Миниатюра (PNG bytes) */
    UPROPERTY()
    TArray<uint8> ThumbnailData;

    /** Имя персонажа */
    UPROPERTY()
    FString CharacterName;

    /** Уровень персонажа */
    UPROPERTY()
    int32 CharacterLevel = 1;

    /** Название локации */
    UPROPERTY()
    FString LocationName;
};

// ═══════════════════════════════════════════════════════════════
// CHARACTER STATE
// ═══════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct FSuspenseCoreActiveEffect
{
    GENERATED_BODY()

    /** ID эффекта */
    UPROPERTY()
    FString EffectId;

    /** Оставшееся время (0 = бесконечно) */
    UPROPERTY()
    float RemainingDuration = 0.0f;

    /** Уровень стаков */
    UPROPERTY()
    int32 StackCount = 1;

    /** Источник эффекта */
    UPROPERTY()
    FString SourceId;
};

USTRUCT(BlueprintType)
struct FSuspenseCoreCharacterState
{
    GENERATED_BODY()

    // Атрибуты
    UPROPERTY()
    float CurrentHealth = 100.0f;

    UPROPERTY()
    float MaxHealth = 100.0f;

    UPROPERTY()
    float CurrentStamina = 100.0f;

    UPROPERTY()
    float CurrentMana = 100.0f;

    // Позиция
    UPROPERTY()
    FVector WorldPosition = FVector::ZeroVector;

    UPROPERTY()
    FRotator WorldRotation = FRotator::ZeroRotator;

    UPROPERTY()
    FName CurrentMapName;

    UPROPERTY()
    FName CurrentCheckpointId;

    // Активные эффекты
    UPROPERTY()
    TArray<FSuspenseCoreActiveEffect> ActiveEffects;

    // Кулдауны абилок
    UPROPERTY()
    TMap<FString, float> AbilityCooldowns;

    // Флаги состояния
    UPROPERTY()
    bool bIsInCombat = false;

    UPROPERTY()
    bool bIsDead = false;
};

// ═══════════════════════════════════════════════════════════════
// INVENTORY STATE
// ═══════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct FSuspenseCoreRuntimeItem
{
    GENERATED_BODY()

    /** ID определения предмета (ссылка на DataAsset) */
    UPROPERTY()
    FString DefinitionId;

    /** Уникальный ID экземпляра */
    UPROPERTY()
    FString InstanceId;

    /** Количество */
    UPROPERTY()
    int32 Quantity = 1;

    /** Индекс слота в инвентаре (-1 = не привязан) */
    UPROPERTY()
    int32 SlotIndex = -1;

    /** Прочность (для оружия/брони) */
    UPROPERTY()
    float Durability = 1.0f;

    /** Уровень улучшения */
    UPROPERTY()
    int32 UpgradeLevel = 0;

    /** Вложения (attachments) */
    UPROPERTY()
    TArray<FString> AttachmentIds;

    /** Произвольные данные (JSON) */
    UPROPERTY()
    FString CustomData;
};

USTRUCT(BlueprintType)
struct FSuspenseCoreInventoryState
{
    GENERATED_BODY()

    /** Все предметы */
    UPROPERTY()
    TArray<FSuspenseCoreRuntimeItem> Items;

    /** Валюты */
    UPROPERTY()
    TMap<FString, int64> Currencies;

    /** Размер инвентаря */
    UPROPERTY()
    int32 InventorySize = 50;

    /** Заблокированные слоты */
    UPROPERTY()
    TArray<int32> LockedSlots;
};

// ═══════════════════════════════════════════════════════════════
// EQUIPMENT STATE
// ═══════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct FSuspenseCoreEquipmentState
{
    GENERATED_BODY()

    /** Экипированные слоты: Slot -> ItemInstanceId */
    UPROPERTY()
    TMap<FString, FString> EquippedSlots;

    /** Активный индекс loadout */
    UPROPERTY()
    int32 ActiveLoadoutIndex = 0;

    /** Быстрый доступ (hotbar): SlotIndex -> ItemInstanceId */
    UPROPERTY()
    TMap<int32, FString> QuickSlots;
};

// ═══════════════════════════════════════════════════════════════
// QUEST STATE
// ═══════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct FSuspenseCoreQuestObjective
{
    GENERATED_BODY()

    UPROPERTY()
    FString ObjectiveId;

    UPROPERTY()
    int32 CurrentProgress = 0;

    UPROPERTY()
    int32 RequiredProgress = 1;

    UPROPERTY()
    bool bCompleted = false;
};

USTRUCT(BlueprintType)
struct FSuspenseCoreQuestProgress
{
    GENERATED_BODY()

    UPROPERTY()
    FString QuestId;

    UPROPERTY()
    FString CurrentStage;

    UPROPERTY()
    TArray<FSuspenseCoreQuestObjective> Objectives;

    UPROPERTY()
    FDateTime StartedAt;

    UPROPERTY()
    TMap<FString, FString> QuestVariables;
};

USTRUCT(BlueprintType)
struct FSuspenseCoreQuestState
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FSuspenseCoreQuestProgress> ActiveQuests;

    UPROPERTY()
    TArray<FString> CompletedQuestIds;

    UPROPERTY()
    TArray<FString> FailedQuestIds;

    UPROPERTY()
    TMap<FString, bool> GlobalFlags;

    UPROPERTY()
    TMap<FString, int32> GlobalCounters;
};

// ═══════════════════════════════════════════════════════════════
// WORLD STATE (for persistent worlds)
// ═══════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct FSuspenseCoreSpawnedActor
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid ActorGuid;

    UPROPERTY()
    FString ActorClass;

    UPROPERTY()
    FTransform Transform;

    UPROPERTY()
    FString CustomData;
};

USTRUCT(BlueprintType)
struct FSuspenseCoreContainerState
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FSuspenseCoreRuntimeItem> Items;

    UPROPERTY()
    bool bHasBeenLooted = false;
};

USTRUCT(BlueprintType)
struct FSuspenseCoreWorldState
{
    GENERATED_BODY()

    /** Уничтоженные акторы (по GUID) */
    UPROPERTY()
    TArray<FGuid> DestroyedActors;

    /** Динамически созданные акторы */
    UPROPERTY()
    TArray<FSuspenseCoreSpawnedActor> SpawnedActors;

    /** Состояния контейнеров */
    UPROPERTY()
    TMap<FString, FSuspenseCoreContainerState> ContainerStates;

    /** Состояния дверей */
    UPROPERTY()
    TMap<FString, bool> DoorStates;

    /** Состояния переключателей */
    UPROPERTY()
    TMap<FString, int32> SwitchStates;

    /** Произвольные данные */
    UPROPERTY()
    TMap<FString, FString> CustomWorldData;
};

// ═══════════════════════════════════════════════════════════════
// FULL SAVE DATA
// ═══════════════════════════════════════════════════════════════

USTRUCT(BlueprintType)
struct FSuspenseCoreSaveData
{
    GENERATED_BODY()

    /** Заголовок с метаданными */
    UPROPERTY()
    FSuspenseCoreSaveHeader Header;

    /** Данные профиля (аккаунт) */
    UPROPERTY()
    FSuspenseCorePlayerData ProfileData;

    /** Состояние персонажа */
    UPROPERTY()
    FSuspenseCoreCharacterState CharacterState;

    /** Состояние инвентаря */
    UPROPERTY()
    FSuspenseCoreInventoryState InventoryState;

    /** Состояние экипировки */
    UPROPERTY()
    FSuspenseCoreEquipmentState EquipmentState;

    /** Состояние квестов */
    UPROPERTY()
    FSuspenseCoreQuestState QuestState;

    /** Состояние мира */
    UPROPERTY()
    FSuspenseCoreWorldState WorldState;

    /** Версия для миграции */
    static constexpr int32 CURRENT_VERSION = 1;
};
```

---

## 6. Интерфейсы

### 6.1 ISuspenseCoreSaveRepository

```cpp
/**
 * Интерфейс репозитория сохранений.
 * Абстрагирует конкретное хранилище (файлы, облако, БД).
 */
UINTERFACE(BlueprintType)
class USuspenseCoreSaveRepository : public UInterface
{
    GENERATED_BODY()
};

class ISuspenseCoreSaveRepository
{
    GENERATED_BODY()

public:
    // ═══════════════════════════════════════════════════════════════
    // CRUD Operations
    // ═══════════════════════════════════════════════════════════════

    /** Сохранить данные в слот */
    virtual bool SaveToSlot(
        const FString& PlayerId,
        int32 SlotIndex,
        const FSuspenseCoreSaveData& SaveData) = 0;

    /** Загрузить данные из слота */
    virtual bool LoadFromSlot(
        const FString& PlayerId,
        int32 SlotIndex,
        FSuspenseCoreSaveData& OutSaveData) = 0;

    /** Удалить слот */
    virtual bool DeleteSlot(
        const FString& PlayerId,
        int32 SlotIndex) = 0;

    /** Проверить существование слота */
    virtual bool SlotExists(
        const FString& PlayerId,
        int32 SlotIndex) = 0;

    // ═══════════════════════════════════════════════════════════════
    // Metadata
    // ═══════════════════════════════════════════════════════════════

    /** Получить заголовки всех сохранений игрока */
    virtual void GetSaveHeaders(
        const FString& PlayerId,
        TArray<FSuspenseCoreSaveHeader>& OutHeaders) = 0;

    /** Получить количество слотов */
    virtual int32 GetMaxSlots() const = 0;

    // ═══════════════════════════════════════════════════════════════
    // Async Operations
    // ═══════════════════════════════════════════════════════════════

    DECLARE_DELEGATE_TwoParams(FOnSaveComplete, bool /*bSuccess*/, const FString& /*ErrorMessage*/);
    DECLARE_DELEGATE_ThreeParams(FOnLoadComplete, bool /*bSuccess*/, const FSuspenseCoreSaveData& /*Data*/, const FString& /*ErrorMessage*/);

    /** Асинхронное сохранение */
    virtual void SaveToSlotAsync(
        const FString& PlayerId,
        int32 SlotIndex,
        const FSuspenseCoreSaveData& SaveData,
        FOnSaveComplete OnComplete) = 0;

    /** Асинхронная загрузка */
    virtual void LoadFromSlotAsync(
        const FString& PlayerId,
        int32 SlotIndex,
        FOnLoadComplete OnComplete) = 0;

    // ═══════════════════════════════════════════════════════════════
    // Info
    // ═══════════════════════════════════════════════════════════════

    virtual FString GetRepositoryType() const = 0;
    virtual bool IsAvailable() const = 0;
};
```

### 6.2 USuspenseCoreSaveManager

```cpp
/**
 * Менеджер сохранений — главный API для игрового кода.
 * GameInstance Subsystem для персистентности.
 */
UCLASS()
class USuspenseCoreSaveManager : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ═══════════════════════════════════════════════════════════════
    // Lifecycle
    // ═══════════════════════════════════════════════════════════════

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Статический доступ */
    static USuspenseCoreSaveManager* Get(const UObject* WorldContext);

    // ═══════════════════════════════════════════════════════════════
    // Quick Save/Load (текущий слот)
    // ═══════════════════════════════════════════════════════════════

    /** Быстрое сохранение */
    UFUNCTION(BlueprintCallable, Category = "SaveSystem")
    void QuickSave();

    /** Быстрая загрузка */
    UFUNCTION(BlueprintCallable, Category = "SaveSystem")
    void QuickLoad();

    // ═══════════════════════════════════════════════════════════════
    // Slot Management
    // ═══════════════════════════════════════════════════════════════

    /** Сохранить в конкретный слот */
    UFUNCTION(BlueprintCallable, Category = "SaveSystem")
    void SaveToSlot(int32 SlotIndex, const FString& SlotName = TEXT(""));

    /** Загрузить из слота */
    UFUNCTION(BlueprintCallable, Category = "SaveSystem")
    void LoadFromSlot(int32 SlotIndex);

    /** Удалить слот */
    UFUNCTION(BlueprintCallable, Category = "SaveSystem")
    void DeleteSlot(int32 SlotIndex);

    /** Получить информацию о слотах */
    UFUNCTION(BlueprintCallable, Category = "SaveSystem")
    TArray<FSuspenseCoreSaveHeader> GetAllSlotHeaders();

    // ═══════════════════════════════════════════════════════════════
    // Auto-Save
    // ═══════════════════════════════════════════════════════════════

    /** Включить/выключить автосохранение */
    UFUNCTION(BlueprintCallable, Category = "SaveSystem")
    void SetAutoSaveEnabled(bool bEnabled);

    /** Настроить интервал автосохранения */
    UFUNCTION(BlueprintCallable, Category = "SaveSystem")
    void SetAutoSaveInterval(float IntervalSeconds);

    /** Принудительное автосохранение */
    UFUNCTION(BlueprintCallable, Category = "SaveSystem")
    void TriggerAutoSave();

    // ═══════════════════════════════════════════════════════════════
    // Data Collection
    // ═══════════════════════════════════════════════════════════════

    /** Собрать текущее состояние игры */
    UFUNCTION(BlueprintCallable, Category = "SaveSystem")
    FSuspenseCoreSaveData CollectCurrentGameState();

    /** Применить загруженные данные */
    UFUNCTION(BlueprintCallable, Category = "SaveSystem")
    void ApplyLoadedState(const FSuspenseCoreSaveData& SaveData);

    // ═══════════════════════════════════════════════════════════════
    // Events
    // ═══════════════════════════════════════════════════════════════

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSaveStarted);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSaveCompleted, bool, bSuccess, const FString&, ErrorMessage);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLoadStarted);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoadCompleted, bool, bSuccess, const FString&, ErrorMessage);

    UPROPERTY(BlueprintAssignable)
    FOnSaveStarted OnSaveStarted;

    UPROPERTY(BlueprintAssignable)
    FOnSaveCompleted OnSaveCompleted;

    UPROPERTY(BlueprintAssignable)
    FOnLoadStarted OnLoadStarted;

    UPROPERTY(BlueprintAssignable)
    FOnLoadCompleted OnLoadCompleted;

protected:
    UPROPERTY()
    TScriptInterface<ISuspenseCoreSaveRepository> ActiveRepository;

    UPROPERTY()
    int32 CurrentSlotIndex = 0;

    UPROPERTY()
    FString CurrentPlayerId;

    // Auto-save
    FTimerHandle AutoSaveTimerHandle;
    float AutoSaveInterval = 300.0f; // 5 минут
    bool bAutoSaveEnabled = true;

    void OnAutoSaveTimer();
};
```

---

## 7. Бэкенды

### 7.1 File Backend (по умолчанию)

```cpp
/**
 * Файловый репозиторий сохранений.
 * Сохраняет в [Project]/Saved/SaveGames/[PlayerId]/Slot_X.sav
 */
UCLASS()
class USuspenseCoreFileSaveRepository : public UObject, public ISuspenseCoreSaveRepository
{
    // Реализация через USaveGame + Binary/JSON сериализация
};
```

**Структура файлов:**
```
[Project]/Saved/
├── Players/                    # Профили (существующее)
│   ├── GUID1.json
│   └── GUID2.json
│
└── SaveGames/                  # Новое: игровые сохранения
    ├── GUID1/
    │   ├── Slot_0.sav         # Слот 1
    │   ├── Slot_0.meta        # Метаданные (для быстрого доступа)
    │   ├── Slot_0.thumb       # Миниатюра
    │   ├── Slot_1.sav
    │   ├── AutoSave.sav       # Автосохранение
    │   └── QuickSave.sav      # Быстрое сохранение
    │
    └── GUID2/
        └── ...
```

### 7.2 Steam Cloud Backend (будущее)

```cpp
/**
 * Steam Cloud репозиторий.
 * Синхронизация через Steam API.
 */
UCLASS()
class USuspenseCoreSteamSaveRepository : public UObject, public ISuspenseCoreSaveRepository
{
    // Использует ISteamRemoteStorage
};
```

### 7.3 Database Backend (для MMO)

```cpp
/**
 * Репозиторий для dedicated server.
 * Подключение к MySQL/PostgreSQL/MongoDB.
 */
UCLASS()
class USuspenseCoreDbSaveRepository : public UObject, public ISuspenseCoreSaveRepository
{
    // Использует DatabaseConnector (отдельный модуль)
};
```

---

## 8. Этапы реализации

### Этап 1: Основа (1-2 недели)

| Задача | Описание | Файлы |
|--------|----------|-------|
| 1.1 | Создать структуры данных | `SuspenseCoreSaveTypes.h` |
| 1.2 | Реализовать ISuspenseCoreSaveRepository | `SuspenseCoreSaveInterfaces.h` |
| 1.3 | Реализовать File Backend | `SuspenseCoreFileSaveRepository.h/cpp` |
| 1.4 | Создать USuspenseCoreSaveManager | `SuspenseCoreSaveManager.h/cpp` |
| 1.5 | Базовые Save/Load функции | - |

### Этап 2: Data Collectors (1 неделя)

| Задача | Описание | Файлы |
|--------|----------|-------|
| 2.1 | Character State Collector | `SuspenseCoreCharacterSaveComponent.h/cpp` |
| 2.2 | Inventory State Collector | Расширить InventoryComponent |
| 2.3 | Equipment State Collector | Расширить EquipmentManager |
| 2.4 | Integration with GAS | Сохранение атрибутов |

### Этап 3: Auto-Save System (3-4 дня)

| Задача | Описание | Файлы |
|--------|----------|-------|
| 3.1 | Timer-based auto-save | SaveManager |
| 3.2 | Checkpoint triggers | `ASuspenseCoreCheckpoint` |
| 3.3 | Save indicators UI | `USuspenseCoreAutoSaveIndicator` |
| 3.4 | Event-based saves | На смерть, выход и т.д. |

### Этап 4: UI Components (1 неделя)

| Задача | Описание | Файлы |
|--------|----------|-------|
| 4.1 | Save/Load screen | `USuspenseCoreSaveLoadWidget` |
| 4.2 | Save slot widget | `USuspenseCoreSaveSlotWidget` |
| 4.3 | Confirmation dialogs | Перезапись, удаление |
| 4.4 | Thumbnails | Скриншоты сохранений |

### Этап 5: In-Game Menu (3-4 дня)

| Задача | Описание | Файлы |
|--------|----------|-------|
| 5.1 | Pause menu | `USuspenseCorePauseMenuWidget` |
| 5.2 | Settings integration | Привязка к Settings |
| 5.3 | Quick Save/Load bindings | Input actions |

### Этап 6: Advanced Features (будущее)

| Задача | Описание | Приоритет |
|--------|----------|-----------|
| 6.1 | Steam Cloud sync | 🟡 |
| 6.2 | Save versioning & migration | 🟡 |
| 6.3 | Save encryption | 🟢 |
| 6.4 | Save compression | 🟢 |
| 6.5 | Database backend | 🟢 |

---

## 9. Интеграция

### 9.1 С существующими системами

```cpp
// В Character Blueprint / C++
void ASuspenseCoreCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Подписка на сохранение
    if (USuspenseCoreSaveManager* SaveMgr = USuspenseCoreSaveManager::Get(this))
    {
        SaveMgr->OnSaveStarted.AddDynamic(this, &ASuspenseCoreCharacter::OnGameSaving);
        SaveMgr->OnLoadCompleted.AddDynamic(this, &ASuspenseCoreCharacter::OnGameLoaded);
    }
}

// Собрать данные персонажа для сохранения
FSuspenseCoreCharacterState ASuspenseCoreCharacter::CollectSaveState() const
{
    FSuspenseCoreCharacterState State;
    State.CurrentHealth = GetHealth();
    State.CurrentStamina = GetStamina();
    State.WorldPosition = GetActorLocation();
    State.WorldRotation = GetActorRotation();
    // ... etc
    return State;
}

// Применить загруженные данные
void ASuspenseCoreCharacter::ApplyLoadedState(const FSuspenseCoreCharacterState& State)
{
    SetHealth(State.CurrentHealth);
    SetStamina(State.CurrentStamina);
    SetActorLocationAndRotation(State.WorldPosition, State.WorldRotation);
    // ... etc
}
```

### 9.2 С EventBus

```cpp
// GameplayTags для событий сохранения
Event.Save.Started
Event.Save.Completed
Event.Save.Failed
Event.Load.Started
Event.Load.Completed
Event.Load.Failed
Event.AutoSave.Triggered
```

### 9.3 Input Bindings

```cpp
// Project Settings → Input
// Quick Save: F5
// Quick Load: F9
// Open Save Menu: Escape → Save/Load
```

---

## 10. UI Компоненты

### 10.1 Save/Load Screen

```
┌─────────────────────────────────────────────────────────────┐
│                     SAVE / LOAD GAME                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ [Thumbnail]  Save Slot 1                            │   │
│  │              "Main Story - Chapter 3"               │   │
│  │              Level 15 | 12:34:56 playtime           │   │
│  │              Last saved: 2025-11-29 14:30           │   │
│  │                                    [LOAD] [DELETE]  │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ [Thumbnail]  Save Slot 2                            │   │
│  │              "Side Quest - Merchant Route"          │   │
│  │              Level 12 | 08:15:00 playtime           │   │
│  │              Last saved: 2025-11-28 20:15           │   │
│  │                                    [LOAD] [DELETE]  │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ [Empty]      Save Slot 3                            │   │
│  │              -- Empty Slot --                       │   │
│  │                                              [SAVE] │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ [AUTO]       Auto-Save                              │   │
│  │              "Auto-saved checkpoint"                │   │
│  │              Level 15 | 12:30:00 playtime           │   │
│  │              Auto-saved: 2025-11-29 14:25           │   │
│  │                                             [LOAD]  │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
│                                              [BACK]         │
└─────────────────────────────────────────────────────────────┘
```

### 10.2 Pause Menu

```
┌─────────────────────────────────────────────────────────────┐
│                         PAUSED                              │
│                                                             │
│                     ┌───────────────┐                       │
│                     │    RESUME     │                       │
│                     ├───────────────┤                       │
│                     │  QUICK SAVE   │ (F5)                  │
│                     ├───────────────┤                       │
│                     │  QUICK LOAD   │ (F9)                  │
│                     ├───────────────┤                       │
│                     │  SAVE / LOAD  │                       │
│                     ├───────────────┤                       │
│                     │   SETTINGS    │                       │
│                     ├───────────────┤                       │
│                     │   MAIN MENU   │                       │
│                     ├───────────────┤                       │
│                     │     QUIT      │                       │
│                     └───────────────┘                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 10.3 Auto-Save Indicator

```
┌──────────────────────────────────────────┐
│ Top-right corner of screen:              │
│                                          │
│                        ┌─────────────┐   │
│                        │ 💾 Saving...│   │
│                        └─────────────┘   │
│                                          │
│ Fades out after 2 seconds                │
└──────────────────────────────────────────┘
```

---

## Следующие шаги

1. **Утвердить архитектуру** - Review этого документа
2. **Начать Этап 1** - Создание базовых структур
3. **Параллельно** - Дизайн UI в редакторе

---

**Автор:** Claude Code Assistant
**Дата создания:** 2025-11-29
**Последнее обновление:** 2025-11-29
