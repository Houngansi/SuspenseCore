# PlayerCore API Reference

**Модуль:** PlayerCore
**Версия:** 2.0.0
**Статус:** Полностью мигрирован, компилируется
**LOC:** ~8,697
**Последнее обновление:** 2025-11-28

---

## Обзор модуля

PlayerCore — центральный модуль, отвечающий за игровой фреймворк: персонажи, контроллеры, состояния игрока, GameMode и GameInstance. Модуль объединяет все подсистемы (GAS, Equipment, Inventory, UI) в единую рабочую систему.

### Архитектурная роль

PlayerCore выступает как "клей" проекта:

1. **Character System** — базовый персонаж с GAS интеграцией, кинематографической камерой, процедурными анимациями
2. **Controller System** — Enhanced Input, маршрутизация ввода в GAS, HUD lifecycle
3. **PlayerState** — ASC хостинг, инвентарь, экипировка, лоадауты
4. **Game Framework** — GameInstance с инициализацией подсистем, GameMode

### Зависимости модуля

```
PlayerCore
├── Core
├── CinematicCamera
├── GameplayAbilities
├── GameplayTags
├── UMG
├── BridgeSystem
├── InventorySystem
├── EquipmentSystem
├── UISystem
└── GAS
```

**Private зависимости:** CoreUObject, Engine, Slate, SlateCore, EnhancedInput

---

## Ключевые компоненты

### 1. ASuspenseCharacter

**Файл:** `Public/Characters/SuspenseCharacter.h`
**Тип:** ACharacter
**Назначение:** Базовый класс персонажа с полной интеграцией всех систем

```cpp
UCLASS()
class PLAYERCORE_API ASuspenseCharacter
    : public ACharacter
    , public ISuspenseCharacterInterface
    , public ISuspenseMovement
```

#### Реализуемые интерфейсы

| Интерфейс | Назначение |
|-----------|------------|
| `ISuspenseCharacterInterface` | Доступ к ASC, оружию, состоянию персонажа |
| `ISuspenseMovement` | Унифицированное управление движением |

#### Компоненты персонажа

```cpp
// First-person mesh (arms)
USkeletalMeshComponent* Mesh1P;

// Camera boom для плавного движения
USpringArmComponent* CameraBoom;

// Cinematic camera с профессиональными настройками
UCineCameraComponent* Camera;

// Custom movement component
USuspenseCharacterMovementComponent* SuspenseMovementComponent;
```

#### Camera System

Кинематографическая камера с продвинутыми настройками:

| Параметр | Тип | Описание |
|----------|-----|----------|
| `CinematicFieldOfView` | float | FOV в градусах (5-170) |
| `CurrentFocalLength` | float | Фокусное расстояние (4-1000mm) |
| `CurrentAperture` | float | Апертура (f/0.7 - f/32) |
| `bEnableDepthOfField` | bool | DOF эффект |
| `ManualFocusDistance` | float | Дистанция фокусировки |
| `DiaphragmBladeCount` | int32 | Лепестки диафрагмы (4-16) |

**Camera API:**
```cpp
void SetCameraFOV(float NewFOV);
void SetCameraFocalLength(float NewFocalLength);
void SetCameraAperture(float NewAperture);
void SetDepthOfFieldEnabled(bool bEnabled);
void SetCameraFocusDistance(float Distance);
void ApplyCinematicPreset(bool bEnableDOF, float Aperture, float FocusDistance);
```

#### Procedural Animation System

Динамические анимации на основе ввода и состояния:

| Параметр | Описание |
|----------|----------|
| `LeanSidesAmount` | Наклон в стороны |
| `LookUpAmount` | Взгляд вверх/вниз |
| `VerticalRecoilAmount` | Вертикальная отдача |
| `HorizontalRecoilAmount` | Горизонтальная отдача |
| `ArmGroupAnimationWeightMultiplier` | Вес анимации рук |

**Animation API:**
```cpp
void UpdateProceduralAnimationValues(float DeltaTime);
float GetAnimationForwardValue() const;  // -2..2 при спринте, -1..1 обычно
float GetAnimationRightValue() const;
bool GetIsJumping() const;
bool GetIsInAir() const;
bool GetIsCrouching() const;
bool GetIsSprinting() const;
ESuspenseMovementMode GetMovementMode() const;
```

