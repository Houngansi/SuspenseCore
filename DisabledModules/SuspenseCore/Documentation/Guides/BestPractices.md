# Best Practices

Лучшие практики разработки для SuspenseCore plugin.

---

## Общие принципы

### 1. Gameplay Ability System (GAS)

#### ✅ DO: Используйте GAS для геймплейной логики

```cpp
// GOOD: Use GameplayEffect for damage
FGameplayEffectSpecHandle DamageSpec = ASC->MakeOutgoingSpec(
    DamageEffectClass, Level, ASC->MakeEffectContext()
);
ASC->ApplyGameplayEffectSpecToSelf(*DamageSpec.Data.Get());
```

```cpp
// BAD: Direct health manipulation
void ACharacter::TakeDamage(float Amount)
{
    Health -= Amount; // Bypasses GAS, no replication, no events
}
```

**Почему:** GAS обеспечивает автоматическую репликацию, prediction, и integration с остальными системами.

---

#### ✅ DO: Используйте GameplayTags для состояний

```cpp
// GOOD: Tag-based state
if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("Status.Stunned"))))
{
    return; // Can't act while stunned
}
```

```cpp
// BAD: Boolean flags
bool bIsStunned;
bool bIsRooted;
bool bIsSilenced;
// Doesn't scale, hard to replicate, no GAS integration
```

---

### 2. Replication

#### ✅ DO: Всегда думайте о сетевой игре

```cpp
// GOOD: Server authority
UFUNCTION(Server, Reliable, WithValidation)
void ServerEquipWeapon(UItemInstance* Weapon);

bool APlayerCharacter::ServerEquipWeapon_Validate(UItemInstance* Weapon)
{
    return Weapon != nullptr && Weapon->CanBeEquipped();
}

void APlayerCharacter::ServerEquipWeapon_Implementation(UItemInstance* Weapon)
{
    // Authority logic
    CurrentWeapon = Weapon;
    OnRep_CurrentWeapon(); // Call manually on server
}
```

```cpp
// BAD: No server validation
void EquipWeapon(UItemInstance* Weapon)
{
    CurrentWeapon = Weapon; // Client can hack
}
```

---

#### ✅ DO: Используйте OnRep для визуального feedback

```cpp
UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
TObjectPtr<AWeapon> CurrentWeapon;

void ACharacter::OnRep_CurrentWeapon(AWeapon* OldWeapon)
{
    // Visual update only
    if (OldWeapon)
    {
        OldWeapon->OnUnequipped();
    }

    if (CurrentWeapon)
    {
        CurrentWeapon->AttachToComponent(GetMesh(), ...);
        CurrentWeapon->OnEquipped();
    }
}
```

---

#### ✅ DO: Используйте правильные Replication Conditions

```cpp
void APlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Only to owner - sensitive data
    DOREPLIFETIME_CONDITION(APlayerCharacter, Inventory, COND_OwnerOnly);

    // To everyone except owner - third person animations
    DOREPLIFETIME_CONDITION(APlayerCharacter, ThirdPersonAnimState, COND_SkipOwner);

    // To everyone - world state
    DOREPLIFETIME(APlayerCharacter, CurrentWeapon);
}
```

**Условия:**
- `COND_None` — всем (default)
- `COND_OwnerOnly` — только владельцу (inventory, private data)
- `COND_SkipOwner` — всем кроме владельца (third person visuals)
- `COND_InitialOnly` — только при spawn
- `COND_Custom` — custom logic

---

### 3. Performance

#### ✅ DO: Избегайте Tick где возможно

```cpp
// GOOD: Event-driven
void AWeapon::OnEquipped()
{
    GetWorld()->GetTimerManager().SetTimer(
        ReloadTimerHandle,
        this,
        &AWeapon::FinishReload,
        ReloadTime,
        false
    );
}
```

```cpp
// BAD: Polling in Tick
void AWeapon::Tick(float DeltaTime)
{
    if (bIsReloading)
    {
        ReloadTimeRemaining -= DeltaTime;
        if (ReloadTimeRemaining <= 0.0f)
        {
            FinishReload();
        }
    }
}
```

