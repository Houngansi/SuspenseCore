# Extraction PvE: "Зона Отчуждения"

## Концепция

Игрок — сталкер-выживальщик в постсоветском дистопийном мире. Действие происходит в заброшенных промышленных районах, разрушенных городах бывшего СССР после неизвестной катастрофы. Задача: проникнуть в опасные зоны, собрать ценные ресурсы и выбраться живым. Каждый рейд — баланс между жадностью и выживанием.

**Сеттинг**: Пост-советская антиутопия. Разрушенные заводы, заброшенные НИИ, мёртвые колхозы, полуразваленные хрущёвки. Атмосфера Сталкера/Тарков с элементами советской эстетики.

**Целевая платформа**: Unreal Engine 5.5 (без World Partition)

---

## Core Loop (Основной Цикл)

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           УБЕЖИЩЕ (Hideout)                             │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐         │
│  │     Схрон       │  │    Верстак      │  │    Торговец     │         │
│  │    (Stash)      │  │   (Workbench)   │  │    (Trader)     │         │
│  └─────────────────┘  └─────────────────┘  └─────────────────┘         │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         DEPLOYMENT (Выход на рейд)                      │
│  - Выбор локации (риск/награда)                                         │
│  - Подготовка снаряжения                                                │
│  - Выбор точки входа                                                    │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                            RAID (Рейд)                                  │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │ EXPLORATION ──► COMBAT ──► LOOT ──► EXTRACTION                   │  │
│  │      │            │          │           │                        │  │
│  │      ▼            ▼          ▼           ▼                        │  │
│  │  Skill XP    Weapon XP   Resources   Survival Bonus              │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  THREATS:                           REWARDS:                            │
│  - Мародёры и бандиты               - Редкое оружие                    │
│  - Мутанты                          - Модификации                       │
│  - Аномалии                         - Медикаменты                       │
│  - Радиация                         - Материалы для крафта              │
└───────────────────────────────┬─────────────────────────────────────────┘
                                │
                    ┌───────────┴───────────┐
                    ▼                       ▼
            ┌─────────────┐         ┌─────────────┐
            │  EXTRACTED  │         │    DEAD     │
            │  (Выжил)    │         │  (Погиб)    │
            ├─────────────┤         ├─────────────┤
            │ Keep Loot   │         │ Lose Loot   │
            │ Keep XP     │         │ Keep XP*    │
            │ Gear Damage │         │ Lose Gear   │
            └─────────────┘         └─────────────┘
                    │                       │
                    └───────────┬───────────┘
                                ▼
                        [Return to Hideout]
```

---

## Level Architecture (UE 5.5 без World Partition)

### Архитектура уровней

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    LEVEL ARCHITECTURE (No World Partition)               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Persistent Level: L_GameMode (минимальный - только GameMode/Spawns)    │
│       │                                                                  │
│       ├─► L_Hub_Hideout     (загружен при старте, ~100x100м)            │
│       │                                                                  │
│       ├─► L_Raid_Outskirts  (стримится при входе в рейд, ~400x400м)     │
│       │                                                                  │
│       └─► L_Raid_Industrial (стримится при входе в рейд, ~350x350м)     │
│                                                                          │
│  Level Streaming: LoadStreamLevel при выборе рейда                      │
│  Unload: UnloadStreamLevel при возврате в Hub                           │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Размеры локаций (Demo)

| Локация | Уровень | Размер | Время рейда | Стиль |
|---------|---------|--------|-------------|-------|
| **Hub (Убежище)** | `L_Hub_Hideout` | 100x100м | - | Interior, безопасная |
| **Окраина** | `L_Raid_Outskirts` | 400x400м | 15-25 мин | Open + Buildings |
| **Промзона** | `L_Raid_Industrial` | 350x350м | 20-30 мин | Industrial, вертикальность |

### Level Streaming Implementation

```cpp
// В GameMode или RaidManager
UCLASS()
class URaidLevelManager : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Raid")
    void LoadRaidLevel(ERaidLocation Location)
    {
        // Выгрузить Hub
        FLatentActionInfo UnloadInfo;
        UnloadInfo.CallbackTarget = this;
        UnloadInfo.ExecutionFunction = "OnHubUnloaded";
        UnloadInfo.Linkage = 0;
        UnloadInfo.UUID = GetUniqueID();

        UGameplayStatics::UnloadStreamLevel(
            GetWorld(),
            FName("L_Hub_Hideout"),
            UnloadInfo,
            false
        );

        // Загрузить рейд
        FString LevelName = GetLevelNameForLocation(Location);
        FLatentActionInfo LoadInfo;
        LoadInfo.CallbackTarget = this;
        LoadInfo.ExecutionFunction = "OnRaidLoaded";
        LoadInfo.Linkage = 1;
        LoadInfo.UUID = GetUniqueID();

        UGameplayStatics::LoadStreamLevel(
            GetWorld(),
            FName(*LevelName),
            true,  // bMakeVisibleAfterLoad
            true,  // bShouldBlockOnLoad
            LoadInfo
        );
    }

    UFUNCTION(BlueprintCallable, Category = "Raid")
    void ReturnToHideout()
    {
        // Выгрузить текущий рейд, загрузить Hub
        // Аналогичная логика
    }

