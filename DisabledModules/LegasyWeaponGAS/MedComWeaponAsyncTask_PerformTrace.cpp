#include "Core/AbilitySystem/Abilities/Tasks/MedComWeaponAsyncTask_PerformTrace.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "Equipment/Base/WeaponActor.h"
#include "Core/Character/MedComCharacter.h"
#include "Core/MedComPlayerState.h"
#include "Equipment/Components/MedComEquipmentComponent.h"
#include "Equipment/Components/EquipmentMeshComponent.h"
#include "Core/Enemy/MedComEnemyCharacter.h"
#include "Core/AbilitySystem/Attributes/MedComWeaponAttributeSet.h"
#include "Core/AbilitySystem/Abilities/Tasks/MedComTraceUtils.h"
#include "GameplayTagContainer.h"

UMedComWeaponAsyncTask_PerformTrace* UMedComWeaponAsyncTask_PerformTrace::PerformWeaponTrace(
    UGameplayAbility* OwningAbility,
    FName TaskInstanceName,
    const FMedComWeaponTraceConfig& InTraceConfig)
{
    UMedComWeaponAsyncTask_PerformTrace* MyObj = NewAbilityTask<UMedComWeaponAsyncTask_PerformTrace>(OwningAbility, TaskInstanceName);
    MyObj->TraceConfig = InTraceConfig;
    MyObj->bUseShotRequest = false;
    MyObj->CachedWeaponAttributeSet = nullptr;
    
    return MyObj;
}

UMedComWeaponAsyncTask_PerformTrace* UMedComWeaponAsyncTask_PerformTrace::PerformWeaponTraceFromRequest(
    UGameplayAbility* OwningAbility,
    FName TaskInstanceName,
    const FMedComShotRequest& InShotRequest)
{
    UMedComWeaponAsyncTask_PerformTrace* MyObj = NewAbilityTask<UMedComWeaponAsyncTask_PerformTrace>(OwningAbility, TaskInstanceName);
    MyObj->ShotRequest = InShotRequest;
    MyObj->bUseShotRequest = true;
    MyObj->CachedWeaponAttributeSet = nullptr;
    
    return MyObj;
}

AActor* UMedComWeaponAsyncTask_PerformTrace::GetAvatarActorFromActorInfo() const
{
    if (!AbilitySystemComponent.IsValid() || !AbilitySystemComponent->AbilityActorInfo.IsValid())
    {
        return nullptr;
    }
    
    return AbilitySystemComponent->AbilityActorInfo->AvatarActor.Get();
}

AWeaponActor* UMedComWeaponAsyncTask_PerformTrace::GetWeaponFromAvatar() const
{
    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
        UE_LOG(LogTemp, Error, TEXT("GetWeaponFromAvatar: No valid avatar actor"));
        return nullptr;
    }
    
    // 1. Пытаемся получить персонажа игрока
    AMedComCharacter* Character = Cast<AMedComCharacter>(AvatarActor);
    if (Character)
    {
        // Получаем оружие через персонажа
        AWeaponActor* Weapon = Character->GetCurrentWeapon();
        if (Weapon)
        {
            UE_LOG(LogTemp, Verbose, TEXT("GetWeaponFromAvatar: Found weapon from character: %s"), *Weapon->GetName());
            return Weapon;
        }
        else
        {
            UE_LOG(LogTemp, Verbose, TEXT("GetWeaponFromAvatar: Character doesn't have an active weapon"));
            
            // Пробуем получить через PlayerState и компонент экипировки
            if (AMedComPlayerState* PS = Character->GetPlayerState<AMedComPlayerState>())
            {
                if (UMedComEquipmentComponent* EquipComp = PS->GetEquipmentComponent())
                {
                    Weapon = EquipComp->GetActiveWeapon();
                    if (Weapon)
                    {
                        UE_LOG(LogTemp, Verbose, TEXT("GetWeaponFromAvatar: Found weapon through EquipmentComponent: %s"), 
                            *Weapon->GetName());
                        return Weapon;
                    }
                }
            }
        }
    }
    // 2. Пытаемся получить противника
    else if (AMedComEnemyCharacter* EnemyCharacter = Cast<AMedComEnemyCharacter>(AvatarActor))
    {
        // Получаем оружие через противника
        AWeaponActor* Weapon = EnemyCharacter->GetCurrentWeapon();
        if (Weapon)
        {
            UE_LOG(LogTemp, Verbose, TEXT("GetWeaponFromAvatar: Found weapon from enemy character: %s"), *Weapon->GetName());
            return Weapon;
        }
        else
        {
            UE_LOG(LogTemp, Verbose, TEXT("GetWeaponFromAvatar: Enemy character doesn't have an active weapon"));
            
            // Пробуем получить через компонент экипировки противника
            if (UMedComEquipmentComponent* EquipComp = EnemyCharacter->GetEquipmentComponent())
            {
                Weapon = EquipComp->GetActiveWeapon();
                if (Weapon)
                {
                    UE_LOG(LogTemp, Verbose, TEXT("GetWeaponFromAvatar: Found enemy weapon through EquipmentComponent: %s"), 
                        *Weapon->GetName());
                    return Weapon;
                }
            }
        }
    }
    
    // 3. Проверяем прикрепленные акторы
    TArray<AActor*> AttachedActors;
    AvatarActor->GetAttachedActors(AttachedActors);
    
    for (AActor* AttachedActor : AttachedActors)
    {
        if (AWeaponActor* WeaponActor = Cast<AWeaponActor>(AttachedActor))
        {
            UE_LOG(LogTemp, Verbose, TEXT("GetWeaponFromAvatar: Found weapon as attached actor: %s"), *WeaponActor->GetName());
            return WeaponActor;
        }
    }
    
    UE_LOG(LogTemp, Warning, TEXT("GetWeaponFromAvatar: Failed to find weapon for avatar"));
    return nullptr;
}

