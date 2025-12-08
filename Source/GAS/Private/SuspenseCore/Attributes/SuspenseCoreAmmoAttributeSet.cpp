// SuspenseCoreAmmoAttributeSet.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Attributes/SuspenseCoreAmmoAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

USuspenseCoreAmmoAttributeSet::USuspenseCoreAmmoAttributeSet()
{
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