private:
    FString GetLevelNameForLocation(ERaidLocation Location)
    {
        switch (Location)
        {
            case ERaidLocation::Outskirts:  return TEXT("L_Raid_Outskirts");
            case ERaidLocation::Industrial: return TEXT("L_Raid_Industrial");
            default: return TEXT("");
        }
    }
};
```

---

## Локация: УБЕЖИЩЕ (Hub)

### Layout

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         HUB - Убежище                                    │
│                       L_Hub_Hideout (~100x100м)                          │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                      Подвал/Бункер (Interior)                     │   │
│  │                                                                   │   │
│  │   ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐            │   │
│  │   │  СХРОН  │  │ ВЕРСТАК │  │ТОРГОВЕЦ │  │ КАРТА   │            │   │
│  │   │ (Stash) │  │(Repairs)│  │ (Trade) │  │(Mission)│            │   │
│  │   └─────────┘  └─────────┘  └─────────┘  └─────────┘            │   │
│  │                                                                   │   │
│  │   ┌─────────┐  ┌─────────────────────────────────┐               │   │
│  │   │ КОЙКА   │  │        ЗОНА ПОДГОТОВКИ          │               │   │
│  │   │(Save/   │  │     (Loadout + Deploy)          │               │   │
│  │   │ Rest)   │  │                                 │               │   │
│  │   └─────────┘  └─────────────────────────────────┘               │   │
│  │                                                                   │   │
│  └──────────────────────────────────────────────────────────────────┘   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Функции Hub

| Зона | Функционал | Реализация |
|------|------------|------------|
| **Схрон (Stash)** | Grid-инвентарь для хранения | Существующая система инвентаря |
| **Верстак** | Ремонт оружия, модификации | Durability system + Trader UI |
| **Торговец** | Покупка/продажа предметов | NPC + Trade Widget |
| **Карта рейдов** | Выбор локации, просмотр заданий | Mission Board UI |
| **Койка** | Сохранение прогресса, heal | Save/Load + Rest mechanic |
| **Зона подготовки** | Loadout экипировки | Equipment Widget перед deploy |

### Hub Lighting

```
Освещение Hub:
├── Baked Lighting (Static)
├── Несколько точечных источников (лампы, свечи)
├── Volumetric Fog для атмосферы
└── Emissive materials для экранов/индикаторов
```

---

## Локация: ОКРАИНА (L_Raid_Outskirts)

### Overview

| Параметр | Значение |
|----------|----------|
| **Размер** | 400x400 метров |
| **Время рейда** | 15-25 минут |
| **Сложность** | Tier 1 (Low) |
| **Стиль** | Открытые пространства + здания |
| **Вертикальность** | Средняя (2-3 этажа) |

### Layout

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         ОКРАИНА - Layout                                 │
│                          (~400x400 метров)                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   [SPAWN A]                                      [SPAWN B]               │
│      ↓                                              ↓                    │
│  ┌────────┐         ┌──────────────┐         ┌────────┐                 │
│  │Гаражи  │─────────│  Автостанция │─────────│Общежитие│                │
│  │(loot)  │         │   (центр)    │         │(3 этажа)│                │
│  └────────┘         └──────────────┘         └────────┘                 │
│      │                     │                      │                      │
│      │    ┌────────────────┼────────────────┐    │                      │
│      │    │                │                │    │                      │
│      ▼    ▼                ▼                ▼    ▼                      │
│  ┌────────────┐    ┌──────────────┐    ┌────────────┐                   │
│  │ Заброшенный│    │   Площадь    │    │  Магазин   │                   │
│  │   двор     │    │  с памятником│    │  "Продукты"│                   │
│  └────────────┘    └──────────────┘    └────────────┘                   │
│        │                   │                  │                          │
│        │         ┌─────────┴─────────┐       │                          │
│        │         │                   │       │                          │
│        ▼         ▼                   ▼       ▼                          │
│  ┌──────────┐ ┌────────┐      ┌────────┐ ┌──────────┐                   │
│  │ Свалка   │ │ Тоннель│      │  КПП   │ │Заправка  │                   │
│  │(extract) │ │(shortcut)     │(extract)│ │ (loot)   │                   │
│  └──────────┘ └────────┘      └────────┘ └──────────┘                   │
│                                                                          │
│  [EXTRACT 1]              [EXTRACT 2]                                   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘

Легенда:
━━━━━━━━
- Spawn A/B: Точки входа игрока (рандом)
- Extract 1/2: Точки эвакуации (1-2 активны за рейд)
- Центральные зоны: Высокий лут, высокий риск
- Периферия: Безопаснее, меньше лута
```