const UMedComWeaponAttributeSet* UMedComWeaponAsyncTask_PerformTrace::GetWeaponAttributeSet() const
{
    // Если уже кэшировали атрибутсет, возвращаем его
    if (CachedWeaponAttributeSet)
    {
        return CachedWeaponAttributeSet;
    }
    
    AWeaponActor* Weapon = GetWeaponFromAvatar();
    if (!Weapon)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetWeaponAttributeSet: No weapon found"));
        return nullptr;
    }
    
    // Кэшируем атрибутсет
    CachedWeaponAttributeSet = Weapon->GetWeaponAttributeSet();
    if (!CachedWeaponAttributeSet)
    {
        UE_LOG(LogTemp, Warning, TEXT("GetWeaponAttributeSet: Weapon does not have a valid attribute set"));
    }
    
    return CachedWeaponAttributeSet;
}

bool UMedComWeaponAsyncTask_PerformTrace::GetAimPoint(FVector& OutCameraLocation, FVector& OutAimPoint) const
{
    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
        return false;
    }
    
    // Получаем контроллер игрока для доступа к камере
    APlayerController* PC = nullptr;
    if (AMedComCharacter* Character = Cast<AMedComCharacter>(AvatarActor))
    {
        PC = Cast<APlayerController>(Character->GetController());
    }
    
    if (!PC)
    {
        // Для ИИ или других случаев используем вектор forward персонажа
        OutCameraLocation = AvatarActor->GetActorLocation();
        
        // Получаем значение дальности для определения конечной точки
        float MaxRange = 10000.0f;
        const UMedComWeaponAttributeSet* WeaponAttrSet = GetWeaponAttributeSet();
        if (WeaponAttrSet)
        {
            // Здесь предполагается, что в атрибутсете есть поле Range
            // Если нет прямого метода, можно получить через GetCurrentValue и имя атрибута
            // MaxRange = WeaponAttrSet->GetRange();
        }
        
        OutAimPoint = OutCameraLocation + AvatarActor->GetActorForwardVector() * MaxRange;
        return false;
    }
    
    // Получаем оружие для игнорирования при трассировке
    AWeaponActor* Weapon = GetWeaponFromAvatar();
    
    // Создаем список игнорируемых акторов
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(AvatarActor);
    if (Weapon)
    {
        ActorsToIgnore.Add(Weapon);
    }
    
    // Получаем значение дальности для определения конечной точки
    float MaxRange = 10000.0f;
    const UMedComWeaponAttributeSet* WeaponAttrSet = GetWeaponAttributeSet();
    if (WeaponAttrSet)
    {
        // Здесь предполагается, что в атрибутсете есть поле Range
        // Если нет прямого метода, можно получить через GetCurrentValue и имя атрибута
        // MaxRange = WeaponAttrSet->GetRange();
    }
    
    // Используем вспомогательную функцию из TraceUtils
    return UMedComTraceUtils::GetAimPoint(
        PC, 
        MaxRange, 
        TraceConfig.TraceProfile, 
        ActorsToIgnore, 
        TraceConfig.bDebug, 
        TraceConfig.DebugDrawTime, 
        OutCameraLocation, 
        OutAimPoint
    );
}

