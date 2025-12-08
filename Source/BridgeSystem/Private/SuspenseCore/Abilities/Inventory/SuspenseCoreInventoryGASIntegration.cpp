// MedComCore/Abilities/Inventory/MedComInventoryGASIntegration.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "Abilities/Inventory/SuspenseInventoryGASIntegration.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayAbilitySpec.h"
#include "AttributeSet.h"

void USuspenseInventoryGASIntegration::Initialize(UAbilitySystemComponent* InASC)
{
    // Store reference
    ASC = InASC;
    
    // Clear maps
    ItemEffectMap.Empty();
    ItemAbilityMap.Empty();
    
    // Invalid weight effect
    WeightEffectHandle = FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle USuspenseInventoryGASIntegration::ApplyItemEffect(FName ItemID, TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
    if (!ASC || !EffectClass)
    {
        return FActiveGameplayEffectHandle();
    }
    
    // Create effect spec
    FGameplayEffectSpecHandle EffectSpecHandle = ASC->MakeOutgoingSpec(EffectClass, Level, ASC->MakeEffectContext());
    if (!EffectSpecHandle.IsValid())
    {
        return FActiveGameplayEffectHandle();
    }
    
    // Add source info
    FGameplayEffectSpec* EffectSpec = EffectSpecHandle.Data.Get();
    if (EffectSpec)
    {
        EffectSpec->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT("Data.ItemID")), static_cast<float>(ItemID.GetNumber()));
    }
    
    // Apply effect
    FActiveGameplayEffectHandle EffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data);
    
    // Add to map
    if (EffectHandle.IsValid())
    {
        ItemEffectMap.FindOrAdd(ItemID).Add(EffectHandle);
    }
    
    return EffectHandle;
}

// MedComInventory/Abilities/Inventory/MedComInventoryGASIntegration.cpp

// Исправленная функция GiveItemAbility
FGameplayAbilitySpecHandle USuspenseInventoryGASIntegration::GiveItemAbility(FName ItemID, TSubclassOf<UGameplayAbility> AbilityClass, int32 Level)
{
    if (!ASC || !AbilityClass)
    {
        return FGameplayAbilitySpecHandle();
    }
    
    // Create ability spec
    FGameplayAbilitySpec AbilitySpec(AbilityClass, Level);
    
    // Add source info
    AbilitySpec.SourceObject = this;
    
    // Правильный способ добавления тегов - используем напрямую DynamicAbilityTags
    // Это свойство помечено как устаревшее, но оно всё еще работает
    AbilitySpec.DynamicAbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.FromItem")));
    
    // Give ability
    FGameplayAbilitySpecHandle AbilityHandle = ASC->GiveAbility(AbilitySpec);
    
    // Add to map
    if (AbilityHandle.IsValid())
    {
        ItemAbilityMap.FindOrAdd(ItemID).Add(AbilityHandle);
    }
    
    return AbilityHandle;
}

bool USuspenseInventoryGASIntegration::RemoveItemEffect(FName ItemID, TSubclassOf<UGameplayEffect> EffectClass)
{
    if (!ASC || !EffectClass)
    {
        return false;
    }
    
    // Get effects for item
    TArray<FActiveGameplayEffectHandle>* Effects = ItemEffectMap.Find(ItemID);
    if (!Effects)
    {
        return false;
    }
    
    // Find matching effects
    TArray<FActiveGameplayEffectHandle> MatchingEffects;
    for (const FActiveGameplayEffectHandle& EffectHandle : *Effects)
    {
        if (EffectHandle.IsValid())
        {
            const FActiveGameplayEffect* ActiveEffect = ASC->GetActiveGameplayEffect(EffectHandle);
            if (ActiveEffect && ActiveEffect->Spec.Def->GetClass() == EffectClass)
            {
                MatchingEffects.Add(EffectHandle);
            }
        }
    }
    
    // Remove effects
    bool bAnyRemoved = false;
    for (const FActiveGameplayEffectHandle& EffectHandle : MatchingEffects)
    {
        if (ASC->RemoveActiveGameplayEffect(EffectHandle))
        {
            Effects->Remove(EffectHandle);
            bAnyRemoved = true;
        }
    }
    
    return bAnyRemoved;
}