### Points of Interest (POI)

| POI | Описание | Лут | Враги | Вертикаль |
|-----|----------|-----|-------|-----------|
| **Автостанция** | Центр карты, 2 этажа + крыша | High (редкие патроны) | 3-5 мародёров | 3 уровня |
| **Общежитие** | 3-этажное здание | Medium-High (медицина) | 2-4 на этаж | 3 этажа |
| **Гаражи** | Боксы с машинами | Medium (инструменты) | 1-2 | Нет |
| **Магазин "Продукты"** | Советский магазин | Medium (медицина, еда) | 2-3 | Нет |
| **Заправка** | АЗС с мини-маркетом | Medium (расходники) | 1-2 | Нет |
| **Площадь** | Открытая зона с памятником | Low | Патруль 2-3 | Нет |
| **Свалка** | Extraction point #1 | Low | Патруль 2-3 | Нет |
| **КПП** | Extraction point #2 | Low-Medium | 2-4 военных | Вышка |
| **Тоннель** | Подземный переход (shortcut) | Low | 1 мутант | Подземный |
| **Заброшенный двор** | Дворик между домами | Low | 0-1 | Нет |

### Задачи на локации

```cpp
// Типы заданий для Окраины
UENUM(BlueprintType)
enum class EOutskirtsObjective : uint8
{
    // === ОСНОВНЫЕ (всегда доступны) ===
    Extract,              // Просто выжить и эвакуироваться

    // === СБОР (рандом 1-2 за рейд) ===
    FindMedSupplies,      // Найти аптечки в Магазине (3 шт)
    CollectScrap,         // Собрать металлолом на Свалке (5 кг)
    FindAmmoCache,        // Найти тайник патронов в Гаражах

    // === УСТРАНЕНИЕ (рандом 0-1 за рейд) ===
    KillMarauders,        // Убить 5 мародёров
    ClearBusStation,      // Зачистить Автостанцию
    EliminateLeader,      // Убить главаря (спавн в Общежитии)

    // === ИССЛЕДОВАНИЕ (рандом 0-1 за рейд) ===
    PlaceMarker,          // Установить маяк на крыше Автостанции
    FindIntel,            // Найти документы в Общежитии
    MarkExtractionRoute   // Разведать путь к КПП
};
```

### Прогрессия сложности по времени

```
┌────────────────────────────────────────────────────────────────┐
│              ОКРАИНА - Difficulty Scaling                       │
├────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Время в рейде:     Сложность:        События:                 │
│                                                                 │
│  0-5 мин           ★☆☆☆☆             Начальный спавн врагов   │
│  5-10 мин          ★★☆☆☆             Патрули активируются     │
│  10-15 мин         ★★★☆☆             Подкрепление на Автостанцию│
│  15-20 мин         ★★★★☆             Военный патруль появляется│
│  20+ мин           ★★★★★             "Зачистка" - все к выходам│
│                                                                 │
│  Бонус за быструю эвакуацию: +15% XP за <10 мин               │
│  Штраф за долгий рейд: Усиленные враги после 20 мин           │
│                                                                 │
└────────────────────────────────────────────────────────────────┘
```

### Spawns & Extraction

```cpp
// Spawn Points - рандомный выбор при старте рейда
UPROPERTY(EditAnywhere, Category = "Spawns")
TArray<FTransform> PlayerSpawnPoints;  // 2+ точек

// Extraction Points - 1-2 активны за рейд (рандом)
UPROPERTY(EditAnywhere, Category = "Extraction")
TArray<FExtractionPoint> ExtractionPoints;

USTRUCT(BlueprintType)
struct FExtractionPoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FTransform Location;

    UPROPERTY(EditAnywhere)
    FText DisplayName;  // "Свалка", "КПП"

    UPROPERTY(EditAnywhere)
    float ExtractionTime = 7.0f;  // Секунд для эвакуации

    UPROPERTY(EditAnywhere)
    bool bRequiresPayment = false;  // Некоторые точки платные

    UPROPERTY(EditAnywhere)
    int32 PaymentAmount = 0;
};
```

---

## Локация: ПРОМЗОНА (L_Raid_Industrial)

### Overview

| Параметр | Значение |
|----------|----------|
| **Размер** | 350x350 метров |
| **Время рейда** | 20-30 минут |
| **Сложность** | Tier 2 (Medium) |
| **Стиль** | Industrial, вертикальность |
| **Вертикальность** | Высокая (краны, трубы, платформы) |

