// Copyright MedCom Team. All Rights Reserved.

#include "Attributes/MedComBaseAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"

UMedComBaseAttributeSet::UMedComBaseAttributeSet()
{
    // IMPORTANT: Initialize all attributes to 0
    // Actual initial values will be set by UMedComInitialAttributesEffect
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
    
    UE_LOG(LogTemp, Log, TEXT("UMedComBaseAttributeSet constructed - all values initialized to 0"));
}

void UMedComBaseAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UMedComBaseAttributeSet, Health);
    DOREPLIFETIME(UMedComBaseAttributeSet, MaxHealth);
    DOREPLIFETIME(UMedComBaseAttributeSet, HealthRegen);
    DOREPLIFETIME(UMedComBaseAttributeSet, Armor);
    DOREPLIFETIME(UMedComBaseAttributeSet, AttackPower);
    DOREPLIFETIME(UMedComBaseAttributeSet, MovementSpeed);
    DOREPLIFETIME(UMedComBaseAttributeSet, Stamina);
    DOREPLIFETIME(UMedComBaseAttributeSet, MaxStamina);
    DOREPLIFETIME(UMedComBaseAttributeSet, StaminaRegen);
}

void UMedComBaseAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
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

void UMedComBaseAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
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

void UMedComBaseAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComBaseAttributeSet, Health, OldHealth);
    
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

AActor* UMedComBaseAttributeSet::GetOwningActor() const
{
    if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
    {
        return ASC->GetOwnerActor();
    }
    return nullptr;
}

void UMedComBaseAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComBaseAttributeSet, MaxHealth, OldMaxHealth);
}

void UMedComBaseAttributeSet::OnRep_HealthRegen(const FGameplayAttributeData& OldHealthRegen)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComBaseAttributeSet, HealthRegen, OldHealthRegen);
}

void UMedComBaseAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldArmor)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComBaseAttributeSet, Armor, OldArmor);
}

void UMedComBaseAttributeSet::OnRep_AttackPower(const FGameplayAttributeData& OldAttackPower)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComBaseAttributeSet, AttackPower, OldAttackPower);
}

void UMedComBaseAttributeSet::OnRep_MovementSpeed(const FGameplayAttributeData& OldMovementSpeed)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComBaseAttributeSet, MovementSpeed, OldMovementSpeed);
    
    // ВАЖНО: Обновляем скорость движения при репликации на клиенте
    UpdateCharacterMovementSpeed();
}

void UMedComBaseAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldStamina)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComBaseAttributeSet, Stamina, OldStamina);
}

void UMedComBaseAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComBaseAttributeSet, MaxStamina, OldMaxStamina);
}

void UMedComBaseAttributeSet::OnRep_StaminaRegen(const FGameplayAttributeData& OldStaminaRegen)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComBaseAttributeSet, StaminaRegen, OldStaminaRegen);
}

void UMedComBaseAttributeSet::UpdateCharacterMovementSpeed()
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