bool USuspenseInventoryGASIntegration::RemoveItemAbility(FName ItemID, TSubclassOf<UGameplayAbility> AbilityClass)
{
    if (!ASC || !AbilityClass)
    {
        return false;
    }
    
    // Get abilities for item
    TArray<FGameplayAbilitySpecHandle>* Abilities = ItemAbilityMap.Find(ItemID);
    if (!Abilities)
    {
        return false;
    }
    
    // Find matching abilities
    TArray<FGameplayAbilitySpecHandle> MatchingAbilities;
    for (const FGameplayAbilitySpecHandle& AbilityHandle : *Abilities)
    {
        if (AbilityHandle.IsValid())
        {
            const FGameplayAbilitySpec* AbilitySpec = ASC->FindAbilitySpecFromHandle(AbilityHandle);
            if (AbilitySpec && AbilitySpec->Ability && AbilitySpec->Ability->GetClass() == AbilityClass)
            {
                MatchingAbilities.Add(AbilityHandle);
            }
        }
    }
    
    // Remove abilities
    bool bAnyRemoved = false;
    for (const FGameplayAbilitySpecHandle& AbilityHandle : MatchingAbilities)
    {
        // Исправление: ClearAbility возвращает void, а не bool, поэтому просто вызываем метод
        // и считаем операцию успешной
        ASC->ClearAbility(AbilityHandle);
        Abilities->Remove(AbilityHandle);
        bAnyRemoved = true;
    }
    
    return bAnyRemoved;
}

TArray<FActiveGameplayEffectHandle> USuspenseInventoryGASIntegration::GetActiveItemEffects(FName ItemID) const
{
    // Return effects for item or empty array
    const TArray<FActiveGameplayEffectHandle>* Effects = ItemEffectMap.Find(ItemID);
    if (Effects)
    {
        return *Effects;
    }
    
    return TArray<FActiveGameplayEffectHandle>();
}

TArray<FGameplayAbilitySpecHandle> USuspenseInventoryGASIntegration::GetActiveItemAbilities(FName ItemID) const
{
    // Return abilities for item or empty array
    const TArray<FGameplayAbilitySpecHandle>* Abilities = ItemAbilityMap.Find(ItemID);
    if (Abilities)
    {
        return *Abilities;
    }
    
    return TArray<FGameplayAbilitySpecHandle>();
}

FActiveGameplayEffectHandle USuspenseInventoryGASIntegration::ApplyWeightEffect(float MaxWeight, float CurrentWeight)
{
    if (!ASC)
    {
        return FActiveGameplayEffectHandle();
    }
    
    // Remove existing effect
    if (WeightEffectHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(WeightEffectHandle);
        WeightEffectHandle = FActiveGameplayEffectHandle();
    }
    
    // We would need an actual effect class for this, but for now we'll just make a note
    // This would actually be implemented with a proper GameplayEffect asset
    
    return WeightEffectHandle;
}

bool USuspenseInventoryGASIntegration::UpdateWeightEffect(float NewCurrentWeight)
{
    if (!ASC || !WeightEffectHandle.IsValid())
    {
        return false;
    }
    
    // This would be implemented to update the magnitude of the effect
    // For now, just a placeholder
    
    return true;
}

void USuspenseInventoryGASIntegration::ClearAllItemEffects()
{
    if (!ASC)
    {
        return;
    }
    
    // Remove all effects
    for (const auto& Pair : ItemEffectMap)
    {
        for (const FActiveGameplayEffectHandle& EffectHandle : Pair.Value)
        {
            if (EffectHandle.IsValid())
            {
                ASC->RemoveActiveGameplayEffect(EffectHandle);
            }
        }
    }
    
    // Remove all abilities
    for (const auto& Pair : ItemAbilityMap)
    {
        for (const FGameplayAbilitySpecHandle& AbilityHandle : Pair.Value)
        {
            if (AbilityHandle.IsValid())
            {
                ASC->ClearAbility(AbilityHandle);
            }
        }
    }
    
    // Clear maps
    ItemEffectMap.Empty();
    ItemAbilityMap.Empty();
    
    // Clear weight effect
    if (WeightEffectHandle.IsValid())
    {
        ASC->RemoveActiveGameplayEffect(WeightEffectHandle);
        WeightEffectHandle = FActiveGameplayEffectHandle();
    }
}