### Layout

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         ПРОМЗОНА - Layout                                │
│                          (~350x350 метров)                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│   [SPAWN A]                                      [SPAWN B]               │
│      ↓                                              ↓                    │
│  ┌──────────┐      ┌────────────────┐      ┌──────────┐                 │
│  │Ж/Д тупик │──────│  Главный цех   │──────│Котельная │                 │
│  │(вагоны)  │      │  (3 уровня)    │      │(трубы)   │                 │
│  └──────────┘      └────────────────┘      └──────────┘                 │
│       │                    │                     │                       │
│       │         ┌──────────┼──────────┐         │                       │
│       ▼         ▼          ▼          ▼         ▼                       │
│  ┌─────────┐ ┌───────┐ ┌────────┐ ┌───────┐ ┌─────────┐                │
│  │Склад    │ │Крановая│ │Двор с  │ │Админ. │ │Гараж    │                │
│  │(контейн)│ │(высота)│ │техникой│ │здание │ │техники  │                │
│  └─────────┘ └───────┘ └────────┘ └───────┘ └─────────┘                │
│       │          │          │          │         │                       │
│       │          │    ┌─────┴─────┐    │         │                       │
│       ▼          ▼    ▼           ▼    ▼         ▼                       │
│  ┌─────────┐  ┌────────────┐  ┌────────────┐  ┌─────────┐              │
│  │Пром.    │  │   Насосная │  │ Подстанция │  │ Проходная│              │
│  │свалка   │  │  станция   │  │ (опасно!)  │  │(extract) │              │
│  │(extract)│  └────────────┘  └────────────┘  └─────────┘              │
│  └─────────┘                                                            │
│                                                                          │
│  [EXTRACT 1]                              [EXTRACT 2]                   │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Points of Interest

| POI | Описание | Лут | Враги | Особенности |
|-----|----------|-----|-------|-------------|
| **Главный цех** | Огромный цех с 3 уровнями | Very High | 5-8 | Вертикальные бои |
| **Крановая** | Башня крана + мостовые краны | High (редкий) | 2-3 | Снайперская позиция |
| **Котельная** | Трубы, котлы, тёмные углы | Medium-High | 3-4 мутантов | Тесные пространства |
| **Склад** | Контейнерный лабиринт | Medium | 3-5 | Засады |
| **Админ. здание** | Офисы, сейфы | High (ценности) | 2-4 | Документы, ключи |
| **Ж/Д тупик** | Старые вагоны | Medium | 1-2 | Лут в вагонах |
| **Подстанция** | Высокое напряжение! | Very High | 1-2 + ловушки | Электро-ловушки |
| **Насосная** | Подвальное помещение | Medium | 2-3 | Затопление? |

### Уникальные механики Промзоны

```cpp
// Особенности локации
USTRUCT(BlueprintType)
struct FIndustrialHazards
{
    GENERATED_BODY()

    // Электрические ловушки на Подстанции
    UPROPERTY(EditAnywhere)
    TArray<AElectricHazard*> ElectricHazards;

    // Нестабильные платформы
    UPROPERTY(EditAnywhere)
    TArray<AUnstablePlatform*> UnstablePlatforms;

    // Движущиеся краны (cover/опасность)
    UPROPERTY(EditAnywhere)
    TArray<AMovingCrane*> Cranes;

    // Токсичные зоны
    UPROPERTY(EditAnywhere)
    TArray<AToxicZone*> ToxicZones;
};
```

---

## Equipment System (Система экипировки)

### Слоты снаряжения (Tarkov-style)

Система использует 17 слотов экипировки, определённых в `EEquipmentSlotType`:

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    EQUIPMENT SLOTS LAYOUT                                │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌─────────────────┐    ┌───────────┐ ┌────┐                            │
│  │ PRIMARY WEAPON  │    │ HEADWEAR  │ │EAR │   ← Шлем + Наушники        │
│  │   (5x2 cells)   │    │  (2x2)    │ │1x1 │                            │
│  │ AR/DMR/SR/LMG   │    └───────────┘ └────┘                            │
│  └─────────────────┘    ┌────┐ ┌────┐                                   │
│                         │EYE │ │FACE│       ← Очки + Балаклава          │
│  ┌──────────────┐       │1x1 │ │1x1 │                                   │
│  │SECONDARY WPN │       └────┘ └────┘                                   │
│  │  (4x2 cells) │                                                       │
│  │ SMG/PDW/SG   │       ┌──────────────┐ ┌─────────┐                    │
│  └──────────────┘       │  BODY ARMOR  │ │TACT.RIG │  ← Броня + Разгруз │
│                         │    (3x3)     │ │  (2x3)  │                    │
│  ┌────────┐ ┌────┐      └──────────────┘ └─────────┘                    │
│  │HOLSTER │ │SCAB│                                                      │
│  │ (2x2)  │ │1x2 │      ┌──────────────┐ ┌─────────┐ ┌────┐             │
│  │Pistol  │ │Knf │      │   BACKPACK   │ │ SECURE  │ │ARM │             │
│  └────────┘ └────┘      │    (3x3)     │ │  (2x2)  │ │BAND│             │
│                         └──────────────┘ └─────────┘ └────┘             │
│                                                                          │
│  ┌────┐ ┌────┐ ┌────┐ ┌────┐                                            │
│  │ Q1 │ │ Q2 │ │ Q3 │ │ Q4 │  ← Quick Slots (Consumable/Medical/Ammo)   │
│  └────┘ └────┘ └────┘ └────┘                                            │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

