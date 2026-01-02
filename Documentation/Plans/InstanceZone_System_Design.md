# Instance Zone System — Технический Дизайн-Документ

## Сектор-17: AAA Server-Authoritative Architecture

**Версия:** 1.0
**Дата:** 2026-01-02
**Движок:** Unreal Engine 5.7
**Архитектура:** Dedicated Server MMO FPS

---

## 1. Анализ Текущей Архитектуры

### 1.1 Существующие Компоненты

```
┌─────────────────────────────────────────────────────────────────┐
│                    ТЕКУЩАЯ АРХИТЕКТУРА                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  USuspenseCoreMapTransitionSubsystem (GameInstanceSubsystem)    │
│  ├── FSuspenseCoreTransitionData                                │
│  │   ├── PlayerId                                               │
│  │   ├── SourceMapName / TargetMapName                          │
│  │   ├── TransitionReason                                       │
│  │   └── CustomData (JSON)                                      │
│  ├── TransitionToGameMap()     → OpenLevel()                    │
│  ├── TransitionToMainMenu()    → OpenLevel()                    │
│  └── Delegates: OnTransitionDataSet, OnMapTransitionBegin       │
│                                                                 │
│  USuspenseCoreProjectSettings (UDeveloperSettings)              │
│  ├── LobbyMap                                                   │
│  ├── CharacterSelectMap                                         │
│  └── DefaultGameMap                                             │
│                                                                 │
│  ASuspenseCoreGameGameMode                                      │
│  └── bUseSeamlessTravel = false (!)                             │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 1.2 Проблемы Текущей Реализации для MMO

| Проблема | Описание | Влияние |
|----------|----------|---------|
| `OpenLevel()` | Клиентский метод, не работает на Dedicated Server | ❌ Критично |
| `GameInstanceSubsystem` | Локален для каждого клиента | ❌ Нет синхронизации |
| Нет Instance Manager | Отсутствует управление инстансами рейдов | ❌ Критично |
| `bUseSeamlessTravel = false` | Полная перезагрузка при переходах | ⚠️ UX проблема |
| Нет Matchmaking | Игроки не могут объединяться в группы | ❌ Критично |

---

## 2. AAA Instance Zone Architecture

### 2.1 Высокоуровневая Архитектура

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         MASTER SERVER CLUSTER                                │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐              │
│  │  Login Server   │  │ Matchmaking     │  │ Instance        │              │
│  │  (Auth/Session) │  │ Service         │  │ Orchestrator    │              │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘              │
│           │                    │                    │                        │
│           └────────────────────┼────────────────────┘                        │
│                                │                                             │
│                    ┌───────────▼───────────┐                                │
│                    │   Redis/Database      │                                │
│                    │   (Session Store)     │                                │
│                    └───────────────────────┘                                │
└─────────────────────────────────────────────────────────────────────────────┘
                                 │
                                 │ (REST API / WebSocket)
                                 ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         GAME SERVER POOL                                     │
│                                                                              │
│  ┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐          │
│  │  HUB SERVER      │  │  RAID INSTANCE   │  │  RAID INSTANCE   │          │
│  │  (Persistent)    │  │  Server #1       │  │  Server #2       │          │
│  │                  │  │                  │  │                  │          │
│  │  L_Hub_Bunker    │  │  L_Raid_Factory  │  │  L_Raid_Labs     │          │
│  │  Max: 64 players │  │  Max: 4 players  │  │  Max: 4 players  │          │
│  └──────────────────┘  └──────────────────┘  └──────────────────┘          │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
                                 │
                                 │ (ClientTravel)
                                 ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                              CLIENTS                                         │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐           │
│  │ Client1 │  │ Client2 │  │ Client3 │  │ Client4 │  │ Client5 │           │
│  └─────────┘  └─────────┘  └─────────┘  └─────────┘  └─────────┘           │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Упрощённая Архитектура (Solo-Developer Friendly)

Для реализации одним разработчиком предлагается упрощённая версия:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    SIMPLIFIED ARCHITECTURE (Phase 1)                         │
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    DEDICATED SERVER                                  │    │
│  │                                                                      │    │
│  │  ┌──────────────────────────────────────────────────────────────┐   │    │
│  │  │  USuspenseCoreInstanceSubsystem (GameInstanceSubsystem)      │   │    │
│  │  │  ├── Manages all instance zones on THIS server               │   │    │
│  │  │  ├── Handles player sessions                                 │   │    │
│  │  │  └── Coordinates Level Streaming                             │   │    │
│  │  └──────────────────────────────────────────────────────────────┘   │    │
│  │                                                                      │    │
│  │  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐                  │    │
│  │  │ PERSISTENT  │  │ STREAMED    │  │ STREAMED    │                  │    │
│  │  │ HUB LEVEL   │  │ INSTANCE 1  │  │ INSTANCE 2  │                  │    │
│  │  │             │◄─┤ (Factory)   │  │ (Labs)      │                  │    │
│  │  │ L_Hub_Bunker│  │             │  │             │                  │    │
│  │  └─────────────┘  └─────────────┘  └─────────────┘                  │    │
│  │                                                                      │    │
│  └──────────────────────────────────────────────────────────────────────┘    │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Ключевые Классы и Компоненты

### 3.1 Иерархия Классов

```
UGameInstanceSubsystem
└── USuspenseCoreInstanceSubsystem          [NEW - Server Instance Manager]
    ├── TMap<FGuid, FInstanceZoneData>      InstanceZones
    ├── TMap<FString, FPlayerSessionData>   PlayerSessions
    └── FOnInstanceStateChanged             Delegate

