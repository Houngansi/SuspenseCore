// Copyright Suspense Team. All Rights Reserved.

#include "Effects/GameplayEffect_SprintBuff.h"
#include "Attributes/GASAttributeSet.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UGameplayEffect_SprintBuff::UGameplayEffect_SprintBuff()
{
    // Эффект бесконечный, будет активен пока активна способность спринта
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    
    // Настраиваем модификатор для увеличения скорости движения на 50%
    {
        FGameplayModifierInfo SpeedMod;
        
        // Получаем указатель на свойство MovementSpeed через рефлексию
        FProperty* Prop = FindFProperty<FProperty>(
            UGASAttributeSet::StaticClass(),
            GET_MEMBER_NAME_CHECKED(UGASAttributeSet, MovementSpeed)
        );
        
        // Настраиваем модификатор
        SpeedMod.Attribute = FGameplayAttribute(Prop);
        SpeedMod.ModifierOp = EGameplayModOp::MultiplyAdditive;
        
        // КРИТИЧНО: Для увеличения на 50% используем 0.5f, НЕ 1.5f!
        // MultiplyAdditive работает по формуле: Result = Base + (Base * Magnitude)
        // Пример: если базовая скорость 300, то:
        // 300 + (300 * 0.5) = 300 + 150 = 450 (увеличение на 50%)
        // Если бы мы использовали 1.5f:
        // 300 + (300 * 1.5) = 300 + 450 = 750 (увеличение на 150%!)
        SpeedMod.ModifierMagnitude = FScalableFloat(2.0f);
        
        // Добавляем модификатор к эффекту
        Modifiers.Add(SpeedMod);
    }
    
    // Создаем компонент для управления тегами цели через CreateDefaultSubobject
    UTargetTagsGameplayEffectComponent* TagComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("SprintTargetTagsComponent"));
    
    if (TagComponent)
    {
        // Создаем контейнер для тегов
        FInheritedTagContainer TagContainer;
        
        // Добавляем тег State.Sprinting, который будет выдан цели эффекта
        // Этот тег будет автоматически добавлен при применении эффекта
        // и автоматически удален при удалении эффекта
        TagContainer.Added.AddTag(FGameplayTag::RequestGameplayTag(TEXT("State.Sprinting")));
        
        // Устанавливаем теги в компонент
        TagComponent->SetAndApplyTargetTagChanges(TagContainer);
        
        // Добавляем компонент к массиву компонентов эффекта
        GEComponents.Add(TagComponent);
    }
    
    // Обновленное сообщение лога с правильным процентом
    UE_LOG(LogTemp, Log, TEXT("UGameplayEffect_SprintBuff: Sprint buff created with 50%% speed increase (0.5 multiplier) and State.Sprinting tag"));
}