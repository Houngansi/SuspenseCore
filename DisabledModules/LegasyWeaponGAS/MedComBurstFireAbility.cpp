#include "Core/AbilitySystem/Abilities/WeaponAbilities/MedComBurstFireAbility.h"
#include "AbilitySystemComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Equipment/Base/WeaponActor.h"
#include "Core/AbilitySystem/Attributes/MedComWeaponAttributeSet.h"
#include "Core/AbilitySystem/Abilities/Tasks/MedComWeaponAsyncTask_PerformTrace.h"

UMedComBurstFireAbility::UMedComBurstFireAbility()
{
    // Установка типа выстрела для очереди
    ShotType = FName("Burst");
    
    // Установка тегов режима огня
    BurstActiveTag = FGameplayTag::RequestGameplayTag("State.Weapon.BurstActive");
    
    // Теги активации/блокировки
    ActivationRequiredTags.AddTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst"));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single"));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto"));
    ActivationBlockedTags.AddTag(BurstActiveTag);
    
    // Теги способности
    FGameplayTagContainer BurstAbilityTags; // Переименовано с AbilityTags на BurstAbilityTags
    BurstAbilityTags.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.Shoot"));
    BurstAbilityTags.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.Fire"));
    BurstAbilityTags.AddTag(FGameplayTag::RequestGameplayTag("Ability.Weapon.FireMode.Burst"));
    SetAssetTags(BurstAbilityTags);
    
    // Настройки очереди
    BurstCount = 3;
    BurstDelay = 0.15f;
    CurrentBurstShotCount = 0;
}

bool UMedComBurstFireAbility::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    // Проверяем базовые условия через родительский метод
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        UE_LOG(LogTemp, Warning, TEXT("BurstFireAbility: базовая проверка Super::CanActivateAbility не пройдена"));
        return false;
    }
    
    // Проверяем, что в ASC нет тега активной очереди
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        if (ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(BurstActiveTag))
        {
            UE_LOG(LogTemp, Warning, TEXT("BurstFireAbility: ASC уже содержит BurstActiveTag – активация отклонена"));
            return false;
        }
    }
    
    // Дополнительная проверка, что нет других активных экземпляров этой способности
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
        for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
        {
            if (Spec.Ability && Spec.Ability->IsA(StaticClass()) && Spec.IsActive())
            {
                UE_LOG(LogTemp, Warning, TEXT("BurstFireAbility: найден уже активный экземпляр – активация отклонена"));
                return false;
            }
        }
    }
    
    return true;
}

void UMedComBurstFireAbility::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    // Очищаем таймер очереди
    if (UWorld* World = GetWorld())
    {
        if (BurstTimerHandle.IsValid())
        {
            World->GetTimerManager().ClearTimer(BurstTimerHandle);
        }
    }
    
    // Удаляем тег активной очереди
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(BurstActiveTag);
    }
    
    // Вызываем родительский метод для остальной очистки
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UMedComBurstFireAbility::FireNextShot()
{
    // Просто запускаем первый выстрел очереди
    // Остальные выстрелы будут вызваны через таймер из ExecuteBurstShot
    const FGameplayAbilitySpecHandle Handle = CurrentSpecHandle;
    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    
    // Добавляем тег активной очереди при начале стрельбы
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        ActorInfo->AbilitySystemComponent->AddLooseGameplayTag(BurstActiveTag);
    }
    
    // Сбрасываем счетчики для новой очереди
    CurrentBurstShotCount = 0;
    ResetBurstState();
    
    // Запускаем первый выстрел очереди
    ExecuteBurstShot(Handle, ActorInfo);
}

void UMedComBurstFireAbility::ResetBurstState()
{
    // Сбрасываем счетчик последовательных выстрелов
    // (эти переменные теперь в базовом классе)
    if (AWeaponActor* Weapon = GetWeaponFromActorInfo())
    {
        // Сбрасываем разброс к базовому значению
        Weapon->ResetSpreadToBase();
        
        UE_LOG(LogTemp, Warning, TEXT("BurstFireAbility: Сброс состояния очереди"));
    }
}

void UMedComBurstFireAbility::ExecuteBurstShot(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo)
{
    if (!ActorInfo)
        return;

    AWeaponActor* Weapon = GetWeaponFromActorInfo();
    if (!Weapon)
    {
        EndAbility(Handle, ActorInfo, GetCurrentActivationInfo(), true, true);
        return;
    }

    // Генерируем запрос на выстрел
    FMedComShotRequest Shot = GenerateShotRequest(ActorInfo);
    ++CurrentBurstShotCount;

    // Отправляем запрос на сервер, если мы локальный клиент
    if (IsLocallyPredicted(ActorInfo))
    {
        ServerFireShot(Shot);
        PendingShots.Add(Shot.ShotID, Shot);
    }
    // Выполняем выстрел на сервере напрямую
    else if (IsServer(ActorInfo))
    {
        TArray<FHitResult> Hits;
        ServerProcessShotTrace(Shot, Hits);

        float Damage = DefaultDamage;
        if (UMedComWeaponAttributeSet* Attr = Weapon->GetWeaponAttributeSet())
            Damage = Attr->GetDamage();

        ApplyDamageToTargets(Hits, Damage);
        ConsumeAmmo(Weapon);

        Weapon->NotifyWeaponFired(
            Shot.Origin,
            Hits.Num() ? Hits[0].ImpactPoint : Shot.Origin + Shot.Direction * 10000.f,
            Hits.Num() > 0,
            Shot.ShotType);
    }

    // Воспроизводим локальные эффекты выстрела
    if (ActorInfo->IsLocallyControlled())
    {
        PlayLocalFireEffects(ActorInfo, false);
        ApplyRecoil(ActorInfo);

        // Асинхронная трассировка для визуальных эффектов
        FMedComWeaponTraceConfig Cfg;
        Cfg.bUseMuzzleToScreenCenter = true;
        Cfg.TraceProfile = FName("BlockAll");
        Cfg.bDebug = false;
        if (NumTraces > 1)
            Cfg.OverrideNumTraces = NumTraces;

        if (UMedComWeaponAsyncTask_PerformTrace* Task =
                UMedComWeaponAsyncTask_PerformTrace::PerformWeaponTrace(this, FName("BurstTrace"), Cfg))
        {
            Task->OnCompleted.AddDynamic(this, &UMedComBaseFireAbility::HandleAsyncTraceResults);
            Task->ReadyForActivation();
        }
    }

    // Проверяем, нужно ли продолжать очередь
    int32 AmmoLeft = 0;
    if (UMedComWeaponAttributeSet* Attr = Weapon->GetWeaponAttributeSet())
        AmmoLeft = FMath::FloorToInt(Attr->GetCurrentAmmo());

    if (CurrentBurstShotCount < BurstCount && AmmoLeft > 0)
    {
        // Планируем следующий выстрел очереди
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                BurstTimerHandle,
                FTimerDelegate::CreateUObject(this, &UMedComBurstFireAbility::ExecuteBurstShot, Handle, ActorInfo),
                BurstDelay,
                false);
        }
        return;
    }

    // Если достигли конца очереди или кончились патроны, завершаем очередь
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(BurstActiveTag);
    }

    // Применяем кулдаун и завершаем способность
    ApplyCooldownOnAbility(Handle, ActorInfo, GetCurrentActivationInfo());
    EndAbility(Handle, ActorInfo, GetCurrentActivationInfo(), true, false);
}