AGameModeBase
├── ASuspenseCoreMenuGameMode               [EXISTS - Lobby/Menu]
├── ASuspenseCoreGameGameMode               [EXISTS - Base Game]
│   └── ASuspenseCoreHubGameMode            [NEW - Hub Specific]
└── ASuspenseCoreRaidGameMode               [NEW - Raid Instance]

AActor
└── ASuspenseCoreInstanceZone               [NEW - Zone Trigger Volume]
    ├── UBoxComponent                       ZoneBounds
    ├── FName                               TargetLevelName
    ├── EInstanceZoneType                   ZoneType
    └── int32                               MaxPlayers

APlayerController
└── ASuspenseCorePlayerController           [EXISTS]
    └── Server_RequestInstanceTransition()  [NEW RPC]

UActorComponent
└── USuspenseCoreInstanceComponent          [NEW - Player Component]
    ├── FGuid                               CurrentInstanceId
    ├── EPlayerInstanceState                State
    └── Client_OnInstanceTransition()       [NEW RPC]
```

### 3.2 Основные Структуры Данных

```cpp
// ═══════════════════════════════════════════════════════════════════════════
// INSTANCE ZONE DATA
// ═══════════════════════════════════════════════════════════════════════════

UENUM(BlueprintType)
enum class EInstanceZoneType : uint8
{
    Hub,            // Постоянная зона (Бункер Сопротивления)
    Raid,           // Инстанс рейда (Завод, Лаборатории, etc.)
    Extraction,     // Точка эвакуации
    Transition      // Переходная зона
};

UENUM(BlueprintType)
enum class EInstanceState : uint8
{
    Inactive,       // Инстанс не загружен
    Loading,        // Идёт загрузка уровня
    WaitingPlayers, // Ожидание игроков (лобби рейда)
    Active,         // Рейд в процессе
    Extraction,     // Фаза эвакуации
    Completed,      // Рейд завершён
    Failed,         // Рейд провален (все погибли)
    Cleanup         // Выгрузка инстанса
};

UENUM(BlueprintType)
enum class EPlayerInstanceState : uint8
{
    InHub,          // Игрок в хабе
    Queuing,        // В очереди на рейд
    Loading,        // Загрузка инстанса
    InRaid,         // В рейде
    Extracting,     // Эвакуируется
    Dead,           // Погиб в рейде
    Extracted       // Успешно эвакуировался
};

USTRUCT(BlueprintType)
struct FInstanceZoneData
{
    GENERATED_BODY()

    // Уникальный ID инстанса
    UPROPERTY(BlueprintReadOnly)
    FGuid InstanceId;

    // Имя уровня для стриминга
    UPROPERTY(BlueprintReadOnly)
    FName LevelName;

    // Тип зоны
    UPROPERTY(BlueprintReadOnly)
    EInstanceZoneType ZoneType = EInstanceZoneType::Raid;

    // Текущее состояние
    UPROPERTY(BlueprintReadOnly)
    EInstanceState State = EInstanceState::Inactive;

    // Игроки в инстансе
    UPROPERTY(BlueprintReadOnly)
    TArray<FString> PlayerIds;

    // Максимум игроков
    UPROPERTY(BlueprintReadOnly)
    int32 MaxPlayers = 4;

    // Время создания
    UPROPERTY(BlueprintReadOnly)
    FDateTime CreationTime;

    // Время до автоматического закрытия (рейд-таймер)
    UPROPERTY(BlueprintReadOnly)
    float RaidTimeLimit = 2400.0f; // 40 минут

    // Сложность (влияет на спавн врагов)
    UPROPERTY(BlueprintReadOnly)
    int32 DifficultyTier = 1;
};

