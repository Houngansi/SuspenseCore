# SuspenseCore Initialization Guide

## Overview

This guide explains how SuspenseCore systems initialize and how to properly set up your project to use EventBus, ServiceLocator, and the GAS integration.

---

## 1. Architecture Entry Point

### USuspenseCoreEventManager (GameInstanceSubsystem)

The **single entry point** for all SuspenseCore systems:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        INITIALIZATION FLOW                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│   1. Engine Start                                                           │
│      │                                                                      │
│      ▼                                                                      │
│   2. UGameInstance Created                                                  │
│      │                                                                      │
│      ▼                                                                      │
│   3. USuspenseCoreEventManager::Initialize() [AUTOMATIC]                    │
│      │                                                                      │
│      ├──► Creates USuspenseCoreEventBus                                    │
│      │                                                                      │
│      ├──► Creates USuspenseCoreServiceLocator                              │
│      │                                                                      │
│      ├──► Starts Tick delegate for deferred events                         │
│      │                                                                      │
│      └──► Publishes "Event.System.Initialized" to EventBus                 │
│                                                                              │
│   4. World/Level Loaded                                                     │
│      │                                                                      │
│      ▼                                                                      │
│   5. Player Joins (Multiplayer) or Spawns (Singleplayer)                   │
│      │                                                                      │
│      ├──► APlayerController::BeginPlay()                                   │
│      │                                                                      │
│      ├──► APlayerState::BeginPlay()                                        │
│      │        └──► ASC Initialized with AttributeSets                      │
│      │                                                                      │
│      └──► ACharacter::PossessedBy()                                        │
│               └──► Character linked to ASC via PlayerState                 │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Accessing Core Systems

### From Any UObject/Actor

```cpp
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"

// Method 1: Static accessor (RECOMMENDED)
USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this);
if (Manager)
{
    // Access EventBus
    USuspenseCoreEventBus* EventBus = Manager->GetEventBus();

    // Access ServiceLocator
    USuspenseCoreServiceLocator* ServiceLocator = Manager->GetServiceLocator();
}

// Method 2: Through GameInstance
UGameInstance* GI = GetGameInstance();
USuspenseCoreEventManager* Manager = GI->GetSubsystem<USuspenseCoreEventManager>();
```

### From Blueprints

```
Get SuspenseCore Event Manager (WorldContext) → Get Event Bus → Publish/Subscribe
```

---

## 3. PlayerState owns AbilitySystemComponent

### Why PlayerState, not Character?

| Location | Pros | Cons |
|----------|------|------|
| **PlayerState** (✅ Our choice) | Survives death/respawn, Replicates to all clients, Persistent abilities | Slightly more complex setup |
| Character (❌) | Simple setup | ASC destroyed on death, Effects/cooldowns lost |

### ASC Initialization Flow

```cpp
// In ASuspenseCorePlayerState::BeginPlay()
void ASuspenseCorePlayerState::BeginPlay()
{
    Super::BeginPlay();

    // 1. Create AttributeSet if class specified
    if (AttributeSetClass)
    {
        AttributeSet = NewObject<USuspenseCoreAttributeSet>(this, AttributeSetClass);
        AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet);
    }

    // 2. Apply initial attributes effect
    if (InitialAttributesEffect)
    {
        AbilitySystemComponent->ApplyEffectToSelf(InitialAttributesEffect, 1.0f);
    }

    // 3. Grant startup abilities
    for (const auto& Entry : StartupAbilities)
    {
        AbilitySystemComponent->GiveAbility(Entry.AbilityClass, Entry.AbilityLevel);
    }

    // 4. Apply passive effects (regen, etc.)
    for (const auto& EffectClass : PassiveEffects)
    {
        AbilitySystemComponent->ApplyEffectToSelf(EffectClass, 1.0f);
    }
}
```

---

## 4. Character Linking to PlayerState ASC

### In ASuspenseCoreCharacter

