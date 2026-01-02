// SuspenseCoreAmmoAttributeSet.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "SuspenseCoreAmmoAttributeSet.generated.h"

// Forward declaration for SSOT DataTable row
struct FSuspenseCoreAmmoAttributeRow;

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)   \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)                 \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)                 \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * USuspenseCoreAmmoAttributeSet
 *
 * Ammunition attributes for hardcore shooters.
 * Models realistic ammo characteristics inspired by Metro, STALKER, Tarkov, DayZ.
 */
UCLASS()
class GAS_API USuspenseCoreAmmoAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    USuspenseCoreAmmoAttributeSet();

    //================================================
    // SSOT DataTable Initialization
    //================================================

    /**
     * Initialize all attributes from DataTable row data
     * This is the SSOT method - replaces hardcoded constructor defaults
     *
     * @param RowData Ammo attribute row from DT_AmmoAttributes
     *
     * @see FSuspenseCoreAmmoAttributeRow
     * @see Documentation/Plans/SSOT_AttributeSet_DataTable_Integration.md
     */
    UFUNCTION(BlueprintCallable, Category = "Ammo|Initialization")
    void InitializeFromData(const FSuspenseCoreAmmoAttributeRow& RowData);

    /**
     * Check if attributes have been initialized from DataTable
     * @return true if InitializeFromData() has been called
     */
    UFUNCTION(BlueprintPure, Category = "Ammo|Initialization")
    bool IsInitializedFromData() const { return bInitializedFromData; }

    //================================================
    // Damage Characteristics
    //================================================

    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Damage", ReplicatedUsing = OnRep_BaseDamage)
    FGameplayAttributeData BaseDamage;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAmmoAttributeSet, BaseDamage)

    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Damage", ReplicatedUsing = OnRep_ArmorPenetration)
    FGameplayAttributeData ArmorPenetration;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAmmoAttributeSet, ArmorPenetration)

    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Damage", ReplicatedUsing = OnRep_StoppingPower)
    FGameplayAttributeData StoppingPower;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAmmoAttributeSet, StoppingPower)

    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Damage", ReplicatedUsing = OnRep_FragmentationChance)
    FGameplayAttributeData FragmentationChance;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAmmoAttributeSet, FragmentationChance)

    //================================================
    // Ballistics
    //================================================

    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Ballistics", ReplicatedUsing = OnRep_MuzzleVelocity)
    FGameplayAttributeData MuzzleVelocity;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAmmoAttributeSet, MuzzleVelocity)

    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Ballistics", ReplicatedUsing = OnRep_DragCoefficient)
    FGameplayAttributeData DragCoefficient;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAmmoAttributeSet, DragCoefficient)

    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Ballistics", ReplicatedUsing = OnRep_BulletMass)
    FGameplayAttributeData BulletMass;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAmmoAttributeSet, BulletMass)

    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Ballistics", ReplicatedUsing = OnRep_EffectiveRange)
    FGameplayAttributeData EffectiveRange;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAmmoAttributeSet, EffectiveRange)

    //================================================
    // Accuracy Modifiers
    //================================================

    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Accuracy", ReplicatedUsing = OnRep_AccuracyModifier)
    FGameplayAttributeData AccuracyModifier;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAmmoAttributeSet, AccuracyModifier)

    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Accuracy", ReplicatedUsing = OnRep_RecoilModifier)
    FGameplayAttributeData RecoilModifier;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAmmoAttributeSet, RecoilModifier)

    //================================================
    // Special Effects
    //================================================

    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Special", ReplicatedUsing = OnRep_RicochetChance)
    FGameplayAttributeData RicochetChance;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAmmoAttributeSet, RicochetChance)

    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Special", ReplicatedUsing = OnRep_TracerVisibility)
    FGameplayAttributeData TracerVisibility;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAmmoAttributeSet, TracerVisibility)

    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Special", ReplicatedUsing = OnRep_IncendiaryDamage)
    FGameplayAttributeData IncendiaryDamage;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAmmoAttributeSet, IncendiaryDamage)

    //================================================
    // Weapon Effects
    //================================================

    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Weapon", ReplicatedUsing = OnRep_WeaponDegradationRate)
    FGameplayAttributeData WeaponDegradationRate;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAmmoAttributeSet, WeaponDegradationRate)

    UPROPERTY(BlueprintReadOnly, Category = "Ammo|Weapon", ReplicatedUsing = OnRep_MisfireChance)
    FGameplayAttributeData MisfireChance;
    ATTRIBUTE_ACCESSORS(USuspenseCoreAmmoAttributeSet, MisfireChance)

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
    UFUNCTION() void OnRep_BaseDamage(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_ArmorPenetration(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_StoppingPower(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_FragmentationChance(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MuzzleVelocity(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_DragCoefficient(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_BulletMass(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_EffectiveRange(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_AccuracyModifier(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_RecoilModifier(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_RicochetChance(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_TracerVisibility(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_IncendiaryDamage(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_WeaponDegradationRate(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MisfireChance(const FGameplayAttributeData& OldValue);

private:
    /** Flag indicating whether attributes were initialized from DataTable */
    bool bInitializedFromData = false;
};
