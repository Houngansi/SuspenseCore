#include "Core/AbilitySystem/Abilities/WeaponAbilities/MedComBaseFireAbility.h"
#include "AbilitySystemComponent.h"
#include "Equipment/Base/WeaponActor.h"
#include "Core/AbilitySystem/Attributes/MedComWeaponAttributeSet.h"
#include "Equipment/Components/MedComEquipmentComponent.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "Core/MedComPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Core/AbilitySystem/Abilities/Tasks/MedComWeaponAsyncTask_PerformTrace.h"
#include "Core/AbilitySystem/Abilities/Tasks/MedComTraceUtils.h"
#include "Animation/AnimInstance.h"
#include "AbilitySystemGlobals.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Particles/ParticleSystem.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Engine/DamageEvents.h"
#include "Core/Character/MedComCharacter.h"
#include "Core/AbilitySystem/Attributes/MedComBaseAttributeSet.h"
#include "Equipment/Components/WeaponFireModeComponent.h"
#include "Camera/CameraShakeBase.h"
// Niagara
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

UMedComBaseFireAbility::UMedComBaseFireAbility()
: DefaultDamage(0.f)
    , MaxDistanceOverride(0.f)
    , DamageEffect(nullptr)
    , CooldownEffect(nullptr)
    , FireMontage(nullptr)
    , FireSound(nullptr)
    , MuzzleFlash(nullptr)
    , MuzzleFlashNiagara(nullptr)
    , BulletTracerNiagara(nullptr)
    , ImpactEffectNiagara(nullptr)
    , NumTraces(1)
    , BaseSpread(0.f)
    , AimingSpreadModifier(0.f)
    , MovementSpreadModifier(0.f)
    , ShotType(TEXT("Default"))
    , TraceProfile(TEXT("Weapon"))
    , bDebugTrace(false)
    , FireCameraShake(nullptr)
    , LastShotID(0)
    , ConsecutiveShotsCount(0)
    , CurrentRecoilMultiplier(1.0f)
    , AccumulatedRecoil(FVector2D::ZeroVector)
{
    // Настройка инстанцирования и репликации
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

    // Инициализация тегов
    ReloadingTag = FGameplayTag::RequestGameplayTag("State.Reloading");
    AimingTag = FGameplayTag::RequestGameplayTag("State.Aiming");
    WeaponCooldownTag = FGameplayTag::RequestGameplayTag("Ability.Weapon.Cooldown");
    FiringTag = FGameplayTag::RequestGameplayTag("Weapon.State.Firing");

    // Общие блокировки активации
    ActivationBlockedTags.AddTag(ReloadingTag);
}

bool UMedComBaseFireAbility::CanActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayTagContainer* SourceTags,
    const FGameplayTagContainer* TargetTags,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    // Проверка базовых условий через родительский метод
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }

    // Получаем оружие через метод
    AWeaponActor* Weapon = GetWeaponFromActorInfo();
    if (!Weapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("UMedComBaseFireAbility::CanActivateAbility: No weapon found"));
        return false;
    }

    // Проверка наличия патронов
    if (!HasAmmo(Weapon))
    {
        UE_LOG(LogTemp, Warning, TEXT("UMedComBaseFireAbility::CanActivateAbility: No ammo"));
        return false;
    }

    return true;
}

void UMedComBaseFireAbility::InputPressed(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    // Вызываем стандартную обработку InputPressed
    Super::InputPressed(Handle, ActorInfo, ActivationInfo);

    // Пытаемся активировать способность при нажатии
    if (ActorInfo && !IsActive())
    {
        ActorInfo->AbilitySystemComponent->TryActivateAbility(Handle);
    }
}

void UMedComBaseFireAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    // Вызываем родительский метод для стандартных проверок
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    // Добавляем тег стрельбы в начале активации
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        ActorInfo->AbilitySystemComponent->AddLooseGameplayTag(FiringTag);
        
        // Уведомляем оружие об изменении состояния
        if (AWeaponActor* Weapon = GetWeaponFromActorInfo())
        {
            Weapon->NotifyWeaponStateChanged(FiringTag, false);
        }
        
        UE_LOG(LogTemp, Warning, TEXT("BaseFireAbility: Added Firing tag at ability start"));
    }

    if (!ActorInfo || !ActorInfo->OwnerActor.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("UMedComBaseFireAbility::ActivateAbility: No valid actor info or owner"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Получаем оружие через метод
    AWeaponActor* Weapon = GetWeaponFromActorInfo();
    if (!Weapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("UMedComBaseFireAbility::ActivateAbility: Failed to get weapon"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Проверяем наличие патронов
    if (!HasAmmo(Weapon))
    {
        UE_LOG(LogTemp, Warning, TEXT("UMedComBaseFireAbility::ActivateAbility: No ammo available"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Здесь начинается последовательность выстрелов, специфичная для каждого типа оружия
    // Вызываем реализацию дочернего класса
    FireNextShot();
}

void UMedComBaseFireAbility::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    // Сообщаем оружию о завершении стрельбы
    AWeaponActor* Weapon = GetWeaponFromActorInfo();
    if (Weapon)
    {
        Weapon->ServerSetIsFiring(false);
    }
    
    // Удаляем тег стрельбы при завершении способности
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        if (ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(FiringTag))
        {
            ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTag(FiringTag);
            
            // Также уведомляем оружие об изменении состояния
            if (Weapon)
            {
                FGameplayTag IdleTag = FGameplayTag::RequestGameplayTag("Weapon.State.Idle");
                Weapon->NotifyWeaponStateChanged(IdleTag, false);
            }
        }
    }
    
    // Вызываем родительский метод
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UMedComBaseFireAbility::IsServer(const FGameplayAbilityActorInfo* ActorInfo) const
{
    return ActorInfo && ActorInfo->IsNetAuthority();
}

bool UMedComBaseFireAbility::IsLocallyPredicted(const FGameplayAbilityActorInfo* ActorInfo) const
{
    return ActorInfo && !ActorInfo->IsNetAuthority() && ActorInfo->IsLocallyControlled();
}

FMedComShotRequest UMedComBaseFireAbility::GenerateShotRequest(const FGameplayAbilityActorInfo* ActorInfo) const
{
    FMedComShotRequest Request;
    LastShotID++;
    Request.ShotID = LastShotID;
    Request.ClientTimeStamp = GetWorld()->GetTimeSeconds();
    Request.ShotType = ShotType;
    Request.RandomSeed = FMath::Rand();
    Request.TraceProfile = TraceProfile.IsNone() ? FName("Weapon") : TraceProfile;
    Request.bDebug = bDebugTrace;
    Request.DebugDrawTime = 2.0f;
    
    // Установка количества трасс (для дробовика)
    Request.NumTraces = NumTraces;
    
    // Получаем оружие
    AWeaponActor* Weapon = GetWeaponFromActorInfo();
    bool bUseWeaponMuzzle = (Weapon != nullptr);

    // Получаем данные камеры - ВАЖНО: инициализируем значением по умолчанию!
    FVector CamLoc = FVector::ZeroVector;
    FRotator CamRot = FRotator::ZeroRotator;
    bool bFoundCameraView = false;
    
    if (ActorInfo && ActorInfo->PlayerController.IsValid())
    {
        APlayerController* PC = Cast<APlayerController>(ActorInfo->PlayerController.Get());
        if (PC)
        {
            PC->GetPlayerViewPoint(CamLoc, CamRot);
            bFoundCameraView = true;
        }
    }
    
    // Если не удалось получить данные камеры, используем данные персонажа
    if (!bFoundCameraView && ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        CamLoc = ActorInfo->AvatarActor->GetActorLocation();
        CamRot = ActorInfo->AvatarActor->GetActorRotation();
        // Отмечаем что нашли хотя бы какие-то данные
        bFoundCameraView = true;
    }
    
    // Определяем точку прицеливания (куда смотрит игрок)
    FVector AimPoint;
    bool bHasAimPoint = false;
    
    if (bFoundCameraView)
    {
        // Создаем список игнорируемых акторов
        TArray<AActor*> ActorsToIgnore;
        if (ActorInfo->AvatarActor.IsValid())
            ActorsToIgnore.Add(ActorInfo->AvatarActor.Get());
        if (Weapon)
            ActorsToIgnore.Add(Weapon);
        
        // Используем наш новый хелпер для определения точки прицеливания
        float MaxRange = MaxDistanceOverride > 0.f ? MaxDistanceOverride : 10000.f;
        bHasAimPoint = UMedComTraceUtils::GetAimPoint(
            Cast<APlayerController>(ActorInfo->PlayerController.Get()), 
            MaxRange, 
            Request.TraceProfile, 
            ActorsToIgnore, 
            Request.bDebug, 
            Request.DebugDrawTime, 
            CamLoc, 
            AimPoint
        );
    }
    
    // Теперь устанавливаем Origin и Direction
    if (bUseWeaponMuzzle)
    {
        // Origin всегда из дула
        Request.Origin = Weapon->GetMuzzleLocation();
        
        // Направление от дула к точке прицеливания
        if (bHasAimPoint)
        {
            Request.Direction = (AimPoint - Request.Origin).GetSafeNormal();
        }
        else
        {
            // Запасной вариант, если не удалось определить точку прицеливания
            Request.Direction = Weapon->GetMuzzleRotation().Vector();
        }
    }
    else
    {
        // Если нет оружия, используем камеру (запасной вариант)
        // Убедимся, что CamLoc не ZeroVector перед использованием
        if (!CamLoc.IsZero())
        {
            Request.Origin = CamLoc;
            Request.Direction = CamRot.Vector();
        }
        else
        {
            // Крайний запасной вариант - используем мировые координаты (0,0,0) и вектор вверх
            Request.Origin = FVector::ZeroVector;
            Request.Direction = FVector::ForwardVector;
            UE_LOG(LogTemp, Warning, TEXT("GenerateShotRequest: CamLoc is zero! Using fallback origin and direction."));
        }
    }

    // Вычисляем максимальную дальность
    Request.MaxRange = MaxDistanceOverride > 0.f ? MaxDistanceOverride : 10000.f;
    
    // Получаем атрибуты оружия для определения дальности, если возможно
    if (Weapon)
    {
        if (UMedComWeaponAttributeSet* AttrSet = Weapon->GetWeaponAttributeSet())
        {
            // Если в атрибутсете есть метод GetRange(), используем его
            // Request.MaxRange = AttrSet->GetRange();
        }
    }

    // Получаем текущее значение разброса
    bool bIsAiming = ActorInfo->AbilitySystemComponent.IsValid() && 
                     ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(AimingTag);
    
    if (Weapon)
    {
        // Получаем готовое значение разброса от оружия
        Request.SpreadAngle = Weapon->GetCurrentSpread();
    }
    else
    {
        // Если нет оружия, используем базовое значение разброса
        Request.SpreadAngle = BaseSpread;
    }
    
    Request.bIsAiming = bIsAiming;
    Request.bUseMuzzleToScreenCenter = true;
    
    return Request;
}

UMedComWeaponAsyncTask_PerformTrace* UMedComBaseFireAbility::CreateWeaponTraceTask(const FMedComShotRequest& ShotRequest)
{
    UMedComWeaponAsyncTask_PerformTrace* TraceTask = UMedComWeaponAsyncTask_PerformTrace::PerformWeaponTraceFromRequest(
        this,
        FName("WeaponTrace"),
        ShotRequest
    );
    
    if (TraceTask)
    {
        // Привязываем обработчик для получения результатов трассировки
        TraceTask->OnCompleted.AddDynamic(this, &UMedComBaseFireAbility::HandleAsyncTraceResults);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CreateWeaponTraceTask: Failed to create trace task"));
    }
    
    return TraceTask;
}

void UMedComBaseFireAbility::ApplyCooldownOnAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, 
    const FGameplayAbilityActivationInfo ActivationInfo)
{
    if (CooldownEffect)
    {
        Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
        
        if (ActorInfo->AbilitySystemComponent.IsValid() && 
            !ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(WeaponCooldownTag))
        {
            ActorInfo->AbilitySystemComponent->AddLooseGameplayTag(WeaponCooldownTag);
        }
    }
}

void UMedComBaseFireAbility::ServerFireShot_Implementation(FMedComShotRequest ShotRequest)
{
    AWeaponActor* Weapon = GetWeaponFromActorInfo();
    if (!Weapon || !HasAmmo(Weapon))
    {
        FMedComShotResult FailResult;
        FailResult.ShotID = ShotRequest.ShotID;
        FailResult.bConfirmed = false;
        FailResult.RejectionReason = FName("NoAmmo");
        ClientReceiveShotResult(FailResult);
        UE_LOG(LogTemp, Warning, TEXT("ServerFireShot: No weapon or no ammo"));
        return;
    }

    if (!ValidateShotRequest(ShotRequest, Weapon))
    {
        FMedComShotResult FailResult;
        FailResult.ShotID = ShotRequest.ShotID;
        FailResult.bConfirmed = false;
        FailResult.RejectionReason = FName("InvalidRequest");
        ClientReceiveShotResult(FailResult);
        UE_LOG(LogTemp, Warning, TEXT("ServerFireShot: Invalid shot request"));
        return;
    }

    TArray<FHitResult> HitResults;
    ServerProcessShotTrace(ShotRequest, HitResults);

    float DamageValue = DefaultDamage;
    if (UMedComWeaponAttributeSet* AttrSet = Weapon->GetWeaponAttributeSet())
    {
        DamageValue = AttrSet->GetDamage();
        UE_LOG(LogTemp, Warning, TEXT("ServerFireShot: Using damage from AttributeSet: %f"), DamageValue);
    }
    ApplyDamageToTargets(HitResults, DamageValue);

    // ВАЖНО: Добавляем лог перед расходом патронов
    UE_LOG(LogTemp, Warning, TEXT("ServerFireShot: About to consume ammo"));
    ConsumeAmmo(Weapon);
    UE_LOG(LogTemp, Warning, TEXT("ServerFireShot: Ammo consumed"));

    FMedComShotResult ShotResult;
    ShotResult.ShotID = ShotRequest.ShotID;
    ShotResult.bConfirmed = true;
    ShotResult.Hits = HitResults;

    ClientReceiveShotResult(ShotResult);

    Weapon->NotifyWeaponFired(
        ShotRequest.Origin,
        HitResults.Num() > 0 ? HitResults[0].ImpactPoint : (ShotRequest.Origin + ShotRequest.Direction * 10000.f),
        (HitResults.Num() > 0),
        ShotRequest.ShotType
    );
}

bool UMedComBaseFireAbility::ServerFireShot_Validate(FMedComShotRequest ShotRequest)
{
    float ServerTime = GetWorld()->GetTimeSeconds();
    if (ShotRequest.ClientTimeStamp > ServerTime + 1.0f)
    {
        return false;
    }
    
    if (ShotRequest.SpreadAngle < 0.0f)
    {
        return false;
    }
    
    if (!ShotRequest.Direction.IsNormalized())
    {
        return false;
    }
    
    return true;
}

void UMedComBaseFireAbility::ClientReceiveShotResult_Implementation(FMedComShotResult ShotResult)
{
    if (FMedComShotRequest* FoundRequest = PendingShots.Find(ShotResult.ShotID))
    {
        if (ShotResult.bConfirmed)
        {
            // Для каждого попадания показываем только эффект попадания (без трассера)
            for (const FHitResult& Hit : ShotResult.Hits)
            {
                if (Hit.bBlockingHit)
                {
                    PlayImpactEffects(Hit);
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("Shot %d rejected: %s"), 
                ShotResult.ShotID, *ShotResult.RejectionReason.ToString());
        }
        PendingShots.Remove(ShotResult.ShotID);
    }
}

bool UMedComBaseFireAbility::ValidateShotRequest(const FMedComShotRequest& ShotRequest, AWeaponActor* Weapon) const
{
    if (!Weapon)
    {
        return false;
    }
    
    float Distance = FVector::Dist(ShotRequest.Origin, Weapon->GetActorLocation());
    float MaxAllowedDistance = 200.f;
    
    ACharacter* OwnerCharacter = Cast<ACharacter>(Weapon->GetOwner());
    if (OwnerCharacter)
    {
        APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
        if (PC && PC->IsLocalController())
        {
            MaxAllowedDistance = 300.f;
        }
    }
    
    if (Distance > MaxAllowedDistance)
    {
        UE_LOG(LogTemp, Warning, TEXT("Shot validation failed: Distance too large (%f > %f)"), 
            Distance, MaxAllowedDistance);
        return false;
    }

    float ServerTime = GetWorld()->GetTimeSeconds();
    if (FMath::Abs(ServerTime - ShotRequest.ClientTimeStamp) > 2.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("Shot validation failed: Time difference too large (%f)"), 
            FMath::Abs(ServerTime - ShotRequest.ClientTimeStamp));
        return false;
    }

    FVector WeaponForward = Weapon->GetActorForwardVector();
    float Angle = FMath::RadiansToDegrees(acosf(FVector::DotProduct(ShotRequest.Direction.GetSafeNormal(), WeaponForward)));
    if (Angle > 45.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("Shot validation failed: Angle too large (%f > 45)"), Angle);
        return false;
    }

    return true;
}

void UMedComBaseFireAbility::ServerProcessShotTrace(const FMedComShotRequest& ShotRequest, TArray<FHitResult>& OutHits)
{
    // ОТЛАДКА: Выводим что используем
    UE_LOG(LogTemp, Warning, TEXT("ServerProcessShotTrace: Origin=%s, Direction=%s"),
           *ShotRequest.Origin.ToString(), *ShotRequest.Direction.ToString());
           
    FRandomStream RandomStream(ShotRequest.RandomSeed);
    AWeaponActor* Weapon = GetWeaponFromActorInfo();
    
    // Дополнительная проверка на валидность Origin 
    if (ShotRequest.Origin.IsZero())
    {
        UE_LOG(LogTemp, Error, TEXT("ShotRequest.Origin is Zero! Using fallback."));
        
        // Используем запасной вариант - положение аватара или оружия, если есть
        FVector FallbackOrigin = Weapon ? 
            Weapon->GetMuzzleLocation() : 
            (GetCurrentActorInfo()->AvatarActor.IsValid() ? 
                GetCurrentActorInfo()->AvatarActor->GetActorLocation() : 
                FVector::ZeroVector);
                
        // Создаем временную копию запроса с исправленной Origin
        FMedComShotRequest FixedRequest = ShotRequest;
        FixedRequest.Origin = FallbackOrigin;
        
        // Рекурсивно вызываем с исправленными данными
        ServerProcessShotTrace(FixedRequest, OutHits);
        return;
    }
    
    // Создаем список игнорируемых акторов
    TArray<AActor*> ActorsToIgnore;
    if (GetCurrentActorInfo()->AvatarActor.IsValid())
        ActorsToIgnore.Add(GetCurrentActorInfo()->AvatarActor.Get());
    if (Weapon)
    {
        ActorsToIgnore.Add(Weapon);
        if (Weapon->GetOwner())
            ActorsToIgnore.Add(Weapon->GetOwner());
    }
    
    for (int32 i = 0; i < ShotRequest.NumTraces; i++)
    {
        float HalfConeAngle = FMath::DegreesToRadians(ShotRequest.SpreadAngle * 0.5f);
        FVector RandomDir = RandomStream.VRandCone(ShotRequest.Direction, HalfConeAngle, HalfConeAngle);
        FVector End = ShotRequest.Origin + (RandomDir * ShotRequest.MaxRange);

        // Используем наш новый хелпер для трассировки
        TArray<FHitResult> TempHits;
        UMedComTraceUtils::PerformLineTrace(
            GetWorld(),
            ShotRequest.Origin,
            End,
            ShotRequest.TraceProfile,
            ActorsToIgnore,
            ShotRequest.bDebug,
            ShotRequest.DebugDrawTime,
            TempHits
        );

        if (TempHits.Num() == 0)
        {
            FHitResult DefaultHit;
            DefaultHit.TraceStart = ShotRequest.Origin;
            DefaultHit.TraceEnd = End;
            DefaultHit.Location = End;
            DefaultHit.ImpactPoint = End;
            OutHits.Add(DefaultHit);
        }
        else
        {
            OutHits.Append(TempHits);
        }
    }
}

void UMedComBaseFireAbility::ApplyDamageToTargets(const TArray<FHitResult>& Hits, float InDamage)
{
    // Check for valid ActorInfo and ASC
    if (!GetCurrentActorInfo())
    {
        UE_LOG(LogTemp, Error, TEXT("ApplyDamageToTargets: ActorInfo is NULL"));
        return;
    }
    
    if (!GetCurrentActorInfo()->AbilitySystemComponent.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("ApplyDamageToTargets: ASC in ActorInfo is NULL"));
        return;
    }

    // Only server should apply real damage
    if (!GetCurrentActorInfo()->IsNetAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("ApplyDamageToTargets: Not executed on authority, skipping damage application"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("ApplyDamageToTargets: Processing %d hits with damage %.2f"), Hits.Num(), InDamage);

    UAbilitySystemComponent* SourceASC = GetCurrentActorInfo()->AbilitySystemComponent.Get();
    for (const FHitResult& Hit : Hits)
    {
        AActor* TargetActor = Hit.GetActor();
        if (!TargetActor)
        {
            UE_LOG(LogTemp, Warning, TEXT("ApplyDamageToTargets: Hit without actor"));
            continue;
        }
        
        UE_LOG(LogTemp, Warning, TEXT("ApplyDamageToTargets: Hit on actor %s, component %s"), 
               *TargetActor->GetName(), 
               *GetNameSafe(Hit.Component.Get()));
        
        // Calculate damage multiplier based on hit location
        float DamageMultiplier = 1.0f;
        FName BoneName = Hit.BoneName;
        UE_LOG(LogTemp, Warning, TEXT("ApplyDamageToTargets: Hit on bone %s"), *BoneName.ToString());
        
        if (BoneName.ToString().Contains("head", ESearchCase::IgnoreCase) ||
            BoneName.ToString().Contains("neck", ESearchCase::IgnoreCase))
        {
            DamageMultiplier = 2.0f;
            UE_LOG(LogTemp, Warning, TEXT("ApplyDamageToTargets: Headshot! Multiplier = %.2f"), DamageMultiplier);
        }
        
        float FinalDamage = InDamage * DamageMultiplier;
        
        // Try to get target's ASC - first check if it's a player character
        UAbilitySystemComponent* TargetASC = nullptr;
        AMedComCharacter* PlayerCharacter = Cast<AMedComCharacter>(TargetActor);
        if (PlayerCharacter)
        {
            UE_LOG(LogTemp, Warning, TEXT("ApplyDamageToTargets: Target is a player character, getting ASC from PlayerState"));
            TargetASC = PlayerCharacter->GetAbilitySystemComponent();
        }
        else
        {
            // If not a player, try standard method
            TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
        }
        
        if (TargetASC && DamageEffect)
        {
            // GAS path: use Gameplay Effect for damage
            UE_LOG(LogTemp, Warning, TEXT("ApplyDamageToTargets: Found TargetASC, applying effect"));
            
            // Create effect context
            FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
            ContextHandle.AddSourceObject(this);
            ContextHandle.AddHitResult(Hit);
            
            // Create damage spec
            FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
                DamageEffect, GetAbilityLevel(), ContextHandle);
                
            if (SpecHandle.IsValid())
            {
                // Set damage magnitude via SetByCaller (negative value for damage)
                SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag("Data.Damage"), -FinalDamage);
                
                // Add headshot tag if applicable
                if (DamageMultiplier > 1.0f)
                {
                    SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag("Data.Damage.Headshot"), 1.0f);
                }
                
                // Apply effect to target
                FActiveGameplayEffectHandle EffectHandle = SourceASC->ApplyGameplayEffectSpecToTarget(
                    *SpecHandle.Data.Get(), TargetASC);
                    
                UE_LOG(LogTemp, Warning, TEXT("ApplyDamageToTargets: Effect applied to target, handle: %s"), 
                    *EffectHandle.ToString());
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("ApplyDamageToTargets: Failed to create valid effect spec"));
            }
        }
        else if (TargetASC)
        {
            // If no effect but has ASC - direct Health attribute modification
            UE_LOG(LogTemp, Warning, TEXT("ApplyDamageToTargets: No effect, applying damage directly to attribute"));
            
            // Find Health attribute
            FGameplayAttribute HealthAttribute = UMedComBaseAttributeSet::GetHealthAttribute();
            
            // Get current value
            float CurrentHealth = TargetASC->GetNumericAttribute(HealthAttribute);
            
            // Calculate new value
            float NewHealth = FMath::Max(0.0f, CurrentHealth - FinalDamage);
            
            // Set new value
            TargetASC->SetNumericAttributeBase(HealthAttribute, NewHealth);
            
            UE_LOG(LogTemp, Warning, TEXT("ApplyDamageToTargets: Damage directly applied to attribute. Health: %.2f -> %.2f"),
                   CurrentHealth, NewHealth);
                   
            // Trigger damage event
            FGameplayEventData Payload;
            Payload.EventTag = FGameplayTag::RequestGameplayTag("Event.Damage");
            Payload.EventMagnitude = FinalDamage;
            Payload.Instigator = GetCurrentActorInfo()->OwnerActor.Get();
            Payload.Target = TargetActor;
            
            TargetASC->HandleGameplayEvent(FGameplayTag::RequestGameplayTag("Event.Damage"), &Payload);
            
            // Trigger headshot event if it was a headshot
            if (DamageMultiplier > 1.0f)
            {
                FGameplayEventData HeadshotPayload;
                HeadshotPayload.EventTag = FGameplayTag::RequestGameplayTag("Event.Damage.Headshot");
                HeadshotPayload.Instigator = GetCurrentActorInfo()->OwnerActor.Get();
                HeadshotPayload.Target = TargetActor;
                
                TargetASC->HandleGameplayEvent(FGameplayTag::RequestGameplayTag("Event.Damage.Headshot"), &HeadshotPayload);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("ApplyDamageToTargets: No AbilitySystemComponent found on target %s"), 
                  *TargetActor->GetName());
        }
    }
}

