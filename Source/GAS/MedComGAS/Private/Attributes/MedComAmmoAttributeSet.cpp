// MedComAmmoAttributeSet.cpp
// Copyright MedCom Team. All Rights Reserved.

#include "Attributes/MedComAmmoAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UMedComAmmoAttributeSet::UMedComAmmoAttributeSet()
{
    // Инициализация базовых значений атрибутов
    // Эти значения будут переопределены данными из DataTable
    
    // Базовые характеристики урона
    BaseDamage = 25.0f;                    // Базовый урон по умолчанию
    ArmorPenetration = 0.0f;               // Без пробития брони
    StoppingPower = 100.0f;                // Стандартное останавливающее действие
    FragmentationChance = 0.0f;            // Без фрагментации
    FragmentationDamageMultiplier = 1.5f;  // 50% доп. урона при фрагментации
    
    // Баллистические характеристики
    MuzzleVelocity = 900.0f;               // Скорость пули м/с (типично для 5.56)
    DragCoefficient = 0.3f;                // Коэффициент сопротивления
    BulletMass = 4.0f;                     // Масса пули в граммах
    EffectiveRange = 300.0f;               // Эффективная дальность в метрах
    MaxRange = 1000.0f;                    // Максимальная дальность
    
    // Характеристики точности
    AccuracyModifier = 0.0f;               // Без модификации точности
    RecoilModifier = 0.0f;                 // Без модификации отдачи
    
    // Специальные эффекты
    RicochetChance = 5.0f;                 // 5% шанс рикошета
    TracerVisibility = 0.0f;               // Не трассер
    IncendiaryDamagePerSecond = 0.0f;     // Не зажигательный
    IncendiaryDuration = 0.0f;             // Без горения
    
    // Влияние на оружие
    WeaponDegradationRate = 1.0f;         // Стандартный износ
    MisfireChance = 0.0f;                  // Без осечек
    JamChance = 0.0f;                      // Без заклиниваний
    
    // Экономические характеристики
    AmmoWeight = 12.0f;                    // Вес патрона в граммах
    NoiseLevel = 140.0f;                   // Уровень шума в дБ
    
    // Параметры магазина
    MagazineSize = 30.0f;                  // Стандартный размер магазина
    ReloadTime = 2.5f;                     // Стандартное время перезарядки
}

void UMedComAmmoAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Репликация всех атрибутов патронов
    // Используем COND_None для важных боевых характеристик
    DOREPLIFETIME(UMedComAmmoAttributeSet, BaseDamage);
    DOREPLIFETIME(UMedComAmmoAttributeSet, ArmorPenetration);
    DOREPLIFETIME(UMedComAmmoAttributeSet, StoppingPower);
    DOREPLIFETIME(UMedComAmmoAttributeSet, FragmentationChance);
    DOREPLIFETIME(UMedComAmmoAttributeSet, FragmentationDamageMultiplier);
    
    // Баллистика может реплицироваться реже
    DOREPLIFETIME_CONDITION(UMedComAmmoAttributeSet, MuzzleVelocity, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComAmmoAttributeSet, DragCoefficient, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComAmmoAttributeSet, BulletMass, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComAmmoAttributeSet, EffectiveRange, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComAmmoAttributeSet, MaxRange, COND_InitialOnly);
    
    // Модификаторы точности важны для геймплея
    DOREPLIFETIME(UMedComAmmoAttributeSet, AccuracyModifier);
    DOREPLIFETIME(UMedComAmmoAttributeSet, RecoilModifier);
    
    // Специальные эффекты
    DOREPLIFETIME(UMedComAmmoAttributeSet, RicochetChance);
    DOREPLIFETIME(UMedComAmmoAttributeSet, TracerVisibility);
    DOREPLIFETIME(UMedComAmmoAttributeSet, IncendiaryDamagePerSecond);
    DOREPLIFETIME(UMedComAmmoAttributeSet, IncendiaryDuration);
    
    // Влияние на оружие
    DOREPLIFETIME_CONDITION(UMedComAmmoAttributeSet, WeaponDegradationRate, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComAmmoAttributeSet, MisfireChance, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComAmmoAttributeSet, JamChance, COND_InitialOnly);
    
    // Экономические параметры реплицируются только при инициализации
    DOREPLIFETIME_CONDITION(UMedComAmmoAttributeSet, AmmoWeight, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComAmmoAttributeSet, NoiseLevel, COND_InitialOnly);
    
    // Параметры магазина
    DOREPLIFETIME_CONDITION(UMedComAmmoAttributeSet, MagazineSize, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComAmmoAttributeSet, ReloadTime, COND_InitialOnly);
}

void UMedComAmmoAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    // Валидация значений перед применением
    // Это вызывается ДО фактического изменения атрибута
    
    Super::PreAttributeChange(Attribute, NewValue);
    
    // Ограничения для процентных значений (0-100%)
    if (Attribute == GetArmorPenetrationAttribute() ||
        Attribute == GetFragmentationChanceAttribute() ||
        Attribute == GetRicochetChanceAttribute() ||
        Attribute == GetTracerVisibilityAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
    }
    // Ограничения для модификаторов (-100% до +100%)
    else if (Attribute == GetAccuracyModifierAttribute() ||
             Attribute == GetRecoilModifierAttribute())
    {
        NewValue = FMath::Clamp(NewValue, -100.0f, 100.0f);
    }
    // Положительные значения
    else if (Attribute == GetBaseDamageAttribute() ||
             Attribute == GetStoppingPowerAttribute() ||
             Attribute == GetMuzzleVelocityAttribute() ||
             Attribute == GetBulletMassAttribute() ||
             Attribute == GetEffectiveRangeAttribute() ||
             Attribute == GetMaxRangeAttribute() ||
             Attribute == GetWeaponDegradationRateAttribute() ||
             Attribute == GetAmmoWeightAttribute() ||
             Attribute == GetNoiseLevelAttribute() ||
             Attribute == GetMagazineSizeAttribute() ||
             Attribute == GetReloadTimeAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
    // Коэффициент сопротивления (0-1)
    else if (Attribute == GetDragCoefficientAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, 1.0f);
    }
    // Множитель урона при фрагментации (минимум 1.0)
    else if (Attribute == GetFragmentationDamageMultiplierAttribute())
    {
        NewValue = FMath::Max(1.0f, NewValue);
    }
    
    // Логирование значительных изменений для отладки
    UE_LOG(LogTemp, Verbose, TEXT("AmmoAttributeSet: PreAttributeChange %s: %.2f -> %.2f"), 
           *Attribute.GetName(), Attribute.GetNumericValue(this), NewValue);
}

void UMedComAmmoAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
    
    // Обработка после применения эффекта
    // Здесь можно реагировать на изменения атрибутов
    
    const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetContext();
    AActor* SourceActor = Context.GetSourceObject() ? Cast<AActor>(Context.GetSourceObject()) : nullptr;
    AActor* TargetActor = nullptr;
    
    if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
    {
        TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
    }
    
    // Обработка изменения урона
    if (Data.EvaluatedData.Attribute == GetBaseDamageAttribute())
    {
        // Убедимся, что урон не отрицательный
        SetBaseDamage(FMath::Max(0.0f, GetBaseDamage()));
        
        UE_LOG(LogTemp, Log, TEXT("AmmoAttributeSet: BaseDamage изменён на %.1f"), GetBaseDamage());
    }
    // Обработка изменения размера магазина
    else if (Data.EvaluatedData.Attribute == GetMagazineSizeAttribute())
    {
        // Округляем размер магазина до целого числа
        SetMagazineSize(FMath::RoundToFloat(GetMagazineSize()));
        
        // Отправляем событие об изменении размера магазина
        if (TargetActor && Data.Target.AbilityActorInfo.IsValid())
        {
            FGameplayEventData Payload;
            Payload.EventTag = FGameplayTag::RequestGameplayTag("Event.Ammo.MagazineSizeChanged");
            Payload.EventMagnitude = GetMagazineSize();
            Payload.Target = TargetActor;
            
            if (UAbilitySystemComponent* ASC = Data.Target.AbilityActorInfo->AbilitySystemComponent.Get())
            {
                ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
            }
        }
    }
    // Обработка изменения качества патронов (влияет на осечки)
    else if (Data.EvaluatedData.Attribute == GetMisfireChanceAttribute() ||
             Data.EvaluatedData.Attribute == GetJamChanceAttribute())
    {
        // Логируем предупреждение о некачественных патронах
        if (GetMisfireChance() > 10.0f || GetJamChance() > 5.0f)
        {
            UE_LOG(LogTemp, Warning, TEXT("AmmoAttributeSet: Низкое качество патронов! Осечки: %.1f%%, Заклинивания: %.1f%%"), 
                   GetMisfireChance(), GetJamChance());
        }
    }
}

// Реализация обработчиков репликации для каждого атрибута
void UMedComAmmoAttributeSet::OnRep_BaseDamage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, BaseDamage, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_ArmorPenetration(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, ArmorPenetration, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_StoppingPower(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, StoppingPower, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_FragmentationChance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, FragmentationChance, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_FragmentationDamageMultiplier(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, FragmentationDamageMultiplier, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_MuzzleVelocity(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, MuzzleVelocity, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_DragCoefficient(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, DragCoefficient, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_BulletMass(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, BulletMass, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_EffectiveRange(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, EffectiveRange, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_MaxRange(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, MaxRange, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_AccuracyModifier(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, AccuracyModifier, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_RecoilModifier(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, RecoilModifier, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_RicochetChance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, RicochetChance, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_TracerVisibility(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, TracerVisibility, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_IncendiaryDamagePerSecond(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, IncendiaryDamagePerSecond, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_IncendiaryDuration(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, IncendiaryDuration, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_WeaponDegradationRate(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, WeaponDegradationRate, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_MisfireChance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, MisfireChance, OldValue);
    
    // Предупреждение игрока о высоком шансе осечки
    if (GetMisfireChance() > 20.0f)
    {
        if (AActor* Owner = GetOwningActor())
        {
            FGameplayEventData Payload;
            Payload.EventTag = FGameplayTag::RequestGameplayTag("Event.Ammo.LowQuality");
            Payload.EventMagnitude = GetMisfireChance();
            
            if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
            {
                ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
            }
        }
    }
}

void UMedComAmmoAttributeSet::OnRep_JamChance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, JamChance, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_AmmoWeight(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, AmmoWeight, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_NoiseLevel(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, NoiseLevel, OldValue);
}

void UMedComAmmoAttributeSet::OnRep_MagazineSize(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, MagazineSize, OldValue);
    
    // При изменении размера магазина на клиенте, обновляем UI
    if (AActor* Owner = GetOwningActor())
    {
        // Отправляем событие для обновления UI
        FGameplayEventData Payload;
        Payload.EventTag = FGameplayTag::RequestGameplayTag("Event.UI.UpdateMagazineSize");
        Payload.EventMagnitude = GetMagazineSize();
        Payload.Target = Owner;
        
        if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
        {
            ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
        }
    }
}

void UMedComAmmoAttributeSet::OnRep_ReloadTime(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComAmmoAttributeSet, ReloadTime, OldValue);
}

// Вспомогательный метод для получения владельца
AActor* UMedComAmmoAttributeSet::GetOwningActor() const
{
    if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
    {
        return ASC->GetOwnerActor();
    }
    return nullptr;
}