USTRUCT(BlueprintType)
struct FPlayerSessionData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString PlayerId;

    UPROPERTY(BlueprintReadOnly)
    FGuid CurrentInstanceId;

    UPROPERTY(BlueprintReadOnly)
    EPlayerInstanceState State = EPlayerInstanceState::InHub;

    UPROPERTY(BlueprintReadOnly)
    FVector LastHubPosition;

    UPROPERTY(BlueprintReadOnly)
    FRotator LastHubRotation;

    // Инвентарь для рейда (сериализованный JSON)
    UPROPERTY(BlueprintReadOnly)
    FString RaidLoadout;

    // Добыча из рейда
    UPROPERTY(BlueprintReadOnly)
    FString ExtractedLoot;
};
```

---

## 4. Flow Диаграммы

### 4.1 Полный Игровой Цикл

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         GAME FLOW: HUB → RAID → EXTRACTION                   │
└─────────────────────────────────────────────────────────────────────────────┘

[КЛИЕНТ]                           [DEDICATED SERVER]
    │                                      │
    │  1. Connect to Server                │
    ├─────────────────────────────────────►│
    │                                      │ PostLogin()
    │                                      │ CreatePlayerSession()
    │                                      │
    │  2. Spawn in Hub                     │
    │◄─────────────────────────────────────┤
    │                                      │
    │  ════════════════════════════════════│═══════════════════════════════
    │           PLAYER IN HUB              │
    │  ════════════════════════════════════│═══════════════════════════════
    │                                      │
    │  3. Approach Raid Terminal           │
    │  (Overlap with ASuspenseCoreInstanceZone)
    │                                      │
    │  4. Select Raid + Loadout            │
    │  (UI: Raid Selection Widget)         │
    │                                      │
    │  5. Server_RequestRaidQueue()        │
    ├─────────────────────────────────────►│
    │                                      │ ValidateLoadout()
    │                                      │ FindOrCreateInstance()
    │                                      │ AddPlayerToInstance()
    │                                      │
    │  6. Client_OnQueueUpdate()           │
    │◄─────────────────────────────────────┤
    │  (Waiting for players / Ready)       │
    │                                      │
    │  ════════════════════════════════════│═══════════════════════════════
    │           RAID LOADING               │
    │  ════════════════════════════════════│═══════════════════════════════
    │                                      │
    │                                      │ LoadStreamLevel(RaidMap)
    │                                      │ SpawnRaidActors()
    │                                      │
    │  7. Client_BeginRaidTransition()     │
    │◄─────────────────────────────────────┤
    │  (Loading screen on client)          │
    │                                      │
    │  8. Server teleports player pawn     │
    │  to raid spawn point                 │
    │◄─────────────────────────────────────┤
    │                                      │
    │  9. Client_OnRaidStarted()           │
    │◄─────────────────────────────────────┤
    │  (Remove loading screen)             │
    │                                      │
    │  ════════════════════════════════════│═══════════════════════════════
    │           RAID IN PROGRESS           │
    │  ════════════════════════════════════│═══════════════════════════════
    │                                      │
    │  10. Player explores, loots, fights  │
    │      (Normal gameplay)               │
    │                                      │
    │  11. Approach Extraction Zone        │
    │  (Overlap with Extraction trigger)   │
    │                                      │
    │  12. Server_RequestExtraction()      │
    ├─────────────────────────────────────►│
    │                                      │ ValidateExtraction()
    │                                      │ SavePlayerLoot()
    │                                      │ UpdatePlayerSession()
    │                                      │
    │  13. Client_BeginExtraction()        │
    │◄─────────────────────────────────────┤
    │  (Extraction countdown UI)           │
    │                                      │
    │  ════════════════════════════════════│═══════════════════════════════
    │           RETURN TO HUB              │
    │  ════════════════════════════════════│═══════════════════════════════
    │                                      │
    │                                      │ RemovePlayerFromInstance()
    │                                      │ TeleportToHub()
    │                                      │
    │  14. Client_OnExtractionComplete()   │
    │◄─────────────────────────────────────┤
    │  (Back in Hub with loot)             │
    │                                      │
    │                                      │ CheckInstanceEmpty()
    │                                      │ if empty: UnloadStreamLevel()
    │                                      │
    └──────────────────────────────────────┘
```