bool UMedComBaseFireAbility::HasAmmo(AWeaponActor* Weapon) const
{
    if (!Weapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("HasAmmo: Weapon is NULL"));
        return false;
    }
    
    UMedComWeaponAttributeSet* AttrSet = Weapon->GetWeaponAttributeSet();
    if (!AttrSet)
    {
        UE_LOG(LogTemp, Warning, TEXT("HasAmmo: Weapon AttributeSet is NULL"));
        return false;
    }
    
    float CurrentAmmo = AttrSet->GetCurrentAmmo();
    UE_LOG(LogTemp, Warning, TEXT("HasAmmo check: Current ammo is %f"), CurrentAmmo);
    
    return (CurrentAmmo > 0.f);
}

void UMedComBaseFireAbility::ConsumeAmmo(AWeaponActor* Weapon)
{
    if (!Weapon)
    {
        UE_LOG(LogTemp, Error, TEXT("ConsumeAmmo: Weapon is NULL"));
        return;
    }
    
    UMedComWeaponAttributeSet* AttrSet = Weapon->GetWeaponAttributeSet();
    if (!AttrSet)
    {
        UE_LOG(LogTemp, Error, TEXT("ConsumeAmmo: Weapon AttributeSet is NULL"));
        return;
    }
    
    UAbilitySystemComponent* WeaponASC = Weapon->GetAbilitySystemComponent();
    if (!WeaponASC)
    {
        UE_LOG(LogTemp, Error, TEXT("ConsumeAmmo: Weapon ASC is NULL"));
        return;
    }
    
    float CurrentAmmo = AttrSet->GetCurrentAmmo();
    UE_LOG(LogTemp, Warning, TEXT("ConsumeAmmo: Current ammo before consumption: %f"), CurrentAmmo);
    
    // Using PreAttributeChange instead of direct modification
    FGameplayAttributeData* AmmoData = AttrSet->GetCurrentAmmoAttribute().GetGameplayAttributeData(AttrSet);
    if (AmmoData)
    {
        // Save the old value
        float OldValue = AmmoData->GetCurrentValue();
        
        // Set the new value
        float NewValue = FMath::Max(0.f, OldValue - 1.f);
        AmmoData->SetCurrentValue(NewValue);
        
        // Force attribute update - this is important!
        WeaponASC->SetNumericAttributeBase(AttrSet->GetCurrentAmmoAttribute(), NewValue);
        
        UE_LOG(LogTemp, Warning, TEXT("ConsumeAmmo: New ammo after consumption: %f"), AttrSet->GetCurrentAmmo());
        
        // Notify UI about ammo change
        Weapon->NotifyAmmoChanged(FMath::FloorToInt(NewValue));
        
        // Check if we're out of ammo
        if (NewValue <= 0.f)
        {
            UE_LOG(LogTemp, Warning, TEXT("ConsumeAmmo: Out of ammo, sending event"));
            FGameplayEventData Payload;
            Payload.EventTag = FGameplayTag::RequestGameplayTag("Event.Weapon.OutOfAmmo");
            
            if (GetCurrentActorInfo() && GetCurrentActorInfo()->AbilitySystemComponent.IsValid())
            {
                GetCurrentActorInfo()->AbilitySystemComponent->HandleGameplayEvent(
                    FGameplayTag::RequestGameplayTag("Event.Weapon.OutOfAmmo"), 
                    &Payload);
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("ConsumeAmmo: Failed to get AmmoData"));
    }
}

void UMedComBaseFireAbility::HandleAsyncTraceResults(const TArray<FHitResult>& HitResults)
{
    // Обработка результатов трассировки
    UE_LOG(LogTemp, Log, TEXT("AsyncTrace completed with %d hits"), HitResults.Num());
    
    // Получаем оружие для позиции дула
    AWeaponActor* Weapon = GetWeaponFromActorInfo();
    UWorld* World = GetWorld();
    if (!Weapon || !World)
    {
        return;
    }
    
    // Определяем начальную точку для трассировки
    FVector MuzzleLocation = Weapon->GetMuzzleLocation();
    
    // СОЗДАЕМ ТРАССЕР на основе результатов асинхронной трассировки
    if (HitResults.Num() > 0 && BulletTracerNiagara)
    {
        const FHitResult& Hit = HitResults[0];
        FVector ImpactPoint = Hit.bBlockingHit ? Hit.ImpactPoint : Hit.TraceEnd;
        FRotator TracerRotation = (ImpactPoint - MuzzleLocation).Rotation();
        
        UNiagaraComponent* TracerComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World,
            BulletTracerNiagara,
            MuzzleLocation,
            TracerRotation,
            FVector(1.0f),
            true,
            true,
            ENCPoolMethod::AutoRelease
        );
        
        if (TracerComp)
        {
            // Добавляем параметры для трассера
            TracerComp->SetVectorParameter("BeamSource", MuzzleLocation);
            TracerComp->SetVectorParameter("BeamTarget", ImpactPoint);
            float TracerDistance = FVector::Dist(MuzzleLocation, ImpactPoint);
            TracerComp->SetFloatParameter("BeamLength", TracerDistance);
            
            UE_LOG(LogTemp, Warning, TEXT("Bullet tracer spawned in HandleAsyncTraceResults"));
        }
    }
    
    // Показываем эффекты попадания для каждого хита
    for (const FHitResult& Hit : HitResults)
    {
        if (Hit.bBlockingHit)
        {
            PlayImpactEffects(Hit);
        }
    }
}

