// MedComArmorAttributeSet.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "Attributes/ArmorAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UArmorAttributeSet::UArmorAttributeSet()
{
    // Основные защитные характеристики
    ArmorClass = 3.0f;                     // Класс 3 - средняя защита
    PhysicalDefense = 30.0f;               // 30% снижение физического урона
    BallisticDefense = 40.0f;              // 40% защита от пуль
    ExplosiveDefense = 20.0f;              // 20% защита от взрывов
    PenetrationResistance = 50.0f;         // 50% сопротивление пробитию
    BluntTraumaAbsorption = 60.0f;         // 60% поглощение тупой травмы
    
    // Зональная защита
    HeadCoverage = 0.0f;                   // Базовая броня не покрывает голову
    TorsoCoverage = 80.0f;                 // 80% покрытие торса
    ArmsCoverage = 0.0f;                   // Без защиты рук
    LegsCoverage = 0.0f;                   // Без защиты ног
    
    // Состояние и износ
    Durability = 100.0f;                   // Новая броня
    MaxDurability = 100.0f;                // Максимальная прочность
    DurabilityLossRate = 2.0f;             // 2% износа за попадание
    RepairEfficiency = 80.0f;              // 80% эффективность ремонта
    
    // Влияние на мобильность
    ArmorWeight = 8.0f;                    // 8 кг - средняя броня
    MovementSpeedPenalty = -15.0f;         // -15% к скорости
    TurnRatePenalty = -10.0f;              // -10% к повороту
    StaminaPenalty = -20.0f;               // -20% к выносливости
    ArmorErgonomics = 40.0f;               // Средняя эргономика
    
    // Специальная защита
    ThermalProtection = 10.0f;             // Базовая термозащита
    ElectricProtection = 5.0f;             // Минимальная электрозащита
    RadiationProtection = 0.0f;            // Без защиты от радиации
    ChemicalProtection = 0.0f;             // Без химзащиты
    
    // Модификации
    PlateSlots = 2.0f;                     // 2 слота для пластин
    ModDefenseBonus = 0.0f;                // Без модификаций
    ModMobilityBonus = 0.0f;               // Без модификаций
    
    // Стелс
    NoiseLevel = 50.0f;                    // Средний уровень шума
    ThermalSignature = 70.0f;              // Заметная тепловая сигнатура
    VisualProfile = 60.0f;                 // Средняя заметность
}

void UArmorAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Критические защитные параметры - всегда реплицируются
    DOREPLIFETIME(UArmorAttributeSet, ArmorClass);
    DOREPLIFETIME(UArmorAttributeSet, PhysicalDefense);
    DOREPLIFETIME(UArmorAttributeSet, BallisticDefense);
    DOREPLIFETIME(UArmorAttributeSet, ExplosiveDefense);
    DOREPLIFETIME(UArmorAttributeSet, PenetrationResistance);
    DOREPLIFETIME(UArmorAttributeSet, BluntTraumaAbsorption);
    
    // Зональная защита - реплицируется при инициализации
    DOREPLIFETIME_CONDITION(UArmorAttributeSet, HeadCoverage, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UArmorAttributeSet, TorsoCoverage, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UArmorAttributeSet, ArmsCoverage, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UArmorAttributeSet, LegsCoverage, COND_InitialOnly);
    
    // Состояние - критично для геймплея
    DOREPLIFETIME(UArmorAttributeSet, Durability);
    DOREPLIFETIME_CONDITION(UArmorAttributeSet, MaxDurability, COND_InitialOnly);
    DOREPLIFETIME(UArmorAttributeSet, DurabilityLossRate);
    DOREPLIFETIME_CONDITION(UArmorAttributeSet, RepairEfficiency, COND_InitialOnly);
    
    // Мобильность - влияет на движение
    DOREPLIFETIME(UArmorAttributeSet, ArmorWeight);
    DOREPLIFETIME(UArmorAttributeSet, MovementSpeedPenalty);
    DOREPLIFETIME(UArmorAttributeSet, TurnRatePenalty);
    DOREPLIFETIME(UArmorAttributeSet, StaminaPenalty);
    DOREPLIFETIME(UArmorAttributeSet, ArmorErgonomics);
    
    // Специальная защита - реплицируется при необходимости
    DOREPLIFETIME_CONDITION(UArmorAttributeSet, ThermalProtection, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UArmorAttributeSet, ElectricProtection, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UArmorAttributeSet, RadiationProtection, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UArmorAttributeSet, ChemicalProtection, COND_InitialOnly);
    
    // Модификации
    DOREPLIFETIME(UArmorAttributeSet, PlateSlots);
    DOREPLIFETIME(UArmorAttributeSet, ModDefenseBonus);
    DOREPLIFETIME(UArmorAttributeSet, ModMobilityBonus);
    
    // Стелс параметры
    DOREPLIFETIME(UArmorAttributeSet, NoiseLevel);
    DOREPLIFETIME(UArmorAttributeSet, ThermalSignature);
    DOREPLIFETIME(UArmorAttributeSet, VisualProfile);
}

void UArmorAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    
    // Ограничение прочности
    if (Attribute == GetDurabilityAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxDurability());
    }
    // Ограничение класса брони (1-6)
    else if (Attribute == GetArmorClassAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 1.0f, 6.0f);
    }
    // Процентные значения защиты (0-100%)
    else if (Attribute == GetPhysicalDefenseAttribute() ||
             Attribute == GetBallisticDefenseAttribute() ||
             Attribute == GetExplosiveDefenseAttribute() ||
             Attribute == GetPenetrationResistanceAttribute() ||
             Attribute == GetBluntTraumaAbsorptionAttribute() ||
             Attribute == GetThermalProtectionAttribute() ||
             Attribute == GetElectricProtectionAttribute() ||
             Attribute == GetRadiationProtectionAttribute() ||
             Attribute == GetChemicalProtectionAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
    }
    // Покрытие зон (0-100%)
    else if (Attribute == GetHeadCoverageAttribute() ||
             Attribute == GetTorsoCoverageAttribute() ||
             Attribute == GetArmsCoverageAttribute() ||
             Attribute == GetLegsCoverageAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
    }
    // Штрафы к мобильности (-100% до 0%)
    else if (Attribute == GetMovementSpeedPenaltyAttribute() ||
             Attribute == GetTurnRatePenaltyAttribute() ||
             Attribute == GetStaminaPenaltyAttribute())
    {
        NewValue = FMath::Clamp(NewValue, -100.0f, 0.0f);
    }
    // Положительные значения
    else if (Attribute == GetArmorWeightAttribute() ||
             Attribute == GetDurabilityLossRateAttribute() ||
             Attribute == GetNoiseLevelAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
    // Эргономика и профили (0-100)
    else if (Attribute == GetArmorErgonomicsAttribute() ||
             Attribute == GetThermalSignatureAttribute() ||
             Attribute == GetVisualProfileAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
    }
}

void UArmorAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);
    
    const FGameplayEffectContextHandle& Context = Data.EffectSpec.GetContext();
    AActor* SourceActor = Context.GetSourceObject() ? Cast<AActor>(Context.GetSourceObject()) : nullptr;
    AActor* TargetActor = GetOwningActor();
    
    // Обработка изменения прочности
    if (Data.EvaluatedData.Attribute == GetDurabilityAttribute())
    {
        SetDurability(FMath::Clamp(GetDurability(), 0.0f, GetMaxDurability()));
        
        float DurabilityPercent = GetDurability() / GetMaxDurability();
        
        // Деградация защиты при износе
        if (DurabilityPercent < 0.5f) // Менее 50% прочности
        {
            float DegradationFactor = (0.5f - DurabilityPercent) * 2.0f; // 0-1
            
            // Снижаем эффективность защиты
            float CurrentDefense = GetBallisticDefense();
            float DegradedDefense = CurrentDefense * (1.0f - DegradationFactor * 0.5f); // До -50%
            SetBallisticDefense(DegradedDefense);
            
            // Снижаем сопротивление пробитию
            float CurrentPenetration = GetPenetrationResistance();
            float DegradedPenetration = CurrentPenetration * (1.0f - DegradationFactor * 0.7f); // До -70%
            SetPenetrationResistance(DegradedPenetration);
            
            // Уведомление о критическом состоянии
            if (DurabilityPercent < 0.2f && TargetActor)
            {
                FGameplayEventData Payload;
                Payload.EventTag = FGameplayTag::RequestGameplayTag("Event.Armor.CriticalCondition");
                Payload.EventMagnitude = DurabilityPercent;
                Payload.Target = TargetActor;
                
                if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
                {
                    ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
                }
            }
        }
        
        UE_LOG(LogTemp, Log, TEXT("ArmorAttributeSet: Durability changed to %.1f%%"), DurabilityPercent * 100.0f);
    }
    // Обработка изменения модификаций
    else if (Data.EvaluatedData.Attribute == GetModDefenseBonusAttribute() ||
             Data.EvaluatedData.Attribute == GetModMobilityBonusAttribute())
    {
        // Пересчитываем финальные характеристики с учётом модификаций
        if (TargetActor)
        {
            FGameplayEventData Payload;
            Payload.EventTag = FGameplayTag::RequestGameplayTag("Event.Armor.ModificationsChanged");
            Payload.Target = TargetActor;
            
            if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
            {
                ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
            }
        }
    }
    // Обработка изменения веса (влияет на движение)
    else if (Data.EvaluatedData.Attribute == GetArmorWeightAttribute())
    {
        // Автоматически обновляем штрафы мобильности на основе веса
        float Weight = GetArmorWeight();
        float BaseSpeedPenalty = -Weight * 1.5f; // -1.5% за кг
        float BaseTurnPenalty = -Weight * 1.0f;  // -1% за кг
        float BaseStaminaPenalty = -Weight * 2.0f; // -2% за кг
        
        // Применяем с учетом эргономики
        float ErgonomicsFactor = GetArmorErgonomics() / 100.0f;
        SetMovementSpeedPenalty(BaseSpeedPenalty * (1.0f - ErgonomicsFactor * 0.5f));
        SetTurnRatePenalty(BaseTurnPenalty * (1.0f - ErgonomicsFactor * 0.5f));
        SetStaminaPenalty(BaseStaminaPenalty * (1.0f - ErgonomicsFactor * 0.3f));
    }
}