```cpp
void ASuspenseCoreCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // Get PlayerState's ASC
    if (APlayerController* PC = Cast<APlayerController>(NewController))
    {
        if (ASuspenseCorePlayerState* PS = PC->GetPlayerState<ASuspenseCorePlayerState>())
        {
            // Cache the ASC reference
            AbilitySystemComponent = PS->GetAbilitySystemComponent();

            // Initialize actor info
            AbilitySystemComponent->InitAbilityActorInfo(PS, this);
        }
    }
}

// IAbilitySystemInterface implementation
UAbilitySystemComponent* ASuspenseCoreCharacter::GetAbilitySystemComponent() const
{
    // Return PlayerState's ASC, not our own
    if (APlayerState* PS = GetPlayerState())
    {
        if (ASuspenseCorePlayerState* SuspensePS = Cast<ASuspenseCorePlayerState>(PS))
        {
            return SuspensePS->GetAbilitySystemComponent();
        }
    }
    return nullptr;
}
```

---

## 5. Registering Custom Services

### Creating a Service

```cpp
// MyCustomService.h
UCLASS()
class UMyCustomService : public UObject, public ISuspenseCoreService
{
    GENERATED_BODY()

public:
    // ISuspenseCoreService Interface
    virtual void InitializeService() override;
    virtual void ShutdownService() override;
    virtual FName GetServiceName() const override { return TEXT("MyCustomService"); }
    virtual bool IsServiceReady() const override { return bInitialized; }

    // Your service methods
    void DoSomething();

private:
    bool bInitialized = false;
};
```

### Registering at Startup

```cpp
// In your module's StartupModule() or a GameInstanceSubsystem::Initialize()
void UMyGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Get EventManager
    USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
    if (!Manager) return;

    // Create and register service
    UMyCustomService* Service = NewObject<UMyCustomService>(this);
    Service->InitializeService();

    Manager->GetServiceLocator()->RegisterService<UMyCustomService>(Service);
}
```

### Retrieving Service

```cpp
// Anywhere in your code
USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this);
if (Manager)
{
    UMyCustomService* Service = Manager->GetServiceLocator()->GetService<UMyCustomService>();
    if (Service && Service->IsServiceReady())
    {
        Service->DoSomething();
    }
}
```

---

## 6. EventBus Usage

### Publishing Events

```cpp
// Simple event
Manager->PublishEvent(
    FGameplayTag::RequestGameplayTag(FName("Event.Player.Spawned")),
    this
);

// Event with data
FSuspenseCoreEventData EventData;
EventData.Source = this;
EventData.NumericValue = 100.0f;
EventData.StringPayload = TEXT("PlayerDied");

Manager->GetEventBus()->Publish(
    FGameplayTag::RequestGameplayTag(FName("Event.Player.Died")),
    EventData
);
```

### Subscribing to Events

```cpp
// In your class constructor or BeginPlay
void AMyActor::BeginPlay()
{
    Super::BeginPlay();

    USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this);
    if (Manager)
    {
        // Subscribe with lambda (C++)
        SubscriptionHandle = Manager->GetEventBus()->SubscribeNative(
            FGameplayTag::RequestGameplayTag(FName("Event.Player.Died")),
            this,
            [this](const FGameplayTag& Tag, const FSuspenseCoreEventData& Data)
            {
                HandlePlayerDied(Data);
            },
            ESuspenseCoreEventPriority::Normal
        );
    }
}

void AMyActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Always unsubscribe!
    if (USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this))
    {
        Manager->GetEventBus()->Unsubscribe(SubscriptionHandle);
    }

    Super::EndPlay(EndPlayReason);
}
```

---

## 7. GameplayTags Hierarchy

### Recommended Tag Structure

```
Event.
├── System.
│   ├── Initialized
│   ├── Shutdown
│   └── WorldChanged
├── GAS.
│   ├── Attribute.
│   │   ├── Health.Changed
│   │   ├── Stamina.Changed
│   │   ├── Shield.Changed
│   │   └── Shield.Broken
│   ├── Ability.
│   │   ├── Activated
│   │   └── Ended
│   └── Effect.
│       ├── Applied
│       └── Removed
├── Player.
│   ├── Spawned
│   ├── Died
│   ├── Respawned
│   └── LevelUp
├── Progression.
│   ├── Experience.Changed
│   ├── LevelUp
│   └── Currency.
│       ├── Soft.Changed
│       └── Hard.Changed
├── Equipment.
│   ├── Equipped
│   ├── Unequipped
│   └── SlotChanged
└── Inventory.
    ├── ItemAdded
    ├── ItemRemoved
    └── SlotChanged
```