### 4.2 Level Streaming Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         LEVEL STREAMING ARCHITECTURE                         │
└─────────────────────────────────────────────────────────────────────────────┘

                    PERSISTENT LEVEL (Always Loaded)
    ┌───────────────────────────────────────────────────────────────────┐
    │                                                                   │
    │   L_Persistent_World                                              │
    │   ├── GameMode: ASuspenseCoreHubGameMode                          │
    │   ├── USuspenseCoreInstanceSubsystem (manages all instances)      │
    │   ├── Global Managers (Audio, Weather, Time of Day)               │
    │   └── Streaming Volume Triggers                                   │
    │                                                                   │
    │   ┌─────────────────────────────────────────────────────────────┐ │
    │   │  STREAMING LEVEL: L_Hub_Bunker (Always Visible)             │ │
    │   │  ├── Hub Geometry & Props                                   │ │
    │   │  ├── NPCs (Барахольщик, Левша, Радист)                      │ │
    │   │  ├── Raid Terminals (Instance Zone Triggers)                │ │
    │   │  ├── Stash Access Points                                    │ │
    │   │  └── Player Spawn Points                                    │ │
    │   └─────────────────────────────────────────────────────────────┘ │
    │                                                                   │
    │   ┌─────────────────────────────────────────────────────────────┐ │
    │   │  STREAMING LEVEL: L_Raid_Factory_Instance_001 (Dynamic)     │ │
    │   │  ├── Factory Geometry                                       │ │
    │   │  ├── Enemy Spawn Points                                     │ │
    │   │  ├── Loot Containers                                        │ │
    │   │  ├── Extraction Points                                      │ │
    │   │  └── Instance-specific state                                │ │
    │   └─────────────────────────────────────────────────────────────┘ │
    │                                                                   │
    │   ┌─────────────────────────────────────────────────────────────┐ │
    │   │  STREAMING LEVEL: L_Raid_Labs_Instance_002 (Dynamic)        │ │
    │   │  └── ... (loaded when players enter)                        │ │
    │   └─────────────────────────────────────────────────────────────┘ │
    │                                                                   │
    └───────────────────────────────────────────────────────────────────┘
```

---

## 5. Технические Детали Реализации

### 5.1 USuspenseCoreInstanceSubsystem

```cpp
// ═══════════════════════════════════════════════════════════════════════════
// FILE: Source/BridgeSystem/Public/SuspenseCore/Subsystems/SuspenseCoreInstanceSubsystem.h
// ═══════════════════════════════════════════════════════════════════════════

UCLASS()
class BRIDGESYSTEM_API USuspenseCoreInstanceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ═══════════════════════════════════════════════════════════════════════
    // INSTANCE MANAGEMENT (Server Only)
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Создать новый инстанс рейда
     * @param LevelName - имя уровня для стриминга (e.g., "L_Raid_Factory")
     * @param ZoneType - тип зоны
     * @param MaxPlayers - максимум игроков
     * @return ID созданного инстанса
     */
    UFUNCTION(BlueprintCallable, Category = "Instance")
    FGuid CreateInstance(FName LevelName, EInstanceZoneType ZoneType, int32 MaxPlayers = 4);

    /**
     * Найти существующий инстанс с местом или создать новый
     */
    UFUNCTION(BlueprintCallable, Category = "Instance")
    FGuid FindOrCreateInstance(FName LevelName, int32 MaxPlayers = 4);

    /**
     * Получить данные инстанса
     */
    UFUNCTION(BlueprintCallable, Category = "Instance")
    bool GetInstanceData(FGuid InstanceId, FInstanceZoneData& OutData) const;

    /**
     * Удалить инстанс (выгрузить уровень)
     */
    UFUNCTION(BlueprintCallable, Category = "Instance")
    void DestroyInstance(FGuid InstanceId);

    // ═══════════════════════════════════════════════════════════════════════
    // PLAYER SESSION MANAGEMENT
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Создать сессию игрока при подключении
     */
    UFUNCTION(BlueprintCallable, Category = "Session")
    void CreatePlayerSession(const FString& PlayerId);

    /**
     * Добавить игрока в инстанс
     */
    UFUNCTION(BlueprintCallable, Category = "Session")
    bool AddPlayerToInstance(const FString& PlayerId, FGuid InstanceId);

    /**
     * Убрать игрока из инстанса
     */
    UFUNCTION(BlueprintCallable, Category = "Session")
    void RemovePlayerFromInstance(const FString& PlayerId);

    /**
     * Получить сессию игрока
     */
    UFUNCTION(BlueprintCallable, Category = "Session")
    bool GetPlayerSession(const FString& PlayerId, FPlayerSessionData& OutSession) const;

    /**
     * Обновить состояние игрока
     */
    UFUNCTION(BlueprintCallable, Category = "Session")
    void UpdatePlayerState(const FString& PlayerId, EPlayerInstanceState NewState);

    // ═══════════════════════════════════════════════════════════════════════
    // LEVEL STREAMING
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * Загрузить уровень инстанса (асинхронно)
     */
    UFUNCTION(BlueprintCallable, Category = "Streaming")
    void LoadInstanceLevel(FGuid InstanceId);

    /**
     * Выгрузить уровень инстанса
     */
    UFUNCTION(BlueprintCallable, Category = "Streaming")
    void UnloadInstanceLevel(FGuid InstanceId);

    // ═══════════════════════════════════════════════════════════════════════
    // DELEGATES
    // ═══════════════════════════════════════════════════════════════════════

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInstanceStateChanged,
        FGuid, InstanceId, EInstanceState, NewState);

    UPROPERTY(BlueprintAssignable)
    FOnInstanceStateChanged OnInstanceStateChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerStateChanged,
        FString, PlayerId, EPlayerInstanceState, NewState);

    UPROPERTY(BlueprintAssignable)
    FOnPlayerStateChanged OnPlayerStateChanged;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInstanceLevelLoaded,
        FGuid, InstanceId);

    UPROPERTY(BlueprintAssignable)
    FOnInstanceLevelLoaded OnInstanceLevelLoaded;