void UMedComBaseFireAbility::ApplyRecoil(const FGameplayAbilityActorInfo* ActorInfo)
{
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !ActorInfo->IsLocallyControlled())
    {
        return;
    }
    
    AWeaponActor* Weapon = GetWeaponFromActorInfo();
    if (!Weapon)
    {
        return;
    }
    
    // Получаем базовую отдачу из атрибутов оружия
    UMedComWeaponAttributeSet* AttrSet = Weapon->GetWeaponAttributeSet();
    float BaseRecoilAmount = AttrSet->GetRecoil();
    
    // Обновляем счетчик последовательных выстрелов
    IncrementShotCounter();
    
    // Проверяем, находится ли персонаж в режиме прицеливания (ADS)
    bool bIsAiming = false;
    if (ActorInfo->AbilitySystemComponent.IsValid())
    {
        bIsAiming = ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(AimingTag);
    }
    
    // Множитель для отдачи в ADS (например, 50% от обычной отдачи)
    float ADSRecoilMultiplier = bIsAiming ? 0.5f : 1.0f;
    
    // Вычисляем итоговую отдачу с учетом прогрессивного увеличения и ADS-множителя
    float FinalRecoilAmount = BaseRecoilAmount * CurrentRecoilMultiplier * ADSRecoilMultiplier;
    
    UE_LOG(LogTemp, Warning, TEXT("Applying recoil: Base=%f, Multiplier=%f, ADSMultiplier=%f, Final=%f, Shots=%d"), 
        BaseRecoilAmount, CurrentRecoilMultiplier, ADSRecoilMultiplier, FinalRecoilAmount, ConsecutiveShotsCount);
    
    if (APlayerController* PC = Cast<APlayerController>(ActorInfo->PlayerController.Get()))
    {
        float PitchRecoil = FinalRecoilAmount * FMath::RandRange(0.8f, 1.2f);
        float YawRecoil = FinalRecoilAmount * FMath::RandRange(-0.5f, 0.5f);
        
        AccumulatedRecoil.X += PitchRecoil;
        AccumulatedRecoil.Y += YawRecoil;
        
        FRotator CurrentRotation = PC->GetControlRotation();
        FRotator NewRotation = CurrentRotation + FRotator(PitchRecoil, YawRecoil, 0.0f);
        
        PC->SetControlRotation(NewRotation);
        
        UE_LOG(LogTemp, Warning, TEXT("Applied recoil: Pitch=%f, Yaw=%f, Accumulated=[%f, %f]"), 
            PitchRecoil, YawRecoil, AccumulatedRecoil.X, AccumulatedRecoil.Y);
        
        StartRecoilRecovery();
        
        float ShakeScale = bIsAiming ? 0.5f : 1.0f;
        if (FireCameraShake)
        {
            PC->ClientStartCameraShake(FireCameraShake, ShakeScale);
        }
    }
}