### Creating Tags in C++

```cpp
// In your module's startup or a static initializer
UE_DEFINE_GAMEPLAY_TAG(TAG_Event_Player_Died, "Event.Player.Died");
UE_DEFINE_GAMEPLAY_TAG(TAG_Event_GAS_Attribute_Health_Changed, "Event.GAS.Attribute.Health.Changed");
```

### Creating Tags in Config (DefaultGameplayTags.ini)

```ini
[/Script/GameplayTags.GameplayTagsSettings]
+GameplayTagList=(Tag="Event.Player.Died")
+GameplayTagList=(Tag="Event.GAS.Attribute.Health.Changed")
```

---

## 8. Blueprint Setup

### Level Blueprint Example

```
Event BeginPlay
    │
    ▼
Get SuspenseCore Event Manager
    │
    ▼
Is Valid? ──No──► Print "EventManager not found!"
    │
   Yes
    │
    ▼
Get Event Bus
    │
    ▼
Subscribe to "Event.Player.Spawned"
    │
    ▼
Bind to Custom Event "OnPlayerSpawned"
```

### GameMode Blueprint

```
Event OnPostLogin (NewPlayer)
    │
    ▼
Get SuspenseCore Event Manager
    │
    ▼
Publish Event
    ├── Tag: "Event.Player.Connected"
    └── Source: NewPlayer
```

---

## 9. Multiplayer Considerations

### Server Authority

```cpp
// ALWAYS check authority for gameplay-affecting operations
void ASuspenseCoreCharacter::TakeDamage(float Damage, AActor* Instigator)
{
    if (!HasAuthority())
    {
        // Client: Request server to apply damage
        Server_RequestDamage(Damage, Instigator);
        return;
    }

    // Server: Apply damage through GAS
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        // Create damage effect...
    }
}
```

### Replication

- **AttributeSets**: All gameplay attributes are marked `ReplicatedUsing`
- **Events**: Use `EventBus->PublishDeferred()` for events that should batch
- **PlayerState**: Automatically replicates ASC and attributes

---

## 10. Common Setup Checklist

### Project Settings

- [ ] Enable GameplayAbilities plugin
- [ ] Enable EnhancedInput plugin
- [ ] Add SuspenseCore to your project's plugin dependencies

### GameMode

- [ ] Set `DefaultPawnClass` to your Character (or `ASuspenseCoreCharacter`)
- [ ] Set `PlayerStateClass` to `ASuspenseCorePlayerState`
- [ ] Set `PlayerControllerClass` to `ASuspenseCorePlayerController`

### PlayerState Blueprint

- [ ] Create BP inheriting from `ASuspenseCorePlayerState`
- [ ] Set `AttributeSetClass` to your AttributeSet
- [ ] Set `InitialAttributesEffect` to your startup effect
- [ ] Add abilities to `StartupAbilities` array
- [ ] Add regen effects to `PassiveEffects` array

### Character Blueprint

- [ ] Create BP inheriting from `ASuspenseCoreCharacter`
- [ ] Ensure it implements `IAbilitySystemInterface`
- [ ] Do NOT add ASC component (it comes from PlayerState)

---

## 11. Debugging

### Console Commands

```
// Print EventBus stats
SuspenseCore.EventBus.Stats

// Enable event logging
SuspenseCore.EventBus.LogEvents 1

// List registered services
SuspenseCore.Services.List
```

### Logging

```cpp
// Enable verbose logging in your DefaultEngine.ini
[Core.Log]
LogSuspenseCore=Verbose
LogAbilitySystem=Verbose
```

---

*Document Version: 1.0*
*Last Updated: 2025-11-29*