### Типы слотов и допустимые предметы

| Слот | Размер | Допустимые типы |
|------|--------|-----------------|
| **PrimaryWeapon** | 5x2 | AR, DMR, SR, Shotgun, LMG |
| **SecondaryWeapon** | 4x2 | SMG, PDW, Shotgun |
| **Holster** | 2x2 | Pistol, Revolver |
| **Scabbard** | 1x2 | Melee/Knife |
| **Headwear** | 2x2 | Helmet, Headwear |
| **Earpiece** | 1x1 | Earpiece (тактические наушники) |
| **Eyewear** | 1x1 | Eyewear (очки, визоры) |
| **FaceCover** | 1x1 | FaceCover (балаклава, респиратор) |
| **BodyArmor** | 3x3 | BodyArmor |
| **TacticalRig** | 2x3 | TacticalRig |
| **Backpack** | 3x3 | Backpack |
| **SecureContainer** | 2x2 | SecureContainer (сохраняется при смерти) |
| **QuickSlot 1-4** | 1x1 | Consumable, Medical, Throwable, Ammo |
| **Armband** | 1x1 | Armband (фракционная метка) |

---

## Weapon System (Система оружия)

### Доступное оружие

На основе `SuspenseCoreWeaponAttributes.json`:

| ID | Название | Тип | Калибр | Урон | ROF | Магазин |
|----|----------|-----|--------|------|-----|---------|
| `Weapon_AK74M` | **АК-74М** | Assault Rifle | 5.45x39mm | 42 | 650 | 30 |
| `Weapon_AK103` | **АК-103** | Assault Rifle | 7.62x39mm | 55 | 600 | 30 |
| `Weapon_SVD` | **СВД** | DMR | 7.62x54mmR | 86 | 120 | 10 |
| `Weapon_MP5SD` | **MP5SD** | SMG | 9x19mm | 35 | 800 | 30 |
| `Weapon_MP7A2` | **MP7A2** | PDW | 4.6x30mm | 30 | 950 | 40 |
| `Weapon_Glock17` | **Glock 17** | Pistol | 9x19mm | 35 | 450 | 17 |
| `Weapon_M9A3` | **Beretta M9A3** | Pistol | 9x19mm | 36 | 400 | 17 |
| `Weapon_M9Bayonet` | **Штык-нож M9** | Melee | - | 45 | 80 | - |