void UMedComBaseFireAbility::IncrementShotCounter()
{
    // Clear any existing reset timer
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RecoilResetTimerHandle);
    }
    
    // Increment the counter
    ConsecutiveShotsCount++;
    
    // Calculate new recoil multiplier (clamped to maximum)
    // Linear approach: each shot increases by a fixed percentage
    CurrentRecoilMultiplier = FMath::Min(1.0f + (ConsecutiveShotsCount - 1) * (ProgressiveRecoilMultiplier - 1.0f), MaximumRecoilMultiplier);
    
    // Schedule a reset after RecoilResetTime
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            RecoilResetTimerHandle,
            this,
            &UMedComBaseFireAbility::ResetShotCounter,
            RecoilResetTime,
            false
        );
    }
}

void UMedComBaseFireAbility::ResetShotCounter()
{
    UE_LOG(LogTemp, Verbose, TEXT("Resetting shot counter from %d to 0"), ConsecutiveShotsCount);
    ConsecutiveShotsCount = 0;
    CurrentRecoilMultiplier = 1.0f;
}

void UMedComBaseFireAbility::StartRecoilRecovery()
{
    // Clear any existing recovery timer
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RecoilRecoveryTimerHandle);
        
        // Start the recovery process after a short delay
        World->GetTimerManager().SetTimer(
            RecoilRecoveryTimerHandle,
            this,
            &UMedComBaseFireAbility::ProcessRecoilRecovery,
            RecoilRecoveryTime,
            true,  // Loop this timer
            RecoilRecoveryDelay  // Initial delay before first recovery
        );
    }
}