**Почему:** Tick вызывается каждый кадр для всех объектов. Используйте его только когда действительно нужно.

---

#### ✅ DO: Используйте Object Pooling для частых spawns

```cpp
// GOOD: Pool projectiles
class UProjectilePool : public UObject
{
    TArray<AProjectile*> AvailableProjectiles;

    AProjectile* AcquireProjectile()
    {
        if (AvailableProjectiles.Num() > 0)
        {
            return AvailableProjectiles.Pop();
        }
        return SpawnNewProjectile();
    }

    void ReleaseProjectile(AProjectile* Proj)
    {
        Proj->SetActorHiddenInGame(true);
        Proj->SetActorEnableCollision(false);
        AvailableProjectiles.Add(Proj);
    }
};
```

---

#### ✅ DO: Кешируйте компоненты

```cpp
// GOOD: Cache in BeginPlay
void ACharacter::BeginPlay()
{
    Super::BeginPlay();

    // Cache once
    InventoryComponent = FindComponentByClass<UInventoryComponent>();
    AbilitySystemComponent = FindComponentByClass<UAbilitySystemComponent>();
}

void ACharacter::UseItem()
{
    if (InventoryComponent) // Fast pointer check
    {
        InventoryComponent->UseCurrentItem();
    }
}
```

```cpp
// BAD: Find every time
void ACharacter::UseItem()
{
    UInventoryComponent* Inv = FindComponentByClass<UInventoryComponent>();
    if (Inv)
    {
        Inv->UseCurrentItem();
    }
}
```

---

### 4. Code Style

#### ✅ DO: Используйте TObjectPtr для UObject*

```cpp
// GOOD: Modern UE (5.0+)
UPROPERTY()
TObjectPtr<UInventoryComponent> Inventory;

TObjectPtr<AActor> CachedActor;
```

```cpp
// OLD: Legacy style (still works)
UPROPERTY()
UInventoryComponent* Inventory;

AActor* CachedActor;
```

**Почему:** TObjectPtr добавляет safety checks в editor builds и подготовлен для будущих оптимизаций движка.

---

#### ✅ DO: Используйте Forward Declarations

```cpp
// Header file
#pragma once

// Forward declarations (fast compilation)
class UInventoryComponent;
class AWeapon;

class PLAYERCORE_API APlayerCharacter : public ACharacter
{
    UPROPERTY()
    TObjectPtr<UInventoryComponent> Inventory;

    TObjectPtr<AWeapon> CurrentWeapon;
};
```

```cpp
// CPP file
#include "PlayerCharacter.h"
#include "InventoryComponent.h" // Full include only in .cpp
#include "Weapon.h"
```

**Почему:** Уменьшает время компиляции, предотвращает циклические зависимости.

---

#### ✅ DO: Используйте const correctness

```cpp
// GOOD: const-correct API
UFUNCTION(BlueprintPure, Category = "Inventory")
const TArray<UItemInstance*>& GetItems() const;

UFUNCTION(BlueprintPure, Category = "Inventory")
int32 GetItemCount(const UItemDefinition* ItemDef) const;

bool CanEquipItem(const UItemInstance* Item) const;
```

**Почему:** Показывает намерение (функция не модифицирует state), помогает компилятору оптимизировать.

---

### 5. Модульность

#### ✅ DO: Используйте интерфейсы для loose coupling

```cpp
// IInteractable.h
#pragma once

#include "UObject/Interface.h"
#include "IInteractable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractable : public UInterface
{
    GENERATED_BODY()
};

class IInteractable
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    void OnInteract(AActor* Interactor);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    FText GetInteractionPrompt() const;
};
```

```cpp
// Usage
void APlayerCharacter::TryInteract()
{
    AActor* HitActor = PerformInteractionTrace();

    if (HitActor && HitActor->Implements<UInteractable>())
    {
        IInteractable::Execute_OnInteract(HitActor, this);
    }
}
```

