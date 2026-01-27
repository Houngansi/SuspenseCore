# EnemySystem Implementation GDD

> **ИНСТРУКЦИЯ:** Прочти этот документ и строго следуй ему.
>
> **Version:** 1.0 FINAL
> **Status:** APPROVED FOR IMPLEMENTATION
> **Target:** Unreal Engine 5.7
> **Module:** EnemySystem

---

## КРИТИЧЕСКИ ВАЖНО

1. **НЕ ИСПОЛЬЗУЙ** legacy код из `DisabledModules/LegasyEnemyCoreSystem/`
2. **НЕ ДОБАВЛЯЙ** комментарии TODO, FIXME, HACK в код
3. **НЕ ИСПОЛЬЗУЙ** алиасы типов (используй полные имена)
4. **СОЗДАВАЙ** файлы СТРОГО в указанном порядке
5. **КОПИРУЙ** код ТОЧНО как написано (не модифицируй)

---

## Содержание

1. [Порядок создания файлов](#порядок-создания-файлов)
2. [Шаг 1: Module Build.cs](#шаг-1-module-buildcs)
3. [Шаг 2: Module Header/CPP](#шаг-2-module-headercpp)
4. [Шаг 3: Native GameplayTags](#шаг-3-native-gameplaytags)
5. [Шаг 4: Enemy State (GAS Host)](#шаг-4-enemy-state-gas-host)
6. [Шаг 5: Enemy Attribute Set](#шаг-5-enemy-attribute-set)
7. [Шаг 6: FSM State Base Class](#шаг-6-fsm-state-base-class)
8. [Шаг 7: FSM Component](#шаг-7-fsm-component)
9. [Шаг 8: Concrete States](#шаг-8-concrete-states)
10. [Шаг 9: Enemy Character](#шаг-9-enemy-character)
11. [Шаг 10: AI Controller](#шаг-10-ai-controller)
12. [Шаг 11: Behavior Data Asset](#шаг-11-behavior-data-asset)
13. [Шаг 12: DefaultGameplayTags.ini](#шаг-12-defaultgameplaytagsini)
14. [Верификация](#верификация)

---

## Порядок создания файлов

```
СТРОГО СОБЛЮДАЙ ПОРЯДОК:

1.  Source/EnemySystem/EnemySystem.Build.cs
2.  Source/EnemySystem/Public/EnemySystem.h
3.  Source/EnemySystem/Private/EnemySystem.cpp
4.  Source/EnemySystem/Public/SuspenseCore/Tags/SuspenseCoreEnemyTags.h
5.  Source/EnemySystem/Private/SuspenseCore/Tags/SuspenseCoreEnemyTags.cpp
6.  Source/EnemySystem/Public/SuspenseCore/Core/SuspenseCoreEnemyState.h
7.  Source/EnemySystem/Private/SuspenseCore/Core/SuspenseCoreEnemyState.cpp
8.  Source/EnemySystem/Public/SuspenseCore/Attributes/SuspenseCoreEnemyAttributeSet.h
9.  Source/EnemySystem/Private/SuspenseCore/Attributes/SuspenseCoreEnemyAttributeSet.cpp
10. Source/EnemySystem/Public/SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h
11. Source/EnemySystem/Private/SuspenseCore/FSM/SuspenseCoreEnemyStateBase.cpp
12. Source/EnemySystem/Public/SuspenseCore/FSM/SuspenseCoreEnemyFSMComponent.h
13. Source/EnemySystem/Private/SuspenseCore/FSM/SuspenseCoreEnemyFSMComponent.cpp
14. Source/EnemySystem/Public/SuspenseCore/FSM/States/SuspenseCoreEnemyIdleState.h
15. Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyIdleState.cpp
16. Source/EnemySystem/Public/SuspenseCore/FSM/States/SuspenseCoreEnemyPatrolState.h
17. Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyPatrolState.cpp
18. Source/EnemySystem/Public/SuspenseCore/FSM/States/SuspenseCoreEnemyChaseState.h
19. Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyChaseState.cpp
20. Source/EnemySystem/Public/SuspenseCore/FSM/States/SuspenseCoreEnemyAttackState.h
21. Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyAttackState.cpp
22. Source/EnemySystem/Public/SuspenseCore/FSM/States/SuspenseCoreEnemyDeathState.h
23. Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyDeathState.cpp
24. Source/EnemySystem/Public/SuspenseCore/Characters/SuspenseCoreEnemy.h
25. Source/EnemySystem/Private/SuspenseCore/Characters/SuspenseCoreEnemy.cpp
26. Source/EnemySystem/Public/SuspenseCore/Controllers/SuspenseCoreEnemyAIController.h
27. Source/EnemySystem/Private/SuspenseCore/Controllers/SuspenseCoreEnemyAIController.cpp
28. Source/EnemySystem/Public/SuspenseCore/Data/SuspenseCoreEnemyBehaviorData.h
29. Source/EnemySystem/Private/SuspenseCore/Data/SuspenseCoreEnemyBehaviorData.cpp
30. Config/DefaultGameplayTags.ini (добавить теги)
```

---

## Шаг 1: Module Build.cs

**Файл:** `Source/EnemySystem/EnemySystem.Build.cs`

```csharp
using UnrealBuildTool;

public class EnemySystem : ModuleRules
{
    public EnemySystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicIncludePaths.AddRange(
            new string[]
            {
                "EnemySystem/Public"
            }
        );

        PrivateIncludePaths.AddRange(
            new string[]
            {
                "EnemySystem/Private"
            }
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "AIModule",
                "NavigationSystem",
                "GameplayTasks",
                "GameplayAbilities",
                "GameplayTags",
                "BridgeSystem",
                "GAS"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Slate",
                "SlateCore"
            }
        );
    }
}
```

---

## Шаг 2: Module Header/CPP

**Файл:** `Source/EnemySystem/Public/EnemySystem.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogEnemySystem, Log, All);

class FEnemySystemModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
```

**Файл:** `Source/EnemySystem/Private/EnemySystem.cpp`

```cpp
#include "EnemySystem.h"

DEFINE_LOG_CATEGORY(LogEnemySystem);

#define LOCTEXT_NAMESPACE "FEnemySystemModule"

void FEnemySystemModule::StartupModule()
{
    UE_LOG(LogEnemySystem, Log, TEXT("EnemySystem module initialized"));
}

void FEnemySystemModule::ShutdownModule()
{
    UE_LOG(LogEnemySystem, Log, TEXT("EnemySystem module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEnemySystemModule, EnemySystem)
```

---

## Шаг 3: Native GameplayTags

**Файл:** `Source/EnemySystem/Public/SuspenseCore/Tags/SuspenseCoreEnemyTags.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace SuspenseCoreEnemyTags
{
    namespace State
    {
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Idle);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Patrol);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Alert);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Chase);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cover);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Retreat);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Death);
    }

    namespace Event
    {
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PlayerDetected);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PlayerLost);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageTaken);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(ReachedDestination);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TargetInRange);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TargetOutOfRange);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(LowHealth);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(AmmoEmpty);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PatrolComplete);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(IdleTimeout);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Died);
    }

    namespace Type
    {
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Scav);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PMC);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss);
        ENEMYSYSTEM_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Raider);
    }
}
```

**Файл:** `Source/EnemySystem/Private/SuspenseCore/Tags/SuspenseCoreEnemyTags.cpp`

```cpp
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"

namespace SuspenseCoreEnemyTags
{
    namespace State
    {
        UE_DEFINE_GAMEPLAY_TAG(Idle, "State.Enemy.Idle");
        UE_DEFINE_GAMEPLAY_TAG(Patrol, "State.Enemy.Patrol");
        UE_DEFINE_GAMEPLAY_TAG(Alert, "State.Enemy.Alert");
        UE_DEFINE_GAMEPLAY_TAG(Chase, "State.Enemy.Chase");
        UE_DEFINE_GAMEPLAY_TAG(Attack, "State.Enemy.Attack");
        UE_DEFINE_GAMEPLAY_TAG(Cover, "State.Enemy.Cover");
        UE_DEFINE_GAMEPLAY_TAG(Retreat, "State.Enemy.Retreat");
        UE_DEFINE_GAMEPLAY_TAG(Death, "State.Enemy.Death");
    }

    namespace Event
    {
        UE_DEFINE_GAMEPLAY_TAG(PlayerDetected, "Event.Enemy.PlayerDetected");
        UE_DEFINE_GAMEPLAY_TAG(PlayerLost, "Event.Enemy.PlayerLost");
        UE_DEFINE_GAMEPLAY_TAG(DamageTaken, "Event.Enemy.DamageTaken");
        UE_DEFINE_GAMEPLAY_TAG(ReachedDestination, "Event.Enemy.ReachedDestination");
        UE_DEFINE_GAMEPLAY_TAG(TargetInRange, "Event.Enemy.TargetInRange");
        UE_DEFINE_GAMEPLAY_TAG(TargetOutOfRange, "Event.Enemy.TargetOutOfRange");
        UE_DEFINE_GAMEPLAY_TAG(LowHealth, "Event.Enemy.LowHealth");
        UE_DEFINE_GAMEPLAY_TAG(AmmoEmpty, "Event.Enemy.AmmoEmpty");
        UE_DEFINE_GAMEPLAY_TAG(PatrolComplete, "Event.Enemy.PatrolComplete");
        UE_DEFINE_GAMEPLAY_TAG(IdleTimeout, "Event.Enemy.IdleTimeout");
        UE_DEFINE_GAMEPLAY_TAG(Died, "Event.Enemy.Died");
    }

    namespace Type
    {
        UE_DEFINE_GAMEPLAY_TAG(Scav, "Type.Enemy.Scav");
        UE_DEFINE_GAMEPLAY_TAG(PMC, "Type.Enemy.PMC");
        UE_DEFINE_GAMEPLAY_TAG(Boss, "Type.Enemy.Boss");
        UE_DEFINE_GAMEPLAY_TAG(Raider, "Type.Enemy.Raider");
    }
}
```

---

## Шаг 4: Enemy State (GAS Host)

**Файл:** `Source/EnemySystem/Public/SuspenseCore/Core/SuspenseCoreEnemyState.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEnemyState.generated.h"

class UAbilitySystemComponent;
class USuspenseCoreEnemyAttributeSet;
class UGameplayAbility;
class UGameplayEffect;

UCLASS()
class ENEMYSYSTEM_API ASuspenseCoreEnemyState : public APlayerState, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ASuspenseCoreEnemyState();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    USuspenseCoreEnemyAttributeSet* GetAttributeSet() const;

    void InitializeAbilities(const TArray<TSubclassOf<UGameplayAbility>>& AbilitiesToGrant);

    void ApplyStartupEffects(const TArray<TSubclassOf<UGameplayEffect>>& EffectsToApply);

    void SetEnemyLevel(int32 NewLevel);

    int32 GetEnemyLevel() const;

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
    TObjectPtr<USuspenseCoreEnemyAttributeSet> AttributeSet;

    UPROPERTY(Replicated)
    int32 EnemyLevel;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
```

**Файл:** `Source/EnemySystem/Private/SuspenseCore/Core/SuspenseCoreEnemyState.cpp`

```cpp
#include "SuspenseCore/Core/SuspenseCoreEnemyState.h"
#include "SuspenseCore/Attributes/SuspenseCoreEnemyAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "EnemySystem.h"

ASuspenseCoreEnemyState::ASuspenseCoreEnemyState()
{
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    AttributeSet = CreateDefaultSubobject<USuspenseCoreEnemyAttributeSet>(TEXT("AttributeSet"));

    EnemyLevel = 1;

    NetUpdateFrequency = 10.0f;
}

void ASuspenseCoreEnemyState::BeginPlay()
{
    Super::BeginPlay();

    if (AbilitySystemComponent && AttributeSet)
    {
        AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet);
        AbilitySystemComponent->InitAbilityActorInfo(this, GetPawn());
    }
}

UAbilitySystemComponent* ASuspenseCoreEnemyState::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

USuspenseCoreEnemyAttributeSet* ASuspenseCoreEnemyState::GetAttributeSet() const
{
    return AttributeSet;
}

void ASuspenseCoreEnemyState::InitializeAbilities(const TArray<TSubclassOf<UGameplayAbility>>& AbilitiesToGrant)
{
    if (!AbilitySystemComponent)
    {
        return;
    }

    if (!HasAuthority())
    {
        return;
    }

    for (const TSubclassOf<UGameplayAbility>& AbilityClass : AbilitiesToGrant)
    {
        if (AbilityClass)
        {
            FGameplayAbilitySpec AbilitySpec(AbilityClass, EnemyLevel, INDEX_NONE, this);
            AbilitySystemComponent->GiveAbility(AbilitySpec);
        }
    }
}

void ASuspenseCoreEnemyState::ApplyStartupEffects(const TArray<TSubclassOf<UGameplayEffect>>& EffectsToApply)
{
    if (!AbilitySystemComponent)
    {
        return;
    }

    if (!HasAuthority())
    {
        return;
    }

    for (const TSubclassOf<UGameplayEffect>& EffectClass : EffectsToApply)
    {
        if (EffectClass)
        {
            FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
            EffectContext.AddSourceObject(this);

            FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
                EffectClass,
                static_cast<float>(EnemyLevel),
                EffectContext
            );

            if (SpecHandle.IsValid())
            {
                AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            }
        }
    }
}

void ASuspenseCoreEnemyState::SetEnemyLevel(int32 NewLevel)
{
    EnemyLevel = FMath::Max(1, NewLevel);
}

int32 ASuspenseCoreEnemyState::GetEnemyLevel() const
{
    return EnemyLevel;
}

void ASuspenseCoreEnemyState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ASuspenseCoreEnemyState, EnemyLevel);
}
```

---

## Шаг 5: Enemy Attribute Set

**Файл:** `Source/EnemySystem/Public/SuspenseCore/Attributes/SuspenseCoreEnemyAttributeSet.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "SuspenseCoreEnemyAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class ENEMYSYSTEM_API USuspenseCoreEnemyAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyAttributeSet();

    UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_Health)
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(USuspenseCoreEnemyAttributeSet, Health)

    UPROPERTY(BlueprintReadOnly, Category = "Health", ReplicatedUsing = OnRep_MaxHealth)
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(USuspenseCoreEnemyAttributeSet, MaxHealth)

    UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_AttackPower)
    FGameplayAttributeData AttackPower;
    ATTRIBUTE_ACCESSORS(USuspenseCoreEnemyAttributeSet, AttackPower)

    UPROPERTY(BlueprintReadOnly, Category = "Combat", ReplicatedUsing = OnRep_Armor)
    FGameplayAttributeData Armor;
    ATTRIBUTE_ACCESSORS(USuspenseCoreEnemyAttributeSet, Armor)

    UPROPERTY(BlueprintReadOnly, Category = "Movement", ReplicatedUsing = OnRep_MovementSpeed)
    FGameplayAttributeData MovementSpeed;
    ATTRIBUTE_ACCESSORS(USuspenseCoreEnemyAttributeSet, MovementSpeed)

    UPROPERTY(BlueprintReadOnly, Category = "Meta")
    FGameplayAttributeData IncomingDamage;
    ATTRIBUTE_ACCESSORS(USuspenseCoreEnemyAttributeSet, IncomingDamage)

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

    bool IsAlive() const;
    float GetHealthPercent() const;

protected:
    UFUNCTION()
    void OnRep_Health(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_AttackPower(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_Armor(const FGameplayAttributeData& OldValue);

    UFUNCTION()
    void OnRep_MovementSpeed(const FGameplayAttributeData& OldValue);

private:
    void HandleHealthChanged(float DeltaValue, const FGameplayEffectModCallbackData& Data);
    void HandleDeath(AActor* DamageInstigator, AActor* DamageCauser);

    bool bIsDead;
};
```

**Файл:** `Source/EnemySystem/Private/SuspenseCore/Attributes/SuspenseCoreEnemyAttributeSet.cpp`

```cpp
#include "SuspenseCore/Attributes/SuspenseCoreEnemyAttributeSet.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"
#include "EnemySystem.h"

USuspenseCoreEnemyAttributeSet::USuspenseCoreEnemyAttributeSet()
    : bIsDead(false)
{
    InitHealth(100.0f);
    InitMaxHealth(100.0f);
    InitAttackPower(25.0f);
    InitArmor(0.0f);
    InitMovementSpeed(400.0f);
    InitIncomingDamage(0.0f);
}

void USuspenseCoreEnemyAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreEnemyAttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreEnemyAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreEnemyAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreEnemyAttributeSet, Armor, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreEnemyAttributeSet, MovementSpeed, COND_None, REPNOTIFY_Always);
}

void USuspenseCoreEnemyAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
    }
    else if (Attribute == GetMaxHealthAttribute())
    {
        NewValue = FMath::Max(NewValue, 1.0f);
    }
    else if (Attribute == GetMovementSpeedAttribute())
    {
        NewValue = FMath::Max(NewValue, 0.0f);
    }
}

void USuspenseCoreEnemyAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
    {
        float DamageValue = GetIncomingDamage();
        SetIncomingDamage(0.0f);

        if (DamageValue > 0.0f)
        {
            float ArmorReduction = GetArmor() * 0.01f;
            float ActualDamage = DamageValue * (1.0f - FMath::Clamp(ArmorReduction, 0.0f, 0.9f));

            float OldHealth = GetHealth();
            float NewHealth = FMath::Max(0.0f, OldHealth - ActualDamage);
            SetHealth(NewHealth);

            HandleHealthChanged(NewHealth - OldHealth, Data);
        }
    }
}

void USuspenseCoreEnemyAttributeSet::HandleHealthChanged(float DeltaValue, const FGameplayEffectModCallbackData& Data)
{
    if (GetHealth() <= 0.0f && !bIsDead)
    {
        AActor* Instigator = nullptr;
        AActor* Causer = nullptr;

        if (Data.EffectSpec.GetContext().GetEffectCauser())
        {
            Causer = Data.EffectSpec.GetContext().GetEffectCauser();
        }

        if (Data.EffectSpec.GetContext().GetInstigator())
        {
            Instigator = Data.EffectSpec.GetContext().GetInstigator();
        }

        HandleDeath(Instigator, Causer);
    }
}

void USuspenseCoreEnemyAttributeSet::HandleDeath(AActor* DamageInstigator, AActor* DamageCauser)
{
    if (bIsDead)
    {
        return;
    }

    bIsDead = true;

    UAbilitySystemComponent* AbilitySystemComponent = GetOwningAbilitySystemComponent();
    if (!AbilitySystemComponent)
    {
        return;
    }

    AbilitySystemComponent->AddLooseGameplayTag(SuspenseCoreEnemyTags::State::Death);
    AbilitySystemComponent->CancelAllAbilities();

    FGameplayEffectQuery Query;
    AbilitySystemComponent->RemoveActiveEffects(Query);

    UE_LOG(LogEnemySystem, Log, TEXT("Enemy died. Instigator: %s, Causer: %s"),
        DamageInstigator ? *DamageInstigator->GetName() : TEXT("None"),
        DamageCauser ? *DamageCauser->GetName() : TEXT("None"));
}

bool USuspenseCoreEnemyAttributeSet::IsAlive() const
{
    return !bIsDead && GetHealth() > 0.0f;
}

float USuspenseCoreEnemyAttributeSet::GetHealthPercent() const
{
    float MaxHealthValue = GetMaxHealth();
    if (MaxHealthValue <= 0.0f)
    {
        return 0.0f;
    }
    return GetHealth() / MaxHealthValue;
}

void USuspenseCoreEnemyAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreEnemyAttributeSet, Health, OldValue);
}

void USuspenseCoreEnemyAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreEnemyAttributeSet, MaxHealth, OldValue);
}

void USuspenseCoreEnemyAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreEnemyAttributeSet, AttackPower, OldValue);
}

void USuspenseCoreEnemyAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreEnemyAttributeSet, Armor, OldValue);
}

void USuspenseCoreEnemyAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(USuspenseCoreEnemyAttributeSet, MovementSpeed, OldValue);
}
```

---

## Шаг 6: FSM State Base Class

**Файл:** `Source/EnemySystem/Public/SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEnemyStateBase.generated.h"

class ASuspenseCoreEnemy;
class USuspenseCoreEnemyFSMComponent;

UCLASS(Abstract, Blueprintable)
class ENEMYSYSTEM_API USuspenseCoreEnemyStateBase : public UObject
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyStateBase();

    virtual void OnEnterState(ASuspenseCoreEnemy* Enemy);
    virtual void OnExitState(ASuspenseCoreEnemy* Enemy);
    virtual void OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime);
    virtual void OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator);

    FGameplayTag GetStateTag() const;
    void SetFSMComponent(USuspenseCoreEnemyFSMComponent* InFSMComponent);

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "State")
    FGameplayTag StateTag;

    UPROPERTY()
    TObjectPtr<USuspenseCoreEnemyFSMComponent> FSMComponent;

    void RequestStateChange(const FGameplayTag& NewStateTag);
    void StartTimer(ASuspenseCoreEnemy* Enemy, FName TimerName, float Duration, bool bLoop);
    void StopTimer(ASuspenseCoreEnemy* Enemy, FName TimerName);

    bool CanSeeTarget(ASuspenseCoreEnemy* Enemy, AActor* Target) const;
    float GetDistanceToTarget(ASuspenseCoreEnemy* Enemy, AActor* Target) const;
    AActor* GetCurrentTarget(ASuspenseCoreEnemy* Enemy) const;
    void SetCurrentTarget(ASuspenseCoreEnemy* Enemy, AActor* NewTarget);
};
```

**Файл:** `Source/EnemySystem/Private/SuspenseCore/FSM/SuspenseCoreEnemyStateBase.cpp`

```cpp
#include "SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyFSMComponent.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemy.h"
#include "AIController.h"
#include "EnemySystem.h"

USuspenseCoreEnemyStateBase::USuspenseCoreEnemyStateBase()
{
    FSMComponent = nullptr;
}

void USuspenseCoreEnemyStateBase::OnEnterState(ASuspenseCoreEnemy* Enemy)
{
    UE_LOG(LogEnemySystem, Verbose, TEXT("[%s] Entering state: %s"),
        Enemy ? *Enemy->GetName() : TEXT("None"),
        *StateTag.ToString());
}

void USuspenseCoreEnemyStateBase::OnExitState(ASuspenseCoreEnemy* Enemy)
{
    UE_LOG(LogEnemySystem, Verbose, TEXT("[%s] Exiting state: %s"),
        Enemy ? *Enemy->GetName() : TEXT("None"),
        *StateTag.ToString());
}

void USuspenseCoreEnemyStateBase::OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime)
{
}

void USuspenseCoreEnemyStateBase::OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator)
{
}

FGameplayTag USuspenseCoreEnemyStateBase::GetStateTag() const
{
    return StateTag;
}

void USuspenseCoreEnemyStateBase::SetFSMComponent(USuspenseCoreEnemyFSMComponent* InFSMComponent)
{
    FSMComponent = InFSMComponent;
}

void USuspenseCoreEnemyStateBase::RequestStateChange(const FGameplayTag& NewStateTag)
{
    if (FSMComponent)
    {
        FSMComponent->RequestStateChange(NewStateTag);
    }
}

void USuspenseCoreEnemyStateBase::StartTimer(ASuspenseCoreEnemy* Enemy, FName TimerName, float Duration, bool bLoop)
{
    if (FSMComponent)
    {
        FSMComponent->StartStateTimer(TimerName, Duration, bLoop);
    }
}

void USuspenseCoreEnemyStateBase::StopTimer(ASuspenseCoreEnemy* Enemy, FName TimerName)
{
    if (FSMComponent)
    {
        FSMComponent->StopStateTimer(TimerName);
    }
}

bool USuspenseCoreEnemyStateBase::CanSeeTarget(ASuspenseCoreEnemy* Enemy, AActor* Target) const
{
    if (!Enemy || !Target)
    {
        return false;
    }

    FVector StartLocation = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
    FVector EndLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Enemy);

    bool bHit = Enemy->GetWorld()->LineTraceSingleByChannel(
        HitResult,
        StartLocation,
        EndLocation,
        ECollisionChannel::ECC_Visibility,
        QueryParams
    );

    if (!bHit)
    {
        return true;
    }

    return HitResult.GetActor() == Target;
}

float USuspenseCoreEnemyStateBase::GetDistanceToTarget(ASuspenseCoreEnemy* Enemy, AActor* Target) const
{
    if (!Enemy || !Target)
    {
        return MAX_FLT;
    }

    return FVector::Dist(Enemy->GetActorLocation(), Target->GetActorLocation());
}

AActor* USuspenseCoreEnemyStateBase::GetCurrentTarget(ASuspenseCoreEnemy* Enemy) const
{
    if (!Enemy)
    {
        return nullptr;
    }
    return Enemy->GetCurrentTarget();
}

void USuspenseCoreEnemyStateBase::SetCurrentTarget(ASuspenseCoreEnemy* Enemy, AActor* NewTarget)
{
    if (Enemy)
    {
        Enemy->SetCurrentTarget(NewTarget);
    }
}
```

---

## Шаг 7: FSM Component

**Файл:** `Source/EnemySystem/Public/SuspenseCore/FSM/SuspenseCoreEnemyFSMComponent.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEnemyFSMComponent.generated.h"

class ASuspenseCoreEnemy;
class USuspenseCoreEnemyStateBase;
class USuspenseCoreEnemyBehaviorData;

USTRUCT()
struct FSuspenseCoreEnemyFSMTransition
{
    GENERATED_BODY()

    UPROPERTY()
    FGameplayTag FromState;

    UPROPERTY()
    FGameplayTag EventTag;

    UPROPERTY()
    FGameplayTag ToState;
};

USTRUCT()
struct FSuspenseCoreEnemyStateTimer
{
    GENERATED_BODY()

    FTimerHandle TimerHandle;
    FName TimerName;
    bool bLoop;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyStateChanged, FGameplayTag, OldState, FGameplayTag, NewState);

UCLASS(ClassGroup = "SuspenseCore", meta = (BlueprintSpawnableComponent))
class ENEMYSYSTEM_API USuspenseCoreEnemyFSMComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyFSMComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    void Initialize(USuspenseCoreEnemyBehaviorData* BehaviorData);

    UFUNCTION(BlueprintCallable, Category = "EnemyAI|FSM")
    void RequestStateChange(const FGameplayTag& NewStateTag);

    UFUNCTION(BlueprintCallable, Category = "EnemyAI|FSM")
    void SendFSMEvent(const FGameplayTag& EventTag, AActor* Instigator);

    UFUNCTION(BlueprintPure, Category = "EnemyAI|FSM")
    FGameplayTag GetCurrentStateTag() const;

    UFUNCTION(BlueprintPure, Category = "EnemyAI|FSM")
    bool IsInState(const FGameplayTag& StateTag) const;

    void StartStateTimer(FName TimerName, float Duration, bool bLoop);
    void StopStateTimer(FName TimerName);
    void StopAllTimers();

    UPROPERTY(BlueprintAssignable, Category = "EnemyAI|FSM")
    FOnEnemyStateChanged OnStateChanged;

protected:
    UPROPERTY()
    TObjectPtr<ASuspenseCoreEnemy> OwnerEnemy;

    UPROPERTY()
    TObjectPtr<USuspenseCoreEnemyStateBase> CurrentState;

    UPROPERTY()
    TMap<FGameplayTag, TObjectPtr<USuspenseCoreEnemyStateBase>> StateMap;

    UPROPERTY()
    TArray<FSuspenseCoreEnemyFSMTransition> Transitions;

    UPROPERTY()
    TMap<FName, FSuspenseCoreEnemyStateTimer> ActiveTimers;

    UPROPERTY()
    FGameplayTag InitialStateTag;

    bool bIsInitialized;
    bool bIsProcessingEvent;

    TQueue<TPair<FGameplayTag, TWeakObjectPtr<AActor>>> EventQueue;

    void PerformStateChange(const FGameplayTag& NewStateTag);
    void ProcessEventQueue();
    FGameplayTag FindTransitionTarget(const FGameplayTag& FromState, const FGameplayTag& EventTag) const;
    void HandleTimerFired(FName TimerName);

    USuspenseCoreEnemyStateBase* CreateStateInstance(TSubclassOf<USuspenseCoreEnemyStateBase> StateClass);
};
```

**Файл:** `Source/EnemySystem/Private/SuspenseCore/FSM/SuspenseCoreEnemyFSMComponent.cpp`

```cpp
#include "SuspenseCore/FSM/SuspenseCoreEnemyFSMComponent.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemy.h"
#include "SuspenseCore/Data/SuspenseCoreEnemyBehaviorData.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "TimerManager.h"
#include "EnemySystem.h"

USuspenseCoreEnemyFSMComponent::USuspenseCoreEnemyFSMComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;

    bIsInitialized = false;
    bIsProcessingEvent = false;
}

void USuspenseCoreEnemyFSMComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerEnemy = Cast<ASuspenseCoreEnemy>(GetOwner());
    if (!OwnerEnemy)
    {
        UE_LOG(LogEnemySystem, Error, TEXT("FSMComponent owner is not ASuspenseCoreEnemy"));
    }
}

void USuspenseCoreEnemyFSMComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopAllTimers();
    Super::EndPlay(EndPlayReason);
}

void USuspenseCoreEnemyFSMComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    ProcessEventQueue();

    if (CurrentState && OwnerEnemy)
    {
        CurrentState->OnTickState(OwnerEnemy, DeltaTime);
    }
}

void USuspenseCoreEnemyFSMComponent::Initialize(USuspenseCoreEnemyBehaviorData* BehaviorData)
{
    if (!BehaviorData)
    {
        UE_LOG(LogEnemySystem, Error, TEXT("Cannot initialize FSM: BehaviorData is null"));
        return;
    }

    StateMap.Empty();
    Transitions.Empty();

    for (const FSuspenseCoreEnemyStateConfig& StateConfig : BehaviorData->States)
    {
        if (StateConfig.StateClass)
        {
            USuspenseCoreEnemyStateBase* StateInstance = CreateStateInstance(StateConfig.StateClass);
            if (StateInstance)
            {
                StateMap.Add(StateConfig.StateTag, StateInstance);
            }
        }
    }

    for (const FSuspenseCoreEnemyTransitionConfig& TransitionConfig : BehaviorData->Transitions)
    {
        FSuspenseCoreEnemyFSMTransition Transition;
        Transition.FromState = TransitionConfig.FromState;
        Transition.EventTag = TransitionConfig.OnEvent;
        Transition.ToState = TransitionConfig.ToState;
        Transitions.Add(Transition);
    }

    InitialStateTag = BehaviorData->InitialState;
    bIsInitialized = true;

    SetComponentTickEnabled(true);

    if (InitialStateTag.IsValid())
    {
        PerformStateChange(InitialStateTag);
    }

    UE_LOG(LogEnemySystem, Log, TEXT("FSM initialized with %d states and %d transitions"),
        StateMap.Num(), Transitions.Num());
}

void USuspenseCoreEnemyFSMComponent::RequestStateChange(const FGameplayTag& NewStateTag)
{
    if (!bIsInitialized)
    {
        return;
    }

    if (!NewStateTag.IsValid())
    {
        return;
    }

    if (CurrentState && CurrentState->GetStateTag() == NewStateTag)
    {
        return;
    }

    PerformStateChange(NewStateTag);
}

void USuspenseCoreEnemyFSMComponent::SendFSMEvent(const FGameplayTag& EventTag, AActor* Instigator)
{
    if (!bIsInitialized)
    {
        return;
    }

    EventQueue.Enqueue(TPair<FGameplayTag, TWeakObjectPtr<AActor>>(EventTag, Instigator));
}

FGameplayTag USuspenseCoreEnemyFSMComponent::GetCurrentStateTag() const
{
    if (CurrentState)
    {
        return CurrentState->GetStateTag();
    }
    return FGameplayTag();
}

bool USuspenseCoreEnemyFSMComponent::IsInState(const FGameplayTag& StateTag) const
{
    return CurrentState && CurrentState->GetStateTag() == StateTag;
}

void USuspenseCoreEnemyFSMComponent::PerformStateChange(const FGameplayTag& NewStateTag)
{
    TObjectPtr<USuspenseCoreEnemyStateBase>* NewStatePtr = StateMap.Find(NewStateTag);
    if (!NewStatePtr || !(*NewStatePtr))
    {
        UE_LOG(LogEnemySystem, Warning, TEXT("State not found in StateMap: %s"), *NewStateTag.ToString());
        return;
    }

    FGameplayTag OldStateTag;

    if (CurrentState)
    {
        OldStateTag = CurrentState->GetStateTag();
        CurrentState->OnExitState(OwnerEnemy);
    }

    StopAllTimers();

    CurrentState = *NewStatePtr;
    CurrentState->OnEnterState(OwnerEnemy);

    OnStateChanged.Broadcast(OldStateTag, NewStateTag);

    UE_LOG(LogEnemySystem, Log, TEXT("[%s] State changed: %s -> %s"),
        OwnerEnemy ? *OwnerEnemy->GetName() : TEXT("None"),
        OldStateTag.IsValid() ? *OldStateTag.ToString() : TEXT("None"),
        *NewStateTag.ToString());
}

void USuspenseCoreEnemyFSMComponent::ProcessEventQueue()
{
    if (bIsProcessingEvent)
    {
        return;
    }

    bIsProcessingEvent = true;

    TPair<FGameplayTag, TWeakObjectPtr<AActor>> EventPair;
    while (EventQueue.Dequeue(EventPair))
    {
        FGameplayTag EventTag = EventPair.Key;
        AActor* Instigator = EventPair.Value.Get();

        if (CurrentState)
        {
            CurrentState->OnFSMEvent(OwnerEnemy, EventTag, Instigator);
        }

        FGameplayTag CurrentStateTag = GetCurrentStateTag();
        FGameplayTag TargetState = FindTransitionTarget(CurrentStateTag, EventTag);

        if (TargetState.IsValid())
        {
            PerformStateChange(TargetState);
        }
    }

    bIsProcessingEvent = false;
}

FGameplayTag USuspenseCoreEnemyFSMComponent::FindTransitionTarget(const FGameplayTag& FromState, const FGameplayTag& EventTag) const
{
    for (const FSuspenseCoreEnemyFSMTransition& Transition : Transitions)
    {
        if (Transition.FromState == FromState && Transition.EventTag == EventTag)
        {
            return Transition.ToState;
        }
    }
    return FGameplayTag();
}

void USuspenseCoreEnemyFSMComponent::StartStateTimer(FName TimerName, float Duration, bool bLoop)
{
    if (!GetWorld())
    {
        return;
    }

    StopStateTimer(TimerName);

    FSuspenseCoreEnemyStateTimer NewTimer;
    NewTimer.TimerName = TimerName;
    NewTimer.bLoop = bLoop;

    FTimerDelegate TimerDelegate;
    TimerDelegate.BindUObject(this, &USuspenseCoreEnemyFSMComponent::HandleTimerFired, TimerName);

    GetWorld()->GetTimerManager().SetTimer(
        NewTimer.TimerHandle,
        TimerDelegate,
        Duration,
        bLoop
    );

    ActiveTimers.Add(TimerName, NewTimer);
}

void USuspenseCoreEnemyFSMComponent::StopStateTimer(FName TimerName)
{
    FSuspenseCoreEnemyStateTimer* Timer = ActiveTimers.Find(TimerName);
    if (Timer && GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(Timer->TimerHandle);
        ActiveTimers.Remove(TimerName);
    }
}

void USuspenseCoreEnemyFSMComponent::StopAllTimers()
{
    if (!GetWorld())
    {
        return;
    }

    for (auto& TimerPair : ActiveTimers)
    {
        GetWorld()->GetTimerManager().ClearTimer(TimerPair.Value.TimerHandle);
    }
    ActiveTimers.Empty();
}

void USuspenseCoreEnemyFSMComponent::HandleTimerFired(FName TimerName)
{
    UE_LOG(LogEnemySystem, Verbose, TEXT("Timer fired: %s"), *TimerName.ToString());

    if (TimerName == FName(TEXT("IdleTimeout")))
    {
        SendFSMEvent(SuspenseCoreEnemyTags::Event::IdleTimeout, nullptr);
    }
    else if (TimerName == FName(TEXT("PatrolWait")))
    {
        SendFSMEvent(SuspenseCoreEnemyTags::Event::PatrolComplete, nullptr);
    }
}

USuspenseCoreEnemyStateBase* USuspenseCoreEnemyFSMComponent::CreateStateInstance(TSubclassOf<USuspenseCoreEnemyStateBase> StateClass)
{
    if (!StateClass)
    {
        return nullptr;
    }

    USuspenseCoreEnemyStateBase* StateInstance = NewObject<USuspenseCoreEnemyStateBase>(this, StateClass);
    if (StateInstance)
    {
        StateInstance->SetFSMComponent(this);
    }
    return StateInstance;
}
```

---

## Шаг 8: Concrete States

### Idle State

**Файл:** `Source/EnemySystem/Public/SuspenseCore/FSM/States/SuspenseCoreEnemyIdleState.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h"
#include "SuspenseCoreEnemyIdleState.generated.h"

UCLASS()
class ENEMYSYSTEM_API USuspenseCoreEnemyIdleState : public USuspenseCoreEnemyStateBase
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyIdleState();

    virtual void OnEnterState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnExitState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime) override;
    virtual void OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Idle")
    float IdleTimeout;

    UPROPERTY(EditDefaultsOnly, Category = "Idle")
    float LookAroundInterval;

    float TimeSinceLastLook;
    FRotator OriginalRotation;
};
```

**Файл:** `Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyIdleState.cpp`

```cpp
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyIdleState.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemy.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "EnemySystem.h"

USuspenseCoreEnemyIdleState::USuspenseCoreEnemyIdleState()
{
    StateTag = SuspenseCoreEnemyTags::State::Idle;
    IdleTimeout = 5.0f;
    LookAroundInterval = 3.0f;
    TimeSinceLastLook = 0.0f;
}

void USuspenseCoreEnemyIdleState::OnEnterState(ASuspenseCoreEnemy* Enemy)
{
    Super::OnEnterState(Enemy);

    if (Enemy)
    {
        OriginalRotation = Enemy->GetActorRotation();
        Enemy->StopMovement();
    }

    TimeSinceLastLook = 0.0f;

    if (IdleTimeout > 0.0f)
    {
        StartTimer(Enemy, FName(TEXT("IdleTimeout")), IdleTimeout, false);
    }
}

void USuspenseCoreEnemyIdleState::OnExitState(ASuspenseCoreEnemy* Enemy)
{
    StopTimer(Enemy, FName(TEXT("IdleTimeout")));
    Super::OnExitState(Enemy);
}

void USuspenseCoreEnemyIdleState::OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime)
{
    Super::OnTickState(Enemy, DeltaTime);

    if (!Enemy)
    {
        return;
    }

    TimeSinceLastLook += DeltaTime;

    if (TimeSinceLastLook >= LookAroundInterval)
    {
        TimeSinceLastLook = 0.0f;

        float RandomYaw = FMath::RandRange(-60.0f, 60.0f);
        FRotator NewRotation = OriginalRotation;
        NewRotation.Yaw += RandomYaw;

        Enemy->SetActorRotation(FMath::RInterpTo(
            Enemy->GetActorRotation(),
            NewRotation,
            DeltaTime,
            2.0f
        ));
    }
}

void USuspenseCoreEnemyIdleState::OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator)
{
    Super::OnFSMEvent(Enemy, EventTag, Instigator);

    if (EventTag == SuspenseCoreEnemyTags::Event::PlayerDetected)
    {
        SetCurrentTarget(Enemy, Instigator);
    }
}
```

### Patrol State

**Файл:** `Source/EnemySystem/Public/SuspenseCore/FSM/States/SuspenseCoreEnemyPatrolState.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h"
#include "Navigation/PathFollowingComponent.h"
#include "SuspenseCoreEnemyPatrolState.generated.h"

UCLASS()
class ENEMYSYSTEM_API USuspenseCoreEnemyPatrolState : public USuspenseCoreEnemyStateBase
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyPatrolState();

    virtual void OnEnterState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnExitState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime) override;
    virtual void OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Patrol")
    float PatrolSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "Patrol")
    float PatrolRadius;

    UPROPERTY(EditDefaultsOnly, Category = "Patrol")
    float WaitTimeAtPoint;

    UPROPERTY(EditDefaultsOnly, Category = "Patrol")
    float AcceptanceRadius;

    TArray<FVector> PatrolPoints;
    int32 CurrentPointIndex;
    FVector SpawnLocation;
    bool bIsWaiting;
    bool bIsMoving;

    void GeneratePatrolPoints(ASuspenseCoreEnemy* Enemy);
    void MoveToNextPoint(ASuspenseCoreEnemy* Enemy);
    void OnReachedPatrolPoint(ASuspenseCoreEnemy* Enemy);

    UFUNCTION()
    void OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result);

    TWeakObjectPtr<class AAIController> CachedController;
};
```

**Файл:** `Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyPatrolState.cpp`

```cpp
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyPatrolState.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemy.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "EnemySystem.h"

USuspenseCoreEnemyPatrolState::USuspenseCoreEnemyPatrolState()
{
    StateTag = SuspenseCoreEnemyTags::State::Patrol;
    PatrolSpeed = 200.0f;
    PatrolRadius = 800.0f;
    WaitTimeAtPoint = 2.0f;
    AcceptanceRadius = 50.0f;
    CurrentPointIndex = 0;
    bIsWaiting = false;
    bIsMoving = false;
}

void USuspenseCoreEnemyPatrolState::OnEnterState(ASuspenseCoreEnemy* Enemy)
{
    Super::OnEnterState(Enemy);

    if (!Enemy)
    {
        return;
    }

    AAIController* AIController = Cast<AAIController>(Enemy->GetController());
    if (AIController)
    {
        CachedController = AIController;
        AIController->ReceiveMoveCompleted.AddDynamic(this, &USuspenseCoreEnemyPatrolState::OnMoveCompleted);
    }

    if (PatrolPoints.Num() == 0)
    {
        SpawnLocation = Enemy->GetActorLocation();
        GeneratePatrolPoints(Enemy);
    }

    CurrentPointIndex = 0;
    bIsWaiting = false;
    bIsMoving = false;

    MoveToNextPoint(Enemy);
}

void USuspenseCoreEnemyPatrolState::OnExitState(ASuspenseCoreEnemy* Enemy)
{
    if (CachedController.IsValid())
    {
        CachedController->ReceiveMoveCompleted.RemoveDynamic(this, &USuspenseCoreEnemyPatrolState::OnMoveCompleted);
        CachedController->StopMovement();
    }

    StopTimer(Enemy, FName(TEXT("PatrolWait")));

    Super::OnExitState(Enemy);
}

void USuspenseCoreEnemyPatrolState::OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime)
{
    Super::OnTickState(Enemy, DeltaTime);
}

void USuspenseCoreEnemyPatrolState::OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator)
{
    Super::OnFSMEvent(Enemy, EventTag, Instigator);

    if (EventTag == SuspenseCoreEnemyTags::Event::PlayerDetected)
    {
        SetCurrentTarget(Enemy, Instigator);
    }
    else if (EventTag == SuspenseCoreEnemyTags::Event::PatrolComplete && bIsWaiting)
    {
        bIsWaiting = false;
        MoveToNextPoint(Enemy);
    }
}

void USuspenseCoreEnemyPatrolState::GeneratePatrolPoints(ASuspenseCoreEnemy* Enemy)
{
    PatrolPoints.Empty();

    if (!Enemy)
    {
        return;
    }

    UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(Enemy->GetWorld());
    if (!NavSystem)
    {
        return;
    }

    int32 NumPoints = 4;
    float AngleStep = 360.0f / static_cast<float>(NumPoints);

    for (int32 Index = 0; Index < NumPoints; ++Index)
    {
        float Angle = FMath::DegreesToRadians(AngleStep * static_cast<float>(Index));
        FVector Offset(
            FMath::Cos(Angle) * PatrolRadius,
            FMath::Sin(Angle) * PatrolRadius,
            0.0f
        );

        FVector TestLocation = SpawnLocation + Offset;
        FNavLocation NavLocation;

        if (NavSystem->ProjectPointToNavigation(TestLocation, NavLocation))
        {
            PatrolPoints.Add(NavLocation.Location);
        }
    }

    if (PatrolPoints.Num() == 0)
    {
        PatrolPoints.Add(SpawnLocation);
    }
}

void USuspenseCoreEnemyPatrolState::MoveToNextPoint(ASuspenseCoreEnemy* Enemy)
{
    if (!Enemy || PatrolPoints.Num() == 0)
    {
        return;
    }

    AAIController* AIController = CachedController.Get();
    if (!AIController)
    {
        return;
    }

    FVector TargetLocation = PatrolPoints[CurrentPointIndex];

    bIsMoving = true;
    AIController->MoveToLocation(TargetLocation, AcceptanceRadius, true, true, false, true);
}

void USuspenseCoreEnemyPatrolState::OnReachedPatrolPoint(ASuspenseCoreEnemy* Enemy)
{
    bIsMoving = false;
    bIsWaiting = true;

    CurrentPointIndex = (CurrentPointIndex + 1) % PatrolPoints.Num();

    StartTimer(Enemy, FName(TEXT("PatrolWait")), WaitTimeAtPoint, false);
}

void USuspenseCoreEnemyPatrolState::OnMoveCompleted(FAIRequestID RequestID, EPathFollowingResult::Type Result)
{
    if (Result == EPathFollowingResult::Success)
    {
        ASuspenseCoreEnemy* Enemy = Cast<ASuspenseCoreEnemy>(CachedController.IsValid() ? CachedController->GetPawn() : nullptr);
        OnReachedPatrolPoint(Enemy);
    }
}
```

### Chase State

**Файл:** `Source/EnemySystem/Public/SuspenseCore/FSM/States/SuspenseCoreEnemyChaseState.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h"
#include "SuspenseCoreEnemyChaseState.generated.h"

UCLASS()
class ENEMYSYSTEM_API USuspenseCoreEnemyChaseState : public USuspenseCoreEnemyStateBase
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyChaseState();

    virtual void OnEnterState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnExitState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime) override;
    virtual void OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Chase")
    float ChaseSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "Chase")
    float AttackRange;

    UPROPERTY(EditDefaultsOnly, Category = "Chase")
    float LoseTargetDistance;

    UPROPERTY(EditDefaultsOnly, Category = "Chase")
    float LoseTargetTime;

    UPROPERTY(EditDefaultsOnly, Category = "Chase")
    float PathUpdateInterval;

    float TimeSinceTargetSeen;
    float TimeSinceLastPathUpdate;
    FVector LastKnownTargetLocation;

    TWeakObjectPtr<class AAIController> CachedController;

    void UpdateChase(ASuspenseCoreEnemy* Enemy, float DeltaTime);
    void MoveToTarget(ASuspenseCoreEnemy* Enemy, const FVector& TargetLocation);
};
```

**Файл:** `Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyChaseState.cpp`

```cpp
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyChaseState.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemy.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyFSMComponent.h"
#include "AIController.h"
#include "EnemySystem.h"

USuspenseCoreEnemyChaseState::USuspenseCoreEnemyChaseState()
{
    StateTag = SuspenseCoreEnemyTags::State::Chase;
    ChaseSpeed = 500.0f;
    AttackRange = 200.0f;
    LoseTargetDistance = 2000.0f;
    LoseTargetTime = 5.0f;
    PathUpdateInterval = 0.5f;
    TimeSinceTargetSeen = 0.0f;
    TimeSinceLastPathUpdate = 0.0f;
}

void USuspenseCoreEnemyChaseState::OnEnterState(ASuspenseCoreEnemy* Enemy)
{
    Super::OnEnterState(Enemy);

    if (!Enemy)
    {
        return;
    }

    AAIController* AIController = Cast<AAIController>(Enemy->GetController());
    if (AIController)
    {
        CachedController = AIController;
    }

    TimeSinceTargetSeen = 0.0f;
    TimeSinceLastPathUpdate = PathUpdateInterval;

    AActor* Target = GetCurrentTarget(Enemy);
    if (Target)
    {
        LastKnownTargetLocation = Target->GetActorLocation();
    }
}

void USuspenseCoreEnemyChaseState::OnExitState(ASuspenseCoreEnemy* Enemy)
{
    if (CachedController.IsValid())
    {
        CachedController->StopMovement();
    }

    Super::OnExitState(Enemy);
}

void USuspenseCoreEnemyChaseState::OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime)
{
    Super::OnTickState(Enemy, DeltaTime);

    UpdateChase(Enemy, DeltaTime);
}

void USuspenseCoreEnemyChaseState::OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator)
{
    Super::OnFSMEvent(Enemy, EventTag, Instigator);
}

void USuspenseCoreEnemyChaseState::UpdateChase(ASuspenseCoreEnemy* Enemy, float DeltaTime)
{
    if (!Enemy)
    {
        return;
    }

    AActor* Target = GetCurrentTarget(Enemy);

    if (!Target)
    {
        if (FSMComponent)
        {
            FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::PlayerLost, nullptr);
        }
        return;
    }

    float DistanceToTarget = GetDistanceToTarget(Enemy, Target);

    if (DistanceToTarget <= AttackRange)
    {
        if (FSMComponent)
        {
            FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::TargetInRange, Target);
        }
        return;
    }

    if (CanSeeTarget(Enemy, Target))
    {
        TimeSinceTargetSeen = 0.0f;
        LastKnownTargetLocation = Target->GetActorLocation();
    }
    else
    {
        TimeSinceTargetSeen += DeltaTime;

        if (TimeSinceTargetSeen >= LoseTargetTime)
        {
            if (FSMComponent)
            {
                FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::PlayerLost, nullptr);
            }
            return;
        }
    }

    if (DistanceToTarget > LoseTargetDistance)
    {
        if (FSMComponent)
        {
            FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::PlayerLost, nullptr);
        }
        return;
    }

    TimeSinceLastPathUpdate += DeltaTime;
    if (TimeSinceLastPathUpdate >= PathUpdateInterval)
    {
        TimeSinceLastPathUpdate = 0.0f;
        MoveToTarget(Enemy, LastKnownTargetLocation);
    }
}

void USuspenseCoreEnemyChaseState::MoveToTarget(ASuspenseCoreEnemy* Enemy, const FVector& TargetLocation)
{
    AAIController* AIController = CachedController.Get();
    if (!AIController)
    {
        return;
    }

    AIController->MoveToLocation(TargetLocation, AttackRange * 0.8f, true, true, false, true);
}
```

### Attack State

**Файл:** `Source/EnemySystem/Public/SuspenseCore/FSM/States/SuspenseCoreEnemyAttackState.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h"
#include "SuspenseCoreEnemyAttackState.generated.h"

UCLASS()
class ENEMYSYSTEM_API USuspenseCoreEnemyAttackState : public USuspenseCoreEnemyStateBase
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyAttackState();

    virtual void OnEnterState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnExitState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime) override;
    virtual void OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Attack")
    float AttackRange;

    UPROPERTY(EditDefaultsOnly, Category = "Attack")
    float AttackCooldown;

    UPROPERTY(EditDefaultsOnly, Category = "Attack")
    float RotationSpeed;

    UPROPERTY(EditDefaultsOnly, Category = "Attack")
    float LoseTargetTime;

    float TimeSinceLastAttack;
    float TimeSinceTargetSeen;

    void PerformAttack(ASuspenseCoreEnemy* Enemy);
    void RotateTowardsTarget(ASuspenseCoreEnemy* Enemy, float DeltaTime);
    bool IsTargetInAttackRange(ASuspenseCoreEnemy* Enemy) const;
};
```

**Файл:** `Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyAttackState.cpp`

```cpp
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyAttackState.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemy.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyFSMComponent.h"
#include "EnemySystem.h"

USuspenseCoreEnemyAttackState::USuspenseCoreEnemyAttackState()
{
    StateTag = SuspenseCoreEnemyTags::State::Attack;
    AttackRange = 200.0f;
    AttackCooldown = 1.5f;
    RotationSpeed = 360.0f;
    LoseTargetTime = 3.0f;
    TimeSinceLastAttack = 0.0f;
    TimeSinceTargetSeen = 0.0f;
}

void USuspenseCoreEnemyAttackState::OnEnterState(ASuspenseCoreEnemy* Enemy)
{
    Super::OnEnterState(Enemy);

    if (Enemy)
    {
        Enemy->StopMovement();
    }

    TimeSinceLastAttack = AttackCooldown;
    TimeSinceTargetSeen = 0.0f;
}

void USuspenseCoreEnemyAttackState::OnExitState(ASuspenseCoreEnemy* Enemy)
{
    Super::OnExitState(Enemy);
}

void USuspenseCoreEnemyAttackState::OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime)
{
    Super::OnTickState(Enemy, DeltaTime);

    if (!Enemy)
    {
        return;
    }

    AActor* Target = GetCurrentTarget(Enemy);

    if (!Target)
    {
        if (FSMComponent)
        {
            FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::PlayerLost, nullptr);
        }
        return;
    }

    if (CanSeeTarget(Enemy, Target))
    {
        TimeSinceTargetSeen = 0.0f;
    }
    else
    {
        TimeSinceTargetSeen += DeltaTime;
        if (TimeSinceTargetSeen >= LoseTargetTime)
        {
            if (FSMComponent)
            {
                FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::PlayerLost, nullptr);
            }
            return;
        }
    }

    if (!IsTargetInAttackRange(Enemy))
    {
        if (FSMComponent)
        {
            FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::TargetOutOfRange, Target);
        }
        return;
    }

    RotateTowardsTarget(Enemy, DeltaTime);

    TimeSinceLastAttack += DeltaTime;
    if (TimeSinceLastAttack >= AttackCooldown)
    {
        PerformAttack(Enemy);
        TimeSinceLastAttack = 0.0f;
    }
}

void USuspenseCoreEnemyAttackState::OnFSMEvent(ASuspenseCoreEnemy* Enemy, const FGameplayTag& EventTag, AActor* Instigator)
{
    Super::OnFSMEvent(Enemy, EventTag, Instigator);
}

void USuspenseCoreEnemyAttackState::PerformAttack(ASuspenseCoreEnemy* Enemy)
{
    if (!Enemy)
    {
        return;
    }

    Enemy->ExecuteAttack();

    UE_LOG(LogEnemySystem, Verbose, TEXT("[%s] Performing attack"), *Enemy->GetName());
}

void USuspenseCoreEnemyAttackState::RotateTowardsTarget(ASuspenseCoreEnemy* Enemy, float DeltaTime)
{
    if (!Enemy)
    {
        return;
    }

    AActor* Target = GetCurrentTarget(Enemy);
    if (!Target)
    {
        return;
    }

    FVector Direction = Target->GetActorLocation() - Enemy->GetActorLocation();
    Direction.Z = 0.0f;
    Direction.Normalize();

    FRotator TargetRotation = Direction.Rotation();
    FRotator CurrentRotation = Enemy->GetActorRotation();

    FRotator NewRotation = FMath::RInterpConstantTo(
        CurrentRotation,
        TargetRotation,
        DeltaTime,
        RotationSpeed
    );

    Enemy->SetActorRotation(NewRotation);
}

bool USuspenseCoreEnemyAttackState::IsTargetInAttackRange(ASuspenseCoreEnemy* Enemy) const
{
    AActor* Target = GetCurrentTarget(Enemy);
    if (!Target)
    {
        return false;
    }

    return GetDistanceToTarget(Enemy, Target) <= AttackRange;
}
```

### Death State

**Файл:** `Source/EnemySystem/Public/SuspenseCore/FSM/States/SuspenseCoreEnemyDeathState.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyStateBase.h"
#include "SuspenseCoreEnemyDeathState.generated.h"

UCLASS()
class ENEMYSYSTEM_API USuspenseCoreEnemyDeathState : public USuspenseCoreEnemyStateBase
{
    GENERATED_BODY()

public:
    USuspenseCoreEnemyDeathState();

    virtual void OnEnterState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnExitState(ASuspenseCoreEnemy* Enemy) override;
    virtual void OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Death")
    float DespawnDelay;

    UPROPERTY(EditDefaultsOnly, Category = "Death")
    bool bEnableRagdoll;

    void EnableRagdoll(ASuspenseCoreEnemy* Enemy);
    void ScheduleDespawn(ASuspenseCoreEnemy* Enemy);
};
```

**Файл:** `Source/EnemySystem/Private/SuspenseCore/FSM/States/SuspenseCoreEnemyDeathState.cpp`

```cpp
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyDeathState.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemy.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnemySystem.h"

USuspenseCoreEnemyDeathState::USuspenseCoreEnemyDeathState()
{
    StateTag = SuspenseCoreEnemyTags::State::Death;
    DespawnDelay = 10.0f;
    bEnableRagdoll = true;
}

void USuspenseCoreEnemyDeathState::OnEnterState(ASuspenseCoreEnemy* Enemy)
{
    Super::OnEnterState(Enemy);

    if (!Enemy)
    {
        return;
    }

    Enemy->StopMovement();

    if (UCharacterMovementComponent* MovementComp = Enemy->GetCharacterMovement())
    {
        MovementComp->DisableMovement();
        MovementComp->StopMovementImmediately();
    }

    if (UCapsuleComponent* CapsuleComp = Enemy->GetCapsuleComponent())
    {
        CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (bEnableRagdoll)
    {
        EnableRagdoll(Enemy);
    }

    if (AController* Controller = Enemy->GetController())
    {
        Enemy->DetachFromControllerPendingDestroy();
    }

    ScheduleDespawn(Enemy);

    UE_LOG(LogEnemySystem, Log, TEXT("[%s] Entered death state"), *Enemy->GetName());
}

void USuspenseCoreEnemyDeathState::OnExitState(ASuspenseCoreEnemy* Enemy)
{
    Super::OnExitState(Enemy);
}

void USuspenseCoreEnemyDeathState::OnTickState(ASuspenseCoreEnemy* Enemy, float DeltaTime)
{
    Super::OnTickState(Enemy, DeltaTime);
}

void USuspenseCoreEnemyDeathState::EnableRagdoll(ASuspenseCoreEnemy* Enemy)
{
    if (!Enemy)
    {
        return;
    }

    USkeletalMeshComponent* MeshComp = Enemy->GetMesh();
    if (MeshComp)
    {
        MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        MeshComp->SetSimulatePhysics(true);
    }
}

void USuspenseCoreEnemyDeathState::ScheduleDespawn(ASuspenseCoreEnemy* Enemy)
{
    if (!Enemy || DespawnDelay <= 0.0f)
    {
        return;
    }

    FTimerHandle DespawnTimerHandle;
    Enemy->GetWorldTimerManager().SetTimer(
        DespawnTimerHandle,
        [Enemy]()
        {
            if (Enemy && IsValid(Enemy))
            {
                Enemy->Destroy();
            }
        },
        DespawnDelay,
        false
    );
}
```

---

## Шаг 9: Enemy Character

**Файл:** `Source/EnemySystem/Public/SuspenseCore/Characters/SuspenseCoreEnemy.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEnemy.generated.h"

class ASuspenseCoreEnemyState;
class USuspenseCoreEnemyFSMComponent;
class USuspenseCoreEnemyBehaviorData;
class UAbilitySystemComponent;

UCLASS()
class ENEMYSYSTEM_API ASuspenseCoreEnemy : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ASuspenseCoreEnemy();

    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void UnPossessed() override;

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void InitializeEnemy(USuspenseCoreEnemyBehaviorData* BehaviorData);

    UFUNCTION(BlueprintPure, Category = "Enemy")
    bool IsAlive() const;

    UFUNCTION(BlueprintPure, Category = "Enemy")
    FGameplayTag GetCurrentStateTag() const;

    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void StopMovement();

    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void ExecuteAttack();

    AActor* GetCurrentTarget() const;
    void SetCurrentTarget(AActor* NewTarget);

    UFUNCTION(BlueprintCallable, Category = "Enemy")
    void OnPerceptionUpdated(AActor* Actor, bool bIsSensed);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USuspenseCoreEnemyFSMComponent> FSMComponent;

    UPROPERTY()
    TObjectPtr<ASuspenseCoreEnemyState> EnemyState;

    UPROPERTY()
    TWeakObjectPtr<AActor> CurrentTarget;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
    TObjectPtr<USuspenseCoreEnemyBehaviorData> DefaultBehaviorData;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Enemy")
    FGameplayTag EnemyTypeTag;

    void SetupAbilitySystem();
};
```

**Файл:** `Source/EnemySystem/Private/SuspenseCore/Characters/SuspenseCoreEnemy.cpp`

```cpp
#include "SuspenseCore/Characters/SuspenseCoreEnemy.h"
#include "SuspenseCore/Core/SuspenseCoreEnemyState.h"
#include "SuspenseCore/FSM/SuspenseCoreEnemyFSMComponent.h"
#include "SuspenseCore/Data/SuspenseCoreEnemyBehaviorData.h"
#include "SuspenseCore/Attributes/SuspenseCoreEnemyAttributeSet.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "EnemySystem.h"

ASuspenseCoreEnemy::ASuspenseCoreEnemy()
{
    PrimaryActorTick.bCanEverTick = false;

    FSMComponent = CreateDefaultSubobject<USuspenseCoreEnemyFSMComponent>(TEXT("FSMComponent"));

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    EnemyTypeTag = SuspenseCoreEnemyTags::Type::Scav;
}

void ASuspenseCoreEnemy::BeginPlay()
{
    Super::BeginPlay();

    if (DefaultBehaviorData)
    {
        InitializeEnemy(DefaultBehaviorData);
    }
}

void ASuspenseCoreEnemy::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    SetupAbilitySystem();
}

void ASuspenseCoreEnemy::UnPossessed()
{
    Super::UnPossessed();
}

UAbilitySystemComponent* ASuspenseCoreEnemy::GetAbilitySystemComponent() const
{
    if (EnemyState)
    {
        return EnemyState->GetAbilitySystemComponent();
    }
    return nullptr;
}

void ASuspenseCoreEnemy::InitializeEnemy(USuspenseCoreEnemyBehaviorData* BehaviorData)
{
    if (!BehaviorData)
    {
        UE_LOG(LogEnemySystem, Error, TEXT("[%s] Cannot initialize: BehaviorData is null"), *GetName());
        return;
    }

    if (FSMComponent)
    {
        FSMComponent->Initialize(BehaviorData);
    }

    if (EnemyState && BehaviorData->StartupAbilities.Num() > 0)
    {
        EnemyState->InitializeAbilities(BehaviorData->StartupAbilities);
    }

    if (EnemyState && BehaviorData->StartupEffects.Num() > 0)
    {
        EnemyState->ApplyStartupEffects(BehaviorData->StartupEffects);
    }

    UE_LOG(LogEnemySystem, Log, TEXT("[%s] Enemy initialized with behavior: %s"),
        *GetName(), *BehaviorData->GetName());
}

void ASuspenseCoreEnemy::SetupAbilitySystem()
{
    AController* MyController = GetController();
    if (!MyController)
    {
        return;
    }

    EnemyState = MyController->GetPlayerState<ASuspenseCoreEnemyState>();
    if (!EnemyState)
    {
        UE_LOG(LogEnemySystem, Warning, TEXT("[%s] EnemyState not found on controller"), *GetName());
        return;
    }

    UAbilitySystemComponent* AbilitySystemComponent = EnemyState->GetAbilitySystemComponent();
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->InitAbilityActorInfo(EnemyState, this);
    }
}

bool ASuspenseCoreEnemy::IsAlive() const
{
    if (!EnemyState)
    {
        return true;
    }

    USuspenseCoreEnemyAttributeSet* AttributeSet = EnemyState->GetAttributeSet();
    if (AttributeSet)
    {
        return AttributeSet->IsAlive();
    }

    return true;
}

FGameplayTag ASuspenseCoreEnemy::GetCurrentStateTag() const
{
    if (FSMComponent)
    {
        return FSMComponent->GetCurrentStateTag();
    }
    return FGameplayTag();
}

void ASuspenseCoreEnemy::StopMovement()
{
    if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
    {
        MovementComp->StopMovementImmediately();
    }

    AAIController* AIController = Cast<AAIController>(GetController());
    if (AIController)
    {
        AIController->StopMovement();
    }
}

void ASuspenseCoreEnemy::ExecuteAttack()
{
    UE_LOG(LogEnemySystem, Verbose, TEXT("[%s] Executing attack on target: %s"),
        *GetName(),
        CurrentTarget.IsValid() ? *CurrentTarget->GetName() : TEXT("None"));
}

AActor* ASuspenseCoreEnemy::GetCurrentTarget() const
{
    return CurrentTarget.Get();
}

void ASuspenseCoreEnemy::SetCurrentTarget(AActor* NewTarget)
{
    CurrentTarget = NewTarget;
}

void ASuspenseCoreEnemy::OnPerceptionUpdated(AActor* Actor, bool bIsSensed)
{
    if (!FSMComponent)
    {
        return;
    }

    if (bIsSensed)
    {
        FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::PlayerDetected, Actor);
    }
    else
    {
        if (CurrentTarget.Get() == Actor)
        {
            FSMComponent->SendFSMEvent(SuspenseCoreEnemyTags::Event::PlayerLost, Actor);
        }
    }
}
```

---

## Шаг 10: AI Controller

**Файл:** `Source/EnemySystem/Public/SuspenseCore/Controllers/SuspenseCoreEnemyAIController.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "SuspenseCoreEnemyAIController.generated.h"

class UAIPerceptionComponent;
class ASuspenseCoreEnemy;

UCLASS()
class ENEMYSYSTEM_API ASuspenseCoreEnemyAIController : public AAIController
{
    GENERATED_BODY()

public:
    ASuspenseCoreEnemyAIController();

    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    TObjectPtr<UAIPerceptionComponent> AIPerceptionComponent;

    UPROPERTY(EditDefaultsOnly, Category = "AI")
    TSubclassOf<class ASuspenseCoreEnemyState> EnemyStateClass;

    UFUNCTION()
    void OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

    TWeakObjectPtr<ASuspenseCoreEnemy> ControlledEnemy;
};
```

**Файл:** `Source/EnemySystem/Private/SuspenseCore/Controllers/SuspenseCoreEnemyAIController.cpp`

```cpp
#include "SuspenseCore/Controllers/SuspenseCoreEnemyAIController.h"
#include "SuspenseCore/Core/SuspenseCoreEnemyState.h"
#include "SuspenseCore/Characters/SuspenseCoreEnemy.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "EnemySystem.h"

ASuspenseCoreEnemyAIController::ASuspenseCoreEnemyAIController()
{
    AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));
    SetPerceptionComponent(*AIPerceptionComponent);

    UAISenseConfig_Sight* SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    SightConfig->SightRadius = 2000.0f;
    SightConfig->LoseSightRadius = 2500.0f;
    SightConfig->PeripheralVisionAngleDegrees = 90.0f;
    SightConfig->SetMaxAge(5.0f);
    SightConfig->AutoSuccessRangeFromLastSeenLocation = 500.0f;
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

    UAISenseConfig_Hearing* HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
    HearingConfig->HearingRange = 1500.0f;
    HearingConfig->SetMaxAge(3.0f);
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;

    AIPerceptionComponent->ConfigureSense(*SightConfig);
    AIPerceptionComponent->ConfigureSense(*HearingConfig);
    AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());

    EnemyStateClass = ASuspenseCoreEnemyState::StaticClass();

    bWantsPlayerState = true;
}

void ASuspenseCoreEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    ControlledEnemy = Cast<ASuspenseCoreEnemy>(InPawn);

    if (AIPerceptionComponent)
    {
        AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
            this,
            &ASuspenseCoreEnemyAIController::OnTargetPerceptionUpdated
        );
    }

    UE_LOG(LogEnemySystem, Log, TEXT("[%s] Possessed enemy: %s"),
        *GetName(),
        InPawn ? *InPawn->GetName() : TEXT("None"));
}

void ASuspenseCoreEnemyAIController::OnUnPossess()
{
    if (AIPerceptionComponent)
    {
        AIPerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(
            this,
            &ASuspenseCoreEnemyAIController::OnTargetPerceptionUpdated
        );
    }

    ControlledEnemy.Reset();

    Super::OnUnPossess();
}

void ASuspenseCoreEnemyAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
    if (!ControlledEnemy.IsValid())
    {
        return;
    }

    bool bIsSensed = Stimulus.WasSuccessfullySensed();

    ControlledEnemy->OnPerceptionUpdated(Actor, bIsSensed);

    UE_LOG(LogEnemySystem, Verbose, TEXT("[%s] Perception updated - Actor: %s, Sensed: %s"),
        *GetName(),
        Actor ? *Actor->GetName() : TEXT("None"),
        bIsSensed ? TEXT("Yes") : TEXT("No"));
}
```

---

## Шаг 11: Behavior Data Asset

**Файл:** `Source/EnemySystem/Public/SuspenseCore/Data/SuspenseCoreEnemyBehaviorData.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreEnemyBehaviorData.generated.h"

class USuspenseCoreEnemyStateBase;
class UGameplayAbility;
class UGameplayEffect;

USTRUCT(BlueprintType)
struct FSuspenseCoreEnemyStateConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag StateTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<USuspenseCoreEnemyStateBase> StateClass;
};

USTRUCT(BlueprintType)
struct FSuspenseCoreEnemyTransitionConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag FromState;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag OnEvent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag ToState;
};

UCLASS(BlueprintType)
class ENEMYSYSTEM_API USuspenseCoreEnemyBehaviorData : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FName BehaviorID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FGameplayTag EnemyTypeTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
    FGameplayTag InitialState;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
    TArray<FSuspenseCoreEnemyStateConfig> States;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FSM")
    TArray<FSuspenseCoreEnemyTransitionConfig> Transitions;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    float MaxHealth;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    float AttackPower;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
    float Armor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
    float WalkSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement")
    float RunSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception")
    float SightRange;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception")
    float HearingRange;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
    TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
    TArray<TSubclassOf<UGameplayEffect>> StartupEffects;

    USuspenseCoreEnemyBehaviorData();
};
```

**Файл:** `Source/EnemySystem/Private/SuspenseCore/Data/SuspenseCoreEnemyBehaviorData.cpp`

```cpp
#include "SuspenseCore/Data/SuspenseCoreEnemyBehaviorData.h"
#include "SuspenseCore/Tags/SuspenseCoreEnemyTags.h"
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyIdleState.h"
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyPatrolState.h"
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyChaseState.h"
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyAttackState.h"
#include "SuspenseCore/FSM/States/SuspenseCoreEnemyDeathState.h"

USuspenseCoreEnemyBehaviorData::USuspenseCoreEnemyBehaviorData()
{
    BehaviorID = FName(TEXT("DefaultEnemy"));
    DisplayName = FText::FromString(TEXT("Default Enemy"));
    EnemyTypeTag = SuspenseCoreEnemyTags::Type::Scav;
    InitialState = SuspenseCoreEnemyTags::State::Idle;

    MaxHealth = 100.0f;
    AttackPower = 25.0f;
    Armor = 0.0f;
    WalkSpeed = 200.0f;
    RunSpeed = 500.0f;
    SightRange = 2000.0f;
    HearingRange = 1500.0f;

    FSuspenseCoreEnemyStateConfig IdleConfig;
    IdleConfig.StateTag = SuspenseCoreEnemyTags::State::Idle;
    IdleConfig.StateClass = USuspenseCoreEnemyIdleState::StaticClass();
    States.Add(IdleConfig);

    FSuspenseCoreEnemyStateConfig PatrolConfig;
    PatrolConfig.StateTag = SuspenseCoreEnemyTags::State::Patrol;
    PatrolConfig.StateClass = USuspenseCoreEnemyPatrolState::StaticClass();
    States.Add(PatrolConfig);

    FSuspenseCoreEnemyStateConfig ChaseConfig;
    ChaseConfig.StateTag = SuspenseCoreEnemyTags::State::Chase;
    ChaseConfig.StateClass = USuspenseCoreEnemyChaseState::StaticClass();
    States.Add(ChaseConfig);

    FSuspenseCoreEnemyStateConfig AttackConfig;
    AttackConfig.StateTag = SuspenseCoreEnemyTags::State::Attack;
    AttackConfig.StateClass = USuspenseCoreEnemyAttackState::StaticClass();
    States.Add(AttackConfig);

    FSuspenseCoreEnemyStateConfig DeathConfig;
    DeathConfig.StateTag = SuspenseCoreEnemyTags::State::Death;
    DeathConfig.StateClass = USuspenseCoreEnemyDeathState::StaticClass();
    States.Add(DeathConfig);

    FSuspenseCoreEnemyTransitionConfig IdleToPatrol;
    IdleToPatrol.FromState = SuspenseCoreEnemyTags::State::Idle;
    IdleToPatrol.OnEvent = SuspenseCoreEnemyTags::Event::IdleTimeout;
    IdleToPatrol.ToState = SuspenseCoreEnemyTags::State::Patrol;
    Transitions.Add(IdleToPatrol);

    FSuspenseCoreEnemyTransitionConfig IdleToChase;
    IdleToChase.FromState = SuspenseCoreEnemyTags::State::Idle;
    IdleToChase.OnEvent = SuspenseCoreEnemyTags::Event::PlayerDetected;
    IdleToChase.ToState = SuspenseCoreEnemyTags::State::Chase;
    Transitions.Add(IdleToChase);

    FSuspenseCoreEnemyTransitionConfig PatrolToChase;
    PatrolToChase.FromState = SuspenseCoreEnemyTags::State::Patrol;
    PatrolToChase.OnEvent = SuspenseCoreEnemyTags::Event::PlayerDetected;
    PatrolToChase.ToState = SuspenseCoreEnemyTags::State::Chase;
    Transitions.Add(PatrolToChase);

    FSuspenseCoreEnemyTransitionConfig ChaseToAttack;
    ChaseToAttack.FromState = SuspenseCoreEnemyTags::State::Chase;
    ChaseToAttack.OnEvent = SuspenseCoreEnemyTags::Event::TargetInRange;
    ChaseToAttack.ToState = SuspenseCoreEnemyTags::State::Attack;
    Transitions.Add(ChaseToAttack);

    FSuspenseCoreEnemyTransitionConfig ChaseToIdle;
    ChaseToIdle.FromState = SuspenseCoreEnemyTags::State::Chase;
    ChaseToIdle.OnEvent = SuspenseCoreEnemyTags::Event::PlayerLost;
    ChaseToIdle.ToState = SuspenseCoreEnemyTags::State::Idle;
    Transitions.Add(ChaseToIdle);

    FSuspenseCoreEnemyTransitionConfig AttackToChase;
    AttackToChase.FromState = SuspenseCoreEnemyTags::State::Attack;
    AttackToChase.OnEvent = SuspenseCoreEnemyTags::Event::TargetOutOfRange;
    AttackToChase.ToState = SuspenseCoreEnemyTags::State::Chase;
    Transitions.Add(AttackToChase);

    FSuspenseCoreEnemyTransitionConfig AttackToIdle;
    AttackToIdle.FromState = SuspenseCoreEnemyTags::State::Attack;
    AttackToIdle.OnEvent = SuspenseCoreEnemyTags::Event::PlayerLost;
    AttackToIdle.ToState = SuspenseCoreEnemyTags::State::Idle;
    Transitions.Add(AttackToIdle);
}
```

---

## Шаг 12: DefaultGameplayTags.ini

**Добавить в:** `Config/DefaultGameplayTags.ini`

```ini
+GameplayTagList=(Tag="State.Enemy.Idle",DevComment="Enemy idle state")
+GameplayTagList=(Tag="State.Enemy.Patrol",DevComment="Enemy patrol state")
+GameplayTagList=(Tag="State.Enemy.Alert",DevComment="Enemy alert state")
+GameplayTagList=(Tag="State.Enemy.Chase",DevComment="Enemy chase state")
+GameplayTagList=(Tag="State.Enemy.Attack",DevComment="Enemy attack state")
+GameplayTagList=(Tag="State.Enemy.Cover",DevComment="Enemy cover state")
+GameplayTagList=(Tag="State.Enemy.Retreat",DevComment="Enemy retreat state")
+GameplayTagList=(Tag="State.Enemy.Death",DevComment="Enemy death state")

+GameplayTagList=(Tag="Event.Enemy.PlayerDetected",DevComment="Enemy detected player")
+GameplayTagList=(Tag="Event.Enemy.PlayerLost",DevComment="Enemy lost player")
+GameplayTagList=(Tag="Event.Enemy.DamageTaken",DevComment="Enemy took damage")
+GameplayTagList=(Tag="Event.Enemy.ReachedDestination",DevComment="Enemy reached destination")
+GameplayTagList=(Tag="Event.Enemy.TargetInRange",DevComment="Target in attack range")
+GameplayTagList=(Tag="Event.Enemy.TargetOutOfRange",DevComment="Target out of attack range")
+GameplayTagList=(Tag="Event.Enemy.LowHealth",DevComment="Enemy health is low")
+GameplayTagList=(Tag="Event.Enemy.AmmoEmpty",DevComment="Enemy ammo is empty")
+GameplayTagList=(Tag="Event.Enemy.PatrolComplete",DevComment="Patrol point reached")
+GameplayTagList=(Tag="Event.Enemy.IdleTimeout",DevComment="Idle timeout expired")
+GameplayTagList=(Tag="Event.Enemy.Died",DevComment="Enemy died")

+GameplayTagList=(Tag="Type.Enemy.Scav",DevComment="Scav enemy type")
+GameplayTagList=(Tag="Type.Enemy.PMC",DevComment="PMC enemy type")
+GameplayTagList=(Tag="Type.Enemy.Boss",DevComment="Boss enemy type")
+GameplayTagList=(Tag="Type.Enemy.Raider",DevComment="Raider enemy type")
```

---

## Шаг 13: Обновить SuspenseCore.uplugin

**Добавить модуль в:** `SuspenseCore.uplugin`

```json
{
    "Name": "EnemySystem",
    "Type": "Runtime",
    "LoadingPhase": "Default"
}
```

---

## Настройка уровня (Level Setup)

### ОБЯЗАТЕЛЬНО: NavMesh (Navigation Mesh)

**Без NavMesh AI не сможет двигаться!** AI Controller использует `MoveToLocation()`, который требует навигационную сетку.

#### Добавление NavMesh:

1. **Откройте уровень** в Unreal Editor
2. **Place Actors Panel** → поиск `NavMeshBoundsVolume`
3. **Перетащите** NavMeshBoundsVolume на уровень
4. **Масштабируйте** его чтобы покрыть всю игровую зону (включая высоту)
5. **Build Navigation:**
   - `Build` → `Build Paths`
   - Или нажмите `P` для просмотра NavMesh (зелёная область = проходимая)
6. **Проверьте** что враги находятся внутри NavMesh области

#### Диагностика NavMesh проблем:

```
LogEnemySystem: Warning: [BP_Enemy] ChaseState: MoveToLocation FAILED! Check NavMesh!
```

Это означает:
- NavMesh отсутствует
- NavMesh не покрывает позицию врага
- NavMesh не покрывает позицию цели
- Есть препятствия блокирующие путь

#### Project Settings для NavMesh:

```
Project Settings → Navigation System:
- Agent Radius: ~35.0 (размер капсулы персонажа)
- Agent Height: ~192.0 (высота персонажа)
- Agent Max Step Height: ~45.0
```

### ОБЯЗАТЕЛЬНО: Animation Blueprint для Locomotion

**Без правильного AnimBP враг будет двигаться но анимация останется в Idle!**

#### Вариант 1: Использовать USuspenseCoreEnemyAnimInstance (рекомендуется)

EnemySystem включает готовый AnimInstance с locomotion данными:

**Файл:** `SuspenseCore/Animation/SuspenseCoreEnemyAnimInstance.h`

**Доступные переменные (BlueprintReadOnly):**
| Переменная | Тип | Описание |
|------------|-----|----------|
| `Speed` | float | Полная скорость (с вертикальной) |
| `GroundSpeed` | float | Горизонтальная скорость (для BlendSpace) |
| `NormalizedSpeed` | float | 0-1 относительно MaxWalkSpeed |
| `MovementDirection` | float | -180 to 180 относительно facing |
| `MoveForward` | float | -1 to 1 компонент вперёд |
| `MoveRight` | float | -1 to 1 компонент вправо |
| `bIsMoving` | bool | Speed > 10 |
| `bIsInAir` | bool | В воздухе |
| `bIsFalling` | bool | Падает |

**Настройка:**
1. Создайте AnimBlueprint на базе `USuspenseCoreEnemyAnimInstance`
2. В AnimGraph используйте `GroundSpeed` для BlendSpace
3. Назначьте AnimBP в Mesh компонент BP_Enemy

#### Вариант 2: Ручная настройка AnimBP

1. **Создайте AnimBlueprint** для скелета врага
2. **В Event Graph** получите скорость персонажа:
   ```
   Try Get Pawn Owner → Get Velocity → Vector Length → "Speed" variable
   ```
3. **Создайте State Machine** с состояниями:
   - `Idle` (когда Speed < 10)
   - `Walk` (когда Speed 10-200)
   - `Run` (когда Speed > 200)
4. **Или используйте Blend Space 1D:**
   - Axis: Speed (0 → 600)
   - Анимации: Idle (0), Walk (150), Run (500)

#### Быстрая проверка AnimBP:

В AnimBP Blueprint добавьте Print String с текущей скоростью:
```
Get Velocity → Vector Length → Print String
```

Если скорость = 0 при движении → проблема в коде (CharacterMovementComponent)
Если скорость > 0 но анимация Idle → проблема в AnimBP (пороги или State Machine)

#### Назначение AnimBP в Blueprint:

1. Откройте `BP_Enemy_*`
2. Выберите `Mesh` компонент
3. В Details: `Animation → Anim Class` → выберите ваш AnimBP
4. Если используете `USuspenseCoreEnemyAnimInstance` — Parent Class должен быть этот класс

---

## Верификация

### Checklist перед компиляцией

1. [ ] Все файлы созданы в правильных директориях
2. [ ] Build.cs содержит все зависимости
3. [ ] Все include пути корректны
4. [ ] GameplayTags добавлены в DefaultGameplayTags.ini
5. [ ] Модуль добавлен в .uplugin

### Компиляция

```bash
# Из корня проекта
UnrealBuildTool.exe -Project="YourProject.uproject" -Target="YourProjectEditor" -Platform=Win64 -Configuration=Development
```

### Тест работоспособности

1. Создать Blueprint `BP_TestEnemy` на базе `ASuspenseCoreEnemy`
2. Создать DataAsset `DA_TestBehavior` на базе `USuspenseCoreEnemyBehaviorData`
3. Назначить `DA_TestBehavior` в `DefaultBehaviorData`
4. **ОБЯЗАТЕЛЬНО:** Добавить `NavMeshBoundsVolume` на уровень и выполнить Build Paths
5. Назначить AnimBlueprint с поддержкой locomotion в Mesh компоненте
6. Разместить `BP_TestEnemy` на уровне (внутри NavMesh области!)
7. Запустить PIE и проверить:
   - Enemy переходит в Idle → Patrol
   - При обнаружении игрока → Chase → Attack
   - При потере игрока → Idle
   - При смерти → Death state + ragdoll

---

## Метаданные документа

| Поле | Значение |
|------|----------|
| **Версия** | 1.0 FINAL |
| **Дата** | 2026-01-26 |
| **Автор** | Claude (Technical Lead) |
| **Статус** | APPROVED FOR IMPLEMENTATION |
| **Целевой движок** | Unreal Engine 5.7 |
| **Кол-во файлов** | 30 |
| **Строк кода** | ~2,500 |

---

**КОНЕЦ ДОКУМЕНТА**