void UMedComBaseFireAbility::ProcessRecoilRecovery()
{
    if (!IsValid(this) || AccumulatedRecoil.IsNearlyZero(0.01f))
    {
        // If there's no recoil left to recover or ability is invalid, stop the timer
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(RecoilRecoveryTimerHandle);
        }
        return;
    }
    
    // Get the player controller
    if (const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo())
    {
        if (APlayerController* PC = Cast<APlayerController>(ActorInfo->PlayerController.Get()))
        {
            // Calculate recovery amounts (recover faster on y-axis for less horizontal drift)
            float PitchRecovery = AccumulatedRecoil.X * RecoilRecoveryRate;
            float YawRecovery = AccumulatedRecoil.Y * RecoilRecoveryRate * 1.2f;
            
            // Update accumulated recoil
            AccumulatedRecoil.X -= PitchRecovery;
            AccumulatedRecoil.Y -= YawRecovery;
            
            // Apply inverse recoil (recovery)
            FRotator CurrentRotation = PC->GetControlRotation();
            FRotator RecoveryRotation = CurrentRotation + FRotator(-PitchRecovery, -YawRecovery, 0.0f);
            PC->SetControlRotation(RecoveryRotation);
            
            UE_LOG(LogTemp, Verbose, TEXT("Recoil Recovery: Pitch=%f, Yaw=%f, Remaining=[%f, %f]"), 
                -PitchRecovery, -YawRecovery, AccumulatedRecoil.X, AccumulatedRecoil.Y);
        }
    }
}