---

#### ✅ DO: Используйте delegates для events

```cpp
// InventoryComponent.h
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FOnItemAdded,
    UItemInstance*, Item,
    int32, Count
);

class UInventoryComponent : public UActorComponent
{
    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnItemAdded OnItemAdded;

    bool AddItem(UItemInstance* Item, int32 Count)
    {
        // ... add logic ...

        // Broadcast event
        OnItemAdded.Broadcast(Item, Count);
        return true;
    }
};
```

```cpp
// Usage
void APlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (InventoryComponent)
    {
        InventoryComponent->OnItemAdded.AddDynamic(this, &APlayerCharacter::HandleItemAdded);
    }
}
```

---

### 6. Memory Management

#### ✅ DO: Используйте UPROPERTY для UObject*

```cpp
// GOOD: GC-safe
UPROPERTY()
TObjectPtr<UItemInstance> CachedItem;
```

```cpp
// BAD: Can be garbage collected!
UItemInstance* CachedItem; // No UPROPERTY = potential crash
```

**Почему:** UPROPERTY регистрирует объект в GC, предотвращая преждевременное удаление.

---

#### ✅ DO: Используйте TSharedPtr для не-UObject данных

```cpp
// GOOD: Shared ownership
TSharedPtr<FComplexData> SharedData;
TWeakPtr<FComplexData> WeakRef = SharedData; // No ownership

// Check before use
if (TSharedPtr<FComplexData> Pinned = WeakRef.Pin())
{
    // Use Pinned
}
```

---

### 7. Error Handling

#### ✅ DO: Используйте check/ensure для assertions

```cpp
// check(): Fatal error in dev builds
void Critical_Function(UObject* Obj)
{
    check(Obj != nullptr); // Crash if null (development builds)
    Obj->DoSomething();
}

// ensure(): Non-fatal warning
void Important_Function(UObject* Obj)
{
    if (!ensure(Obj != nullptr))
    {
        // Log error and return safely
        return;
    }
    Obj->DoSomething();
}
```

**Когда использовать:**
- `check()` — для действительно критических ошибок (programmer error)
- `ensure()` — для важных, но recoverable ситуаций
- `verify()` — как check(), но выполняется и в shipping builds

---

#### ✅ DO: Логируйте осмысленные сообщения

```cpp
// GOOD: Informative logging
UE_LOG(LogInventory, Warning,
    TEXT("Failed to add item %s: Inventory full (Current: %d, Max: %d)"),
    *Item->GetName(), CurrentItemCount, MaxItemCount);
```

```cpp
// BAD: Useless logging
UE_LOG(LogTemp, Warning, TEXT("Error")); // What error? Where?
```

---

### 8. Blueprint Integration

#### ✅ DO: Делайте C++ extensible из Blueprint

```cpp
// C++ base class
UCLASS(Abstract, Blueprintable)
class PLAYERCORE_API AWeaponBase : public AActor
{
    GENERATED_BODY()

public:
    // Designers can override in BP
    UFUNCTION(BlueprintNativeEvent, Category = "Weapon")
    void OnFire();

protected:
    virtual void OnFire_Implementation();
};
```

```cpp
void AWeaponBase::OnFire_Implementation()
{
    // Default C++ implementation
    UE_LOG(LogWeapon, Log, TEXT("Weapon fired"));
}
```

**В Blueprint:** Designers могут override `OnFire` и вызвать `Parent::OnFire` если нужно.

---

#### ✅ DO: Используйте BlueprintPure для getters

```cpp
// Pure function: no exec pins in BP, can be called multiple times
UFUNCTION(BlueprintPure, Category = "Inventory")
int32 GetItemCount() const;

// Impure function: has exec pins, should have side effects
UFUNCTION(BlueprintCallable, Category = "Inventory")
void SortInventory();
```

---

### 9. Testing

#### ✅ DO: Пишите тесты для критичной логики

