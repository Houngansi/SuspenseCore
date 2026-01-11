#include "Core/AbilitySystem/Abilities/WeaponAbilities/MedComAutoFireAbility.h"
#include "AbilitySystemComponent.h"
#include "Equipment/Base/WeaponActor.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Core/AbilitySystem/Attributes/MedComWeaponAttributeSet.h"
#include "Core/AbilitySystem/Abilities/Tasks/MedComWeaponAsyncTask_PerformTrace.h"

UMedComAutoFireAbility::UMedComAutoFireAbility()
{
    // Установка типа выстрела для автоматического режима
    ShotType = FName("Auto");
    
    // Инициализация тега активного авто режима
    AutoFireActiveTag = FGameplayTag::RequestGameplayTag("State.Weapon.AutoActive");
    
    // Требуем наличие тега авто режима для активации
    ActivationRequiredTags.AddTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"));
    
    // Блокируем активацию для других режимов огня
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"));
    ActivationBlockedTags.AddTag(AutoFireActiveTag); // Блокируем при активном авто режиме
    
    // Устанавливаем теги для способности
    FGameplayTagContainer AbilityTagContainer;
    AbilityTagContainer.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.Shoot"));
    AbilityTagContainer.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.Fire"));
    AbilityTagContainer.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.FireMode.Auto"));
    SetAssetTags(AbilityTagContainer);
    
    // Интервал между выстрелами (будет уточнен из атрибутов оружия)
    FireRate = 0.1f;
    
    // Флаги работы автоматической стрельбы
    bIsFiring = false;
    bIsAutoFireActive = false;
    
    // Начальное время активности абилки
    AutoFireStartTime = 0.0f;
}

bool UMedComAutoFireAbility::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    // Проверяем базовые условия через родительский класс
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }
    
    // Проверяем, активна ли сейчас автоматическая стрельба
    if (bIsAutoFireActive)
    {
        UE_LOG(LogTemp, Verbose, TEXT("UMedComAutoFireAbility::CanActivateAbility: Auto fire is already active"));
        return false;
    }
    
    return true;
}

void UMedComAutoFireAbility::InputReleased(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputReleased(Handle, ActorInfo, ActivationInfo);
    
    if (ActorInfo && IsActive())
    {
        // Останавливаем автоматическую стрельбу при отпускании кнопки
        StopAutoFire();
        
        // Сбрасываем флаг активности
        bIsAutoFireActive = false;
        
        // Удаляем тег блокировки
        if (ActorInfo->AbilitySystemComponent.IsValid())
        {
            ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(AutoFireActiveTag);
        }
        
        // Включаем кулдаун и завершаем способность
        ApplyCooldownOnAbility(Handle, ActorInfo, ActivationInfo);
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UMedComAutoFireAbility::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    // Останавливаем автоматическую стрельбу
    StopAutoFire();
    
    // Сбрасываем флаг активности
    bIsAutoFireActive = false;
    
    // Удаляем тег автоматической стрельбы
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(AutoFireActiveTag);
    }
    
    // Вызываем родительский метод для стандартной очистки
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UMedComAutoFireAbility::FireNextShot()
{
    // Запускаем автоматический огонь
    
    // Устанавливаем время начала стрельбы
    AutoFireStartTime = GetWorld()->GetTimeSeconds();
    
    // Устанавливаем флаг активной автоматической стрельбы
    bIsAutoFireActive = true;
    
    // Добавляем тег автоматической стрельбы
    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        ActorInfo->AbilitySystemComponent->AddLooseGameplayTag(AutoFireActiveTag);
    }
    
    // Начинаем автоматическую стрельбу
    StartAutoFire();
}

void UMedComAutoFireAbility::StartAutoFire()
{
    if (bIsFiring)
    {
        return; // Уже стреляем
    }
    
    bIsFiring = true;
    
    // Уведомляем оружие о начале стрельбы
    AWeaponActor* Weapon = GetWeaponFromActorInfo();
    if (Weapon)
    {
        Weapon->ServerSetIsFiring(true);
    }
    
    // Выполняем первый выстрел немедленно
    ExecuteAutoShot();
    
    // Создаем таймер для последующих выстрелов
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            AutoFireTimerHandle,
            this,
            &UMedComAutoFireAbility::ExecuteAutoShot,
            FireRate,
            true // Циклический таймер
        );
    }
}

