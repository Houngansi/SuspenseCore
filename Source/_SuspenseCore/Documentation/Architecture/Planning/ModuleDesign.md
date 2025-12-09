# Архитектура модулей

Подробное описание архитектурных решений SuspenseCore plugin.

---

## Принципы архитектуры

### 1. Модульность

Каждый модуль SuspenseCore — это независимый UE Module со своими:
- Public API (Public/)
- Private implementation (Private/)
- Build configuration (.Build.cs)
- Четко определенными зависимостями

**Цель:** Максимальная переиспользуемость и возможность отключения ненужных модулей.

---

### 2. Слабая связность (Loose Coupling)

Модули общаются через:
- **Интерфейсы** (IInteractable, IEquippable, etc.)
- **Delegates/Events** (OnItemAdded, OnAbilityActivated)
- **BridgeSystem** (Service Locator pattern)

**Избегаем:**
- Прямых зависимостей между модулями высокого уровня
- Включения конкретных классов из других модулей в headers
- Tight coupling через singleton

---

### 3. Server Authority

Все критичные операции проходят через сервер:
- Inventory changes
- Ability activation
- Equipment changes
- Combat interactions

**Client** → Request (RPC) → **Server** (validate & execute) → Replicate → **All Clients**

---

### 4. GAS-First Design

Gameplay Ability System — центральная система для:
- Character stats (AttributeSets)
- Abilities (Gameplay Abilities)
- Status effects (Gameplay Effects)
- State management (Gameplay Tags)

**Все модули интегрируются с GAS**, а не создают параллельные системы.

---

## Граф зависимостей модулей

```
                    ┌─────────────────┐
                    │  SuspenseCore   │ (Core utilities, interfaces)
                    └────────┬────────┘
                             │
              ┌──────────────┼──────────────┐
              │              │              │
              ▼              ▼              ▼
      ┌──────────────┐ ┌──────────┐ ┌──────────────┐
      │ BridgeSystem │ │   GAS    │ │ PlayerCore   │
      └──────────────┘ └────┬─────┘ └──────┬───────┘
                            │              │
              ┌─────────────┼──────────────┼───────────┐
              │             │              │           │
              ▼             ▼              ▼           ▼
      ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
      │ Inventory    │ │ Equipment    │ │ Interaction  │
      │ System       │ │ System       │ │ System       │
      └──────┬───────┘ └──────┬───────┘ └──────┬───────┘
             │                │                │
             └────────────────┼────────────────┘
                              │
                              ▼
                      ┌──────────────┐
                      │  UISystem    │
                      └──────────────┘
```

### Правила зависимостей:

1. **SuspenseCore** → Никаких зависимостей на другие модули плагина
2. **BridgeSystem** → Зависит только от SuspenseCore
3. **GAS** → SuspenseCore + GameplayAbilities (engine module)
4. **Mid-level модули** → SuspenseCore + GAS + BridgeSystem
5. **UISystem** → Может зависеть от всех (находится на верхнем уровне)

---

## Описание модулей

### SuspenseCore (Ядро)

**Ответственность:**
- Базовые интерфейсы
- Logging categories
- Core utilities
- Common data structures

**Public API:**
```cpp
// Logging
DECLARE_LOG_CATEGORY_EXTERN(LogSuspenseCore, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogInventory, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogEquipment, Log, All);

// Core interfaces
class ISuspenseModule
{
public:
    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;
};
```

**Зависимости:**
- Core (UE)
- CoreUObject (UE)
- Engine (UE)

---

### BridgeSystem (Межмодульная коммуникация)

**Ответственность:**
- Service Locator для модулей
- Event Bus для cross-module events
- Dependency Injection

**Паттерны:**

#### Service Locator
```cpp
// Register service
UModuleBridge::Get().RegisterService<UInventoryManager>(InventoryManagerInstance);

// Get service
UInventoryManager* InvMgr = UModuleBridge::Get().GetService<UInventoryManager>();
if (InvMgr)
{
    InvMgr->AddItem(...);
}
```

#### Event Bus
```cpp
// Subscribe to event
UEventBus::Get().Subscribe(
    TEXT("Item.Equipped"),
    this,
    &APlayerCharacter::OnAnyItemEquipped
);

// Publish event
FItemEquippedEvent Event;
Event.Item = EquippedItem;
Event.Character = this;
UEventBus::Get().Publish(TEXT("Item.Equipped"), Event);
```

**Зависимости:**
- SuspenseCore

**Цель:** Разорвать циклические зависимости между модулями.

---

### GAS (Gameplay Ability System)

**Ответственность:**
- Custom AbilitySystemComponent
- Base Gameplay Abilities
- AttributeSets (Health, Stamina, etc.)
- GameplayEffects library
- GameplayTags configuration

**Ключевые классы:**