void UMedComWeaponAsyncTask_PerformTrace::Activate()
{
    if (bUseShotRequest)
    {
        ExecuteTraceFromRequest();
    }
    else
    {
        ExecuteTrace();
    }
}

void UMedComWeaponAsyncTask_PerformTrace::ExternalCancel()
{
    OnCancelled.Broadcast(TArray<FHitResult>());
    EndTask();
}

FString UMedComWeaponAsyncTask_PerformTrace::GetDebugString() const
{
    if (bUseShotRequest)
    {
        return FString::Printf(TEXT("MedComWeaponAsyncTask_PerformTrace: Using ShotRequest, ShotID=%d"), 
                               ShotRequest.ShotID);
    }
    else
    {
        // Получаем актуальные значения атрибутов
        float Range = 10000.0f;
        float Spread = 1.0f;
        int32 NumTraces = TraceConfig.OverrideNumTraces > 0 ? TraceConfig.OverrideNumTraces : 1;
        
        const UMedComWeaponAttributeSet* WeaponAttrSet = GetWeaponAttributeSet();
        if (WeaponAttrSet)
        {
            // Получаем значения из атрибутсета
            Spread = WeaponAttrSet->GetSpread();
            // Range = WeaponAttrSet->GetRange();
            // Для NumTraces нужно добавить соответствующий атрибут в WeaponAttributeSet
        }
        
        return FString::Printf(TEXT("MedComWeaponAsyncTask_PerformTrace: Range=%.1f, Spread=%.2f, NumTraces=%d"), 
                              Range, Spread, NumTraces);
    }
}