// Реализация всех OnRep методов
void UArmorAttributeSet::OnRep_ArmorClass(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, ArmorClass, OldValue);
}

void UArmorAttributeSet::OnRep_PhysicalDefense(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, PhysicalDefense, OldValue);
}

void UArmorAttributeSet::OnRep_BallisticDefense(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, BallisticDefense, OldValue);
}

void UArmorAttributeSet::OnRep_ExplosiveDefense(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, ExplosiveDefense, OldValue);
}

void UArmorAttributeSet::OnRep_PenetrationResistance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, PenetrationResistance, OldValue);
}

void UArmorAttributeSet::OnRep_BluntTraumaAbsorption(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, BluntTraumaAbsorption, OldValue);
}

void UArmorAttributeSet::OnRep_HeadCoverage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, HeadCoverage, OldValue);
}

void UArmorAttributeSet::OnRep_TorsoCoverage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, TorsoCoverage, OldValue);
}

void UArmorAttributeSet::OnRep_ArmsCoverage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, ArmsCoverage, OldValue);
}

void UArmorAttributeSet::OnRep_LegsCoverage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, LegsCoverage, OldValue);
}

void UArmorAttributeSet::OnRep_Durability(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, Durability, OldValue);
    
    // Обновляем UI при изменении прочности
    if (AActor* Owner = GetOwningActor())
    {
        FGameplayEventData Payload;
        Payload.EventTag = FGameplayTag::RequestGameplayTag("Event.UI.UpdateArmorDurability");
        Payload.EventMagnitude = GetDurability() / GetMaxDurability();
        Payload.Target = Owner;
        
        if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
        {
            ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
        }
    }
}

void UArmorAttributeSet::OnRep_MaxDurability(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, MaxDurability, OldValue);
}

void UArmorAttributeSet::OnRep_DurabilityLossRate(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, DurabilityLossRate, OldValue);
}

void UArmorAttributeSet::OnRep_RepairEfficiency(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, RepairEfficiency, OldValue);
}

void UArmorAttributeSet::OnRep_ArmorWeight(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, ArmorWeight, OldValue);
}

void UArmorAttributeSet::OnRep_MovementSpeedPenalty(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, MovementSpeedPenalty, OldValue);
}

void UArmorAttributeSet::OnRep_TurnRatePenalty(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, TurnRatePenalty, OldValue);
}

void UArmorAttributeSet::OnRep_StaminaPenalty(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, StaminaPenalty, OldValue);
}

void UArmorAttributeSet::OnRep_ArmorErgonomics(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, ArmorErgonomics, OldValue);
}

void UArmorAttributeSet::OnRep_ThermalProtection(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, ThermalProtection, OldValue);
}

void UArmorAttributeSet::OnRep_ElectricProtection(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, ElectricProtection, OldValue);
}

void UArmorAttributeSet::OnRep_RadiationProtection(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, RadiationProtection, OldValue);
}

void UArmorAttributeSet::OnRep_ChemicalProtection(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, ChemicalProtection, OldValue);
}

void UArmorAttributeSet::OnRep_PlateSlots(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, PlateSlots, OldValue);
}

void UArmorAttributeSet::OnRep_ModDefenseBonus(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, ModDefenseBonus, OldValue);
}

void UArmorAttributeSet::OnRep_ModMobilityBonus(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, ModMobilityBonus, OldValue);
}

void UArmorAttributeSet::OnRep_NoiseLevel(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, NoiseLevel, OldValue);
}

void UArmorAttributeSet::OnRep_ThermalSignature(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, ThermalSignature, OldValue);
}

void UArmorAttributeSet::OnRep_VisualProfile(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UArmorAttributeSet, VisualProfile, OldValue);
}

AActor* UArmorAttributeSet::GetOwningActor() const
{
    if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
    {
        return ASC->GetOwnerActor();
    }
    return nullptr;
}