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
