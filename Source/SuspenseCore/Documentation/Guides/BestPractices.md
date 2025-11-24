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

**Последнее обновление:** 2025-11-23
