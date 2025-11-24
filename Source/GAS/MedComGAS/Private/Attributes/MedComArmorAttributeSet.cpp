// MedComArmorAttributeSet.cpp
// Copyright MedCom Team. All Rights Reserved.

#include "Attributes/MedComArmorAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UMedComArmorAttributeSet::UMedComArmorAttributeSet()
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

void UMedComArmorAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Критические защитные параметры - всегда реплицируются
    DOREPLIFETIME(UMedComArmorAttributeSet, ArmorClass);
    DOREPLIFETIME(UMedComArmorAttributeSet, PhysicalDefense);
    DOREPLIFETIME(UMedComArmorAttributeSet, BallisticDefense);
    DOREPLIFETIME(UMedComArmorAttributeSet, ExplosiveDefense);
    DOREPLIFETIME(UMedComArmorAttributeSet, PenetrationResistance);
    DOREPLIFETIME(UMedComArmorAttributeSet, BluntTraumaAbsorption);
    
    // Зональная защита - реплицируется при инициализации
    DOREPLIFETIME_CONDITION(UMedComArmorAttributeSet, HeadCoverage, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComArmorAttributeSet, TorsoCoverage, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComArmorAttributeSet, ArmsCoverage, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComArmorAttributeSet, LegsCoverage, COND_InitialOnly);
    
    // Состояние - критично для геймплея
    DOREPLIFETIME(UMedComArmorAttributeSet, Durability);
    DOREPLIFETIME_CONDITION(UMedComArmorAttributeSet, MaxDurability, COND_InitialOnly);
    DOREPLIFETIME(UMedComArmorAttributeSet, DurabilityLossRate);
    DOREPLIFETIME_CONDITION(UMedComArmorAttributeSet, RepairEfficiency, COND_InitialOnly);
    
    // Мобильность - влияет на движение
    DOREPLIFETIME(UMedComArmorAttributeSet, ArmorWeight);
    DOREPLIFETIME(UMedComArmorAttributeSet, MovementSpeedPenalty);
    DOREPLIFETIME(UMedComArmorAttributeSet, TurnRatePenalty);
    DOREPLIFETIME(UMedComArmorAttributeSet, StaminaPenalty);
    DOREPLIFETIME(UMedComArmorAttributeSet, ArmorErgonomics);
    
    // Специальная защита - реплицируется при необходимости
    DOREPLIFETIME_CONDITION(UMedComArmorAttributeSet, ThermalProtection, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComArmorAttributeSet, ElectricProtection, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComArmorAttributeSet, RadiationProtection, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComArmorAttributeSet, ChemicalProtection, COND_InitialOnly);
    
    // Модификации
    DOREPLIFETIME(UMedComArmorAttributeSet, PlateSlots);
    DOREPLIFETIME(UMedComArmorAttributeSet, ModDefenseBonus);
    DOREPLIFETIME(UMedComArmorAttributeSet, ModMobilityBonus);
    
    // Стелс параметры
    DOREPLIFETIME(UMedComArmorAttributeSet, NoiseLevel);
    DOREPLIFETIME(UMedComArmorAttributeSet, ThermalSignature);
    DOREPLIFETIME(UMedComArmorAttributeSet, VisualProfile);
}

void UMedComArmorAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
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

void UMedComArmorAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
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
void UMedComArmorAttributeSet::OnRep_ArmorClass(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, ArmorClass, OldValue);
}

void UMedComArmorAttributeSet::OnRep_PhysicalDefense(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, PhysicalDefense, OldValue);
}

void UMedComArmorAttributeSet::OnRep_BallisticDefense(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, BallisticDefense, OldValue);
}

void UMedComArmorAttributeSet::OnRep_ExplosiveDefense(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, ExplosiveDefense, OldValue);
}

void UMedComArmorAttributeSet::OnRep_PenetrationResistance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, PenetrationResistance, OldValue);
}

void UMedComArmorAttributeSet::OnRep_BluntTraumaAbsorption(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, BluntTraumaAbsorption, OldValue);
}

void UMedComArmorAttributeSet::OnRep_HeadCoverage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, HeadCoverage, OldValue);
}

void UMedComArmorAttributeSet::OnRep_TorsoCoverage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, TorsoCoverage, OldValue);
}

void UMedComArmorAttributeSet::OnRep_ArmsCoverage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, ArmsCoverage, OldValue);
}

void UMedComArmorAttributeSet::OnRep_LegsCoverage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, LegsCoverage, OldValue);
}

void UMedComArmorAttributeSet::OnRep_Durability(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, Durability, OldValue);
    
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

void UMedComArmorAttributeSet::OnRep_MaxDurability(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, MaxDurability, OldValue);
}

void UMedComArmorAttributeSet::OnRep_DurabilityLossRate(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, DurabilityLossRate, OldValue);
}

void UMedComArmorAttributeSet::OnRep_RepairEfficiency(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, RepairEfficiency, OldValue);
}

void UMedComArmorAttributeSet::OnRep_ArmorWeight(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, ArmorWeight, OldValue);
}

void UMedComArmorAttributeSet::OnRep_MovementSpeedPenalty(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, MovementSpeedPenalty, OldValue);
}

void UMedComArmorAttributeSet::OnRep_TurnRatePenalty(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, TurnRatePenalty, OldValue);
}

void UMedComArmorAttributeSet::OnRep_StaminaPenalty(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, StaminaPenalty, OldValue);
}

void UMedComArmorAttributeSet::OnRep_ArmorErgonomics(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, ArmorErgonomics, OldValue);
}

void UMedComArmorAttributeSet::OnRep_ThermalProtection(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, ThermalProtection, OldValue);
}

void UMedComArmorAttributeSet::OnRep_ElectricProtection(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, ElectricProtection, OldValue);
}

void UMedComArmorAttributeSet::OnRep_RadiationProtection(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, RadiationProtection, OldValue);
}

void UMedComArmorAttributeSet::OnRep_ChemicalProtection(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, ChemicalProtection, OldValue);
}

void UMedComArmorAttributeSet::OnRep_PlateSlots(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, PlateSlots, OldValue);
}

void UMedComArmorAttributeSet::OnRep_ModDefenseBonus(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, ModDefenseBonus, OldValue);
}

void UMedComArmorAttributeSet::OnRep_ModMobilityBonus(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, ModMobilityBonus, OldValue);
}

void UMedComArmorAttributeSet::OnRep_NoiseLevel(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, NoiseLevel, OldValue);
}

void UMedComArmorAttributeSet::OnRep_ThermalSignature(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, ThermalSignature, OldValue);
}

void UMedComArmorAttributeSet::OnRep_VisualProfile(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComArmorAttributeSet, VisualProfile, OldValue);
}

AActor* UMedComArmorAttributeSet::GetOwningActor() const
{
    if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
    {
        return ASC->GetOwnerActor();
    }
    return nullptr;
}