#include "Core/AbilitySystem/Abilities/WeaponAbilities/MedComSingleShotAbility.h"
#include "AbilitySystemComponent.h"
#include "Equipment/Base/WeaponActor.h"
#include "Core/AbilitySystem/Attributes/MedComWeaponAttributeSet.h"
#include "Core/AbilitySystem/Abilities/Tasks/MedComWeaponAsyncTask_PerformTrace.h"

UMedComSingleShotAbility::UMedComSingleShotAbility()
{
    // Устанавливаем тип выстрела для одиночного режима
    ShotType = FName("Single");
    
    // Корректная настройка тегов режимов огня
    // Требуем наличие тега одиночного режима для активации
    ActivationRequiredTags.AddTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"));
    
    // Блокируем активацию для других режимов огня (но не для нашего)
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"));
    
    // Добавляем тег к способности (для идентификации)
    FGameplayTagContainer AbilityTagContainer;
    AbilityTagContainer.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.Shoot"));
    AbilityTagContainer.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.Fire"));
    AbilityTagContainer.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.FireMode.Single"));
    SetAssetTags(AbilityTagContainer);
}

void UMedComSingleShotAbility::FireNextShot()
{
    const FGameplayAbilitySpecHandle Handle = CurrentSpecHandle;
    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();
    
    // Получаем оружие
    AWeaponActor* Weapon = GetWeaponFromActorInfo();
    if (!Weapon)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Генерируем запрос выстрела с использованием позиции дула
    FMedComShotRequest ShotRequest = GenerateShotRequest(ActorInfo);

    // Если клиент, отправляем запрос на сервер
    if (IsLocallyPredicted(ActorInfo))
    {
        ServerFireShot(ShotRequest);
        PendingShots.Add(ShotRequest.ShotID, ShotRequest);
    }
    // Используем else if для обработки на сервере
    else if (IsServer(ActorInfo))
    {
        // Если мы на сервере (Dedicated или Listen), сразу делаем трассировку
        TArray<FHitResult> HitResults;
        ServerProcessShotTrace(ShotRequest, HitResults);

        // Применяем урон
        float DamageValue = DefaultDamage;
        if (UMedComWeaponAttributeSet* AttrSet = Weapon->GetWeaponAttributeSet())
        {
            DamageValue = AttrSet->GetDamage();
            UE_LOG(LogTemp, Warning, TEXT("Server using damage from AttributeSet: %f"), DamageValue);
        }
        ApplyDamageToTargets(HitResults, DamageValue);

        // Расходуем патроны на сервере
        UE_LOG(LogTemp, Warning, TEXT("Server consuming ammo"));
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
        UE_LOG(LogTemp, Verbose, TEXT("UMedComSingleShotAbility::FireNextShot: Playing local effects"));
        
        // Передаем флаг, чтобы отключить создание трассера в PlayLocalFireEffects
        // и применяем визуальные эффекты, кроме эффекта трассера пули
        PlayLocalFireEffects(ActorInfo, false); // false = не создавать трассер
        
        // Применяем отдачу
        ApplyRecoil(ActorInfo);

        // Используем AsyncTask для трассировки и визуализации
        FMedComWeaponTraceConfig TraceConfig;
        TraceConfig.bUseMuzzleToScreenCenter = true;  // Включаем трассировку из дула в центр экрана
        TraceConfig.TraceProfile = FName("BlockAll");
        
        // Для отладки (включить чтобы увидеть траекторию пули)
        TraceConfig.bDebug = false;
        TraceConfig.DebugDrawTime = 2.0f;
        
        // OverrideNumTraces для дробовиков если нужно
        if (NumTraces > 1)
        {
            TraceConfig.OverrideNumTraces = NumTraces;
        }
        
        // Создаем и запускаем асинк-таск трассировки
        UMedComWeaponAsyncTask_PerformTrace* TraceTask = UMedComWeaponAsyncTask_PerformTrace::PerformWeaponTrace(
            this, FName("SingleShotTrace"), TraceConfig);
            
        if (TraceTask)
        {
            // Подключаем обработчик результатов для создания эффектов попадания и трассера
            TraceTask->OnCompleted.AddDynamic(this, &UMedComBaseFireAbility::HandleAsyncTraceResults);
            TraceTask->ReadyForActivation();
        }
    }

    // Применяем кулдаун
    ApplyCooldownOnAbility(Handle, ActorInfo, ActivationInfo);

    // Завершаем способность (одноразовая)
    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}