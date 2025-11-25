// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GASAttributeSet.generated.h"

// Макрос для доступа к атрибутам (стандартные макросы GAS)
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)   \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)                 \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)                 \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Базовый набор атрибутов для персонажей
 * Включает здоровье, броню, стамину и другие основные характеристики
 */
UCLASS()
class GAS_API UGASAttributeSet : public UAttributeSet
{
    GENERATED_BODY()
public:
    UGASAttributeSet();

    // Health
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Health)
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(UGASAttributeSet, Health)

    // MaxHealth
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxHealth)
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(UGASAttributeSet, MaxHealth)

    // HealthRegen
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_HealthRegen)
    FGameplayAttributeData HealthRegen;
    ATTRIBUTE_ACCESSORS(UGASAttributeSet, HealthRegen)

    // Armor
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Armor)
    FGameplayAttributeData Armor;
    ATTRIBUTE_ACCESSORS(UGASAttributeSet, Armor)

    // AttackPower
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_AttackPower)
    FGameplayAttributeData AttackPower;
    ATTRIBUTE_ACCESSORS(UGASAttributeSet, AttackPower)

    // MovementSpeed
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MovementSpeed)
    FGameplayAttributeData MovementSpeed;
    ATTRIBUTE_ACCESSORS(UGASAttributeSet, MovementSpeed)

    // Stamina
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Stamina)
    FGameplayAttributeData Stamina;
    ATTRIBUTE_ACCESSORS(UGASAttributeSet, Stamina)

    // MaxStamina
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxStamina)
    FGameplayAttributeData MaxStamina;
    ATTRIBUTE_ACCESSORS(UGASAttributeSet, MaxStamina)

    // StaminaRegen
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_StaminaRegen)
    FGameplayAttributeData StaminaRegen;
    ATTRIBUTE_ACCESSORS(UGASAttributeSet, StaminaRegen)

    // Переопределяем метод для получения реплицируемых свойств
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Переопределяем PreAttributeChange для клэмпинга значений
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

    // Дополнительная обработка после применения эффекта
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

    // Получить владельца атрибутов
    AActor* GetOwningActor() const;
    // Обновляет реальную скорость движения персонажа на основе атрибута
    void UpdateCharacterMovementSpeed();
protected:
    // Обработчики репликации атрибутов
    UFUNCTION()
    virtual void OnRep_Health(const FGameplayAttributeData& OldHealth);
    
    UFUNCTION()
    virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth);
    
    UFUNCTION()
    virtual void OnRep_HealthRegen(const FGameplayAttributeData& OldHealthRegen);
    
    UFUNCTION()
    virtual void OnRep_Armor(const FGameplayAttributeData& OldArmor);
    
    UFUNCTION()
    virtual void OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower);
    
    UFUNCTION()
    virtual void OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed);

    // Репликация параметров стамины
    UFUNCTION()
    virtual void OnRep_Stamina(const FGameplayAttributeData& OldStamina);
    
    UFUNCTION()
    virtual void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina);
    
    UFUNCTION()
    virtual void OnRep_StaminaRegen(const FGameplayAttributeData& OldStaminaRegen);
    
private:

};