#### ISuspenseMovement Implementation

```cpp
// Speed Management
float GetCurrentMovementSpeed_Implementation() const;
void SetMovementSpeed_Implementation(float NewSpeed);
float GetMaxWalkSpeed_Implementation() const;

// Sprint Management
bool CanSprint_Implementation() const;
bool IsSprinting_Implementation() const;
void StartSprinting_Implementation();
void StopSprinting_Implementation();

// Jump Management
void Jump_Implementation();
void StopJumping_Implementation();
bool CanJump_Implementation() const;
bool IsGrounded_Implementation() const;

// Crouch Management
void Crouch_Implementation();
void UnCrouch_Implementation();
bool CanCrouch_Implementation() const;
bool IsCrouching_Implementation() const;

// State via GameplayTags
FGameplayTag GetMovementState_Implementation() const;
void SetMovementState_Implementation(FGameplayTag NewState);
FGameplayTagContainer GetActiveMovementTags_Implementation() const;
```

---

### 2. USuspenseCharacterMovementComponent

**Файл:** `Public/Characters/SuspenseCharacterMovementComponent.h`
**Тип:** UCharacterMovementComponent
**Назначение:** Кастомный компонент движения с GAS интеграцией

```cpp
UCLASS()
class PLAYERCORE_API USuspenseCharacterMovementComponent : public UCharacterMovementComponent
```

#### ESuspenseMovementMode

```cpp
enum class ESuspenseMovementMode : uint8
{
    None,
    Walking,
    Sprinting,
    Crouching,
    Jumping,
    Falling,
    Sliding,
    Swimming,
    Flying
};
```

#### Ключевое API

```cpp
// CRITICAL: единственный способ синхронизировать скорость с AttributeSet
void SyncMovementSpeedFromAttributes();

// Состояния (синхронизированы с GAS тегами)
ESuspenseMovementMode GetCurrentMovementMode() const;
bool IsSprintingFromGAS() const;  // bIsSprintingGAS
bool IsCrouchingFromGAS() const;  // bIsCrouchingGAS
bool IsJumping() const;
bool IsSliding() const;

// Sliding mechanics
void StartSliding();
void StopSliding();
bool CanSlide() const;
```

#### Sliding Parameters

```cpp
float MinSlideSpeed = 400.0f;   // Минимальная скорость для скольжения
float SlideSpeed = 600.0f;      // Скорость скольжения
float SlideFriction = 0.1f;     // Трение при скольжении
float SlideDuration = 1.5f;     // Длительность скольжения
```

---

### 3. ASuspensePlayerController

**Файл:** `Public/Core/SuspensePlayerController.h`
**Тип:** APlayerController
**Назначение:** Контроллер с Enhanced Input, HUD management, GAS маршрутизацией

```cpp
UCLASS()
class PLAYERCORE_API ASuspensePlayerController
    : public APlayerController
    , public ISuspenseController
```

#### HUD Management

```cpp
// Конфигурация
TSubclassOf<UUserWidget> MainHUDClass;  // Установить в Blueprint!
float HUDCreationDelay = 0.1f;
bool bAutoCreateHUD = true;

// API
void CreateHUD();
void DestroyHUD();
UUserWidget* GetHUDWidget() const;
void SetHUDVisibility(bool bShow);
bool IsHUDCreated() const;

// Menu
void ShowInGameMenu();
void HideInGameMenu();
void ToggleInventory();  // Открывает Character Screen
void ShowCharacterScreen(const FGameplayTag& DefaultTab);
```

#### Enhanced Input Configuration

```cpp
// Input Mapping Context
UInputMappingContext* DefaultContext;

// Input Actions
UInputAction* IA_Move;
UInputAction* IA_Look;
UInputAction* IA_Jump;
UInputAction* IA_Sprint;
UInputAction* IA_Crouch;
UInputAction* IA_Interact;
UInputAction* IA_OpenInventory;

// Weapon Switching
UInputAction* IA_NextWeapon;
UInputAction* IA_PrevWeapon;
UInputAction* IA_QuickSwitch;
UInputAction* IA_WeaponSlot1..5;
```

