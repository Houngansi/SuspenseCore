// Copyright MedCom Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "MedComBaseAttributeSet.generated.h"

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
class MEDCOMGAS_API UMedComBaseAttributeSet : public UAttributeSet
{
    GENERATED_BODY()
public:
    UMedComBaseAttributeSet();

    // Health
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Health)
    FGameplayAttributeData Health;
    ATTRIBUTE_ACCESSORS(UMedComBaseAttributeSet, Health)

    // MaxHealth
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxHealth)
    FGameplayAttributeData MaxHealth;
    ATTRIBUTE_ACCESSORS(UMedComBaseAttributeSet, MaxHealth)

    // HealthRegen
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_HealthRegen)
    FGameplayAttributeData HealthRegen;
    ATTRIBUTE_ACCESSORS(UMedComBaseAttributeSet, HealthRegen)

    // Armor
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Armor)
    FGameplayAttributeData Armor;
    ATTRIBUTE_ACCESSORS(UMedComBaseAttributeSet, Armor)

    // AttackPower
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_AttackPower)
    FGameplayAttributeData AttackPower;
    ATTRIBUTE_ACCESSORS(UMedComBaseAttributeSet, AttackPower)

    // MovementSpeed
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MovementSpeed)
    FGameplayAttributeData MovementSpeed;
    ATTRIBUTE_ACCESSORS(UMedComBaseAttributeSet, MovementSpeed)

    // Stamina
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_Stamina)
    FGameplayAttributeData Stamina;
    ATTRIBUTE_ACCESSORS(UMedComBaseAttributeSet, Stamina)

    // MaxStamina
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_MaxStamina)
    FGameplayAttributeData MaxStamina;
    ATTRIBUTE_ACCESSORS(UMedComBaseAttributeSet, MaxStamina)

    // StaminaRegen
    UPROPERTY(BlueprintReadOnly, Category = "Attributes", ReplicatedUsing = OnRep_StaminaRegen)
    FGameplayAttributeData StaminaRegen;
    ATTRIBUTE_ACCESSORS(UMedComBaseAttributeSet, StaminaRegen)

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