```cpp
// Custom ASC with suspense-specific features
class SUSPENSEGAS_API USuspenseAbilitySystemComponent : public UAbilitySystemComponent
{
    // Enhanced replication
    // Custom ability granting
    // Tag relationship handling
};

// Base attribute set
class SUSPENSEGAS_API USuspenseAttributeSet : public UAttributeSet
{
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health)
    FGameplayAttributeData Health;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth)
    FGameplayAttributeData MaxHealth;

    // ... other attributes
};

// Base ability class
class SUSPENSEGAS_API USuspenseGameplayAbility : public UGameplayAbility
{
    // Cooldown handling
    // Cost checking
    // Network prediction
};
```

**GameplayTags структура:**
```
Ability
  ├─ Ability.Attack
  │   ├─ Ability.Attack.Primary
  │   └─ Ability.Attack.Secondary
  ├─ Ability.Defend
  └─ Ability.Interact

Status
  ├─ Status.Buff
  │   ├─ Status.Buff.Speed
  │   └─ Status.Buff.Damage
  └─ Status.Debuff
      ├─ Status.Debuff.Stun
      └─ Status.Debuff.Slow

Item
  ├─ Item.Weapon
  │   ├─ Item.Weapon.Rifle
  │   └─ Item.Weapon.Pistol
  └─ Item.Consumable
      ├─ Item.Consumable.Health
      └─ Item.Consumable.Ammo
```

**Зависимости:**
- SuspenseCore
- GameplayAbilities
- GameplayTags
- GameplayTasks

---

### PlayerCore (Системы игрока)

**Ответственность:**
- Base character class
- Player controller
- Camera management
- Input handling
- Movement integration

**Ключевые классы:**

```cpp
// Base character with GAS integration
class PLAYERCORE_API APlayerCharacterBase : public ACharacter, public IAbilitySystemInterface
{
public:
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated)
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<USuspenseAttributeSet> AttributeSet;

    // Components from other modules (loosely coupled)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UInventoryComponent> InventoryComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UEquipmentManagerComponent> EquipmentManager;
};
```

**Зависимости:**
- SuspenseCore
- GAS
- InventorySystem (optional, via component)
- EquipmentSystem (optional, via component)

---

### InventorySystem (Инвентарь)

**Ответственность:**
- Item definitions
- Item instances
- Inventory component
- Stack management
- Replication

**Архитектура:**

```
ItemDefinition (data asset)
      │
      │ Creates
      ▼
ItemInstance (runtime)
      │
      │ Stored in
      ▼
InventoryComponent (on Character/Container)
```

**Ключевые классы:**

```cpp
// Data asset: defines item properties
UCLASS(BlueprintType)
class UItemDefinition : public UPrimaryDataAsset
{
    UPROPERTY(EditDefaultsOnly)
    FText ItemName;

    UPROPERTY(EditDefaultsOnly)
    int32 MaxStackSize;

    UPROPERTY(EditDefaultsOnly)
    FGameplayTagContainer ItemTags;
};

// Runtime instance: represents actual item
UCLASS()
class UItemInstance : public UObject
{
    UPROPERTY(Replicated)
    TObjectPtr<const UItemDefinition> ItemDef;

    UPROPERTY(Replicated)
    int32 StackCount;

    UPROPERTY(Replicated)
    FGameplayTagContainer InstanceTags;
};

// Component: manages collection of items
UCLASS()
class UInventoryComponent : public UActorComponent
{
    UPROPERTY(ReplicatedUsing = OnRep_Items)
    TArray<TObjectPtr<UItemInstance>> Items;
};
```

**Replication:**
- Items array реплицируется целиком
- OnRep_Items вызывает UI update
- Server authority для всех операций

**Зависимости:**
- SuspenseCore
- GAS (для GameplayTags)

---

### EquipmentSystem (Экипировка)

**Ответственность:**
- Weapon management
- Equipment slots
- Attachment system
- Equip/unequip logic

**Архитектура:**

```
EquipmentDefinition (ItemDefinition subclass)
      │
      ▼
WeaponInstance/ArmorInstance (ItemInstance subclass)
      │
      │ Managed by
      ▼
EquipmentManagerComponent
      │
      │ Spawns
      ▼
AWeaponActor (physical representation in world)
```

**Ключевые классы:**

```cpp
// Equipment-specific item definition
UCLASS()
class UEquipmentDefinition : public UItemDefinition
{
    UPROPERTY(EditDefaultsOnly)
    TSubclassOf<AActor> EquipmentActorClass;

    UPROPERTY(EditDefaultsOnly)
    FName AttachSocketName;

    UPROPERTY(EditDefaultsOnly)
    TSubclassOf<UGameplayAbility> GrantedAbilities;
};

// Equipment manager
UCLASS()
class UEquipmentManagerComponent : public UActorComponent
{
public:
    // Equip item to slot
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerEquipItem(UItemInstance* Item, EEquipmentSlot Slot);

protected:
    UPROPERTY(ReplicatedUsing = OnRep_EquippedItems)
    TMap<EEquipmentSlot, TObjectPtr<UItemInstance>> EquippedItems;

    // Spawned actors
    UPROPERTY(Replicated)
    TMap<EEquipmentSlot, TObjectPtr<AActor>> EquipmentActors;
};
```

**Интеграция с GAS:**
- При экипировке оружия → Grant abilities
- При снятии → Remove abilities