#### ISuspenseController Implementation

```cpp
void NotifyWeaponChanged_Implementation(AActor* NewWeapon);
AActor* GetCurrentWeapon_Implementation() const;
void NotifyWeaponStateChanged_Implementation(FGameplayTag WeaponState);
APawn* GetControlledPawn_Implementation() const;
bool CanUseWeapon_Implementation() const;
bool HasValidPawn_Implementation() const;
void UpdateInputBindings_Implementation();
int32 GetInputPriority_Implementation() const;
USuspenseEventManager* GetDelegateManager() const;
```

#### GAS Input Routing

```cpp
// Активация способности через GAS
void ActivateAbility(const FGameplayTag& Tag, bool bPressed);

// Input handlers автоматически вызывают ActivateAbility
void OnJumpPressed(const FInputActionValue& Value);   // → Jump ability
void OnSprintPressed(const FInputActionValue& Value); // → Sprint ability
void OnCrouchPressed(const FInputActionValue& Value); // → Crouch ability
void OnInteractPressed(const FInputActionValue& Value); // → Interact ability
```

---

### 4. ASuspensePlayerState

**Файл:** `Public/Core/SuspensePlayerState.h`
**Тип:** APlayerState
**Назначение:** Хостинг ASC, инвентаря, экипировки, лоадаутов

```cpp
UCLASS()
class PLAYERCORE_API ASuspensePlayerState
    : public APlayerState
    , public IAbilitySystemInterface
    , public ISuspenseCharacterInterface
    , public ISuspenseAttributeProvider
    , public ISuspenseLoadout
```

#### Архитектура ASC

**Важно:** ASC располагается на PlayerState, а не на Character:
- Позволяет сохранять состояние при respawn
- Правильная репликация в multiplayer
- Поддержка seamless travel

```cpp
// Core Components (Replicated)
UGASAbilitySystemComponent* ASC;
USuspenseInventoryComponent* InventoryComponent;
UGASAttributeSet* Attributes;
```

#### Equipment Module Components

```cpp
// Per-Player Components (все реплицируются)
USuspenseEquipmentDataStore* EquipmentDataStore;
USuspenseEquipmentTransactionProcessor* EquipmentTxnProcessor;
USuspenseEquipmentOperationExecutor* EquipmentOps;
USuspenseEquipmentPredictionSystem* EquipmentPrediction;
USuspenseEquipmentReplicationManager* EquipmentReplication;
USuspenseEquipmentNetworkDispatcher* EquipmentNetworkDispatcher;
USuspenseEquipmentEventDispatcher* EquipmentEventDispatcher;
USuspenseWeaponStateManager* WeaponStateManager;
USuspenseEquipmentInventoryBridge* EquipmentInventoryBridge;
USuspenseEquipmentSlotValidator* EquipmentSlotValidator;
```

#### Loadout System

```cpp
// Конфигурация
FName DefaultLoadoutID = TEXT("Default_Soldier");
bool bAutoApplyDefaultLoadout = true;

// API
bool ApplyLoadout(const FName& LoadoutID, bool bForceReapply = false);
bool SwitchLoadout(const FName& NewLoadoutID, bool bPreserveRuntimeData = false);
FName GetCurrentLoadout() const;
bool HasLoadout() const;
void LogLoadoutStatus();
```

#### GAS Configuration

```cpp
// Attribute initialization
TSubclassOf<UGASAttributeSet> InitialAttributeSetClass;
TSubclassOf<UGameplayEffect> InitialAttributesEffect;

// Abilities
TArray<FAbilityInfo> AbilityPool;  // FAbilityInfo { Ability, InputID }
TSubclassOf<UGameplayAbility> InteractAbility;
TSubclassOf<UGameplayAbility> SprintAbility;
TSubclassOf<UGameplayAbility> CrouchAbility;
TSubclassOf<UGameplayAbility> JumpAbility;
TSubclassOf<UGameplayAbility> WeaponSwitchAbility;

// Passive Effects
TSubclassOf<UGameplayEffect> PassiveHealthRegenEffect;
TSubclassOf<UGameplayEffect> PassiveStaminaRegenEffect;
```

#### Lifecycle

