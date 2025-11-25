// ArmorAttributeSet.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ArmorAttributeSet.generated.h"

// Макросы для быстрого доступа к атрибутам
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)   \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)                 \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)                 \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Система атрибутов брони для хардкорного тактического шутера
 * 
 * Моделирует реалистичную защиту, вдохновленную Escape from Tarkov,
 * STALKER, Ghost Recon и другими тактическими играми.
 * 
 * Включает:
 * - Зональную защиту (голова, торс, конечности)
 * - Классы брони и пробитие
 * - Износ и деградация защиты
 * - Влияние на мобильность и эргономику
 * - Специальные свойства (термозащита, радиация и т.д.)
 */
UCLASS()
class GAS_API UArmorAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UArmorAttributeSet();

    //================================================
    // Основные защитные характеристики
    //================================================
    
    /** Класс брони (1-6, где 6 - максимальная защита) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Protection", ReplicatedUsing = OnRep_ArmorClass)
    FGameplayAttributeData ArmorClass;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, ArmorClass)
    
    /** Базовая защита от физического урона */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Protection", ReplicatedUsing = OnRep_PhysicalDefense)
    FGameplayAttributeData PhysicalDefense;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, PhysicalDefense)
    
    /** Защита от баллистического урона (пули) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Protection", ReplicatedUsing = OnRep_BallisticDefense)
    FGameplayAttributeData BallisticDefense;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, BallisticDefense)
    
    /** Защита от взрывного урона */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Protection", ReplicatedUsing = OnRep_ExplosiveDefense)
    FGameplayAttributeData ExplosiveDefense;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, ExplosiveDefense)
    
    /** Эффективность против пробития (0-100%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Protection", ReplicatedUsing = OnRep_PenetrationResistance)
    FGameplayAttributeData PenetrationResistance;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, PenetrationResistance)
    
    /** Поглощение урона тупой травмы (0-100%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Protection", ReplicatedUsing = OnRep_BluntTraumaAbsorption)
    FGameplayAttributeData BluntTraumaAbsorption;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, BluntTraumaAbsorption)

    //================================================
    // Зональная защита
    //================================================
    
    /** Покрытие головы (0-100%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Coverage", ReplicatedUsing = OnRep_HeadCoverage)
    FGameplayAttributeData HeadCoverage;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, HeadCoverage)
    
    /** Покрытие торса (0-100%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Coverage", ReplicatedUsing = OnRep_TorsoCoverage)
    FGameplayAttributeData TorsoCoverage;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, TorsoCoverage)
    
    /** Покрытие рук (0-100%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Coverage", ReplicatedUsing = OnRep_ArmsCoverage)
    FGameplayAttributeData ArmsCoverage;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, ArmsCoverage)
    
    /** Покрытие ног (0-100%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Coverage", ReplicatedUsing = OnRep_LegsCoverage)
    FGameplayAttributeData LegsCoverage;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, LegsCoverage)

    //================================================
    // Состояние и износ
    //================================================
    
    /** Текущая прочность брони */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Durability", ReplicatedUsing = OnRep_Durability)
    FGameplayAttributeData Durability;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, Durability)
    
    /** Максимальная прочность */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Durability", ReplicatedUsing = OnRep_MaxDurability)
    FGameplayAttributeData MaxDurability;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, MaxDurability)
    
    /** Скорость износа за попадание */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Durability", ReplicatedUsing = OnRep_DurabilityLossRate)
    FGameplayAttributeData DurabilityLossRate;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, DurabilityLossRate)
    
    /** Эффективность ремонта (0-100%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Durability", ReplicatedUsing = OnRep_RepairEfficiency)
    FGameplayAttributeData RepairEfficiency;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, RepairEfficiency)

    //================================================
    // Влияние на мобильность
    //================================================
    
    /** Вес брони (кг) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Mobility", ReplicatedUsing = OnRep_ArmorWeight)
    FGameplayAttributeData ArmorWeight;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, ArmorWeight)
    
    /** Штраф к скорости движения (-100% до 0%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Mobility", ReplicatedUsing = OnRep_MovementSpeedPenalty)
    FGameplayAttributeData MovementSpeedPenalty;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, MovementSpeedPenalty)
    
    /** Штраф к скорости поворота (-100% до 0%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Mobility", ReplicatedUsing = OnRep_TurnRatePenalty)
    FGameplayAttributeData TurnRatePenalty;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, TurnRatePenalty)
    
    /** Штраф к выносливости (-100% до 0%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Mobility", ReplicatedUsing = OnRep_StaminaPenalty)
    FGameplayAttributeData StaminaPenalty;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, StaminaPenalty)
    
    /** Эргономика брони (0-100) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Mobility", ReplicatedUsing = OnRep_ArmorErgonomics)
    FGameplayAttributeData ArmorErgonomics;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, ArmorErgonomics)

    //================================================
    // Специальная защита
    //================================================
    
    /** Защита от термического урона (0-100%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Special", ReplicatedUsing = OnRep_ThermalProtection)
    FGameplayAttributeData ThermalProtection;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, ThermalProtection)
    
    /** Защита от электрического урона (0-100%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Special", ReplicatedUsing = OnRep_ElectricProtection)
    FGameplayAttributeData ElectricProtection;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, ElectricProtection)
    
    /** Защита от радиации (0-100%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Special", ReplicatedUsing = OnRep_RadiationProtection)
    FGameplayAttributeData RadiationProtection;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, RadiationProtection)
    
    /** Защита от химического урона (0-100%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Special", ReplicatedUsing = OnRep_ChemicalProtection)
    FGameplayAttributeData ChemicalProtection;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, ChemicalProtection)

    //================================================
    // Модификации и улучшения
    //================================================
    
    /** Количество слотов для бронепластин */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Modifications", ReplicatedUsing = OnRep_PlateSlots)
    FGameplayAttributeData PlateSlots;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, PlateSlots)
    
    /** Бонус к защите от модификаций (%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Modifications", ReplicatedUsing = OnRep_ModDefenseBonus)
    FGameplayAttributeData ModDefenseBonus;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, ModDefenseBonus)
    
    /** Бонус к мобильности от модификаций (%) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Modifications", ReplicatedUsing = OnRep_ModMobilityBonus)
    FGameplayAttributeData ModMobilityBonus;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, ModMobilityBonus)

    //================================================
    // Стелс и обнаружение
    //================================================
    
    /** Уровень шума при движении (дБ) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Stealth", ReplicatedUsing = OnRep_NoiseLevel)
    FGameplayAttributeData NoiseLevel;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, NoiseLevel)
    
    /** Тепловая сигнатура (0-100) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Stealth", ReplicatedUsing = OnRep_ThermalSignature)
    FGameplayAttributeData ThermalSignature;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, ThermalSignature)
    
    /** Визуальная заметность (0-100) */
    UPROPERTY(BlueprintReadOnly, Category = "Armor|Stealth", ReplicatedUsing = OnRep_VisualProfile)
    FGameplayAttributeData VisualProfile;
    ATTRIBUTE_ACCESSORS(UArmorAttributeSet, VisualProfile)

    // Репликация
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    
    // Валидация значений
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    
    // Обработка после эффектов
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
    
    // Получить владельца
    AActor* GetOwningActor() const;

protected:
    // Обработчики репликации
    UFUNCTION() virtual void OnRep_ArmorClass(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_PhysicalDefense(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_BallisticDefense(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ExplosiveDefense(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_PenetrationResistance(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_BluntTraumaAbsorption(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_HeadCoverage(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_TorsoCoverage(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ArmsCoverage(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_LegsCoverage(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_Durability(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MaxDurability(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_DurabilityLossRate(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_RepairEfficiency(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ArmorWeight(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MovementSpeedPenalty(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_TurnRatePenalty(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_StaminaPenalty(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ArmorErgonomics(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ThermalProtection(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ElectricProtection(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_RadiationProtection(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ChemicalProtection(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_PlateSlots(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ModDefenseBonus(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ModMobilityBonus(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_NoiseLevel(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ThermalSignature(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_VisualProfile(const FGameplayAttributeData& OldValue);
};