```cpp
// InventoryTests.cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "InventoryComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FInventoryAddItemTest,
    "SuspenseCore.Inventory.AddItem",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter
)

bool FInventoryAddItemTest::RunTest(const FString& Parameters)
{
    // Setup
    UInventoryComponent* Inventory = NewObject<UInventoryComponent>();
    UItemDefinition* ItemDef = CreateTestItemDefinition();

    // Execute
    bool bAdded = Inventory->AddItem(ItemDef, 1);

    // Verify
    TestTrue(TEXT("Item should be added"), bAdded);
    TestEqual(TEXT("Item count should be 1"), Inventory->GetItemCount(ItemDef), 1);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
```

---

## Anti-Patterns (избегайте)

### ❌ DON'T: Сast вместо интерфейсов

```cpp
// BAD
AActor* Actor = GetSomeActor();
APickupActor* Pickup = Cast<APickupActor>(Actor);
if (Pickup)
{
    Pickup->OnPickedUp();
}
```

```cpp
// GOOD
AActor* Actor = GetSomeActor();
if (Actor->Implements<UInteractable>())
{
    IInteractable::Execute_OnInteract(Actor, this);
}
```

---

### ❌ DON'T: GetWorld() в конструкторе

```cpp
// BAD: Crash!
AMyActor::AMyActor()
{
    UWorld* World = GetWorld(); // nullptr in constructor!
    World->SpawnActor(...);
}
```

```cpp
// GOOD
void AMyActor::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld(); // Valid now
    World->SpawnActor(...);
}
```

---

### ❌ DON'T: Хранить сырые указатели на Actors

```cpp
// BAD: Can become invalid
AActor* CachedActor;

void Foo()
{
    CachedActor->DoSomething(); // Crash if actor destroyed!
}
```

```cpp
// GOOD: Use TWeakObjectPtr
TWeakObjectPtr<AActor> WeakActor;

void Foo()
{
    if (AActor* Actor = WeakActor.Get())
    {
        Actor->DoSomething(); // Safe
    }
}
```

---

## Чеклист Code Review

Используйте при проверке кода:

### Архитектура
- [ ] Код в правильном модуле
- [ ] Нет циклических зависимостей
- [ ] Использованы интерфейсы где нужно
- [ ] Loose coupling между системами

### Code Style
- [ ] Следует UE Coding Standard
- [ ] Правильное именование (PascalCase, prefixes)
- [ ] Осмысленные комментарии на английском
- [ ] Нет закомментированного кода

### Performance
- [ ] Нет Tick без необходимости
- [ ] Компоненты кешируются
- [ ] Используется object pooling для spawns
- [ ] Правильные replication conditions

### Replication
- [ ] Server authority для важных операций
- [ ] Validation для server RPCs
- [ ] OnRep функции для визуала
- [ ] Протестировано в multiplayer

### GAS
- [ ] Используется GAS вместо прямых изменений
- [ ] GameplayTags вместо booleans
- [ ] AttributeSets для stats
- [ ] GameplayEffects для модификаций

### Safety
- [ ] check/ensure для assertions
- [ ] Null checks где нужно
- [ ] TWeakObjectPtr для actor references
- [ ] UPROPERTY для UObject pointers

---

## 10. EventBus & GameplayTags

### ⚠️ КРИТИЧНО: Формат GameplayTags для событий

**Все события EventBus ДОЛЖНЫ использовать префикс `SuspenseCore.Event.`**

```cpp
// ✅ ПРАВИЛЬНО: С префиксом SuspenseCore.Event.
FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterSelect.Highlighted"))
FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.Died"))
FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Changed"))

// ❌ НЕПРАВИЛЬНО: Без префикса (события не будут работать!)
FGameplayTag::RequestGameplayTag(FName("Event.UI.CharacterSelect.Highlighted"))  // BROKEN
FGameplayTag::RequestGameplayTag(FName("UI.CharacterSelect.Highlighted"))        // BROKEN
```

