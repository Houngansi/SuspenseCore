// Copyright Suspense Team. All Rights Reserved.

#include "Effects/GameplayEffect_JumpCost.h"
#include "Attributes/GASAttributeSet.h"
#include "GameplayEffectComponents/AssetTagsGameplayEffectComponent.h"
#include "GameplayTagContainer.h"

UGameplayEffect_JumpCost::UGameplayEffect_JumpCost()
{
    // Мгновенный эффект - применяется единоразово при активации
    DurationPolicy = EGameplayEffectDurationType::Instant;
    
    // Настраиваем модификатор стамины
    {
        FGameplayModifierInfo StaminaCost;
        StaminaCost.Attribute = UGASAttributeSet::GetStaminaAttribute();
        StaminaCost.ModifierOp = EGameplayModOp::Additive;
        
        // Используем SetByCaller для динамического определения расхода стамины
        FSetByCallerFloat SetByCallerValue;
        SetByCallerValue.DataTag = FGameplayTag::RequestGameplayTag(TEXT("Cost.Stamina"));
        SetByCallerValue.DataName = NAME_None;
        
        StaminaCost.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerValue);
        
        Modifiers.Add(StaminaCost);
    }
    
    // ВАЖНО: НЕ добавляем TargetTagsGameplayEffectComponent для мгновенных эффектов!
    // Вместо этого используем только AssetTags для идентификации самого эффекта
    
    // Добавляем компонент для управления тегами самого эффекта (не цели!)
    UAssetTagsGameplayEffectComponent* AssetTagsComponent = 
        CreateDefaultSubobject<UAssetTagsGameplayEffectComponent>(TEXT("JumpCostAssetTagsComponent"));
    
    if (AssetTagsComponent)
    {
        // Используем FInheritedTagContainer для идентификационных тегов эффекта
        FInheritedTagContainer AssetTagContainer;
        
        // Теги для идентификации эффекта в системе
        AssetTagContainer.Added.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Cost.Jump")));
        AssetTagContainer.Added.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Cost.Stamina")));
        
        // Применяем теги к компоненту
        AssetTagsComponent->SetAndApplyAssetTagChanges(AssetTagContainer);
        
        // Добавляем компонент к массиву компонентов
        GEComponents.Add(AssetTagsComponent);
    }
    
    UE_LOG(LogTemp, Log, TEXT("JumpCostEffect: Configured with SetByCaller stamina cost (Instant effect)"));
}