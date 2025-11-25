// Copyright Suspense Team. All Rights Reserved.

#include "Effects/InitialAttributesEffect.h"
#include "Attributes/GASAttributeSet.h"

UInitialAttributesEffect::UInitialAttributesEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    
    // CRITICAL: This is the ONLY place where initial attribute values should be set!
    // Do not set initial values in AttributeSet constructor
    
    // Health
    {
        FGameplayModifierInfo ModifierInfo;
        ModifierInfo.Attribute = UGASAttributeSet::GetHealthAttribute();
        ModifierInfo.ModifierOp = EGameplayModOp::Override;
        ModifierInfo.ModifierMagnitude = FScalableFloat(100.0f);
        Modifiers.Add(ModifierInfo);
    }
    
    // MaxHealth
    {
        FGameplayModifierInfo ModifierInfo;
        ModifierInfo.Attribute = UGASAttributeSet::GetMaxHealthAttribute();
        ModifierInfo.ModifierOp = EGameplayModOp::Override;
        ModifierInfo.ModifierMagnitude = FScalableFloat(100.0f);
        Modifiers.Add(ModifierInfo);
    }
    
    // HealthRegen
    {
        FGameplayModifierInfo ModifierInfo;
        ModifierInfo.Attribute = UGASAttributeSet::GetHealthRegenAttribute();
        ModifierInfo.ModifierOp = EGameplayModOp::Override;
        ModifierInfo.ModifierMagnitude = FScalableFloat(1.0f);
        Modifiers.Add(ModifierInfo);
    }
    
    // Armor
    {
        FGameplayModifierInfo ModifierInfo;
        ModifierInfo.Attribute = UGASAttributeSet::GetArmorAttribute();
        ModifierInfo.ModifierOp = EGameplayModOp::Override;
        ModifierInfo.ModifierMagnitude = FScalableFloat(0.0f);
        Modifiers.Add(ModifierInfo);
    }
    
    // AttackPower
    {
        FGameplayModifierInfo ModifierInfo;
        ModifierInfo.Attribute = UGASAttributeSet::GetAttackPowerAttribute();
        ModifierInfo.ModifierOp = EGameplayModOp::Override;
        ModifierInfo.ModifierMagnitude = FScalableFloat(10.0f);
        Modifiers.Add(ModifierInfo);
    }
    
    // MovementSpeed - FIXED to correct value!
    {
        FGameplayModifierInfo ModifierInfo;
        ModifierInfo.Attribute = UGASAttributeSet::GetMovementSpeedAttribute();
        ModifierInfo.ModifierOp = EGameplayModOp::Override;
        ModifierInfo.ModifierMagnitude = FScalableFloat(300.0f);
        Modifiers.Add(ModifierInfo);
    }
    
    // Stamina
    {
        FGameplayModifierInfo ModifierInfo;
        ModifierInfo.Attribute = UGASAttributeSet::GetStaminaAttribute();
        ModifierInfo.ModifierOp = EGameplayModOp::Override;
        ModifierInfo.ModifierMagnitude = FScalableFloat(100.0f);
        Modifiers.Add(ModifierInfo);
    }
    
    // MaxStamina
    {
        FGameplayModifierInfo ModifierInfo;
        ModifierInfo.Attribute = UGASAttributeSet::GetMaxStaminaAttribute();
        ModifierInfo.ModifierOp = EGameplayModOp::Override;
        ModifierInfo.ModifierMagnitude = FScalableFloat(100.0f);
        Modifiers.Add(ModifierInfo);
    }
    
    // StaminaRegen
    {
        FGameplayModifierInfo ModifierInfo;
        ModifierInfo.Attribute = UGASAttributeSet::GetStaminaRegenAttribute();
        ModifierInfo.ModifierOp = EGameplayModOp::Override;
        ModifierInfo.ModifierMagnitude = FScalableFloat(5.0f);
        Modifiers.Add(ModifierInfo);
    }
    
    UE_LOG(LogTemp, Warning, TEXT("UInitialAttributesEffect created - MovementSpeed will be set to 300"));
}