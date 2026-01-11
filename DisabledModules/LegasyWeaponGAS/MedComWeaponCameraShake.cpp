// Fill out your copyright notice in the Description page of Project Settings.

#include "Core/AbilitySystem/Abilities/WeaponAbilities/CameraShaik/MedComWeaponCameraShake.h"

// Имплементация паттерна тряски камеры
UMedComWeaponCameraShakePattern::UMedComWeaponCameraShakePattern(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Настройка параметров длительности
    Duration = 0.18f;
    BlendInTime = 0.0f;
    BlendOutTime = 0.12f;

    // Настройка вращательной осцилляции по оси Pitch
    PitchOscillator.Amplitude = 1.8f;        // Увеличиваем амплитуду
    PitchOscillator.Frequency = 15.0f;       // Увеличиваем частоту
    
    // Настройка вращательной осцилляции по оси Yaw
    YawOscillator.Amplitude = 0.1f;
    YawOscillator.Frequency = 12.0f;
    
    // Настройка позиционной осцилляции
    // Отдача назад
    YLocationOscillator.Amplitude = -4.0f;   // Значительно увеличиваем амплитуду отдачи назад
    YLocationOscillator.Frequency = 4.0f;    // Увеличиваем частоту для более быстрого возврата
    
    // Вертикальная составляющая отдачи
    ZLocationOscillator.Amplitude = 0.5f;
    ZLocationOscillator.Frequency = 15.0f;
    
    // Небольшое боковое смещение для реализма
    XLocationOscillator.Amplitude = 0.2f;
    XLocationOscillator.Frequency = 18.0f;
    
    // Дополнительный эффект FOV
    FOVOscillator.Amplitude = 0.2f;
    FOVOscillator.Frequency = 20.0f;
}

void UMedComWeaponCameraShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
    // Установка длительности и параметров блендинга
    OutInfo.Duration = FCameraShakeDuration(Duration);
    OutInfo.BlendIn = BlendInTime;
    OutInfo.BlendOut = BlendOutTime;
}

void UMedComWeaponCameraShakePattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
    const float DeltaTime = Params.DeltaTime;
    const float Scale = Params.GetTotalScale();
    
    // Применяем осцилляцию к повороту (вращение)
    OutResult.Rotation.Pitch = PitchOscillator.Update(DeltaTime, Scale);
    OutResult.Rotation.Yaw = YawOscillator.Update(DeltaTime, Scale);
    OutResult.Rotation.Roll = RollOscillator.Update(DeltaTime, Scale);
    
    // Применяем осцилляцию к позиции (смещение)
    OutResult.Location.X = XLocationOscillator.Update(DeltaTime, Scale);
    OutResult.Location.Y = YLocationOscillator.Update(DeltaTime, Scale);
    OutResult.Location.Z = ZLocationOscillator.Update(DeltaTime, Scale);
    
    // Применяем осцилляцию к полю зрения
    OutResult.FOV = FOVOscillator.Update(DeltaTime, Scale);
}

// Имплементация класса тряски камеры с правильным созданием подобъекта
UMedComWeaponCameraShake::UMedComWeaponCameraShake(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Правильное создание паттерна как подобъекта с использованием ObjectInitializer
    // Обратите внимание: базовый класс UCameraShakeBase уже создает RootShakePattern в своем конструкторе,
    // но нам нужна собственная реализация паттерна, поэтому мы используем RootShakePattern, созданный родителем,
    // и затем устанавливаем наш собственный паттерн
    
    // Создаем новый паттерн и устанавливаем его как корневой
    UMedComWeaponCameraShakePattern* ShakePattern = 
        ObjectInitializer.CreateDefaultSubobject<UMedComWeaponCameraShakePattern>(
            this, 
            TEXT("WeaponShakePattern")
        );
    
    SetRootShakePattern(ShakePattern);
}