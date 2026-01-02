// SuspenseCoreAmmoAttributeSet.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Attributes/SuspenseCoreAmmoAttributeSet.h"
#include "SuspenseCore/Types/GAS/SuspenseCoreGASAttributeRows.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

DEFINE_LOG_CATEGORY_STATIC(LogAmmoAttributeSet, Log, All);

USuspenseCoreAmmoAttributeSet::USuspenseCoreAmmoAttributeSet()
{
    // Default values (fallback if InitializeFromData is not called)
    // These will be overwritten by SSOT DataTable values
    InitBaseDamage(50.0f);
    InitArmorPenetration(20.0f);
    InitStoppingPower(50.0f);
    InitFragmentationChance(0.0f);
    InitMuzzleVelocity(800.0f);
    InitDragCoefficient(0.3f);
    InitBulletMass(9.0f);
    InitEffectiveRange(300.0f);
    InitAccuracyModifier(0.0f);
    InitRecoilModifier(0.0f);
    InitRicochetChance(10.0f);
    InitTracerVisibility(0.0f);
    InitIncendiaryDamage(0.0f);
    InitWeaponDegradationRate(0.1f);
    InitMisfireChance(0.0f);
}

void USuspenseCoreAmmoAttributeSet::InitializeFromData(const FSuspenseCoreAmmoAttributeRow& RowData)
{
    // Initialize all attributes from DataTable row (SSOT)
    // This replaces hardcoded constructor defaults with data-driven values

    // Damage attributes
    InitBaseDamage(RowData.BaseDamage);
    InitArmorPenetration(RowData.ArmorPenetration);
    InitStoppingPower(RowData.StoppingPower);
    InitFragmentationChance(RowData.FragmentationChance);

    // Ballistics attributes
    InitMuzzleVelocity(RowData.MuzzleVelocity);
    InitDragCoefficient(RowData.DragCoefficient);
    InitBulletMass(RowData.BulletMass);
    InitEffectiveRange(RowData.EffectiveRange);

    // Accuracy modifiers
    InitAccuracyModifier(RowData.AccuracyModifier);
    InitRecoilModifier(RowData.RecoilModifier);

    // Special effects
    InitRicochetChance(RowData.RicochetChance);
    InitTracerVisibility(RowData.TracerVisibility);
    InitIncendiaryDamage(RowData.IncendiaryDamage);

    // Weapon effects
    InitWeaponDegradationRate(RowData.WeaponDegradationRate);
    InitMisfireChance(RowData.MisfireChance);

    bInitializedFromData = true;

    UE_LOG(LogAmmoAttributeSet, Log,
        TEXT("InitializeFromData: AmmoID=%s, Damage=%.1f, Penetration=%.0f, Velocity=%.0f"),
        *RowData.AmmoID.ToString(),
        RowData.BaseDamage,
        RowData.ArmorPenetration,
        RowData.MuzzleVelocity);
}

void USuspenseCoreAmmoAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAmmoAttributeSet, BaseDamage, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAmmoAttributeSet, ArmorPenetration, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAmmoAttributeSet, StoppingPower, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAmmoAttributeSet, FragmentationChance, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAmmoAttributeSet, MuzzleVelocity, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAmmoAttributeSet, DragCoefficient, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAmmoAttributeSet, BulletMass, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAmmoAttributeSet, EffectiveRange, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAmmoAttributeSet, AccuracyModifier, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAmmoAttributeSet, RecoilModifier, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAmmoAttributeSet, RicochetChance, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAmmoAttributeSet, TracerVisibility, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAmmoAttributeSet, IncendiaryDamage, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAmmoAttributeSet, WeaponDegradationRate, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreAmmoAttributeSet, MisfireChance, COND_None, REPNOTIFY_Always);
}

void USuspenseCoreAmmoAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetArmorPenetrationAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
    }
    else if (Attribute == GetFragmentationChanceAttribute() ||
             Attribute == GetRicochetChanceAttribute() ||
             Attribute == GetMisfireChanceAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
    }
    else if (Attribute == GetTracerVisibilityAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
    }
}

void USuspenseCoreAmmoAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
}

void USuspenseCoreAmmoAttributeSet::OnRep_BaseDamage(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreAmmoAttributeSet::OnRep_ArmorPenetration(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreAmmoAttributeSet::OnRep_StoppingPower(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreAmmoAttributeSet::OnRep_FragmentationChance(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreAmmoAttributeSet::OnRep_MuzzleVelocity(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreAmmoAttributeSet::OnRep_DragCoefficient(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreAmmoAttributeSet::OnRep_BulletMass(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreAmmoAttributeSet::OnRep_EffectiveRange(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreAmmoAttributeSet::OnRep_AccuracyModifier(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreAmmoAttributeSet::OnRep_RecoilModifier(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreAmmoAttributeSet::OnRep_RicochetChance(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreAmmoAttributeSet::OnRep_TracerVisibility(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreAmmoAttributeSet::OnRep_IncendiaryDamage(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreAmmoAttributeSet::OnRep_WeaponDegradationRate(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreAmmoAttributeSet::OnRep_MisfireChance(const FGameplayAttributeData& OldValue) {}