**Зависимости:**
- SuspenseCore
- GAS
- InventorySystem (items come from inventory)
- PlayerCore (to attach to character)

---

### InteractionSystem (Взаимодействия)

**Ответственность:**
- Interaction interface
- Trace/overlap detection
- Interaction prompts
- Interactable actors

**Архитектура:**

```cpp
// Interface
class IInteractable
{
    virtual void OnInteract(AActor* Interactor) = 0;
    virtual FText GetInteractionPrompt() const = 0;
    virtual bool CanInteract(AActor* Interactor) const = 0;
};

// Component for detecting interactables
UCLASS()
class UInteractionComponent : public UActorComponent
{
public:
    // Called by player input
    void TryInteract();

protected:
    // Find interactable in front of player
    AActor* FindBestInteractable() const;

    // Current best target
    UPROPERTY(ReplicatedUsing = OnRep_CurrentTarget)
    TWeakObjectPtr<AActor> CurrentTarget;
};

// Base interactable actor
UCLASS()
class AInteractableActor : public AActor, public IInteractable
{
    virtual void OnInteract_Implementation(AActor* Interactor) override;
    virtual FText GetInteractionPrompt_Implementation() const override;
};
```

**Примеры использования:**
- Двери (AInteractableDoor)
- Предметы для подбора (APickupItem)
- NPC (AInteractableNPC)
- Terminals (AInteractableTerminal)

**Зависимости:**
- SuspenseCore
- PlayerCore

---

### UISystem (Пользовательский интерфейс)

**Ответственность:**
- HUD management
- Inventory UI
- Equipment UI
- Ability UI
- Interaction prompts

**Архитектура:**

```cpp
// Base HUD
UCLASS()
class USuspenseHUD : public UUserWidget
{
protected:
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UInventoryWidget> InventoryWidget;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UAbilityBarWidget> AbilityBar;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UInteractionPrompt> InteractionPrompt;
};

// Inventory UI
UCLASS()
class UInventoryWidget : public UUserWidget
{
public:
    UFUNCTION(BlueprintCallable)
    void RefreshInventory(const TArray<UItemInstance*>& Items);

protected:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UScrollBox> ItemList;
};
```

**MVVM Pattern:**
- **Model**: UInventoryComponent, UEquipmentManager (game state)
- **ViewModel**: Events/delegates binding Model to View
- **View**: UMG widgets

**Зависимости:**
- SuspenseCore
- UMG, Slate, SlateCore (UE)
- InventorySystem (для inventory UI)
- EquipmentSystem (для equipment UI)
- GAS (для ability UI)

---

## Паттерны взаимодействия

### Pattern 1: Command Pattern для actions

```cpp
// Base command
class IGameplayCommand
{
public:
    virtual void Execute() = 0;
    virtual void Undo() = 0;
};

// Example: Equip command
class FEquipItemCommand : public IGameplayCommand
{
    UEquipmentManager* Manager;
    UItemInstance* Item;
    EEquipmentSlot Slot;

    virtual void Execute() override
    {
        Manager->EquipItem(Item, Slot);
    }

    virtual void Undo() override
    {
        Manager->UnequipSlot(Slot);
    }
};
```

### Pattern 2: Observer Pattern для events

```cpp
// Subject
class UInventoryComponent
{
    FOnItemAdded OnItemAdded; // Multicast delegate

    void AddItem(UItemInstance* Item)
    {
        Items.Add(Item);
        OnItemAdded.Broadcast(Item); // Notify observers
    }
};

// Observer
class UInventoryWidget
{
    void BindToInventory(UInventoryComponent* Inventory)
    {
        Inventory->OnItemAdded.AddDynamic(this, &UInventoryWidget::HandleItemAdded);
    }

    void HandleItemAdded(UItemInstance* Item)
    {
        // Update UI
    }
};
```

### Pattern 3: Factory Pattern для item creation

```cpp
class UItemFactory
{
public:
    static UItemInstance* CreateItem(const UItemDefinition* Def, int32 Count)
    {
        UItemInstance* Instance = NewObject<UItemInstance>();
        Instance->ItemDef = Def;
        Instance->StackCount = FMath::Min(Count, Def->MaxStackSize);
        return Instance;
    }
};
```

---

## Масштабируемость (MMO considerations)

### Replication Graph

Для MMO масштабов используем Replication Graph:

```cpp
// Custom replication graph
class USuspenseReplicationGraph : public UReplicationGraph
{
    // Spatial partitioning for proximity-based replication
    TMap<FVector, TArray<AActor*>> SpatialHash;

    // Always replicate to owner
    TArray<AActor*> AlwaysRelevantActors;

    // Frequency buckets (1Hz, 5Hz, 10Hz, 30Hz)
    TMap<float, TArray<AActor*>> FrequencyBuckets;
};
```

**Правила:**
- Owner's character: 30Hz
- Nearby players: 10Hz
- Distant players: 1Hz
- Items on ground: 5Hz (только если видимы)

### Load Balancing

- Inventory operations batched (max 10 per frame)
- GAS abilities use prediction для снижения latency
- UI updates throttled (max 30 FPS)

---

**Последнее обновление:** 2025-11-23