```
1. BeginPlay()
   ├── InitAttributes()
   ├── GrantStartupAbilities()
   ├── ApplyPassiveStartupEffects()
   └── ApplyLoadout(DefaultLoadoutID)
       └── WireEquipmentModule()

2. WireEquipmentModule()
   ├── Connect per-player components
   ├── Wire with global services (via ServiceLocator)
   └── Retry mechanism (max 20 × 50ms)

3. EndPlay()
   └── CleanupComponentListeners()
```

---

### 5. USuspenseGameInstance

**Файл:** `Public/Core/SuspenseGameInstance.h`
**Тип:** UGameInstance
**Назначение:** Инициализация глобальных подсистем

```cpp
UCLASS()
class PLAYERCORE_API USuspenseGameInstance : public UGameInstance
```

#### DataTable Configuration

```cpp
// REQUIRED: Установить в BP_SuspenseGameInstance!
TObjectPtr<UDataTable> LoadoutConfigurationsTable;  // FLoadoutConfiguration
TObjectPtr<UDataTable> WeaponAnimationsTable;       // FAnimationStateData
TObjectPtr<UDataTable> ItemDataTable;               // FSuspenseUnifiedItemData
```

#### Subsystem Access

```cpp
static USuspenseGameInstance* GetSuspenseGameInstance(const UObject* WorldContextObject);
USuspenseLoadoutManager* GetLoadoutManager() const;
UWeaponAnimationSubsystem* GetWeaponAnimationSubsystem() const;
USuspenseItemManager* GetItemManager() const;
FName GetDefaultLoadoutID() const;
```

#### Validation Settings

```cpp
bool bValidateLoadoutsOnStartup = true;
bool bValidateAnimationsOnStartup = true;
bool bValidateItemsOnStartup = true;
bool bStrictItemValidation = true;  // Блокирует запуск при ошибках
```

#### Lifecycle

```
Init()
├── CacheGameVersion()
├── RegisterGlobalEventHandlers()
├── InitializeLoadoutSystem()
├── InitializeAnimationSystem()
└── InitializeItemSystem()
    └── ValidateCriticalItems() // При strict режиме
```

---

## Паттерны использования

### Получение ASC персонажа

```cpp
// Через Character (если нужен avatar)
if (ASuspenseCharacter* Char = Cast<ASuspenseCharacter>(GetPawn()))
{
    UAbilitySystemComponent* ASC = Char->GetASC_Implementation();
}

// Через PlayerState (рекомендуется)
if (ASuspensePlayerState* PS = GetPlayerState<ASuspensePlayerState>())
{
    UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
}
```

### Работа с лоадаутом

```cpp
// В PlayerState
if (HasAuthority())
{
    // Применить новый лоадаут
    if (ApplyLoadout(TEXT("Heavy_Soldier"), true))
    {
        UE_LOG(LogTemp, Log, TEXT("Loadout applied successfully"));
    }

    // Переключить с сохранением runtime данных
    SwitchLoadout(TEXT("Medic_Support"), true);

    // Получить текущий
    FName CurrentLoadout = GetCurrentLoadout();
}
```

### Инициализация HUD

```cpp
// В PlayerController Blueprint
// 1. Установить MainHUDClass = WBP_MainHUD

// В C++ (автоматически при possession)
void ASuspensePlayerController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    // CreateHUD() вызывается автоматически с задержкой
}

// Ручное управление
CreateHUD();
SetHUDVisibility(false);  // Скрыть
DestroyHUD();
```

### Маршрутизация ввода в GAS

```cpp
// Система автоматически делает это через Enhanced Input
void ASuspensePlayerController::OnSprintPressed(const FInputActionValue& Value)
{
    // Активирует Sprint ability через GAS тег
    ActivateAbility(FGameplayTag::RequestGameplayTag("Ability.Movement.Sprint"), true);
}

void ASuspensePlayerController::OnSprintReleased(const FInputActionValue& Value)
{
    ActivateAbility(FGameplayTag::RequestGameplayTag("Ability.Movement.Sprint"), false);
}
```

---

## Риски и потенциальные проблемы

### 1. Сложность ASuspensePlayerState

**Проблема:** PlayerState содержит 10+ компонентов экипировки и реализует 4 интерфейса.