### Характеристики оружия

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    WEAPON ATTRIBUTES                                      │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                           │
│  BaseDamage         - Базовый урон                                       │
│  RateOfFire         - Скорострельность (выстр/мин)                       │
│  EffectiveRange     - Эффективная дальность (м)                          │
│  MaxRange           - Максимальная дальность (м)                         │
│  MagazineSize       - Ёмкость магазина                                   │
│  TacticalReloadTime - Перезарядка с патроном в патроннике (сек)          │
│  FullReloadTime     - Полная перезарядка (сек)                           │
│  MOA                - Точность (меньше = лучше)                          │
│  HipFireSpread      - Разброс при стрельбе от бедра                      │
│  AimSpread          - Разброс при прицеливании                           │
│  VerticalRecoil     - Вертикальная отдача                                │
│  HorizontalRecoil   - Горизонтальная отдача                              │
│  Durability         - Текущая прочность (0-100)                          │
│  MaxDurability      - Максимальная прочность                             │
│  MisfireChance      - Шанс осечки                                        │
│  JamChance          - Шанс заклинивания                                  │
│  Ergonomics         - Эргономика (влияет на ADS speed)                   │
│  AimDownSightTime   - Время прицеливания (сек)                           │
│  WeaponWeight       - Вес оружия (кг)                                    │
│                                                                           │
└──────────────────────────────────────────────────────────────────────────┘
```

### Режимы огня (FireModes)

| Оружие | Режимы огня |
|--------|-------------|
| АК-74М | Single, Auto |
| АК-103 | Single, Auto |
| СВД | Single |
| MP5SD | Single, Burst, Auto |
| MP7A2 | Single, Auto |
| Glock 17 | Single |
| M9A3 | Single |

### Weapon Degradation (Износ оружия)

```
┌──────────────────────────────────────────────────────────────────┐
│ WEAPON DURABILITY: 100% ──────────────────────────────────► 0%   │
├──────────────────────────────────────────────────────────────────┤
│ 100-75%  │ Full performance                                      │
│  75-50%  │ -10% accuracy, occasional jam (5%)                    │
│  50-25%  │ -20% accuracy, frequent jam (15%), -10% damage        │
│  25-10%  │ -35% accuracy, constant jam (30%), -25% damage        │
│  10-0%   │ BROKEN - Cannot fire, must repair                     │
└──────────────────────────────────────────────────────────────────┘
```

---

## Ammo System (Система патронов)

### Калибр 5.45x39mm (для АК-74М)

На основе `SuspenseCoreAmmoAttributes.json`:

| ID | Название | Урон | Пробитие | Фрагментация |
|----|----------|------|----------|--------------|
| `Ammo_545x39_PS` | 5.45x39 ПС гс | 42 | 25 | 40% |
| `Ammo_545x39_BP` | 5.45x39 БП | 37 | 45 | 17% |
| `Ammo_545x39_BS` | 5.45x39 БС | 40 | 57 | 15% |
| `Ammo_545x39_T` | 5.45x39 Т (трассер) | 40 | 20 | 25% |
| `Ammo_545x39_HP` | 5.45x39 HP (экспанс.) | 68 | 8 | 85% |

### Калибр 9x19mm (для Glock/M9A3/MP5SD)

| ID | Название | Урон | Пробитие | Фрагментация |
|----|----------|------|----------|--------------|
| `Ammo_9x19_PST` | 9x19 ПСТ гж | 50 | 18 | 15% |
| `Ammo_9x19_AP` | 9x19 AP 6.3 | 45 | 30 | 5% |
| `Ammo_9x19_RIP` | 9x19 RIP | 102 | 2 | 100% |
| `Ammo_9x19_Luger` | 9x19 Luger CCI | 55 | 10 | 20% |

### Атрибуты патронов

```cpp
// Каждый тип патрона влияет на:
BaseDamage           // Базовый урон
ArmorPenetration     // Пробитие брони (0-100)
StoppingPower        // Останавливающая сила
FragmentationChance  // Шанс фрагментации (доп. урон)
MuzzleVelocity       // Начальная скорость пули (м/с)
DragCoefficient      // Коэффициент сопротивления воздуха
EffectiveRange       // Эффективная дальность
AccuracyModifier     // Модификатор точности
RecoilModifier       // Модификатор отдачи
RicochetChance       // Шанс рикошета
TracerVisibility     // Видимость трассера (0.0-1.0)
IncendiaryDamage     // Зажигательный урон
WeaponDegradationRate // Скорость износа оружия
MisfireChance        // Шанс осечки
```

---

## UE 5.5 Optimization (Оптимизация)

### Nanite Configuration

```ini
; Config/DefaultEngine.ini

[/Script/Engine.RendererSettings]
; Nanite - для статической геометрии зданий
r.Nanite=1
r.Nanite.MaxPixelsPerEdge=1
r.Nanite.ViewMeshLODBias=0

; Virtual Shadow Maps
r.Shadow.Virtual.Enable=1
r.Shadow.Virtual.MaxPhysicalPages=2048
```

### Lumen Settings (для небольших карт)

```ini
; Lumen настройки
r.Lumen.TraceMeshSDFs=1
r.Lumen.ScreenProbeGather=1
r.LumenScene.SoftwareRayTracing=1

; Для интерьеров Hub
r.Lumen.Reflections.ScreenSpaceReconstruction=1
```

### HLOD Setup

```
Окраина - HLOD Setup (400x400м):
├── HLOD_0 (полная детализация):      0-50м
├── HLOD_1 (упрощённая геометрия):    50-150м
├── HLOD_2 (сильно упрощённая):       150-300м
└── HLOD_3 (силуэты/impostors):       300-400м

Что использует Nanite:
├── Здания (стены, крыши, полы)
├── Статичные объекты (машины, мусор, контейнеры)
├── Рельеф
└── Крупные пропсы

Что НЕ использует Nanite:
├── Интерактивные объекты (двери, лут-контейнеры)
├── Skeletal meshes (враги, NPC)
├── Мелкие детали с физикой
└── VFX / Particles
```

### Occlusion Culling

```cpp
// Project Settings -> Engine -> Rendering
// Важно для зданий с интерьерами

