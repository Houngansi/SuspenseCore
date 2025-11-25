// AmmoAttributeSet.h
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Attributes/GASAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AmmoAttributeSet.generated.h"

// Макросы для быстрого доступа к атрибутам
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)   \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)                 \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)                 \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Атрибуты патронов для хардкорных шутеров
 * 
 * Эта система моделирует реалистичные характеристики боеприпасов,
 * вдохновленные такими играми как Metro, STALKER, Escape from Tarkov и DayZ.
 * 
 * Каждый тип патронов имеет уникальные характеристики, влияющие на:
 * - Наносимый урон и пробитие брони
 * - Баллистику и точность
 * - Износ оружия
 * - Специальные эффекты (трассеры, зажигательные и т.д.)
 */
UCLASS()
class GAS_API UAmmoAttributeSet : public UGASAttributeSet
{
    GENERATED_BODY()

public:
    UAmmoAttributeSet();

    //================================================
    // Базовые характеристики урона
    //================================================
    
    /** Базовый урон патрона по незащищенным целям */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Damage", ReplicatedUsing = OnRep_BaseDamage)
    FGameplayAttributeData BaseDamage;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, BaseDamage)
    
    /** Пробитие брони (0-100%) - процент игнорируемой брони */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Damage", ReplicatedUsing = OnRep_ArmorPenetration)
    FGameplayAttributeData ArmorPenetration;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, ArmorPenetration)
    
    /** Останавливающее действие - импульс, передаваемый цели */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Damage", ReplicatedUsing = OnRep_StoppingPower)
    FGameplayAttributeData StoppingPower;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, StoppingPower)
    
    /** Шанс фрагментации (0-100%) - разрыв пули внутри цели для доп. урона */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Damage", ReplicatedUsing = OnRep_FragmentationChance)
    FGameplayAttributeData FragmentationChance;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, FragmentationChance)
    
    /** Множитель урона при фрагментации */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Damage", ReplicatedUsing = OnRep_FragmentationDamageMultiplier)
    FGameplayAttributeData FragmentationDamageMultiplier;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, FragmentationDamageMultiplier)

    //================================================
    // Баллистические характеристики
    //================================================
    
    /** Начальная скорость пули (м/с) */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Ballistics", ReplicatedUsing = OnRep_MuzzleVelocity)
    FGameplayAttributeData MuzzleVelocity;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, MuzzleVelocity)
    
    /** Коэффициент сопротивления воздуха - влияет на замедление пули */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Ballistics", ReplicatedUsing = OnRep_DragCoefficient)
    FGameplayAttributeData DragCoefficient;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, DragCoefficient)
    
    /** Масса пули (граммы) - влияет на баллистику и пробитие */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Ballistics", ReplicatedUsing = OnRep_BulletMass)
    FGameplayAttributeData BulletMass;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, BulletMass)
    
    /** Эффективная дальность (метры) - дистанция без значительной потери урона */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Ballistics", ReplicatedUsing = OnRep_EffectiveRange)
    FGameplayAttributeData EffectiveRange;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, EffectiveRange)
    
    /** Максимальная дальность (метры) */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Ballistics", ReplicatedUsing = OnRep_MaxRange)
    FGameplayAttributeData MaxRange;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, MaxRange)

    //================================================
    // Характеристики точности
    //================================================
    
    /** Модификатор точности (-100% до +100%) */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Accuracy", ReplicatedUsing = OnRep_AccuracyModifier)
    FGameplayAttributeData AccuracyModifier;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, AccuracyModifier)
    
    /** Модификатор отдачи (-100% до +100%) */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Accuracy", ReplicatedUsing = OnRep_RecoilModifier)
    FGameplayAttributeData RecoilModifier;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, RecoilModifier)

    //================================================
    // Специальные эффекты
    //================================================
    
    /** Шанс рикошета (0-100%) от твердых поверхностей */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Special", ReplicatedUsing = OnRep_RicochetChance)
    FGameplayAttributeData RicochetChance;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, RicochetChance)
    
    /** Видимость трассера (0-100%) - 0 = невидимый, 100 = яркий трассер */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Special", ReplicatedUsing = OnRep_TracerVisibility)
    FGameplayAttributeData TracerVisibility;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, TracerVisibility)
    
    /** Зажигательный урон в секунду */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Special", ReplicatedUsing = OnRep_IncendiaryDamagePerSecond)
    FGameplayAttributeData IncendiaryDamagePerSecond;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, IncendiaryDamagePerSecond)
    
    /** Длительность горения (секунды) */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Special", ReplicatedUsing = OnRep_IncendiaryDuration)
    FGameplayAttributeData IncendiaryDuration;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, IncendiaryDuration)

    //================================================
    // Влияние на оружие
    //================================================
    
    /** Износ оружия за выстрел (0-100) */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Weapon", ReplicatedUsing = OnRep_WeaponDegradationRate)
    FGameplayAttributeData WeaponDegradationRate;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, WeaponDegradationRate)
    
    /** Шанс осечки (0-100%) - для старых/некачественных патронов */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Weapon", ReplicatedUsing = OnRep_MisfireChance)
    FGameplayAttributeData MisfireChance;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, MisfireChance)
    
    /** Шанс заклинивания (0-100%) */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Weapon", ReplicatedUsing = OnRep_JamChance)
    FGameplayAttributeData JamChance;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, JamChance)

    //================================================
    // Экономические характеристики
    //================================================
    
    /** Вес одного патрона (граммы) */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Economy", ReplicatedUsing = OnRep_AmmoWeight)
    FGameplayAttributeData AmmoWeight;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, AmmoWeight)
    
    /** Уровень шума выстрела (дБ) - для стелс-механик */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Stealth", ReplicatedUsing = OnRep_NoiseLevel)
    FGameplayAttributeData NoiseLevel;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, NoiseLevel)

    //================================================
    // Параметры магазина (для конкретного оружия)
    //================================================
    
    /** Размер магазина для данного типа патронов в конкретном оружии */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Magazine", ReplicatedUsing = OnRep_MagazineSize)
    FGameplayAttributeData MagazineSize;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, MagazineSize)
    
    /** Время перезарядки с данным типом патронов */
    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Magazine", ReplicatedUsing = OnRep_ReloadTime)
    FGameplayAttributeData ReloadTime;
    ATTRIBUTE_ACCESSORS(UAmmoAttributeSet, ReloadTime)

    // Репликация
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    
    // Валидация значений перед изменением
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    
    // Обработка после применения эффекта
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
    // Обработчики репликации
    UFUNCTION() virtual void OnRep_BaseDamage(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ArmorPenetration(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_StoppingPower(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_FragmentationChance(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_FragmentationDamageMultiplier(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MuzzleVelocity(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_DragCoefficient(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_BulletMass(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_EffectiveRange(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MaxRange(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_AccuracyModifier(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_RecoilModifier(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_RicochetChance(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_TracerVisibility(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_IncendiaryDamagePerSecond(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_IncendiaryDuration(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_WeaponDegradationRate(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MisfireChance(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_JamChance(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_AmmoWeight(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_NoiseLevel(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_MagazineSize(const FGameplayAttributeData& OldValue);
    UFUNCTION() virtual void OnRep_ReloadTime(const FGameplayAttributeData& OldValue);

    /**
     * Вспомогательный метод для получения актора-владельца AttributeSet
     * Используется для генерации событий и получения контекста
     * @return Актор-владелец или nullptr
     */
    AActor* GetOwningActor() const;
};