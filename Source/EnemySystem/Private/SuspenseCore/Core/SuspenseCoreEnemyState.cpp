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

    SetNetUpdateFrequency(10.0f);
}

void ASuspenseCoreEnemyState::BeginPlay()
{
    Super::BeginPlay();

    if (AbilitySystemComponent && AttributeSet)
    {
        AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());
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
