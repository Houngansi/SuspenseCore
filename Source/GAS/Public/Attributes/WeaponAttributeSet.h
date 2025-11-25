// WeaponAttributeSet.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "WeaponAttributeSet.generated.h"

// Макросы для быстрого доступа к атрибутам
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)   \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)                 \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)                 \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Комплексная система атрибутов оружия для хардкорного FPS
 * 
 * Моделирует реалистичные характеристики огнестрельного оружия,
 * основываясь на механиках из Escape from Tarkov, STALKER, DayZ.
 * 
 * Система включает:
 * - Боевые характеристики (урон, скорострельность, дальность)
 * - Точность и баллистику (MOA, разброс, отдача)
 * - Надёжность и износ (прочность, шанс осечки)
 * - Эргономику (скорость манипуляций, штрафы)
 * - Модульность (совместимость с обвесами)
 */
UCLASS()
class GAS_API UWeaponAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    UWeaponAttributeSet();

    //================================================
    // Боевые характеристики
    //================================================
    
    /** Базовый урон оружия (модифицируется патронами) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Combat", ReplicatedUsing = OnRep_BaseDamage)
    FGameplayAttributeData BaseDamage;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, BaseDamage)
    
    /** Скорострельность (выстрелов в минуту) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Combat", ReplicatedUsing = OnRep_RateOfFire)
    FGameplayAttributeData RateOfFire;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, RateOfFire)
    
    /** Эффективная дальность стрельбы (метры) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Combat", ReplicatedUsing = OnRep_EffectiveRange)
    FGameplayAttributeData EffectiveRange;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, EffectiveRange)
    
    /** Максимальная дальность (метры) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Combat", ReplicatedUsing = OnRep_MaxRange)
    FGameplayAttributeData MaxRange;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, MaxRange)
    
    /** Размер магазина */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Combat", ReplicatedUsing = OnRep_MagazineSize)
    FGameplayAttributeData MagazineSize;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, MagazineSize)
    
    /** Время тактической перезарядки (секунды) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Combat", ReplicatedUsing = OnRep_TacticalReloadTime)
    FGameplayAttributeData TacticalReloadTime;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, TacticalReloadTime)
    
    /** Время полной перезарядки (секунды) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Combat", ReplicatedUsing = OnRep_FullReloadTime)
    FGameplayAttributeData FullReloadTime;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, FullReloadTime)

    //================================================
    // Характеристики точности
    //================================================
    
    /** MOA (Minute of Angle) - механическая точность оружия */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Accuracy", ReplicatedUsing = OnRep_MOA)
    FGameplayAttributeData MOA;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, MOA)
    
    /** Базовый разброс от бедра (градусы) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Accuracy", ReplicatedUsing = OnRep_HipFireSpread)
    FGameplayAttributeData HipFireSpread;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, HipFireSpread)
    
    /** Разброс в прицеле (градусы) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Accuracy", ReplicatedUsing = OnRep_AimSpread)
    FGameplayAttributeData AimSpread;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, AimSpread)
    
    /** Вертикальная отдача */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Accuracy", ReplicatedUsing = OnRep_VerticalRecoil)
    FGameplayAttributeData VerticalRecoil;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, VerticalRecoil)
    
    /** Горизонтальная отдача */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Accuracy", ReplicatedUsing = OnRep_HorizontalRecoil)
    FGameplayAttributeData HorizontalRecoil;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, HorizontalRecoil)
    
    /** Скорость восстановления после отдачи */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Accuracy", ReplicatedUsing = OnRep_RecoilRecoverySpeed)
    FGameplayAttributeData RecoilRecoverySpeed;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, RecoilRecoverySpeed)
    
    /** Увеличение разброса за выстрел */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Accuracy", ReplicatedUsing = OnRep_SpreadIncreasePerShot)
    FGameplayAttributeData SpreadIncreasePerShot;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, SpreadIncreasePerShot)
    
    /** Максимальный разброс при стрельбе */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Accuracy", ReplicatedUsing = OnRep_MaxSpread)
    FGameplayAttributeData MaxSpread;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, MaxSpread)

    //================================================
    // Надёжность и износ
    //================================================
    
    /** Текущая прочность оружия (0-100) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Reliability", ReplicatedUsing = OnRep_Durability)
    FGameplayAttributeData Durability;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, Durability)
    
    /** Максимальная прочность */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Reliability", ReplicatedUsing = OnRep_MaxDurability)
    FGameplayAttributeData MaxDurability;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, MaxDurability)
    
    /** Износ за выстрел */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Reliability", ReplicatedUsing = OnRep_DurabilityLossPerShot)
    FGameplayAttributeData DurabilityLossPerShot;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, DurabilityLossPerShot)
    
    /** Базовый шанс осечки (%) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Reliability", ReplicatedUsing = OnRep_MisfireChance)
    FGameplayAttributeData MisfireChance;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, MisfireChance)
    
    /** Шанс заклинивания (%) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Reliability", ReplicatedUsing = OnRep_JamChance)
    FGameplayAttributeData JamChance;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, JamChance)
    
    /** Время устранения осечки (секунды) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Reliability", ReplicatedUsing = OnRep_MisfireClearTime)
    FGameplayAttributeData MisfireClearTime;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, MisfireClearTime)
    
    /** Время устранения заклинивания (секунды) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Reliability", ReplicatedUsing = OnRep_JamClearTime)
    FGameplayAttributeData JamClearTime;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, JamClearTime)

    //================================================
    // Эргономика
    //================================================
    
    /** Эргономика оружия (0-100) - влияет на скорость манипуляций */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Ergonomics", ReplicatedUsing = OnRep_Ergonomics)
    FGameplayAttributeData Ergonomics;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, Ergonomics)
    
    /** Время прицеливания (секунды) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Ergonomics", ReplicatedUsing = OnRep_AimDownSightTime)
    FGameplayAttributeData AimDownSightTime;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, AimDownSightTime)
    
    /** Скорость поворота в прицеле (множитель) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Ergonomics", ReplicatedUsing = OnRep_AimSensitivityMultiplier)
    FGameplayAttributeData AimSensitivityMultiplier;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, AimSensitivityMultiplier)
    
    /** Вес оружия (кг) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Ergonomics", ReplicatedUsing = OnRep_WeaponWeight)
    FGameplayAttributeData WeaponWeight;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, WeaponWeight)
    
    /** Штраф к выносливости при прицеливании */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Ergonomics", ReplicatedUsing = OnRep_StaminaDrainRate)
    FGameplayAttributeData StaminaDrainRate;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, StaminaDrainRate)
    
    /** Время смены оружия (секунды) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Ergonomics", ReplicatedUsing = OnRep_WeaponSwitchTime)
    FGameplayAttributeData WeaponSwitchTime;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, WeaponSwitchTime)

    //================================================
    // Модификации и совместимость
    //================================================
    
    /** Количество слотов для модификаций */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Modifications", ReplicatedUsing = OnRep_ModSlotCount)
    FGameplayAttributeData ModSlotCount;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, ModSlotCount)
    
    /** Бонус к точности от модификаций (%) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Modifications", ReplicatedUsing = OnRep_ModAccuracyBonus)
    FGameplayAttributeData ModAccuracyBonus;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, ModAccuracyBonus)
    
    /** Бонус к эргономике от модификаций */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Modifications", ReplicatedUsing = OnRep_ModErgonomicsBonus)
    FGameplayAttributeData ModErgonomicsBonus;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, ModErgonomicsBonus)

    //================================================
    // Специальные характеристики
    //================================================
    
    /** Уровень шума выстрела (дБ) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Special", ReplicatedUsing = OnRep_NoiseLevel)
    FGameplayAttributeData NoiseLevel;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, NoiseLevel)
    
    /** Эффективность с глушителем (0-100%) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Special", ReplicatedUsing = OnRep_SuppressorEfficiency)
    FGameplayAttributeData SuppressorEfficiency;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, SuppressorEfficiency)
    
    /** Скорость переключения режимов огня (секунды) */
    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Special", ReplicatedUsing = OnRep_FireModeSwitchTime)
    FGameplayAttributeData FireModeSwitchTime;
    ATTRIBUTE_ACCESSORS(UWeaponAttributeSet, FireModeSwitchTime)

    // Репликация
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    
    // Валидация значений
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    
    // Обработка после эффектов
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
    
    // Получить владельца
    AActor* GetOwningActor() const;

protected:
    // Обработчики репликации для каждого атрибута
    UFUNCTION() virtual void OnRep_BaseDamage(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_RateOfFire(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_EffectiveRange(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MaxRange(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MagazineSize(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_TacticalReloadTime(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_FullReloadTime(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MOA(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_HipFireSpread(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_AimSpread(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_VerticalRecoil(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_HorizontalRecoil(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_RecoilRecoverySpeed(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_SpreadIncreasePerShot(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MaxSpread(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_Durability(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MaxDurability(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_DurabilityLossPerShot(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MisfireChance(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_JamChance(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MisfireClearTime(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_JamClearTime(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_Ergonomics(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_AimDownSightTime(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_AimSensitivityMultiplier(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_WeaponWeight(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_StaminaDrainRate(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_WeaponSwitchTime(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ModSlotCount(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ModAccuracyBonus(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ModErgonomicsBonus(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_NoiseLevel(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_SuppressorEfficiency(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_FireModeSwitchTime(const FGameplayAttributeData& OldValue);
};