**Типичная ошибка:**
Виджеты публикуют события с тегом `Event.UI.*`, а подписчики ожидают `SuspenseCore.Event.UI.*` — события "пропадают" и UI перестаёт работать.

---

### ✅ DO: Используйте макросы для тегов событий

```cpp
// Рекомендуется использовать макросы из SuspenseCoreEventTags.h
#include "SuspenseCore/Events/SuspenseCoreEventTags.h"

// Публикация
EventBus->Publish(SUSPENSE_CORE_EVENT_PLAYER_DIED, EventData);

// Подписка
EventBus->Subscribe(SUSPENSE_CORE_EVENT_UI_WIDGET_OPENED, Callback);
```

**Почему:** Макросы гарантируют правильный формат и кэшируют теги для производительности.

---

### ✅ DO: Проверьте DefaultGameplayTags.ini при добавлении событий

Все теги должны быть определены в `Config/DefaultGameplayTags.ini`:

```ini
; UI События
+GameplayTagList=(Tag="SuspenseCore.Event.UI.Registration.Success",DevComment="Registration success")
+GameplayTagList=(Tag="SuspenseCore.Event.UI.CharacterSelect.Highlighted",DevComment="Character highlighted")
+GameplayTagList=(Tag="SuspenseCore.Event.UI.MainMenu.PlayClicked",DevComment="Play button clicked")

; Player События
+GameplayTagList=(Tag="SuspenseCore.Event.Player.Spawned",DevComment="Player spawned")
+GameplayTagList=(Tag="SuspenseCore.Event.Player.Died",DevComment="Player died")
+GameplayTagList=(Tag="SuspenseCore.Event.Player.ClassChanged",DevComment="Character class changed")
```

---

### ✅ DO: Всегда отписывайтесь от событий

```cpp
// Header
FSuspenseCoreSubscriptionHandle CharacterHighlightedEventHandle;

// NativeConstruct
CharacterHighlightedEventHandle = CachedEventBus->SubscribeNative(
    FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterSelect.Highlighted")),
    this,
    FSuspenseCoreNativeEventCallback::CreateUObject(this, &UMyWidget::OnCharacterHighlighted),
    ESuspenseCoreEventPriority::Normal
);

// NativeDestruct - ОБЯЗАТЕЛЬНО!
if (CachedEventBus.IsValid() && CharacterHighlightedEventHandle.IsValid())
{
    CachedEventBus->Unsubscribe(CharacterHighlightedEventHandle);
}
```

---

### ❌ DON'T: Прямые делегаты между виджетами

```cpp
// ❌ ПЛОХО: Прямая связь между виджетами (tight coupling)
CharacterSelectWidget->OnCharacterHighlightedDelegate.AddDynamic(
    this, &UMainMenuWidget::HandleCharacterHighlighted);

// ✅ ХОРОШО: Через EventBus (loose coupling)
EventBus->SubscribeNative(
    FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.UI.CharacterSelect.Highlighted")),
    this,
    FSuspenseCoreNativeEventCallback::CreateUObject(this, &UMainMenuWidget::OnCharacterHighlighted),
    ESuspenseCoreEventPriority::Normal
);
```

**Почему:** EventBus обеспечивает слабую связность между модулями. Виджеты не знают друг о друге — они знают только о событиях.

---

### Иерархия тегов событий SuspenseCore

```
SuspenseCore.Event
├── System
│   ├── Initialized
│   ├── Shutdown
│   └── Error
│
├── Player
│   ├── Spawned
│   ├── Died
│   ├── ClassChanged
│   └── StateChanged
│
├── UI
│   ├── Registration
│   │   ├── Success
│   │   └── Failed
│   ├── CharacterSelect
│   │   ├── Selected
│   │   ├── Highlighted
│   │   ├── Deleted
│   │   └── CreateNew
│   ├── MainMenu
│   │   └── PlayClicked
│   ├── WidgetOpened
│   ├── WidgetClosed
│   └── Notification
│
├── GAS
│   ├── Ability.*
│   ├── Attribute.*
│   └── Effect.*
│
├── Weapon.*
├── Inventory.*
├── Equipment.*
└── Interaction.*
```