void UMedComAutoFireAbility::StopAutoFire()
{
    // Сбрасываем флаг ведения огня
    bIsFiring = false;
    
    // Уведомляем оружие о завершении стрельбы
    AWeaponActor* Weapon = GetWeaponFromActorInfo();
    if (Weapon)
    {
        Weapon->ServerSetIsFiring(false);
    }
    
    // Останавливаем таймер стрельбы
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(AutoFireTimerHandle);
    }
    
    UE_LOG(LogTemp, Log, TEXT("AutoFireAbility: Стрельба остановлена"));
}

void UMedComAutoFireAbility::ExecuteAutoShot()
{
    const FGameplayAbilitySpecHandle Handle = CurrentSpecHandle;
    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    
    if (!ActorInfo)
    {
        return;
    }
    
    AWeaponActor* Weapon = GetWeaponFromActorInfo();
    if (!Weapon)
    {
        StopAutoFire();
        bIsAutoFireActive = false;
        if (ActorInfo->AbilitySystemComponent.IsValid())
        {
            ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(AutoFireActiveTag);
        }
        EndAbility(Handle, ActorInfo, GetCurrentActivationInfo(), true, true);
        return;
    }
    
    // Проверяем наличие патронов
    if (!HasAmmo(Weapon))
    {
        StopAutoFire();
        bIsAutoFireActive = false;
        if (ActorInfo->AbilitySystemComponent.IsValid())
        {
            ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(AutoFireActiveTag);
        }
        EndAbility(Handle, ActorInfo, GetCurrentActivationInfo(), true, true);
        return;
    }
    
    // Генерируем запрос выстрела
    FMedComShotRequest ShotRequest = GenerateShotRequest(ActorInfo);
    
    // Если клиент и не сервер – отправляем запрос на сервер
    if (IsLocallyPredicted(ActorInfo))
    {
        ServerFireShot(ShotRequest);
        PendingShots.Add(ShotRequest.ShotID, ShotRequest);
    }
    else if (IsServer(ActorInfo))
    {
        // Если на сервере – сразу делаем трассировку
        TArray<FHitResult> HitResults;
        ServerProcessShotTrace(ShotRequest, HitResults);
        
        // Получаем урон из атрибутов оружия
        float DamageValue = DefaultDamage;
        if (UMedComWeaponAttributeSet* AttrSet = Weapon->GetWeaponAttributeSet())
        {
            DamageValue = AttrSet->GetDamage();
        }
        
        // Применяем урон
        ApplyDamageToTargets(HitResults, DamageValue);
        
        // Расходуем патроны
        ConsumeAmmo(Weapon);
        
        // Уведомляем о выстреле
        Weapon->NotifyWeaponFired(
            ShotRequest.Origin,
            HitResults.Num() > 0 ? HitResults[0].ImpactPoint : (ShotRequest.Origin + ShotRequest.Direction * 10000.f),
            (HitResults.Num() > 0),
            ShotRequest.ShotType
        );
    }
    
    // Локальные эффекты (анимация, звук, партиклы)
    if (ActorInfo->IsLocallyControlled())
    {
        // Воспроизводим звуки, эффекты, анимации
        PlayLocalFireEffects(ActorInfo, false);
        
        // Применяем отдачу с модификатором для авто-стрельбы
        ApplyRecoil(ActorInfo);
        
        // Используем AsyncTask для трассировки
        FMedComWeaponTraceConfig TraceConfig;
        TraceConfig.bUseMuzzleToScreenCenter = true;
        TraceConfig.TraceProfile = FName("BlockAll");
        TraceConfig.bDebug = false;
        
        if (NumTraces > 1)
        {
            TraceConfig.OverrideNumTraces = NumTraces;
        }
        
        // Создаем и запускаем асинк-таск трассировки
        UMedComWeaponAsyncTask_PerformTrace* TraceTask = UMedComWeaponAsyncTask_PerformTrace::PerformWeaponTrace(
            this, FName("AutoShotTrace"), TraceConfig);
            
        if (TraceTask)
        {
            // Подключаем обработчик результатов
            TraceTask->OnCompleted.AddDynamic(this, &UMedComBaseFireAbility::HandleAsyncTraceResults);
            TraceTask->ReadyForActivation();
        }
    }
}