protected:
    // Активные инстансы
    UPROPERTY()
    TMap<FGuid, FInstanceZoneData> InstanceZones;

    // Сессии игроков
    UPROPERTY()
    TMap<FString, FPlayerSessionData> PlayerSessions;

    // Загруженные уровни (InstanceId -> LevelStreamingDynamic)
    UPROPERTY()
    TMap<FGuid, ULevelStreamingDynamic*> LoadedLevels;

private:
    void OnLevelLoaded(FGuid InstanceId);
    void OnLevelUnloaded(FGuid InstanceId);
    void CleanupEmptyInstances();
};
```

### 5.2 Server RPC Flow

```cpp
// ═══════════════════════════════════════════════════════════════════════════
// ASuspenseCorePlayerController - Server RPCs
// ═══════════════════════════════════════════════════════════════════════════

// Клиент запрашивает вход в рейд
UFUNCTION(Server, Reliable)
void Server_RequestRaidQueue(FName RaidLevelName, const FString& LoadoutJson);

// Клиент запрашивает эвакуацию
UFUNCTION(Server, Reliable)
void Server_RequestExtraction(FGuid InstanceId);

// Клиент отменяет очередь
UFUNCTION(Server, Reliable)
void Server_CancelQueue();

// ═══════════════════════════════════════════════════════════════════════════
// ASuspenseCorePlayerController - Client RPCs
// ═══════════════════════════════════════════════════════════════════════════

// Сервер уведомляет о статусе очереди
UFUNCTION(Client, Reliable)
void Client_OnQueueUpdate(EQueueStatus Status, int32 PlayersInQueue, int32 MaxPlayers);

// Сервер начинает переход в рейд
UFUNCTION(Client, Reliable)
void Client_BeginRaidTransition(FGuid InstanceId, FName LevelName);

// Сервер уведомляет о старте рейда
UFUNCTION(Client, Reliable)
void Client_OnRaidStarted(FGuid InstanceId, float RaidTimeLimit);

// Сервер начинает эвакуацию
UFUNCTION(Client, Reliable)
void Client_BeginExtraction(float CountdownSeconds);

// Сервер завершает эвакуацию
UFUNCTION(Client, Reliable)
void Client_OnExtractionComplete(const FString& LootSummaryJson);

// Сервер уведомляет о смерти в рейде
UFUNCTION(Client, Reliable)
void Client_OnRaidDeath();
```

### 5.3 Level Streaming Implementation

```cpp
// ═══════════════════════════════════════════════════════════════════════════
// LEVEL STREAMING HELPERS
// ═══════════════════════════════════════════════════════════════════════════

void USuspenseCoreInstanceSubsystem::LoadInstanceLevel(FGuid InstanceId)
{
    FInstanceZoneData* Instance = InstanceZones.Find(InstanceId);
    if (!Instance)
    {
        UE_LOG(LogInstance, Error, TEXT("LoadInstanceLevel: Instance %s not found"),
            *InstanceId.ToString());
        return;
    }

    // Обновляем состояние
    Instance->State = EInstanceState::Loading;
    OnInstanceStateChanged.Broadcast(InstanceId, EInstanceState::Loading);

    // Формируем уникальное имя для инстанса
    FString UniqueLevelName = FString::Printf(TEXT("%s_Instance_%s"),
        *Instance->LevelName.ToString(),
        *InstanceId.ToString().Left(8));

    // Путь к уровню
    FString LevelPath = FString::Printf(TEXT("/Game/Maps/Raids/%s"),
        *Instance->LevelName.ToString());

    // Создаём динамический стриминг
    bool bSuccess = false;
    ULevelStreamingDynamic* StreamingLevel = ULevelStreamingDynamic::LoadLevelInstance(
        GetWorld(),
        LevelPath,
        FVector::ZeroVector,    // Location offset
        FRotator::ZeroRotator,  // Rotation offset
        bSuccess
    );

    if (bSuccess && StreamingLevel)
    {
        // Сохраняем ссылку
        LoadedLevels.Add(InstanceId, StreamingLevel);

        // Подписываемся на завершение загрузки
        StreamingLevel->OnLevelLoaded.AddDynamic(this, &ThisClass::HandleLevelLoaded);

        // Устанавливаем видимость
        StreamingLevel->SetShouldBeLoaded(true);
        StreamingLevel->SetShouldBeVisible(true);

        UE_LOG(LogInstance, Log, TEXT("LoadInstanceLevel: Started loading %s for instance %s"),
            *LevelPath, *InstanceId.ToString());
    }
    else
    {
        UE_LOG(LogInstance, Error, TEXT("LoadInstanceLevel: Failed to create streaming level %s"),
            *LevelPath);
        Instance->State = EInstanceState::Failed;
        OnInstanceStateChanged.Broadcast(InstanceId, EInstanceState::Failed);
    }
}

