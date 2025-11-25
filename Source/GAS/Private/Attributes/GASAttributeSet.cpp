// Copyright Suspense Team. All Rights Reserved.

#include "Attributes/GASAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

UGASAttributeSet::UGASAttributeSet()
{
    // IMPORTANT: Initialize all attributes to 0
    // Actual initial values will be set by UInitialAttributesEffect
    // This ensures we have a single source of truth for initialization
    
    Health = 0.0f;
    MaxHealth = 0.0f;
    HealthRegen = 0.0f;
    Armor = 0.0f;
    AttackPower = 0.0f;
    MovementSpeed = 0.0f;
    Stamina = 0.0f;
    MaxStamina = 0.0f;
    StaminaRegen = 0.0f;
    
    UE_LOG(LogTemp, Log, TEXT("UGASAttributeSet constructed - all values initialized to 0"));
}

void UGASAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UGASAttributeSet, Health);
    DOREPLIFETIME(UGASAttributeSet, MaxHealth);
    DOREPLIFETIME(UGASAttributeSet, HealthRegen);
    DOREPLIFETIME(UGASAttributeSet, Armor);
    DOREPLIFETIME(UGASAttributeSet, AttackPower);
    DOREPLIFETIME(UGASAttributeSet, MovementSpeed);
    DOREPLIFETIME(UGASAttributeSet, Stamina);
    DOREPLIFETIME(UGASAttributeSet, MaxStamina);
    DOREPLIFETIME(UGASAttributeSet, StaminaRegen);
}

void UGASAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    // Всегда жёстко клампим
    if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
    }
    else if (Attribute == GetStaminaAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
    }
    else if (Attribute == GetMovementSpeedAttribute())
    {
        // Ограничиваем скорость движения разумными пределами
        NewValue = FMath::Clamp(NewValue, 0.0f, 2000.0f);
    }
}

void UGASAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    // Обработка изменений здоровья
    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        // Ограничиваем здоровье в допустимом диапазоне
        SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
        
        // Если модификатор отрицательный – значит, нанесён урон
        if (Data.EvaluatedData.Magnitude < 0.f)
        {
            // Получаем контекст эффекта
            const FGameplayEffectContextHandle& EffectContext = Data.EffectSpec.GetContext();
            
            // Определяем исходный и целевой акторы
            AActor* SourceActor = nullptr;
            AActor* TargetActor = nullptr;
            
            if (EffectContext.GetSourceObject())
            {
                SourceActor = Cast<AActor>(EffectContext.GetSourceObject());
            }
            
            if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
            {
                TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
            }
            
            // Если целевой актор найден, отправляем событие о полученном уроне
            if (TargetActor)
            {
                FGameplayEventData Payload;
                Payload.EventTag = FGameplayTag::RequestGameplayTag("Event.Damage");
                Payload.EventMagnitude = FMath::Abs(Data.EvaluatedData.Magnitude);
                Payload.Instigator = SourceActor;
                Payload.Target = TargetActor;
                
                if (Data.Target.AbilityActorInfo.IsValid() && 
                    Data.Target.AbilityActorInfo->AbilitySystemComponent.IsValid())
                {
                    Data.Target.AbilityActorInfo->AbilitySystemComponent->HandleGameplayEvent(
                        FGameplayTag::RequestGameplayTag("Event.Damage"), 
                        &Payload);
                }
                
                // Проверяем, был ли нанесён хедшот
                float HeadshotMagnitude = Data.EffectSpec.GetSetByCallerMagnitude(
                    FGameplayTag::RequestGameplayTag("Data.Damage.Headshot"), false);
                bool bWasHeadshot = (HeadshotMagnitude > 0.f);
                        
                if (bWasHeadshot)
                {
                    // Отправляем событие о хедшоте
                    FGameplayEventData HeadshotPayload;
                    HeadshotPayload.EventTag = FGameplayTag::RequestGameplayTag("Event.Damage.Headshot");
                    HeadshotPayload.Instigator = SourceActor;
                    HeadshotPayload.Target = TargetActor;
                    
                    if (Data.Target.AbilityActorInfo.IsValid() && 
                        Data.Target.AbilityActorInfo->AbilitySystemComponent.IsValid())
                    {
                        Data.Target.AbilityActorInfo->AbilitySystemComponent->HandleGameplayEvent(
                            FGameplayTag::RequestGameplayTag("Event.Damage.Headshot"), 
                            &HeadshotPayload);
                    }
                }
            }
        }
    }
    else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
    {
        // Ограничиваем значение стамины в допустимом диапазоне
        SetStamina(FMath::Clamp(GetStamina(), 0.f, GetMaxStamina()));
    }
    else if (Data.EvaluatedData.Attribute == GetMovementSpeedAttribute())
    {
        // КРИТИЧНО: При изменении атрибута скорости обновляем реальную скорость персонажа
        UpdateCharacterMovementSpeed();
        
        UE_LOG(LogTemp, Log, TEXT("AttributeSet: MovementSpeed изменён на %.1f"), GetMovementSpeed());
    }
}

void UGASAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGASAttributeSet, Health, OldHealth);
    
    // You can add additional visual or audio cues for significant health changes
    if (GetHealth() < OldHealth.GetCurrentValue() * 0.5f)
    {
        // Critical health change - maybe trigger a reaction
        AActor* OwningActor = GetOwningActor();
        if (OwningActor)
        {
            // Notify owner of significant health drop
            if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningActor))
            {
                FGameplayEventData Payload;
                Payload.EventMagnitude = GetHealth() / GetMaxHealth();
                ASC->HandleGameplayEvent(FGameplayTag::RequestGameplayTag("Event.Health.Critical"), &Payload);
            }
        }
    }
}

AActor* UGASAttributeSet::GetOwningActor() const
{
    if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
    {
        return ASC->GetOwnerActor();
    }
    return nullptr;
}

void UGASAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGASAttributeSet, MaxHealth, OldMaxHealth);
}

void UGASAttributeSet::OnRep_HealthRegen(const FGameplayAttributeData& OldHealthRegen)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGASAttributeSet, HealthRegen, OldHealthRegen);
}

void UGASAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldArmor)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGASAttributeSet, Armor, OldArmor);
}

void UGASAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGASAttributeSet, AttackPower, OldAttackPower);
}

void UGASAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGASAttributeSet, MovementSpeed, OldMovementSpeed);
    
    // ВАЖНО: Обновляем скорость движения при репликации на клиенте
    UpdateCharacterMovementSpeed();
}

void UGASAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldStamina)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGASAttributeSet, Stamina, OldStamina);
}

void UGASAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGASAttributeSet, MaxStamina, OldMaxStamina);
}

void UGASAttributeSet::OnRep_StaminaRegen(const FGameplayAttributeData& OldStaminaRegen)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UGASAttributeSet, StaminaRegen, OldStaminaRegen);
}

void UGASAttributeSet::UpdateCharacterMovementSpeed()
{
    // Get the owning actor
    AActor* Owner = GetOwningActor();
    if (!Owner)
    {
        return;
    }
    
    // Get the character (either directly or through PlayerState)
    ACharacter* Character = nullptr;
    
    if (APlayerState* PS = Cast<APlayerState>(Owner))
    {
        if (APawn* Pawn = PS->GetPawn())
        {
            Character = Cast<ACharacter>(Pawn);
        }
    }
    else
    {
        Character = Cast<ACharacter>(Owner);
    }
    
    if (!Character)
    {
        return;
    }
    
    // Get movement component
    UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
    if (!MovementComp)
    {
        return;
    }
    
    // Get the current modified speed from ASC
    UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
    if (!ASC)
    {
        return;
    }
    
    float NewSpeed = ASC->GetNumericAttribute(GetMovementSpeedAttribute());
    float OldSpeed = MovementComp->MaxWalkSpeed;
    
    // Only update if there's a significant change
    if (!FMath::IsNearlyEqual(OldSpeed, NewSpeed, 0.1f))
    {
        MovementComp->MaxWalkSpeed = NewSpeed;
        
        UE_LOG(LogTemp, Log, TEXT("UpdateCharacterMovementSpeed: %s speed changed %.1f -> %.1f"), 
            *Character->GetName(), OldSpeed, NewSpeed);
        
        // Force network update on server
        if (Character->GetLocalRole() == ROLE_Authority)
        {
            MovementComp->SetComponentTickEnabled(true);
            Character->ForceNetUpdate();
        }
    }
}