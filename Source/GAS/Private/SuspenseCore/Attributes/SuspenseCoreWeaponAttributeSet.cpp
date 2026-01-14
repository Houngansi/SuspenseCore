// SuspenseCoreWeaponAttributeSet.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Attributes/SuspenseCoreWeaponAttributeSet.h"
#include "SuspenseCore/Types/GAS/SuspenseCoreGASAttributeRows.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponAttributeSet, Log, All);

USuspenseCoreWeaponAttributeSet::USuspenseCoreWeaponAttributeSet()
{
    // Default values (fallback if InitializeFromData is not called)
    // These will be overwritten by SSOT DataTable values
    InitBaseDamage(100.0f);
    InitRateOfFire(600.0f);
    InitEffectiveRange(300.0f);
    InitMaxRange(800.0f);
    InitMagazineSize(30.0f);
    InitTacticalReloadTime(2.0f);
    InitFullReloadTime(3.0f);
    InitMOA(2.0f);
    InitHipFireSpread(5.0f);
    InitAimSpread(1.0f);
    InitVerticalRecoil(1.0f);
    InitHorizontalRecoil(0.5f);

    // Recoil Dynamics (Tarkov-Style Convergence)
    InitConvergenceSpeed(5.0f);      // 5 deg/sec return to aim
    InitConvergenceDelay(0.1f);      // 100ms delay before recovery
    InitRecoilAngleBias(0.0f);       // 0 = random horizontal
    InitRecoilPatternStrength(0.3f); // 30% pattern, 70% random

    InitDurability(100.0f);
    InitMaxDurability(100.0f);
    InitMisfireChance(0.0f);
    InitJamChance(0.0f);
    InitErgonomics(50.0f);
    InitAimDownSightTime(0.3f);
    InitWeaponWeight(3.5f);
}

void USuspenseCoreWeaponAttributeSet::InitializeFromData(const FSuspenseCoreWeaponAttributeRow& RowData)
{
    // Initialize all attributes from DataTable row (SSOT)
    // This replaces hardcoded constructor defaults with data-driven values

    // Combat attributes
    InitBaseDamage(RowData.BaseDamage);
    InitRateOfFire(RowData.RateOfFire);
    InitEffectiveRange(RowData.EffectiveRange);
    InitMaxRange(RowData.MaxRange);
    InitMagazineSize(RowData.MagazineSize);
    InitTacticalReloadTime(RowData.TacticalReloadTime);
    InitFullReloadTime(RowData.FullReloadTime);

    // Accuracy attributes
    InitMOA(RowData.MOA);
    InitHipFireSpread(RowData.HipFireSpread);
    InitAimSpread(RowData.AimSpread);
    InitVerticalRecoil(RowData.VerticalRecoil);
    InitHorizontalRecoil(RowData.HorizontalRecoil);

    // Recoil Dynamics (Tarkov-Style Convergence)
    InitConvergenceSpeed(RowData.ConvergenceSpeed);
    InitConvergenceDelay(RowData.ConvergenceDelay);
    InitRecoilAngleBias(RowData.RecoilAngleBias);
    InitRecoilPatternStrength(RowData.RecoilPatternStrength);

    // Reliability attributes
    InitDurability(RowData.Durability);
    InitMaxDurability(RowData.MaxDurability);
    InitMisfireChance(RowData.MisfireChance);
    InitJamChance(RowData.JamChance);

    // Ergonomics attributes
    InitErgonomics(RowData.Ergonomics);
    InitAimDownSightTime(RowData.AimDownSightTime);
    InitWeaponWeight(RowData.WeaponWeight);

    bInitializedFromData = true;

    UE_LOG(LogWeaponAttributeSet, Log,
        TEXT("InitializeFromData: WeaponID=%s, Damage=%.1f, ROF=%.0f, Ergonomics=%.0f"),
        *RowData.WeaponID.ToString(),
        RowData.BaseDamage,
        RowData.RateOfFire,
        RowData.Ergonomics);
}

void USuspenseCoreWeaponAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, BaseDamage, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, RateOfFire, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, EffectiveRange, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, MaxRange, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, MagazineSize, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, TacticalReloadTime, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, FullReloadTime, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, MOA, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, HipFireSpread, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, AimSpread, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, VerticalRecoil, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, HorizontalRecoil, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, ConvergenceSpeed, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, ConvergenceDelay, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, RecoilAngleBias, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, RecoilPatternStrength, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, Durability, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, MaxDurability, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, MisfireChance, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, JamChance, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, Ergonomics, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, AimDownSightTime, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(USuspenseCoreWeaponAttributeSet, WeaponWeight, COND_None, REPNOTIFY_Always);
}

void USuspenseCoreWeaponAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetDurabilityAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxDurability());
    }
    else if (Attribute == GetMisfireChanceAttribute() || Attribute == GetJamChanceAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
    }
    else if (Attribute == GetErgonomicsAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
    }
}

void USuspenseCoreWeaponAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
}

void USuspenseCoreWeaponAttributeSet::OnRep_BaseDamage(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_RateOfFire(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_EffectiveRange(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_MaxRange(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_MagazineSize(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_TacticalReloadTime(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_FullReloadTime(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_MOA(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_HipFireSpread(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_AimSpread(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_VerticalRecoil(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_HorizontalRecoil(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_ConvergenceSpeed(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_ConvergenceDelay(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_RecoilAngleBias(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_RecoilPatternStrength(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_Durability(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_MaxDurability(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_MisfireChance(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_JamChance(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_Ergonomics(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_AimDownSightTime(const FGameplayAttributeData& OldValue) {}
void USuspenseCoreWeaponAttributeSet::OnRep_WeaponWeight(const FGameplayAttributeData& OldValue) {}