См. полную документацию: `Documentation/Architecture/Planning/EventBus/README.md`

---

---

## 11. ServiceProvider (Dependency Injection) — NEW

### ✅ DO: Используйте ServiceProvider для доступа к сервисам

```cpp
// ✅ ПРАВИЛЬНО: Через ServiceProvider
USuspenseCoreServiceProvider* Provider = USuspenseCoreServiceProvider::Get(WorldContextObject);
if (Provider)
{
    USuspenseCoreEventBus* EventBus = Provider->GetEventBus();
    USuspenseCoreDataManager* DataManager = Provider->GetDataManager();
}

// Или используя макросы
SUSPENSE_GET_EVENTBUS(WorldContextObject, EventBus);
SUSPENSE_GET_DATAMANAGER(WorldContextObject, DataManager);
```

```cpp
// ❌ ПЛОХО: Прямой доступ к подсистемам
UGameInstance* GI = GetGameInstance();
USuspenseCoreDataManager* DataManager = GI->GetSubsystem<USuspenseCoreDataManager>();
```

**Почему:** ServiceProvider обеспечивает правильный порядок инициализации и упрощает тестирование.

---

### ✅ DO: Используйте макросы для публикации событий

```cpp
// Быстрая публикация через макрос
SUSPENSE_PUBLISH_EVENT(WorldContextObject,
    FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.Player.Died")),
    EventData);

// Простое событие без данных
SUSPENSE_PUBLISH_SIMPLE_EVENT(WorldContextObject,
    FGameplayTag::RequestGameplayTag(TEXT("SuspenseCore.Event.System.Ready")));
```

---

### ✅ DO: Реализуйте ISuspenseCoreServiceConsumer для автоинъекции

```cpp
// Компонент с автоматической инъекцией сервисов
UCLASS()
class UMyComponent : public UActorComponent, public ISuspenseCoreServiceConsumer
{
    GENERATED_BODY()

public:
    // ISuspenseCoreServiceConsumer
    virtual void OnServicesReady(USuspenseCoreServiceProvider* Provider) override
    {
        // Сервисы готовы к использованию
        EventBus = Provider->GetEventBus();
        DataManager = Provider->GetDataManager();
    }

private:
    UPROPERTY()
    TObjectPtr<USuspenseCoreEventBus> EventBus;

    UPROPERTY()
    TObjectPtr<USuspenseCoreDataManager> DataManager;
};
```

---

## 12. ReplicationGraph (MMO Scalability) — NEW

### ✅ DO: Используйте правильные Replication Conditions с ReplicationGraph

```cpp
// Инвентарь - только владельцу (OwnerOnly node)
DOREPLIFETIME_CONDITION(APlayerCharacter, InventoryComponent, COND_OwnerOnly);

// Оружие - всем в зоне видимости (SpatialGrid)
DOREPLIFETIME(APlayerCharacter, CurrentWeapon);

// Приватные данные - никому кроме владельца
DOREPLIFETIME_CONDITION(APlayerCharacter, Currency, COND_OwnerOnly);
```

---

### ✅ DO: Поддерживайте Dormancy для оборудования

```cpp
// Equipment Actor с поддержкой Dormancy
void ASuspenseEquipmentActor::OnEquipped()
{
    // Пробудить от Dormancy - началась активность
    FlushNetDormancy();
    SetNetDormancy(DORM_Awake);
}

void ASuspenseEquipmentActor::OnHolstered()
{
    // Разрешить уйти в Dormancy - нет активности
    SetNetDormancy(DORM_DormantAll);
}
```

---

### ✅ DO: Проектируйте классы с учётом ReplicationGraph

```cpp
// ✅ ХОРОШО: Pickup с правильным cull distance
UCLASS()
class ASuspenseCorePickupItem : public AActor
{
    // Подберётся SpatialGrid с PickupCullDistance (50m default)
    // Не будет реплицироваться далёким игрокам
};
```

