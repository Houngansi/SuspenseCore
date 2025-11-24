// Copyright MedCom Team. All Rights Reserved.

#include "Interfaces/Core/IMedComMovementInterface.h"
#include "Delegates/EventDelegateManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

void IMedComMovementInterface::NotifyMovementSpeedChanged(const UObject* Source, float OldSpeed, float NewSpeed, bool bIsSprinting)
{
    if (!Source)
    {
        return;
    }

    UEventDelegateManager* Manager = GetDelegateManagerStatic(Source);
    if (Manager)
    {
        // Также отправляем как событие оборудования для совместимости
        FGameplayTag EventTag;
        if (bIsSprinting && NewSpeed > OldSpeed)
        {
            EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.SprintStarted"));
        }
        else if (!bIsSprinting && NewSpeed < OldSpeed)
        {
            EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.SprintEnded"));
        }
        else
        {
            EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.SpeedChanged"));
        }

        FString EventData = FString::Printf(
            TEXT("OldSpeed:%.1f,NewSpeed:%.1f,IsSprinting:%s,SpeedMultiplier:%.2f,SpeedDelta:%.1f"), 
            OldSpeed, 
            NewSpeed, 
            bIsSprinting ? TEXT("true") : TEXT("false"),
            OldSpeed > 0 ? NewSpeed / OldSpeed : 1.0f,
            NewSpeed - OldSpeed
        );

        Manager->NotifyEquipmentEvent(const_cast<UObject*>(Source), EventTag, EventData);
        
        // Используем новый метод делегата движения
        Manager->NotifyMovementSpeedChanged(OldSpeed, NewSpeed, bIsSprinting);
    }
}

void IMedComMovementInterface::NotifyMovementStateChanged(const UObject* Source, FGameplayTag NewState, bool bIsTransitioning)
{
    if (!Source)
    {
        return;
    }

    UEventDelegateManager* Manager = GetDelegateManagerStatic(Source);
    if (Manager)
    {
        // Определяем тег события на основе нового состояния
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.StateChanged"));
        
        // Специфичные теги для разных состояний
        if (NewState.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Movement.Sprinting"))))
        {
            EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.SprintStarted"));
        }
        else if (NewState.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Movement.Walking"))))
        {
            EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.SprintEnded"));
        }
        else if (NewState.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Movement.Jumping"))))
        {
            EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.JumpStarted"));
        }
        else if (NewState.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Movement.Crouching"))))
        {
            EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.CrouchStarted"));
        }
        else if (NewState.MatchesTagExact(FGameplayTag::RequestGameplayTag(TEXT("Movement.Falling"))))
        {
            EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.FallStarted"));
        }
        
        FString EventData = FString::Printf(
            TEXT("NewState:%s,IsTransitioning:%s,Timestamp:%.3f"), 
            *NewState.ToString(),
            bIsTransitioning ? TEXT("true") : TEXT("false"),
            Source->GetWorld() ? Source->GetWorld()->GetTimeSeconds() : 0.0f
        );

        Manager->NotifyEquipmentEvent(const_cast<UObject*>(Source), EventTag, EventData);
        
        // Используем новый метод делегата движения
        Manager->NotifyMovementStateChanged(NewState, bIsTransitioning);
    }
}

void IMedComMovementInterface::NotifyJumpStateChanged(const UObject* Source, bool bIsJumping)
{
    if (!Source)
    {
        return;
    }

    UEventDelegateManager* Manager = GetDelegateManagerStatic(Source);
    if (Manager)
    {
        // Используем существующие теги!
        FGameplayTag EventTag = bIsJumping 
            ? FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.JumpStarted"))
            : FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.Landed")); // Вместо JumpEnded используем Landed!
        
        FString EventData = FString::Printf(
            TEXT("IsJumping:%s,Timestamp:%.3f"), 
            bIsJumping ? TEXT("true") : TEXT("false"),
            Source->GetWorld() ? Source->GetWorld()->GetTimeSeconds() : 0.0f
        );

        Manager->NotifyEquipmentEvent(const_cast<UObject*>(Source), EventTag, EventData);
        
        // Используем новый метод делегата движения
        Manager->NotifyJumpStateChanged(bIsJumping);
    }
}

void IMedComMovementInterface::NotifyCrouchStateChanged(const UObject* Source, bool bIsCrouching)
{
    if (!Source)
    {
        return;
    }

    UEventDelegateManager* Manager = GetDelegateManagerStatic(Source);
    if (Manager)
    {
        FGameplayTag EventTag = bIsCrouching 
            ? FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.CrouchStarted"))
            : FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.CrouchEnded"));
        
        FString EventData = FString::Printf(
            TEXT("IsCrouching:%s,Timestamp:%.3f"), 
            bIsCrouching ? TEXT("true") : TEXT("false"),
            Source->GetWorld() ? Source->GetWorld()->GetTimeSeconds() : 0.0f
        );

        Manager->NotifyEquipmentEvent(const_cast<UObject*>(Source), EventTag, EventData);
        
        // Используем новый метод делегата движения
        Manager->NotifyCrouchStateChanged(bIsCrouching);
    }
}

void IMedComMovementInterface::NotifyLanded(const UObject* Source, float ImpactVelocity)
{
    if (!Source)
    {
        return;
    }

    UEventDelegateManager* Manager = GetDelegateManagerStatic(Source);
    if (Manager)
    {
        FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Movement.Landed"));
        
        // Определяем тип приземления по силе удара
        FString LandingType = TEXT("Soft");
        if (FMath::Abs(ImpactVelocity) > 1000.0f)
        {
            LandingType = TEXT("Hard");
        }
        else if (FMath::Abs(ImpactVelocity) > 500.0f)
        {
            LandingType = TEXT("Medium");
        }
        
        FString EventData = FString::Printf(
            TEXT("ImpactVelocity:%.1f,LandingType:%s,Timestamp:%.3f"), 
            ImpactVelocity,
            *LandingType,
            Source->GetWorld() ? Source->GetWorld()->GetTimeSeconds() : 0.0f
        );

        Manager->NotifyEquipmentEvent(const_cast<UObject*>(Source), EventTag, EventData);
        
        // Также отправляем событие Event.Landed для совместимости с GameplayEvent
        FGameplayTag LandedEventTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Landed"));
        Manager->NotifyEquipmentEvent(const_cast<UObject*>(Source), LandedEventTag, EventData);
        
        // Используем новый метод делегата движения
        Manager->NotifyLanded(ImpactVelocity);
    }
}

UEventDelegateManager* IMedComMovementInterface::GetDelegateManagerStatic(const UObject* WorldContextObject)
{
    if (!WorldContextObject)
    {
        return nullptr;
    }
    
    return UEventDelegateManager::Get(WorldContextObject);
}