void USuspenseCoreInstanceSubsystem::UnloadInstanceLevel(FGuid InstanceId)
{
    ULevelStreamingDynamic** StreamingLevel = LoadedLevels.Find(InstanceId);
    if (!StreamingLevel || !*StreamingLevel)
    {
        return;
    }

    // Обновляем состояние
    if (FInstanceZoneData* Instance = InstanceZones.Find(InstanceId))
    {
        Instance->State = EInstanceState::Cleanup;
        OnInstanceStateChanged.Broadcast(InstanceId, EInstanceState::Cleanup);
    }

    // Выгружаем уровень
    (*StreamingLevel)->SetShouldBeLoaded(false);
    (*StreamingLevel)->SetShouldBeVisible(false);

    // Удаляем из карты
    LoadedLevels.Remove(InstanceId);

    UE_LOG(LogInstance, Log, TEXT("UnloadInstanceLevel: Unloaded instance %s"),
        *InstanceId.ToString());
}
```

---

## 6. Последовательность Реализации (Pipeline)

### Phase 1: Core Infrastructure (Неделя 1-2)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ PHASE 1: CORE INFRASTRUCTURE                                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ 1.1 Создать USuspenseCoreInstanceSubsystem                                  │
│     ├── FInstanceZoneData struct                                            │
│     ├── FPlayerSessionData struct                                           │
│     ├── CreateInstance() / DestroyInstance()                                │
│     ├── CreatePlayerSession() / UpdatePlayerState()                         │
│     └── Basic delegates                                                     │
│                                                                              │
│ 1.2 Расширить ASuspenseCorePlayerController                                 │
│     ├── Server_RequestRaidQueue()                                           │
│     ├── Server_RequestExtraction()                                          │
│     ├── Client_BeginRaidTransition()                                        │
│     └── Client_OnExtractionComplete()                                       │
│                                                                              │
│ 1.3 Создать ASuspenseCoreInstanceZone (Actor)                               │
│     ├── UBoxComponent для триггера                                          │
│     ├── OnOverlapBegin/End                                                  │
│     └── Blueprint-настраиваемые параметры                                   │
│                                                                              │
│ 1.4 Создать ASuspenseCoreHubGameMode                                        │
│     └── Наследует ASuspenseCoreGameGameMode                                 │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Phase 2: Level Streaming (Неделя 3-4)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ PHASE 2: LEVEL STREAMING                                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ 2.1 Реализовать LoadInstanceLevel() / UnloadInstanceLevel()                 │
│     ├── ULevelStreamingDynamic integration                                  │
│     ├── Async loading callbacks                                             │
│     └── Instance-specific level naming                                      │
│                                                                              │
│ 2.2 Создать тестовые уровни                                                 │
│     ├── L_Persistent_World (Persistent Level)                               │
│     ├── L_Hub_Bunker (Streaming - Hub)                                      │
│     └── L_Raid_TestFactory (Streaming - Test Raid)                          │
│                                                                              │
│ 2.3 Реализовать телепортацию между зонами                                   │
│     ├── Hub → Raid spawn points                                             │
│     ├── Raid → Hub return points                                            │
│     └── Player state preservation                                           │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Phase 3: Player Flow (Неделя 5-6)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ PHASE 3: PLAYER FLOW                                                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ 3.1 Реализовать очередь на рейд                                             │
│     ├── Queue state machine                                                 │
│     ├── Timeout handling                                                    │
│     └── Cancel queue logic                                                  │
│                                                                              │
│ 3.2 Реализовать систему эвакуации                                           │
│     ├── Extraction zone triggers                                            │
│     ├── Countdown timer                                                     │
│     ├── Loot transfer to stash                                              │
│     └── Return to hub position                                              │
│                                                                              │
│ 3.3 Реализовать смерть в рейде                                              │
│     ├── Lose carried items                                                  │
│     ├── Insurance system (optional)                                         │
│     └── Respawn in hub                                                      │
│                                                                              │
│ 3.4 UI Integration                                                          │
│     ├── Raid selection widget                                               │
│     ├── Loading screen                                                      │
│     ├── Extraction countdown                                                │
│     └── Raid results screen                                                 │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Phase 4: Polish & Optimization (Неделя 7-8)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ PHASE 4: POLISH & OPTIMIZATION                                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ 4.1 Instance Cleanup                                                        │
│     ├── Auto-cleanup empty instances                                        │
│     ├── Memory management                                                   │
│     └── Garbage collection optimization                                     │
│                                                                              │
│ 4.2 Network Optimization                                                    │
│     ├── Relevancy settings per instance                                     │
│     ├── Network LOD                                                         │
│     └── Bandwidth optimization                                              │
│                                                                              │
│ 4.3 Error Handling                                                          │
│     ├── Disconnect during raid                                              │
│     ├── Server crash recovery                                               │
│     └── Client reconnection                                                 │
│                                                                              │
│ 4.4 Testing                                                                 │
│     ├── Automated tests                                                     │
│     ├── Load testing                                                        │
│     └── Edge case validation                                                │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Конфигурация Проекта

### 7.1 Расширение USuspenseCoreProjectSettings

```cpp
// Добавить в SuspenseCoreProjectSettings.h