```cpp
// ❌ ПЛОХО: Pickup, реплицирующийся всем
UCLASS()
class ABadPickupItem : public AActor
{
    virtual bool IsNetRelevantFor(...) override
    {
        return true; // Всем 100 игрокам? Bandwidth disaster!
    }
};
```

---

### ✅ DO: Настройте Project Settings для ReplicationGraph

```ini
; DefaultEngine.ini в игровом проекте
[/Script/OnlineSubsystemUtils.IpNetDriver]
ReplicationDriverClassName="/Script/BridgeSystem.SuspenseCoreReplicationGraph"
```

**Настройки в Project Settings → Game → SuspenseCore Replication:**
- SpatialGridCellSize: 10000 (100m) для outdoor maps
- CharacterCullDistance: 15000 (150m)
- EquipmentDormancyTimeout: 5.0s

---

### Типичные ошибки ReplicationGraph

| Ошибка | Проблема | Решение |
|--------|----------|---------|
| Inventory реплицируется всем | Bandwidth explosion | COND_OwnerOnly + OwnerOnlyNode |
| Equipment никогда не dormant | Постоянная нагрузка | Вызвать SetNetDormancy при holster |
| Pickup виден за 1км | Лишний трафик | Настроить PickupCullDistance |
| PlayerState обновляется 60Hz всем | O(n²) complexity | PlayerStateFrequency buckets |

---

## Иерархия тегов событий (обновлённая)

```
SuspenseCore.Event
├── System
│   ├── Initialized
│   ├── Shutdown
│   └── Error
│
├── Services (NEW)
│   ├── Initialized
│   ├── ServiceRegistered
│   └── ServiceMissing
│
├── Replication (NEW)
│   ├── Initialized
│   ├── ActorAdded
│   ├── ActorRemoved
│   ├── DormancyChanged
│   └── FrequencyBucketChanged
│
├── Player
│   ├── Spawned
│   ├── Died
│   └── ...
│
├── UI / GAS / Weapon / Inventory / Equipment / Interaction
│   └── ...
│
├── Security (NEW)
│   ├── ViolationDetected
│   ├── RateLimitExceeded
│   ├── SuspiciousActivity
│   ├── AuthorityDenied
│   └── PlayerKicked
```

---

## 13. Security Patterns (Server Authority) — NEW

### ✅ DO: Используйте централизованный SecurityValidator

```cpp
// ✅ ПРАВИЛЬНО: Используем SecurityValidator
#include "SuspenseCore/Security/SuspenseCoreSecurityMacros.h"

bool USuspenseCoreInventoryComponent::AddItem(FName ItemID, int32 Quantity)
{
    // Проверка authority - клиент получит false
    SUSPENSE_CHECK_COMPONENT_AUTHORITY(this, AddItem);

    // Rate limiting
    SUSPENSE_CHECK_RATE_LIMIT(GetOwner(), AddItem, 10.0f);

    // Остальная логика выполняется только на сервере
    return AddItemInternal(ItemID, Quantity);
}
```

```cpp
// ❌ ПЛОХО: Прямая модификация без проверки authority
bool USuspenseCoreInventoryComponent::AddItem(FName ItemID, int32 Quantity)
{
    // НЕТ проверки! Клиент может читерить!
    ItemInstances.Add(CreateItem(ItemID, Quantity));
    return true;
}
```

---

### ✅ DO: Используйте Server RPCs с WithValidation

```cpp
// Header
UFUNCTION(Server, Reliable, WithValidation)
void Server_AddItem(FName ItemID, int32 Quantity);

// Implementation - Validation (runs BEFORE the function)
bool UMyComponent::Server_AddItem_Validate(FName ItemID, int32 Quantity)
{
    // Базовая валидация параметров
    return SUSPENSE_VALIDATE_ITEM_RPC(ItemID, Quantity);
}

// Implementation - Actual logic (runs only if validation passes)
void UMyComponent::Server_AddItem_Implementation(FName ItemID, int32 Quantity)
{
    // Это выполняется ТОЛЬКО на сервере
    AddItemInternal(ItemID, Quantity);
}
```