void UMedComBaseFireAbility::PlayLocalFireEffects(const FGameplayAbilityActorInfo* ActorInfo, bool bCreateTracer)
{
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid())
    {
        return;
    }
    
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    
    bool bIsLocallyControlled = ActorInfo->IsLocallyControlled();
    AActor* Avatar = ActorInfo->AvatarActor.Get();
    AWeaponActor* Weapon = GetWeaponFromActorInfo();
    
    // Сначала пытаемся обработать персонажа игрока
    AMedComCharacter* PlayerCharacter = Cast<AMedComCharacter>(Avatar);
    if (PlayerCharacter && bIsLocallyControlled && PlayerCharacter->Mesh1P)
    {
        // Проигрываем анимацию только для игрока от первого лица
        if (FireMontage)
        {
            UE_LOG(LogTemp, Warning, TEXT("Playing FireAnimMontage on Mesh1P for player character"));
            
            UAnimInstance* AnimInstance = PlayerCharacter->Mesh1P->GetAnimInstance();
            if (AnimInstance)
            {
                AnimInstance->Montage_Play(FireMontage, 1.0f);
            }
        }
    }
    // Проверяем, является ли аватар противником
    else if (AMedComEnemyCharacter* EnemyCharacter = Cast<AMedComEnemyCharacter>(Avatar))
    {
        // Для противника можно проиграть анимацию на основном меше, если нужно
        if (FireMontage && EnemyCharacter->GetMesh())
        {
            UE_LOG(LogTemp, Warning, TEXT("Playing FireAnimMontage on enemy character mesh"));
            
            UAnimInstance* AnimInstance = EnemyCharacter->GetMesh()->GetAnimInstance();
            if (AnimInstance)
            {
                AnimInstance->Montage_Play(FireMontage, 1.0f);
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Verbose, TEXT("Avatar is neither player nor enemy character"));
    }
    
    // Получаем положение и ориентацию дула
    FVector MuzzleLocation = Weapon ? Weapon->GetMuzzleLocation() : Avatar->GetActorLocation();
    FRotator MuzzleRotation = Weapon ? Weapon->GetMuzzleRotation() : Avatar->GetActorRotation();
    
    // Проигрываем звук выстрела
    if (FireSound)
    {
        UGameplayStatics::PlaySoundAtLocation(World, FireSound, MuzzleLocation);
    }
    
    // Спавним Niagara эффект дульного пламени
    if (MuzzleFlashNiagara)
    {
        UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World,
            MuzzleFlashNiagara,
            MuzzleLocation,
            MuzzleRotation,
            FVector(1.0f),
            true,
            true
        );
    
        if (NiagaraComp)
        {
            UE_LOG(LogTemp, Warning, TEXT("MuzzleFlash Niagara spawned"));
        }
    }
    
    // Создаем трассер пули только если запрошено
    if (bCreateTracer)
    {
        // Создаем список игнорируемых акторов
        TArray<AActor*> ActorsToIgnore;
        ActorsToIgnore.Add(Avatar);
        if (Weapon) ActorsToIgnore.Add(Weapon);
        
        // Трассировка для определения точки попадания
        FVector EndTrace = MuzzleLocation + MuzzleRotation.Vector() * 10000.f;
        
        // Используем новый хелпер для трассировки
        TArray<FHitResult> TempHits;
        UMedComTraceUtils::PerformLineTrace(
            World,
            MuzzleLocation,
            EndTrace,
            TraceProfile.IsNone() ? FName("BlockAll") : TraceProfile,
            ActorsToIgnore,
            bDebugTrace,
            2.0f,
            TempHits
        );
        
        FVector TracerEnd = TempHits.Num() > 0 && TempHits[0].bBlockingHit ? 
                            TempHits[0].ImpactPoint : EndTrace;
        
        // Спавним Niagara трассер пули
        if (BulletTracerNiagara)
        {
            UNiagaraComponent* TracerComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                World,
                BulletTracerNiagara,
                MuzzleLocation,
                (TracerEnd - MuzzleLocation).Rotation(),
                FVector(1.0f)
            );
            
            if (TracerComp)
            {
                // Устанавливаем параметры для трассера
                TracerComp->SetVectorParameter("Position", MuzzleLocation);
                TracerComp->SetVectorParameter("Velocity", (TracerEnd - MuzzleLocation).GetSafeNormal() * 5000.0f);
                TracerComp->SetFloatParameter("RibbonUVDistance", FVector::Dist(MuzzleLocation, TracerEnd));
                TracerComp->SetFloatParameter("DistanceTraveled", FVector::Dist(MuzzleLocation, TracerEnd));
                
                UE_LOG(LogTemp, Warning, TEXT("Bullet tracer spawned in PlayLocalFireEffects"));
            }
        }
        
        // Если попали, спавним эффект попадания
        if (TempHits.Num() > 0 && TempHits[0].bBlockingHit && ImpactEffectNiagara)
        {
            const FHitResult& Hit = TempHits[0];
            PlayImpactEffects(Hit);
        }
    }
}