// В DefaultEngine.ini
[/Script/Engine.RendererSettings]
r.HZBOcclusion=1
r.AllowOcclusionQueries=1

// Precomputed Visibility Volumes
// Размещать в каждом здании для оптимизации interior culling
```

### Distance Culling по типам

| Тип объекта | Min Draw Distance | Max Draw Distance |
|-------------|-------------------|-------------------|
| Здания | 0 | Unlimited (Nanite) |
| Враги | 0 | 200м |
| Лут (мелкий) | 0 | 50м |
| Лут (крупный) | 0 | 100м |
| Decals | 0 | 30м |
| Particles | 0 | 100м |

### Memory Budget (Demo)

```
Target Memory Budget:
├── Hub:       ~500MB (всегда загружен)
├── Окраина:   ~1.5GB (стримится)
├── Промзона:  ~1.2GB (стримится)
└── Shared:    ~300MB (общие ассеты)

Total Peak: ~2.3GB (Hub + один рейд)
```

---

## Skill Progression System (GAS-Based)

### Архитектура навыков

```cpp
// Категории навыков (AttributeSet)
UPROPERTY() float CombatSkill;      // Боевые навыки
UPROPERTY() float SurvivalSkill;    // Выживание
UPROPERTY() float TechnicalSkill;   // Техника
UPROPERTY() float MobilitySkill;    // Мобильность
```

### Прокачка через использование

| Действие | Навык | XP за действие |
|----------|-------|----------------|
| Убийство врага | Combat | 10-50 (по типу) |
| Попадание в голову | Combat | +15 бонус |
| Использование аптечки | Survival | 5 |
| Найден тайник | Survival | 20 |
| Взлом замка | Technical | 15 |
| Ремонт оружия | Technical | 10 |
| Успешный спринт-побег | Mobility | 8 |
| Прыжок через препятствие | Mobility | 3 |

### Уровни навыков и бонусы

```
┌────────────────────────────────────────────────────────────────┐
│ COMBAT SKILL                                                    │
├────────────────────────────────────────────────────────────────┤
│ Lvl 1-10:   Base damage                                        │
│ Lvl 11-20:  +5% damage, -3% recoil                            │
│ Lvl 21-30:  +10% damage, -6% recoil, faster reload            │
│ Lvl 31-40:  +15% damage, -10% recoil, +10% crit chance        │
│ Lvl 41-50:  +20% damage, -15% recoil, +15% crit, elite perks  │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│ SURVIVAL SKILL                                                  │
├────────────────────────────────────────────────────────────────┤
│ Lvl 1-10:   Base healing                                       │
│ Lvl 11-20:  +15% healing efficiency                            │
│ Lvl 21-30:  Slower stamina drain, radiation resistance         │
│ Lvl 31-40:  Detect nearby loot, bonus HP                       │
│ Lvl 41-50:  Second chance (once per raid)                      │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│ TECHNICAL SKILL                                                 │
├────────────────────────────────────────────────────────────────┤
│ Lvl 1-10:   Basic repairs                                      │
│ Lvl 11-20:  -20% repair cost, craft basic items               │
│ Lvl 21-30:  Access locked containers, weapon mods             │
│ Lvl 31-40:  Hack terminals, craft advanced items              │
│ Lvl 41-50:  Disable security systems, craft rare items        │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│ MOBILITY SKILL                                                  │
├────────────────────────────────────────────────────────────────┤
│ Lvl 1-10:   Base movement speed                                │
│ Lvl 11-20:  +5% sprint speed, quieter footsteps               │
│ Lvl 21-30:  +10% sprint, -15% stamina cost                    │
│ Lvl 31-40:  Slide ability, longer sprint duration             │
│ Lvl 41-50:  Wall climb, silent movement                       │
└────────────────────────────────────────────────────────────────┘
```

---

## Enemy Types (Типы врагов)

### Для Demo (3 типа)

| Тип | Оружие | HP | Поведение | Локации |
|-----|--------|----|-----------| ---------|
| **Мародёр** | Pistol/SMG | 100 | Патрулирует, атакует в группах | Везде |
| **Бандит** | AK/Shotgun | 150 | Агрессивный, укрытия | Центральные зоны |
| **Мутант** | Melee | 200 | Быстрый, атакует в ближнем бою | Тоннели, подвалы |

### AI Behavior (базовый)

```cpp
UENUM(BlueprintType)
enum class EEnemyBehavior : uint8
{
    Patrol,      // Ходит по точкам
    Guard,       // Стоит на месте, реагирует на звуки
    Hunt,        // Активно ищет игрока
    Flee,        // Убегает при низком HP
    Ambush       // Ждёт в засаде
};
```

---

## Item Degradation (Анти-абуз система)

### Degradation Types

| Item Type | Degrades By | Rate |
|-----------|-------------|------|
| Weapons | Shots fired | 0.05-0.15%/shot |
| Armor | Damage taken | 1% per 10 HP damage |
| Backpack | Weight carried | 0.1%/kg/minute |
| Tools | Uses | 5-10%/use |
| Medicine | Time (shelf life) | Expires after 5 raids |

### Repair Economy

```
Repair Cost Formula:
Cost = (MaxDurability - CurrentDurability) * BaseCost * QualityMultiplier

