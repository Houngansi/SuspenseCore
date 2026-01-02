// SuspenseCoreWeaponAttributeSet.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "SuspenseCoreWeaponAttributeSet.generated.h"

// Forward declaration for SSOT DataTable row
struct FSuspenseCoreWeaponAttributeRow;

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName)   \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName)                 \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName)                 \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * USuspenseCoreWeaponAttributeSet
 *
 * Weapon attributes for hardcore FPS mechanics.
 * Models realistic firearm characteristics inspired by Tarkov, STALKER, DayZ.
 */
UCLASS()
class GAS_API USuspenseCoreWeaponAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:
    USuspenseCoreWeaponAttributeSet();

    //================================================
    // SSOT DataTable Initialization
    //================================================

    /**
     * Initialize all attributes from DataTable row data
     * This is the SSOT method - replaces hardcoded constructor defaults
     *
     * @param RowData Weapon attribute row from DT_WeaponAttributes
     *
     * @see FSuspenseCoreWeaponAttributeRow
     * @see Documentation/Plans/SSOT_AttributeSet_DataTable_Integration.md
     */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Initialization")
    void InitializeFromData(const FSuspenseCoreWeaponAttributeRow& RowData);

    /**
     * Check if attributes have been initialized from DataTable
     * @return true if InitializeFromData() has been called
     */
    UFUNCTION(BlueprintPure, Category = "Weapon|Initialization")
    bool IsInitializedFromData() const { return bInitializedFromData; }

    //================================================
    // Combat Characteristics
    //================================================

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Combat", ReplicatedUsing = OnRep_BaseDamage)
    FGameplayAttributeData BaseDamage;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, BaseDamage)

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Combat", ReplicatedUsing = OnRep_RateOfFire)
    FGameplayAttributeData RateOfFire;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, RateOfFire)

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Combat", ReplicatedUsing = OnRep_EffectiveRange)
    FGameplayAttributeData EffectiveRange;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, EffectiveRange)

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Combat", ReplicatedUsing = OnRep_MaxRange)
    FGameplayAttributeData MaxRange;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, MaxRange)

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Combat", ReplicatedUsing = OnRep_MagazineSize)
    FGameplayAttributeData MagazineSize;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, MagazineSize)

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Combat", ReplicatedUsing = OnRep_TacticalReloadTime)
    FGameplayAttributeData TacticalReloadTime;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, TacticalReloadTime)

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Combat", ReplicatedUsing = OnRep_FullReloadTime)
    FGameplayAttributeData FullReloadTime;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, FullReloadTime)

    //================================================
    // Accuracy Characteristics
    //================================================

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Accuracy", ReplicatedUsing = OnRep_MOA)
    FGameplayAttributeData MOA;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, MOA)

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Accuracy", ReplicatedUsing = OnRep_HipFireSpread)
    FGameplayAttributeData HipFireSpread;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, HipFireSpread)

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Accuracy", ReplicatedUsing = OnRep_AimSpread)
    FGameplayAttributeData AimSpread;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, AimSpread)

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Accuracy", ReplicatedUsing = OnRep_VerticalRecoil)
    FGameplayAttributeData VerticalRecoil;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, VerticalRecoil)

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Accuracy", ReplicatedUsing = OnRep_HorizontalRecoil)
    FGameplayAttributeData HorizontalRecoil;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, HorizontalRecoil)

    //================================================
    // Reliability & Wear
    //================================================

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Reliability", ReplicatedUsing = OnRep_Durability)
    FGameplayAttributeData Durability;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, Durability)

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Reliability", ReplicatedUsing = OnRep_MaxDurability)
    FGameplayAttributeData MaxDurability;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, MaxDurability)

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Reliability", ReplicatedUsing = OnRep_MisfireChance)
    FGameplayAttributeData MisfireChance;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, MisfireChance)

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Reliability", ReplicatedUsing = OnRep_JamChance)
    FGameplayAttributeData JamChance;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, JamChance)

    //================================================
    // Ergonomics
    //================================================

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Ergonomics", ReplicatedUsing = OnRep_Ergonomics)
    FGameplayAttributeData Ergonomics;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, Ergonomics)

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Ergonomics", ReplicatedUsing = OnRep_AimDownSightTime)
    FGameplayAttributeData AimDownSightTime;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, AimDownSightTime)

    UPROPERTY(BlueprintReadOnly, Category = "Weapon|Ergonomics", ReplicatedUsing = OnRep_WeaponWeight)
    FGameplayAttributeData WeaponWeight;
    ATTRIBUTE_ACCESSORS(USuspenseCoreWeaponAttributeSet, WeaponWeight)

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
    UFUNCTION() void OnRep_BaseDamage(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_RateOfFire(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_EffectiveRange(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MaxRange(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MagazineSize(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_TacticalReloadTime(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_FullReloadTime(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MOA(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_HipFireSpread(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_AimSpread(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_VerticalRecoil(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_HorizontalRecoil(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_Durability(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MaxDurability(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_MisfireChance(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_JamChance(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_Ergonomics(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_AimDownSightTime(const FGameplayAttributeData& OldValue);
    UFUNCTION() void OnRep_WeaponWeight(const FGameplayAttributeData& OldValue);

private:
    /** Flag indicating whether attributes were initialized from DataTable */
    bool bInitializedFromData = false;
};
