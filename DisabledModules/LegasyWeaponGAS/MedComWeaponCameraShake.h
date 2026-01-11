// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "MedComWeaponCameraShake.generated.h"

/**
 * Паттерн для тряски камеры от оружия
 */
UCLASS()
class MEDCOMCORE_API UMedComWeaponCameraShakePattern : public UCameraShakePattern
{
    GENERATED_BODY()

public:
    // Конструктор с корректным FObjectInitializer
    UMedComWeaponCameraShakePattern(const FObjectInitializer& ObjectInitializer);

protected:
    // Структура для хранения параметров осциллятора
    struct FOscillator
    {
        float Amplitude;
        float Frequency;
        float Time;
        
        FOscillator() : Amplitude(0.f), Frequency(0.f), Time(0.f) {}
        
        float Update(float DeltaTime, float Scale)
        {
            Time += DeltaTime;
            return FMath::Sin(Time * Frequency) * Amplitude * Scale;
        }
    };

    // Осцилляторы для различных типов движения
    FOscillator PitchOscillator;
    FOscillator YawOscillator;
    FOscillator RollOscillator;
    FOscillator XLocationOscillator;
    FOscillator YLocationOscillator;
    FOscillator ZLocationOscillator;
    FOscillator FOVOscillator;

    // Длительность и параметры блендинга
    float Duration;
    float BlendInTime;
    float BlendOutTime;

    // Переопределенные методы паттерна
    virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
    virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
};

/**
 * Основной класс тряски камеры оружия
 */
UCLASS()
class MEDCOMCORE_API UMedComWeaponCameraShake : public UCameraShakeBase
{
    GENERATED_BODY()

public:
    // Конструктор с корректным FObjectInitializer
    UMedComWeaponCameraShake(const FObjectInitializer& ObjectInitializer);
};