```cpp
// ❌ ПЛОХО: Server RPC без validation
UFUNCTION(Server, Reliable) // Нет WithValidation!
void Server_AddItem(FName ItemID, int32 Quantity);
// Клиент может отправить ItemID = "SuperWeapon", Quantity = 999999
```

---

### ✅ DO: Паттерн "Client calls RPC, Server executes"

```cpp
// Публичный API
bool UInventoryComponent::AddItem(FName ItemID, int32 Quantity)
{
    // Клиент: вызывает RPC и возвращает false
    // Сервер: выполняет логику
    if (!GetOwner()->HasAuthority())
    {
        Server_AddItem(ItemID, Quantity);
        return false; // Клиент возвращает false, сервер обработает
    }

    // Серверная логика
    return AddItemInternal(ItemID, Quantity);
}

// Server RPC
void UInventoryComponent::Server_AddItem_Implementation(FName ItemID, int32 Quantity)
{
    AddItemInternal(ItemID, Quantity);
    // Результат реплицируется клиенту через ReplicatedUsing
}
```

---

### ✅ DO: Валидируйте ВСЕ параметры в _Validate функциях

```cpp
bool UInventoryComponent::Server_MoveItem_Validate(int32 FromSlot, int32 ToSlot)
{
    const int32 MaxSlots = GetGridWidth() * GetGridHeight();

    // 1. Проверка диапазона
    if (!SUSPENSE_VALIDATE_SLOTS(FromSlot, ToSlot, MaxSlots))
    {
        return false;
    }

    // 2. Rate limiting
    USuspenseCoreSecurityValidator* Security = USuspenseCoreSecurityValidator::Get(this);
    if (Security && !Security->CheckRateLimit(GetOwner(), TEXT("MoveItem"), 20.0f))
    {
        return false;
    }

    return true;
}
```

---

### ✅ DO: Используйте Rate Limiting для всех операций

```cpp
// Разные операции имеют разные лимиты
SUSPENSE_CHECK_RATE_LIMIT(GetOwner(), AddItem, 10.0f);      // 10/сек для добавления
SUSPENSE_CHECK_RATE_LIMIT(GetOwner(), MoveItem, 20.0f);     // 20/сек для перемещения
SUSPENSE_CHECK_RATE_LIMIT(GetOwner(), TradeItem, 2.0f);     // 2/сек для торговли (High security)
```

---

### ❌ DON'T: Доверяйте клиенту

```cpp
// ❌ НИКОГДА не делайте так!
void UInventoryComponent::Client_AddItem(FName ItemID, int32 Quantity)
{
    // Клиент сам добавляет предметы - ЭТО ЧИТ!
    ItemInstances.Add(CreateItem(ItemID, Quantity));
}

// ❌ НИКОГДА не доверяйте Client RPC для изменения данных
UFUNCTION(Client, Reliable)
void Client_GiveCurrency(int32 Amount); // Читер отправит 999999999
```

---

### Security Checklist для Code Review

- [ ] Все операции изменения данных имеют Server RPC
- [ ] Все Server RPC имеют WithValidation
- [ ] _Validate функции проверяют ВСЕ параметры
- [ ] Rate limiting используется для предотвращения спама
- [ ] Нет прямых модификаций данных на клиенте
- [ ] CheckAuthority вызывается перед операциями
- [ ] Sensitive операции логируются
- [ ] Нет Client RPC для изменения серверных данных

---

### Иерархия уровней безопасности

| Уровень | Операции | Rate Limit | Kick после |
|---------|----------|------------|-----------|
| Low | Запросы, UI | 30/сек | Никогда |
| Normal | Inventory, Move | 10/сек | 10 нарушений |
| High | Trade, Currency | 2/сек | 5 нарушений |
| Critical | Admin, Ban | 1/сек | 2 нарушения |

---

**Последнее обновление:** 2025-12-05