void UMedComWeaponAsyncTask_PerformTrace::ExecuteTrace()
{
    UE_LOG(LogTemp, Display, TEXT("UMedComWeaponAsyncTask_PerformTrace::ExecuteTrace - Starting"));
    
    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
        UE_LOG(LogTemp, Error, TEXT("ExecuteTrace: No valid avatar actor"));
        HandleTraceCompleted(TArray<FHitResult>());
        return;
    }

    // Получаем оружие
    AWeaponActor* Weapon = GetWeaponFromAvatar();
    if (!Weapon)
    {
        UE_LOG(LogTemp, Error, TEXT("ExecuteTrace: No valid weapon"));
        HandleTraceCompleted(TArray<FHitResult>());
        return;
    }
    
    // Получаем атрибуты оружия (ленивая инициализация)
    const UMedComWeaponAttributeSet* WeaponAttrSet = GetWeaponAttributeSet();
    
    // Получаем значения из атрибутов
    float MaxRange = 10000.0f;
    float Spread = 1.0f;
    int32 NumTraces = TraceConfig.OverrideNumTraces > 0 ? TraceConfig.OverrideNumTraces : 1;
    
    if (WeaponAttrSet)
    {
        // Получаем значения из атрибутсета
        Spread = WeaponAttrSet->GetSpread();
        // Range = WeaponAttrSet->GetRange();
        // Для NumTraces нужно добавить соответствующий атрибут в WeaponAttributeSet
    }
    
    // Проверяем статус персонажа для модификаторов разброса
    if (AbilitySystemComponent.IsValid())
    {
        // Проверяем прицеливание
        static const FGameplayTag AimingTag = FGameplayTag::RequestGameplayTag("State.Aiming");
        if (AbilitySystemComponent->HasMatchingGameplayTag(AimingTag))
        {
            // Если персонаж прицеливается, уменьшаем разброс
            float AimingMod = 0.3f; // Модификатор по умолчанию (снижение на 70%)
            Spread *= AimingMod;
        }
        
        // Проверяем приседание
        static const FGameplayTag CrouchingTag = FGameplayTag::RequestGameplayTag("State.Crouching");
        if (AbilitySystemComponent->HasMatchingGameplayTag(CrouchingTag))
        {
            // Если персонаж присел, уменьшаем разброс
            float CrouchingMod = 0.7f; // Модификатор по умолчанию (снижение на 30%)
            Spread *= CrouchingMod;
        }
        
        // Проверяем бег
        static const FGameplayTag SprintingTag = FGameplayTag::RequestGameplayTag("State.Sprinting");
        if (AbilitySystemComponent->HasMatchingGameplayTag(SprintingTag))
        {
            // Если персонаж бежит, увеличиваем разброс
            float SprintingMod = 3.0f; // Модификатор по умолчанию (увеличение в 3 раза)
            Spread *= SprintingMod;
        }
        
        // Проверяем прыжок
        static const FGameplayTag JumpingTag = FGameplayTag::RequestGameplayTag("State.Jumping");
        if (AbilitySystemComponent->HasMatchingGameplayTag(JumpingTag))
        {
            // Если персонаж в прыжке, сильно увеличиваем разброс
            float JumpingMod = 4.0f; // Модификатор по умолчанию (увеличение в 4 раза)
            Spread *= JumpingMod;
        }
        
        // Проверяем режим стрельбы
        static const FGameplayTag SingleFireTag = FGameplayTag::RequestGameplayTag("Weapon.FireMode.Single");
        static const FGameplayTag BurstFireTag = FGameplayTag::RequestGameplayTag("Weapon.FireMode.Burst");
        static const FGameplayTag AutoFireTag = FGameplayTag::RequestGameplayTag("Weapon.FireMode.Auto");
        
        if (AbilitySystemComponent->HasMatchingGameplayTag(AutoFireTag))
        {
            // Автоматическая стрельба (сильно увеличенный разброс)
            float AutoFireMod = 2.0f;
            Spread *= AutoFireMod;
        }
        else if (AbilitySystemComponent->HasMatchingGameplayTag(BurstFireTag))
        {
            // Стрельба очередями (увеличенный разброс)
            float BurstFireMod = 1.5f;
            Spread *= BurstFireMod;
        }
        // Для SingleFire используется базовый разброс
    }
    
    // Проверяем, движется ли персонаж и применяем модификатор
    float Speed = AvatarActor->GetVelocity().Size();
    if (Speed > 10.0f)  // Если скорость выше минимального порога
    {
        // Применяем модификатор движения
        float MovementMod = 1.5f;  // Модификатор по умолчанию
        Spread *= MovementMod;
    }
    
    // Определяем начальную точку трассировки (позиция дула)
    FVector StartLocation;
    
    // Если есть оружие и его дуло, используем его как начальную точку
    if (Weapon && Weapon->GetMeshComponent() && Weapon->GetMeshComponent()->DoesSocketExist(Weapon->GetMuzzleSocketName()))
    {
        StartLocation = Weapon->GetMuzzleLocation();
        UE_LOG(LogTemp, Verbose, TEXT("Trace Start: Using muzzle location: %s"), *StartLocation.ToString());
    }
    else
    {
        // Запасной вариант: используем позицию персонажа с небольшим смещением
        StartLocation = AvatarActor->GetActorLocation() + FVector(0, 0, 50.0f);
        UE_LOG(LogTemp, Warning, TEXT("Trace Start: No muzzle, using avatar location: %s"), *StartLocation.ToString());
    }
    
    // Определяем направление выстрела через точку прицеливания
    FVector CameraLocation, AimPoint;
    bool bHasAimPoint = GetAimPoint(CameraLocation, AimPoint);
    
    // Направление от дула к точке прицеливания
    FVector ForwardDir;
    
    if (bHasAimPoint && TraceConfig.bUseMuzzleToScreenCenter)
    {
        // Современная механика шутеров - направление от дула к точке прицеливания
        ForwardDir = (AimPoint - StartLocation).GetSafeNormal();
        UE_LOG(LogTemp, Verbose, TEXT("Trace Direction: Muzzle to aim point (%s)"), *ForwardDir.ToString());
    }
    else if (Weapon)
    {
        // Запасной вариант: используем направление дула оружия
        FRotator MuzzleRotation = Weapon->GetMuzzleRotation();
        ForwardDir = MuzzleRotation.Vector();
        UE_LOG(LogTemp, Verbose, TEXT("Trace Direction: Using muzzle rotation (%s)"), *ForwardDir.ToString());
    }
    else
    {
        // Запасной запасного: используем направление персонажа
        ForwardDir = AvatarActor->GetActorForwardVector();
        UE_LOG(LogTemp, Warning, TEXT("Trace Direction: Fallback to avatar forward (%s)"), *ForwardDir.ToString());
    }
    
    // Результаты всех трассировок
    TArray<FHitResult> AllHits;
    
    // Создаем список игнорируемых акторов
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(AvatarActor);
    if (Weapon) ActorsToIgnore.Add(Weapon);
    
    // Выполняем трассировки (одну или несколько для дробовика)
    for (int32 i = 0; i < NumTraces; i++)
    {
        // Применяем разброс к направлению выстрела
        FVector ShotDir = ForwardDir;
        
        if (Spread > 0.0f)
        {
            // Применяем конический разброс
            float HalfConeAngle = FMath::DegreesToRadians(Spread * 0.5f);
            FRandomStream RandomStream(FMath::Rand());
            ShotDir = RandomStream.VRandCone(ForwardDir, HalfConeAngle, HalfConeAngle);
        }
        
        // Вычисляем конечную точку
        FVector EndLocation = StartLocation + (ShotDir * MaxRange);
        
        // Выполняем трассировку используя утилиты
        TArray<FHitResult> TraceHits;
        bool bSuccess = UMedComTraceUtils::PerformLineTrace(
            GetWorld(),
            StartLocation,
            EndLocation,
            TraceConfig.TraceProfile,
            ActorsToIgnore,
            TraceConfig.bDebug,
            TraceConfig.DebugDrawTime,
            TraceHits
        );
        
        // Добавляем результаты к общему списку
        AllHits.Append(TraceHits);
    }
    
    // Оповещаем оружие об обновлении разброса (для UI)
    if (Weapon)
    {
        Weapon->NotifySpreadUpdated(Spread);
    }
    
    // Завершаем задачу с результатами трассировки
    HandleTraceCompleted(AllHits);
}