**Последствия:**
- God Object anti-pattern
- Сложность тестирования
- Высокий coupling между системами

**Рекомендации:**
- Вынести Equipment components в отдельный ActorComponent
- Рассмотреть Composition over Inheritance
- Создать PlayerStateBuilder для инициализации

### 2. Retry Mechanism для Equipment Wiring

**Проблема:** `TryWireEquipmentModuleOnce()` использует retry с таймером (20 × 50ms).

**Последствия:**
- Неопределённое время инициализации (до 1 секунды)
- Race conditions при быстром respawn
- Усложнённая отладка

**Рекомендации:**
- Заменить на event-driven инициализацию
- Использовать зависимости через ServiceLocator
- Добавить чёткие состояния lifecycle

### 3. Дублирование интерфейсов на Character и PlayerState

**Проблема:** ISuspenseCharacterInterface реализован и на Character, и на PlayerState.

**Последствия:**
- Неясно какую реализацию использовать
- Потенциальная рассинхронизация данных
- Confusion при отладке

**Рекомендации:**
- Документировать когда использовать какую реализацию
- Рассмотреть delegation от Character к PlayerState

### 4. Жёсткая связь с DataTables

**Проблема:** GameInstance требует 3 DataTable с конкретными Row Structures.

**Последствия:**
- Хрупкая инициализация
- Сложность unit-тестирования
- Невозможность динамической загрузки

**Рекомендации:**
- Добавить fallback значения
- Поддержать загрузку из нескольких источников
- Предоставить mock DataTables для тестов

### 5. Cinematic Camera Overhead

**Проблема:** UCineCameraComponent тяжелее стандартной UCameraComponent.

**Последствия:**
- Увеличенное потребление памяти
- Overhead на расчёт DOF даже когда отключён
- Потенциальные проблемы производительности на слабых GPU

**Рекомендации:**
- Профилировать влияние на FPS
- Рассмотреть отключаемый режим без DOF
- Добавить графические настройки качества

### 6. Отсутствие Server-side Prediction

**Проблема:** CharacterMovementComponent синхронизирует скорость через `SyncMovementSpeedFromAttributes()` каждый тик.

**Последствия:**
- Потенциальные visual jitters при высоком ping
- Неоптимальное использование bandwidth

**Рекомендации:**
- Использовать GAS replication для атрибутов движения
- Рассмотреть custom CharacterMovementComponent с prediction

### 7. Legacy Naming в Комментариях

**Проблема:** Встречаются упоминания MedCom в комментариях и Category.

**Последствия:**
- Путаница при поиске по коду
- Несоответствие naming conventions

**Рекомендации:**
- Провести поиск и замену оставшихся MedCom references
- Обновить Blueprint categories

---

## Метрики модуля

| Метрика | Значение |
|---------|----------|
| Файлов (.h) | 8 |
| Классов | 8 |
| Интерфейсов реализовано | 6 |
| Equipment компонентов в PlayerState | 10 |
| LOC (приблизительно) | 8,697 |

---

## Связь с другими модулями

```
┌─────────────────────────────────────────────────────────┐
│                      PlayerCore                          │
│  ┌─────────┐  ┌──────────────┐  ┌────────────────────┐  │
│  │Character│  │  Controller  │  │    PlayerState     │  │
│  │         │  │ (Input,HUD)  │  │ (ASC,Inv,Equip)    │  │
│  └────┬────┘  └──────┬───────┘  └─────────┬──────────┘  │
└───────┼──────────────┼────────────────────┼─────────────┘
        │              │                    │
        ▼              ▼                    ▼
   ┌─────────┐   ┌───────────┐    ┌─────────────────┐
   │   GAS   │   │ UISystem  │    │EquipmentSystem │
   │(Ability)│   │  (HUD)    │    │(Slots,Weapons) │
   └─────────┘   └───────────┘    └─────────────────┘
                                          │
                                          ▼
                                  ┌─────────────────┐
                                  │InventorySystem │
                                  │   (Items)       │
                                  └─────────────────┘
```

**PlayerCore зависит от всех геймплейных модулей** — это high-level модуль, объединяющий систему.

---

**Дата создания:** 2025-11-28
**Автор:** Tech Lead
**Версия документа:** 1.0