void UMedComBaseFireAbility::PlayImpactEffects(const FHitResult& HitResult)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    
    // Получаем физический материал поверхности
    UPhysicalMaterial* PhysMat = HitResult.PhysMaterial.Get();
    
    // ----- NIAGARA ЭФФЕКТ ПОПАДАНИЯ -----
    // Спавним Niagara-эффект попадания
    if (ImpactEffectNiagara)
    {
        UNiagaraComponent* ImpactComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            World,
            ImpactEffectNiagara,
            HitResult.ImpactPoint,
            HitResult.ImpactNormal.Rotation(),
            FVector(1.0f),
            true,
            true,
            ENCPoolMethod::AutoRelease
        );
        
        if (ImpactComp)
        {
            // Устанавливаем параметры для Niagara-системы
            if (PhysMat)
            {
                // Например, если в Niagara-системе есть параметр "SurfaceType"
                ImpactComp->SetIntParameter("SurfaceType", PhysMat->SurfaceType);
            }
            
            // Добавляем нормаль поверхности для правильной ориентации эффекта
            ImpactComp->SetVectorParameter("SurfaceNormal", HitResult.ImpactNormal);
            
            UE_LOG(LogTemp, Verbose, TEXT("Spawned Niagara impact effect at %s"), *HitResult.ImpactPoint.ToString());
        }
    }
    
    // Проигрываем звук попадания
    USoundBase* ImpactSound = nullptr;
    // Здесь можно добавить логику выбора звука в зависимости от типа поверхности
    if (ImpactSound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            World,
            ImpactSound,
            HitResult.ImpactPoint
        );
    }
    
    // Спавним декаль на поверхности попадания
    FVector DecalSize = FVector(10.0f, 10.0f, 10.0f);
    float DecalLifeSpan = 10.0f;
    
    UPrimitiveComponent* HitComponent = HitResult.Component.Get();
    if (HitComponent && HitComponent->IsA(UStaticMeshComponent::StaticClass()))
    {
        UGameplayStatics::SpawnDecalAttached(
            nullptr, // Здесь можно создать материал декали в зависимости от PhysMat
            DecalSize,
            HitComponent,
            NAME_None,
            HitResult.ImpactPoint,
            HitResult.ImpactNormal.Rotation(),
            EAttachLocation::KeepWorldPosition,
            DecalLifeSpan
        );
    }
}

AWeaponActor* UMedComBaseFireAbility::GetWeaponFromActorInfo() const
{
    const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
    if (!ActorInfo)
    {
        UE_LOG(LogTemp, Error, TEXT("GetWeaponFromActorInfo: ActorInfo равен NULL"));
        return nullptr;
    }
    
    // Логирование информации об акторах
    if (ActorInfo->AvatarActor.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("AvatarActor: %s"), *ActorInfo->AvatarActor->GetName());
    }
    
    if (ActorInfo->OwnerActor.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("OwnerActor: %s"), *ActorInfo->OwnerActor->GetName());
    }
    
    // ИСПРАВЛЕНИЕ: Проверяем, является ли AvatarActor врагом
    if (ActorInfo->AvatarActor.IsValid())
    {
        // 1. Пробуем привести к типу персонажа игрока
        AMedComCharacter* Character = Cast<AMedComCharacter>(ActorInfo->AvatarActor.Get());
        if (Character)
        {
            UE_LOG(LogTemp, Warning, TEXT("Успешное приведение к AMedComCharacter: %s"), *Character->GetName());
            
            AWeaponActor* Weapon = Character->GetCurrentWeapon();
            if (Weapon)
            {
                UE_LOG(LogTemp, Warning, TEXT("Персонаж имеет оружие: %s"), *Weapon->GetName());
                return Weapon;
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Character->GetCurrentWeapon() вернул NULL"));
                
                // Попытка получить оружие через компонент экипировки
                if (AMedComPlayerState* PS = Character->GetPlayerState<AMedComPlayerState>())
                {
                    if (UMedComEquipmentComponent* EquipComp = PS->GetEquipmentComponent())
                    {
                        AWeaponActor* FoundWeapon = EquipComp->GetActiveWeapon();
                        if (FoundWeapon)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("Найдено оружие через EquipmentComponent: %s"), *FoundWeapon->GetName());
                            return FoundWeapon;
                        }
                    }
                }
            }
        }
        // 2. ИСПРАВЛЕНИЕ: Используем имя класса для проверки на AMedComEnemyCharacter
        else
        {
            // Проверяем имя класса, так как прямое приведение может не сработать
            FString ClassName = ActorInfo->AvatarActor->GetClass()->GetName();
            if (ClassName.Contains("Enemy"))
            {
                UE_LOG(LogTemp, Warning, TEXT("Обнаружен враг по имени класса: %s"), *ClassName);
                
                // Проверка на наличие оружия через обращение к компонентам
                UActorComponent* WeaponComp = ActorInfo->AvatarActor->GetComponentByClass(USkeletalMeshComponent::StaticClass());
                
                // Ищем все прикрепленные акторы
                TArray<AActor*> AttachedActors;
                ActorInfo->AvatarActor->GetAttachedActors(AttachedActors);
                
                for (AActor* Actor : AttachedActors)
                {
                    if (AWeaponActor* WeaponActor = Cast<AWeaponActor>(Actor))
                    {
                        UE_LOG(LogTemp, Warning, TEXT("GetWeaponFromAvatar: Найдено оружие у врага: %s"), *WeaponActor->GetName());
                        return WeaponActor;
                    }
                }
                
                // Пытаемся получить WeaponHandlerComponent
                TArray<UActorComponent*> Components;
                ActorInfo->AvatarActor->GetComponents(Components);
                
                for (UActorComponent* Comp : Components)
                {
                    if (Comp->GetName().Contains("WeaponHandler"))
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Найден WeaponHandlerComponent, возможно оружие доступно через него"));
                        break;
                    }
                }
            }
        }
    }
    
    // Проверка OwnerActor на оружие
    if (ActorInfo->OwnerActor.IsValid())
    {
        AWeaponActor* OwnerWeapon = Cast<AWeaponActor>(ActorInfo->OwnerActor.Get());
        if (OwnerWeapon)
        {
            UE_LOG(LogTemp, Warning, TEXT("Найдено оружие непосредственно как OwnerActor: %s"), *OwnerWeapon->GetName());
            return OwnerWeapon;
        }
    }
    
    // Поиск оружия в прикрепленных акторах
    if (ActorInfo->AvatarActor.IsValid())
    {
        TArray<AActor*> ChildActors;
        ActorInfo->AvatarActor->GetAttachedActors(ChildActors);
        
        for (AActor* Child : ChildActors)
        {
            if (AWeaponActor* WeaponChild = Cast<AWeaponActor>(Child))
            {
                UE_LOG(LogTemp, Warning, TEXT("Найдено оружие как прикрепленный актор: %s"), *WeaponChild->GetName());
                return WeaponChild;
            }
        }
    }
    
    UE_LOG(LogTemp, Error, TEXT("GetWeaponFromActorInfo: Не удалось найти оружие любым методом"));
    return nullptr;
}