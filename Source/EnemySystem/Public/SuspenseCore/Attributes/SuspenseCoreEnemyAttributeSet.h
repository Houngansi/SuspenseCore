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