/** Путь к Persistent Level */
UPROPERTY(Config, EditAnywhere, Category = "Maps|Instance")
FSoftObjectPath PersistentWorldMap;

/** Путь к уровню Хаба */
UPROPERTY(Config, EditAnywhere, Category = "Maps|Instance")
FSoftObjectPath HubLevelMap;

/** Доступные рейдовые карты */
UPROPERTY(Config, EditAnywhere, Category = "Maps|Instance")
TArray<FSoftObjectPath> RaidMaps;

/** Максимум активных инстансов на сервере */
UPROPERTY(Config, EditAnywhere, Category = "Instance")
int32 MaxActiveInstances = 16;

/** Время жизни пустого инстанса (секунды) */
UPROPERTY(Config, EditAnywhere, Category = "Instance")
float EmptyInstanceTimeout = 60.0f;

/** Максимальное время рейда по умолчанию */
UPROPERTY(Config, EditAnywhere, Category = "Instance")
float DefaultRaidTimeLimit = 2400.0f; // 40 минут
```

### 7.2 Структура Папок

```
Content/
├── Maps/
│   ├── Persistent/
│   │   └── L_Persistent_World.umap      ← Главный Persistent Level
│   ├── Hub/
│   │   └── L_Hub_Bunker.umap            ← Бункер Сопротивления
│   ├── Raids/
│   │   ├── L_Raid_Factory.umap          ← Завод "Прогресс"
│   │   ├── L_Raid_Labs.umap             ← Лаборатории
│   │   ├── L_Raid_Docks.umap            ← Доки
│   │   └── L_Raid_Residential.umap      ← Жилой сектор
│   └── Menu/
│       ├── L_MainMenu.umap
│       └── L_CharacterSelect.umap
│
├── Blueprints/
│   ├── GameModes/
│   │   ├── BP_HubGameMode.uasset
│   │   └── BP_RaidGameMode.uasset
│   ├── Actors/
│   │   ├── BP_InstanceZone_RaidTerminal.uasset
│   │   └── BP_InstanceZone_Extraction.uasset
│   └── UI/
│       ├── WBP_RaidSelection.uasset
│       ├── WBP_LoadingScreen.uasset
│       └── WBP_ExtractionCountdown.uasset
```

---

## 8. Сетевая Модель

### 8.1 Authority Model

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         SERVER AUTHORITY MODEL                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  SERVER OWNS (AUTHORITY):                                                    │
│  ├── USuspenseCoreInstanceSubsystem                                         │
│  │   └── All instance state, creation, destruction                          │
│  ├── Player Sessions                                                        │
│  │   └── State transitions, validation                                      │
│  ├── Level Streaming                                                        │
│  │   └── Load/Unload decisions                                              │
│  ├── Player Teleportation                                                   │
│  │   └── Position authority during transitions                              │
│  └── Loot/Inventory                                                         │
│      └── Item spawning, extraction validation                               │
│                                                                              │
│  CLIENT REQUESTS (NO AUTHORITY):                                             │
│  ├── Server_RequestRaidQueue()      → Server validates & processes          │
│  ├── Server_RequestExtraction()     → Server validates & processes          │
│  ├── Server_CancelQueue()           → Server validates & processes          │
│  └── All gameplay actions           → Server validates & replicates         │
│                                                                              │
│  CLIENT RECEIVES (READ ONLY):                                                │
│  ├── Client_OnQueueUpdate()         ← UI feedback                           │
│  ├── Client_BeginRaidTransition()   ← Loading screen trigger                │
│  ├── Client_OnRaidStarted()         ← Gameplay enable                       │
│  └── Client_OnExtractionComplete()  ← Results display                       │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 8.2 Replication Strategy

```cpp
// Инстансы НЕ реплицируются напрямую клиентам
// Клиенты получают только:
// 1. Своё текущее состояние (EPlayerInstanceState)
// 2. ID текущего инстанса
// 3. Информацию о других игроках в том же инстансе