void UMedComWeaponAsyncTask_PerformTrace::ExecuteTraceFromRequest()
{
    UE_LOG(LogTemp, Display, TEXT("UMedComWeaponAsyncTask_PerformTrace::ExecuteTraceFromRequest - Starting"));
    
    // Получаем оружие для игнорирования при трассировке
    AActor* AvatarActor = GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
        UE_LOG(LogTemp, Error, TEXT("ExecuteTraceFromRequest: No valid avatar actor"));
        HandleTraceCompleted(TArray<FHitResult>());
        return;
    }

    // Получаем оружие
    AWeaponActor* Weapon = GetWeaponFromAvatar();
    
    // Создаем список игнорируемых акторов
    TArray<AActor*> ActorsToIgnore;
    ActorsToIgnore.Add(AvatarActor);
    if (Weapon) ActorsToIgnore.Add(Weapon);
    
    // Результаты всех трассировок
    TArray<FHitResult> AllHits;
    
    // Выполняем трассировки (одну или несколько для дробовика)
    for (int32 i = 0; i < ShotRequest.NumTraces; i++)
    {
        // Применяем разброс к направлению выстрела
        FVector ShotDir = ShotRequest.Direction;
        
        if (ShotRequest.SpreadAngle > 0.0f)
        {
            // Применяем конический разброс
            float HalfConeAngle = FMath::DegreesToRadians(ShotRequest.SpreadAngle * 0.5f);
            FRandomStream RandomStream(ShotRequest.RandomSeed + i); // Используем последовательные сиды для каждой трассы
            ShotDir = RandomStream.VRandCone(ShotRequest.Direction, HalfConeAngle, HalfConeAngle);
        }
        
        // Вычисляем конечную точку
        FVector EndLocation = ShotRequest.Origin + (ShotDir * ShotRequest.MaxRange);
        
        // Выполняем трассировку используя утилиты
        TArray<FHitResult> TraceHits;
        bool bSuccess = UMedComTraceUtils::PerformLineTrace(
            GetWorld(),
            ShotRequest.Origin,
            EndLocation,
            ShotRequest.TraceProfile,
            ActorsToIgnore,
            ShotRequest.bDebug,
            ShotRequest.DebugDrawTime,
            TraceHits
        );
        
        // Добавляем результаты к общему списку
        AllHits.Append(TraceHits);
    }
    
    // Оповещаем оружие об обновлении разброса (для UI), если оно доступно
    if (Weapon)
    {
        Weapon->NotifySpreadUpdated(ShotRequest.SpreadAngle);
    }
    
    // Завершаем задачу с результатами трассировки
    HandleTraceCompleted(AllHits);
}

void UMedComWeaponAsyncTask_PerformTrace::HandleTraceCompleted(const TArray<FHitResult>& HitResults)
{
    OnCompleted.Broadcast(HitResults);
    EndTask();
}