Quality Multipliers:
- Scrap:    0.5x cost, max repair to 60%
- Standard: 1.0x cost, max repair to 85%
- Premium:  2.0x cost, max repair to 100%
```

---

## Technical Implementation (GAS Integration)

### New Native Tags Needed

```cpp
namespace SuspenseCoreTags
{
    // Skills
    namespace Skill
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combat);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Survival);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Technical);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mobility);
    }

    // Weapon States
    namespace Weapon
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Jammed);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Broken);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Degraded);
    }

    // Raid States
    namespace Raid
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(InProgress);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Extracted);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Failed);
    }

    // Locations
    namespace Location
    {
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hub);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Outskirts);
        UE_DECLARE_GAMEPLAY_TAG_EXTERN(Industrial);
    }
}
```

### Raid Manager

```cpp
UCLASS()
class ARaidGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    // Время рейда
    UPROPERTY(EditDefaultsOnly, Category = "Raid")
    float MaxRaidTime = 1800.0f;  // 30 минут

    // Текущее время
    UPROPERTY(BlueprintReadOnly, Category = "Raid")
    float CurrentRaidTime = 0.0f;

    // Активные точки эвакуации
    UPROPERTY(BlueprintReadOnly, Category = "Raid")
    TArray<AExtractionPoint*> ActiveExtractionPoints;

    // Активные задания
    UPROPERTY(BlueprintReadOnly, Category = "Raid")
    TArray<FRaidObjective> ActiveObjectives;

    UFUNCTION(BlueprintCallable, Category = "Raid")
    void StartRaid(ERaidLocation Location);

    UFUNCTION(BlueprintCallable, Category = "Raid")
    void EndRaid(bool bExtracted);

    UFUNCTION(BlueprintCallable, Category = "Raid")
    void PlayerDied();
};
```

---

## MVP Scope (Demo Version)

### Phase 1: Core Systems

| Задача | Статус | Приоритет |
|--------|--------|-----------|
| Hub Level (L_Hub_Hideout) | TODO | P0 |
| Level Streaming setup | TODO | P0 |
| Окраина Level (L_Raid_Outskirts) | TODO | P0 |
| Spawn/Extraction system | TODO | P0 |
| Basic AI (Мародёр) | TODO | P0 |
| Raid timer | TODO | P1 |

### Phase 2: Gameplay

| Задача | Статус | Приоритет |
|--------|--------|-----------|
| Loot spawns на Окраине | TODO | P0 |
| 3 enemy types (Мародёр, Бандит, Мутант) | TODO | P1 |
| Weapon durability | TODO | P1 |
| Basic objectives | TODO | P1 |
| Death/Extraction flow | TODO | P0 |

### Phase 3: Second Location

| Задача | Статус | Приоритет |
|--------|--------|-----------|
| Промзона Level (L_Raid_Industrial) | TODO | P2 |
| Industrial hazards | TODO | P2 |
| Vertical gameplay | TODO | P2 |
| Trader NPC в Hub | TODO | P2 |

### Level Checklist

```
L_Hub_Hideout:
├── [ ] Blockout (100x100м interior)
├── [ ] Stash interaction point
├── [ ] Workbench interaction point
├── [ ] Mission board UI trigger
├── [ ] Deploy zone (переход в рейд)
├── [ ] Baked lighting
└── [ ] Nav mesh

L_Raid_Outskirts:
├── [ ] Blockout (400x400м)
├── [ ] 8 POI buildings (blockout)
├── [ ] 2 Spawn points
├── [ ] 2 Extraction points
├── [ ] Enemy spawn volumes (x10)
├── [ ] Loot spawn points (x50+)
├── [ ] Nav mesh
├── [ ] HLOD setup
├── [ ] Nanite meshes
└── [ ] Occlusion volumes
```

---

*Document Version: 3.0*
*Created: 2025-12-27*
*Updated: 2025-12-27 - Added level design, UE 5.5 optimization, detailed locations*
*Architecture: SuspenseCore GAS + EventBus*
*Engine: Unreal Engine 5.5 (No World Partition)*
*Data Sources: SuspenseCoreWeaponAttributes.json, SuspenseCoreAmmoAttributes.json, SuspenseCoreLoadoutSettings.h*
