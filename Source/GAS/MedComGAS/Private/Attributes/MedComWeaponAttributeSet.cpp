// MedComWeaponAttributeSet.cpp
// Copyright MedCom Team. All Rights Reserved.

#include "Attributes/MedComWeaponAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

UMedComWeaponAttributeSet::UMedComWeaponAttributeSet()
{
    // Боевые характеристики - базовые значения для типичной штурмовой винтовки
    BaseDamage = 35.0f;                    // Базовый урон (будет модифицирован патронами)
    RateOfFire = 600.0f;                   // 600 выстрелов в минуту
    EffectiveRange = 300.0f;               // 300 метров эффективная дальность
    MaxRange = 800.0f;                     // 800 метров максимальная дальность
    MagazineSize = 30.0f;                  // Стандартный магазин на 30 патронов
    TacticalReloadTime = 2.5f;             // Тактическая перезарядка (с патроном в патроннике)
    FullReloadTime = 3.5f;                 // Полная перезарядка (пустой магазин)
    
    // Характеристики точности
    MOA = 2.0f;                            // 2 MOA - хорошая точность для боевой винтовки
    HipFireSpread = 5.0f;                  // Разброс от бедра в градусах
    AimSpread = 0.5f;                      // Разброс в прицеле
    VerticalRecoil = 3.0f;                 // Вертикальная отдача
    HorizontalRecoil = 1.5f;               // Горизонтальная отдача
    RecoilRecoverySpeed = 5.0f;            // Скорость восстановления после отдачи
    SpreadIncreasePerShot = 0.3f;          // Увеличение разброса за выстрел
    MaxSpread = 10.0f;                     // Максимальный разброс
    
    // Надёжность и износ
    Durability = 100.0f;                   // Новое оружие - 100% прочности
    MaxDurability = 100.0f;                // Максимальная прочность
    DurabilityLossPerShot = 0.01f;         // Теряет 0.01% прочности за выстрел
    MisfireChance = 0.0f;                  // Новое оружие - без осечек
    JamChance = 0.0f;                      // Новое оружие - без заклиниваний
    MisfireClearTime = 1.5f;               // 1.5 секунды на устранение осечки
    JamClearTime = 3.0f;                   // 3 секунды на устранение заклинивания
    
    // Эргономика
    Ergonomics = 50.0f;                    // Средняя эргономика (0-100)
    AimDownSightTime = 0.3f;               // 0.3 секунды на прицеливание
    AimSensitivityMultiplier = 0.65f;      // 65% чувствительности в прицеле
    WeaponWeight = 3.5f;                   // 3.5 кг - типичный вес винтовки
    StaminaDrainRate = 2.0f;               // Расход выносливости в секунду при прицеливании
    WeaponSwitchTime = 1.0f;               // 1 секунда на смену оружия
    
    // Модификации
    ModSlotCount = 5.0f;                   // 5 слотов для модификаций
    ModAccuracyBonus = 0.0f;               // Без модов - без бонусов
    ModErgonomicsBonus = 0.0f;             // Без модов - без бонусов
    
    // Специальные характеристики
    NoiseLevel = 160.0f;                   // 160 дБ - громкий выстрел
    SuppressorEfficiency = 0.0f;           // Без глушителя
    FireModeSwitchTime = 0.5f;             // 0.5 секунды на переключение режима огня
}

void UMedComWeaponAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    
    // Критические боевые характеристики - реплицируются всегда
    DOREPLIFETIME(UMedComWeaponAttributeSet, BaseDamage);
    DOREPLIFETIME(UMedComWeaponAttributeSet, RateOfFire);
    DOREPLIFETIME(UMedComWeaponAttributeSet, MagazineSize);
    DOREPLIFETIME(UMedComWeaponAttributeSet, TacticalReloadTime);
    DOREPLIFETIME(UMedComWeaponAttributeSet, FullReloadTime);
    
    // Статические характеристики - реплицируются только при инициализации
    DOREPLIFETIME_CONDITION(UMedComWeaponAttributeSet, EffectiveRange, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComWeaponAttributeSet, MaxRange, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComWeaponAttributeSet, MOA, COND_InitialOnly);
    
    // Динамические характеристики точности
    DOREPLIFETIME(UMedComWeaponAttributeSet, HipFireSpread);
    DOREPLIFETIME(UMedComWeaponAttributeSet, AimSpread);
    DOREPLIFETIME(UMedComWeaponAttributeSet, VerticalRecoil);
    DOREPLIFETIME(UMedComWeaponAttributeSet, HorizontalRecoil);
    DOREPLIFETIME(UMedComWeaponAttributeSet, RecoilRecoverySpeed);
    DOREPLIFETIME(UMedComWeaponAttributeSet, SpreadIncreasePerShot);
    DOREPLIFETIME(UMedComWeaponAttributeSet, MaxSpread);
    
    // Состояние оружия - критично для геймплея
    DOREPLIFETIME(UMedComWeaponAttributeSet, Durability);
    DOREPLIFETIME_CONDITION(UMedComWeaponAttributeSet, MaxDurability, COND_InitialOnly);
    DOREPLIFETIME(UMedComWeaponAttributeSet, DurabilityLossPerShot);
    DOREPLIFETIME(UMedComWeaponAttributeSet, MisfireChance);
    DOREPLIFETIME(UMedComWeaponAttributeSet, JamChance);
    DOREPLIFETIME_CONDITION(UMedComWeaponAttributeSet, MisfireClearTime, COND_InitialOnly);
    DOREPLIFETIME_CONDITION(UMedComWeaponAttributeSet, JamClearTime, COND_InitialOnly);
    
    // Эргономика - влияет на геймплей
    DOREPLIFETIME(UMedComWeaponAttributeSet, Ergonomics);
    DOREPLIFETIME(UMedComWeaponAttributeSet, AimDownSightTime);
    DOREPLIFETIME(UMedComWeaponAttributeSet, AimSensitivityMultiplier);
    DOREPLIFETIME(UMedComWeaponAttributeSet, WeaponWeight);
    DOREPLIFETIME(UMedComWeaponAttributeSet, StaminaDrainRate);
    DOREPLIFETIME_CONDITION(UMedComWeaponAttributeSet, WeaponSwitchTime, COND_InitialOnly);
    
    // Модификации
    DOREPLIFETIME(UMedComWeaponAttributeSet, ModSlotCount);
    DOREPLIFETIME(UMedComWeaponAttributeSet, ModAccuracyBonus);
    DOREPLIFETIME(UMedComWeaponAttributeSet, ModErgonomicsBonus);
    
    // Специальные
    DOREPLIFETIME_CONDITION(UMedComWeaponAttributeSet, NoiseLevel, COND_InitialOnly);
    DOREPLIFETIME(UMedComWeaponAttributeSet, SuppressorEfficiency);
    DOREPLIFETIME_CONDITION(UMedComWeaponAttributeSet, FireModeSwitchTime, COND_InitialOnly);
}

void UMedComWeaponAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);
    
    // Ограничение прочности
    if (Attribute == GetDurabilityAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxDurability());
    }
    // Ограничение размера магазина
    else if (Attribute == GetMagazineSizeAttribute())
    {
        NewValue = FMath::Max(0.0f, FMath::RoundToFloat(NewValue));
    }
    // Ограничение процентных значений
    else if (Attribute == GetMisfireChanceAttribute() || 
             Attribute == GetJamChanceAttribute() ||
             Attribute == GetSuppressorEfficiencyAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
    }
    // Ограничение эргономики
    else if (Attribute == GetErgonomicsAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.0f, 100.0f);
    }
    // Минимальные значения для временных параметров
    else if (Attribute == GetTacticalReloadTimeAttribute() ||
             Attribute == GetFullReloadTimeAttribute() ||
             Attribute == GetAimDownSightTimeAttribute() ||
             Attribute == GetWeaponSwitchTimeAttribute() ||
             Attribute == GetFireModeSwitchTimeAttribute())
    {
        NewValue = FMath::Max(0.1f, NewValue); // Минимум 0.1 секунды
    }
    // Положительные значения
    else if (Attribute == GetRateOfFireAttribute() ||
             Attribute == GetEffectiveRangeAttribute() ||
             Attribute == GetMaxRangeAttribute() ||
             Attribute == GetWeaponWeightAttribute())
    {
        NewValue = FMath::Max(0.0f, NewValue);
    }
}

void UMedComWeaponAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
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
        
        // Деградация характеристик при износе
        if (DurabilityPercent < 0.8f) // Менее 80% прочности
        {
            // Увеличиваем шанс осечек и заклиниваний
            float DegradationFactor = (0.8f - DurabilityPercent) / 0.8f; // 0-1
            
            SetMisfireChance(GetMisfireChance() + DegradationFactor * 5.0f); // До +5% осечек
            SetJamChance(GetJamChance() + DegradationFactor * 3.0f); // До +3% заклиниваний
            
            // Ухудшаем точность
            SetMOA(GetMOA() * (1.0f + DegradationFactor * 0.5f)); // До +50% MOA
            
            // Уведомляем о критическом состоянии
            if (DurabilityPercent < 0.2f && TargetActor)
            {
                FGameplayEventData Payload;
                Payload.EventTag = FGameplayTag::RequestGameplayTag("Event.Weapon.CriticalCondition");
                Payload.EventMagnitude = DurabilityPercent;
                Payload.Target = TargetActor;
                
                if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
                {
                    ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
                }
            }
        }
        
        UE_LOG(LogTemp, Log, TEXT("WeaponAttributeSet: Durability changed to %.1f%%"), DurabilityPercent * 100.0f);
    }
    // Обработка модификаций
    else if (Data.EvaluatedData.Attribute == GetModAccuracyBonusAttribute() ||
             Data.EvaluatedData.Attribute == GetModErgonomicsBonusAttribute())
    {
        // Пересчитываем финальные характеристики с учётом модификаций
        if (TargetActor)
        {
            FGameplayEventData Payload;
            Payload.EventTag = FGameplayTag::RequestGameplayTag("Event.Weapon.ModificationsChanged");
            Payload.Target = TargetActor;
            
            if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
            {
                ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
            }
        }
    }
}

// Реализация всех OnRep методов
void UMedComWeaponAttributeSet::OnRep_BaseDamage(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, BaseDamage, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_RateOfFire(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, RateOfFire, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_EffectiveRange(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, EffectiveRange, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_MaxRange(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, MaxRange, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_MagazineSize(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, MagazineSize, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_TacticalReloadTime(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, TacticalReloadTime, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_FullReloadTime(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, FullReloadTime, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_MOA(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, MOA, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_HipFireSpread(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, HipFireSpread, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_AimSpread(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, AimSpread, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_VerticalRecoil(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, VerticalRecoil, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_HorizontalRecoil(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, HorizontalRecoil, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_RecoilRecoverySpeed(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, RecoilRecoverySpeed, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_SpreadIncreasePerShot(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, SpreadIncreasePerShot, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_MaxSpread(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, MaxSpread, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_Durability(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, Durability, OldValue);
    
    // Обновляем UI при изменении прочности
    if (AActor* Owner = GetOwningActor())
    {
        FGameplayEventData Payload;
        Payload.EventTag = FGameplayTag::RequestGameplayTag("Event.UI.UpdateWeaponDurability");
        Payload.EventMagnitude = GetDurability() / GetMaxDurability();
        Payload.Target = Owner;
        
        if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Owner))
        {
            ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
        }
    }
}

void UMedComWeaponAttributeSet::OnRep_MaxDurability(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, MaxDurability, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_DurabilityLossPerShot(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, DurabilityLossPerShot, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_MisfireChance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, MisfireChance, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_JamChance(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, JamChance, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_MisfireClearTime(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, MisfireClearTime, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_JamClearTime(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, JamClearTime, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_Ergonomics(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, Ergonomics, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_AimDownSightTime(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, AimDownSightTime, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_AimSensitivityMultiplier(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, AimSensitivityMultiplier, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_WeaponWeight(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, WeaponWeight, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_StaminaDrainRate(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, StaminaDrainRate, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_WeaponSwitchTime(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, WeaponSwitchTime, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_ModSlotCount(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, ModSlotCount, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_ModAccuracyBonus(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, ModAccuracyBonus, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_ModErgonomicsBonus(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, ModErgonomicsBonus, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_NoiseLevel(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, NoiseLevel, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_SuppressorEfficiency(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, SuppressorEfficiency, OldValue);
}

void UMedComWeaponAttributeSet::OnRep_FireModeSwitchTime(const FGameplayAttributeData& OldValue)
{
    GAMEPLAYATTRIBUTE_REPNOTIFY(UMedComWeaponAttributeSet, FireModeSwitchTime, OldValue);
}

AActor* UMedComWeaponAttributeSet::GetOwningActor() const
{
    if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
    {
        return ASC->GetOwnerActor();
    }
    return nullptr;
}