// Network Relevancy:
// - Игроки в разных инстансах НЕ видят друг друга
// - Реализуется через:
//   a) AActor::IsNetRelevantFor() override
//   b) Network Dormancy
//   c) или просто разные Streaming Levels (UE автоматически)

bool ASuspenseCoreCharacter::IsNetRelevantFor(
    const AActor* RealViewer,
    const AActor* ViewTarget,
    const FVector& SrcLocation) const
{
    // Проверяем, что игроки в одном инстансе
    if (const ASuspenseCorePlayerController* MyPC = GetController<ASuspenseCorePlayerController>())
    {
        if (const ASuspenseCorePlayerController* ViewerPC = Cast<ASuspenseCorePlayerController>(RealViewer))
        {
            // Если разные инстансы - не реплицируем
            if (MyPC->GetCurrentInstanceId() != ViewerPC->GetCurrentInstanceId())
            {
                return false;
            }
        }
    }

    return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}
```

---

## 9. Интеграция с Существующими Системами

### 9.1 Связь с SuspenseCoreMapTransitionSubsystem

```cpp
// Существующий SuspenseCoreMapTransitionSubsystem используется для:
// - Menu → Hub transition (регистрация персонажа → игра)
// - Хранение PlayerId между картами

// Новый SuspenseCoreInstanceSubsystem используется для:
// - Hub ↔ Raid transitions (внутри игровой сессии)
// - Instance management
// - Player session tracking

// Они работают вместе:
// 1. Игрок заходит: MapTransitionSubsystem → TransitionToGameMap(PlayerId, HubMap)
// 2. Игрок в хабе: InstanceSubsystem → CreatePlayerSession(PlayerId)
// 3. Игрок идёт в рейд: InstanceSubsystem → AddPlayerToInstance()
// 4. Игрок выходит из игры: MapTransitionSubsystem → TransitionToMainMenu()
```

### 9.2 Интеграция с Save System

```cpp
// При эвакуации из рейда:
void ASuspenseCoreRaidGameMode::OnPlayerExtracted(const FString& PlayerId, const FString& LootJson)
{
    // 1. Получаем сессию игрока
    USuspenseCoreInstanceSubsystem* InstanceSS = USuspenseCoreInstanceSubsystem::Get(this);
    FPlayerSessionData Session;
    InstanceSS->GetPlayerSession(PlayerId, Session);

    // 2. Сохраняем добычу в stash
    USuspenseCoreSaveSubsystem* SaveSS = USuspenseCoreSaveSubsystem::Get(this);
    SaveSS->AddItemsToStash(PlayerId, LootJson);

    // 3. Сохраняем прогресс
    SaveSS->SavePlayerProgress(PlayerId);

    // 4. Обновляем сессию
    Session.ExtractedLoot = LootJson;
    Session.State = EPlayerInstanceState::Extracted;
    InstanceSS->UpdatePlayerSession(PlayerId, Session);
}
```

---

## 10. Checklist для Реализации

### 10.1 Первый Milestone: Базовый Переход Hub → Raid → Hub

- [ ] Создать `USuspenseCoreInstanceSubsystem`
- [ ] Создать `FInstanceZoneData` и `FPlayerSessionData` структуры
- [ ] Реализовать `CreateInstance()` / `DestroyInstance()`
- [ ] Реализовать `AddPlayerToInstance()` / `RemovePlayerFromInstance()`
- [ ] Создать `ASuspenseCoreInstanceZone` actor
- [ ] Добавить Server RPCs в `ASuspenseCorePlayerController`
- [ ] Добавить Client RPCs для обратной связи
- [ ] Реализовать `LoadInstanceLevel()` с `ULevelStreamingDynamic`
- [ ] Создать тестовый Hub level (L_Hub_Test)
- [ ] Создать тестовый Raid level (L_Raid_Test)
- [ ] Протестировать полный цикл: Hub → Raid → Extraction → Hub

### 10.2 Validation Criteria

```
✓ Игрок может войти в рейд из хаба
✓ Уровень рейда загружается динамически
✓ Игрок телепортируется на spawn point рейда
✓ Игрок может дойти до точки эвакуации
✓ При эвакуации игрок возвращается в хаб
✓ Уровень рейда выгружается когда пуст
✓ Множественные инстансы работают параллельно
✓ Игроки в разных инстансах не видят друг друга
✓ Всё работает на Dedicated Server
```

---

## Заключение

Данный документ описывает полную архитектуру Instance Zone System для MMO FPS "Сектор-17". Архитектура спроектирована с учётом:

1. **Server Authority** — все критические решения принимаются сервером
2. **Масштабируемость** — поддержка множества параллельных инстансов
3. **Level Streaming** — динамическая загрузка/выгрузка рейдовых карт
4. **Solo-Developer Friendly** — упрощённая архитектура без внешних сервисов

Следующий шаг: Реализация `USuspenseCoreInstanceSubsystem` и базовых RPCs.
