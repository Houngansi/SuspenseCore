// Copyright MedCom Team. All Rights Reserved.

#include "Effects/MedComGameplayEffect_CrouchDebuff.h"
#include "Attributes/MedComBaseAttributeSet.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UMedComGameplayEffect_CrouchDebuff::UMedComGameplayEffect_CrouchDebuff()
{
    // Эффект бесконечный, будет активен пока активна способность приседания
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    
    // Настраиваем модификатор для уменьшения скорости движения на 50%
    {
        FGameplayModifierInfo SpeedMod;
        
        // Получаем указатель на свойство MovementSpeed через рефлексию
        FProperty* Prop = FindFProperty<FProperty>(
            UMedComBaseAttributeSet::StaticClass(),
            GET_MEMBER_NAME_CHECKED(UMedComBaseAttributeSet, MovementSpeed)
        );
        
        // Настраиваем модификатор
        SpeedMod.Attribute = FGameplayAttribute(Prop);
        SpeedMod.ModifierOp = EGameplayModOp::MultiplyAdditive;
        
        // Важно: для уменьшения на 50% используем -0.5f
        // Это означает: текущая_скорость + (текущая_скорость * -0.5) = текущая_скорость * 0.5
        SpeedMod.ModifierMagnitude = FScalableFloat(-0.5f);
        
        // Добавляем модификатор к эффекту
        Modifiers.Add(SpeedMod);
    }
    
    // Создаем компонент для управления тегами цели через CreateDefaultSubobject
    UTargetTagsGameplayEffectComponent* TagComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("CrouchTargetTagsComponent"));
    
    if (TagComponent)
    {
        // Создаем контейнер для тегов
        FInheritedTagContainer TagContainer;
        
        // Добавляем тег State.Crouching, который будет выдан цели эффекта
        TagContainer.Added.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Crouching")));
        
        // Устанавливаем теги в компонент
        TagComponent->SetAndApplyTargetTagChanges(TagContainer);
        
        // Добавляем компонент к массиву компонентов эффекта
        GEComponents.Add(TagComponent);
    }
    
    UE_LOG(LogTemp, Log, TEXT("UMedComGameplayEffect_CrouchDebuff: Crouch debuff created with 50%% speed decrease and State.Crouching tag"));
}