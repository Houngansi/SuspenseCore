#include "Core/Enemy/MedComEnemyCharacter.h"
#include "CoreMinimal.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Core/AbilitySystem/Attributes/MedComBaseAttributeSet.h"
#include "Core/Enemy/FSM/MedComEnemyFSMComponent.h"
#include "Core/Enemy/FSM/EnemyBehaviorDataAsset.h"
#include "Core/Enemy/FSM/EnemyFSMManager.h"
#include "Core/Enemy/NPCSignificanceManager.h"
#include "Core/Enemy/CrowdManagerSubsystem.h"
#include "Inventory/Components/MedComInventoryComponent.h"
#include "Equipment/Components/MedComEquipmentComponent.h"
#include "Core/Enemy/Components/MedComWeaponHandlerComponent.h"
#include "Core/Enemy/Components/MedComAIMovementComponent.h"
#include "Equipment/Base/WeaponActor.h"
#include "GameplayEffect.h"
#include "Engine/DamageEvents.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"

#include "ProfilingDebugging/CsvProfiler.h"
#include "Runtime/Launch/Resources/Version.h"

// Для CSV профилирования
#include "ProfilingDebugging/CsvProfiler.h"
#include "Runtime/Launch/Resources/Version.h"


// Определение новой категории лога
DEFINE_LOG_CATEGORY_STATIC(LogMedComEnemy, Log, All);
CSV_DEFINE_CATEGORY(EnemyTakeDamage, true);

AMedComEnemyCharacter::AMedComEnemyCharacter()
{
    /* базовые настройки */
    PrimaryActorTick.bCanEverTick = false;
    bIsInitializing               = false;
    RagdollImpulseScale           = 10.f;
    bHasRifle                     = false;
    CurrentWeapon                 = nullptr;
    WeaponAttachSocketName        = TEXT("GripPoint");
    LastNotifiedHealth            = 0.f;
    HealthNotificationThreshold   = 0.05f;
    CurrentDetailLevel            = EAIDetailLevel::Full;
    MinimalLODHealth              = 100.0f;

    /* репликация Pawn */
    bReplicates = true;
    SetNetUpdateFrequency    (10.f);  // Снижено с 20.f
    SetMinNetUpdateFrequency (5.f);   // Снижено с 10.f

    /* Ability-System */
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

    /* FSM */
    FSMComponent = CreateDefaultSubobject<UMedComEnemyFSMComponent>(TEXT("FSMComponent"));

    /* инвентарь / экипировка / AI-движение */
    InventoryComponent  = CreateDefaultSubobject<UMedComInventoryComponent >(TEXT("InventoryComponent"));
    EquipmentComponent  = CreateDefaultSubobject<UMedComEquipmentComponent >(TEXT("EquipmentComponent"));
    AIMovementComponent = CreateDefaultSubobject<UMedComAIMovementComponent>(TEXT("AIMovementComponent"));
    
    /* обработчик оружия */
    WeaponHandlerComponent = CreateDefaultSubobject<UMedComWeaponHandlerComponent>(TEXT("WeaponHandlerComponent"));
    
    /* компонент восприятия - критичные изменения */
    PerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComponent"));

    // Настройка сенсора зрения
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    SightConfig->SightRadius = 2000.0f;  // Увеличиваем радиус зрения
    SightConfig->LoseSightRadius = 2500.0f;  // Увеличиваем радиус потери зрения
    SightConfig->PeripheralVisionAngleDegrees = 80.0f;  // Расширяем угол обзора
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;  // Включаем обнаружение дружественных
    SightConfig->AutoSuccessRangeFromLastSeenLocation = 500.0f;  // Для обнаружения поблизости от последней локации

    // Настройка сенсора слуха
    HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
    HearingConfig->HearingRange = 2000.0f;  // Увеличиваем радиус слуха
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;  // Включаем обнаружение дружественных

    // Добавляем конфигурации в компонент восприятия и устанавливаем более короткие интервалы
    PerceptionComponent->ConfigureSense(*SightConfig);
    PerceptionComponent->ConfigureSense(*HearingConfig);
    PerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());

    // Установка более корректных величин для параметра MaxAge
    for (auto ConfigIt = PerceptionComponent->GetSensesConfigIterator(); ConfigIt; ++ConfigIt)
    {
        UAISenseConfig* Config = *ConfigIt;
        if (Config)
        {
            Config->SetMaxAge(3.0f);  // 3 секунды вместо 5
        }
    }

    // Убедимся, что делегат OnPerceptionUpdated привязан
    PerceptionComponent->OnPerceptionUpdated.AddDynamic(this, &AMedComEnemyCharacter::OnPerceptionUpdated);
    
    /* компоненты движения */
    EnemyCharacterMovement = GetCharacterMovement();
    FloatingMovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingMovementComponent"));
    FloatingMovementComponent->SetUpdatedComponent(GetRootComponent());
    FloatingMovementComponent->MaxSpeed = 450.0f;
    FloatingMovementComponent->Acceleration = 1024.0f;
    FloatingMovementComponent->Deceleration = 1024.0f;
    
    // Изначально отключаем FloatingPawnMovement
    FloatingMovementComponent->SetComponentTickEnabled(false);

    /* геймплей-теги состояний */
    DeadTag    = FGameplayTag::RequestGameplayTag(TEXT("State.Dead"));
    IdleTag    = FGameplayTag::RequestGameplayTag(TEXT("State.Idle"));
    PatrolTag  = FGameplayTag::RequestGameplayTag(TEXT("State.Patrol"));
    ChaseTag   = FGameplayTag::RequestGameplayTag(TEXT("State.Chase"));
    AttackTag  = FGameplayTag::RequestGameplayTag(TEXT("State.Attacking"));
    StunnedTag = FGameplayTag::RequestGameplayTag(TEXT("State.Stunned"));
}

void AMedComEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Инициализация систем
    InitializeAbilitySystem();
    InitializeInventorySystem();

    // Настройка движения
    if (EnemyCharacterMovement)
    {
        EnemyCharacterMovement->MaxWalkSpeed = 450.f;
        EnemyCharacterMovement->MinAnalogWalkSpeed = 20.f;
        EnemyCharacterMovement->MaxAcceleration = 2048.f;
        EnemyCharacterMovement->BrakingDecelerationWalking = 2048.f;
        EnemyCharacterMovement->GroundFriction = 8.f;
        EnemyCharacterMovement->bOrientRotationToMovement = true;
        EnemyCharacterMovement->RotationRate = FRotator(0.f, 300.f, 0.f);
        EnemyCharacterMovement->SetMovementMode(MOVE_Walking);
    }

    // Сохраняем начальную позицию для AI
    InitialPosition = GetActorLocation();

    // Инициализация FSM компонента с Data Asset
    if (HasAuthority() && FSMComponent && BehaviorAsset)
    {
        FSMComponent->Initialize(BehaviorAsset, this);
    }
    
    // Настройка PathFollowing и Perception
    if (AAIController* AICtrl = Cast<AAIController>(GetController()))
    {
        if (UPathFollowingComponent* PathComp = AICtrl->GetPathFollowingComponent())
        {
            PathComp->SetComponentTickInterval(0.15f);
        }
    }
    
    // Применяем настройки тиков компонентов
    ConfigureComponentTickIntervals(CurrentDetailLevel);
    
    // Регистрация в централизованных менеджерах
    if (UEnemyFSMManager* FSMManager = GetWorld()->GetSubsystem<UEnemyFSMManager>())
    {
        FSMManager->RegisterFSM(FSMComponent);
    }
    
    if (UNPCSignificanceManager* SignificanceManager = GetWorld()->GetSubsystem<UNPCSignificanceManager>())
    {
        SignificanceManager->RegisterNPC(this);
    }
    
    if (UCrowdManagerSubsystem* CrowdManager = GetWorld()->GetSubsystem<UCrowdManagerSubsystem>())
    {
        CrowdManager->RegisterAgent(this);
    }
  ////  
    // Улучшенная настройка восприятия
    if (PerceptionComponent)
    {
        // Настраиваем зрение
        if (SightConfig)
        {
            SightConfig->SightRadius = 1500.0f;
            SightConfig->LoseSightRadius = 2000.0f;
            SightConfig->PeripheralVisionAngleDegrees = 70.0f; // Расширяем угол обзора
            SightConfig->SetMaxAge(3.0f);
        }
        
        // Настраиваем слух
        if (HearingConfig)
        {
            HearingConfig->HearingRange = 1500.0f;
            HearingConfig->SetMaxAge(4.0f);
        }
        
        // Обновляем конфигурацию и убеждаемся, что делегат привязан правильно
        PerceptionComponent->OnPerceptionUpdated.RemoveDynamic(this, &AMedComEnemyCharacter::OnPerceptionUpdated);
        PerceptionComponent->OnPerceptionUpdated.AddDynamic(this, &AMedComEnemyCharacter::OnPerceptionUpdated);
        
        // Обновляем конфигурацию и убеждаемся, что она применилась
        PerceptionComponent->RequestStimuliListenerUpdate();
        
#if !UE_BUILD_SHIPPING
        UE_LOG(LogMedComEnemy, Log, TEXT("%s: Perception component configured"), *GetName());
#endif
    }

    ///////
    // По умолчанию используем CharacterMovement
    SwitchMovementComponent(true);
    
    // Регистрация в ReplicationGraph
    RegisterWithReplicationGraph();
    
    // Добавляем нужные компоненты для репликации на основе LOD
    ManageComponentReplication();
    
    #if !UE_BUILD_SHIPPING
    UE_LOG(LogMedComEnemy, Log, TEXT("%s: Begin Play completed"), *GetName());
    #endif
}

void AMedComEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Отменяем регистрацию в менеджерах
    if (UEnemyFSMManager* FSMManager = GetWorld()->GetSubsystem<UEnemyFSMManager>())
    {
        FSMManager->UnregisterFSM(FSMComponent);
    }
    
    if (UNPCSignificanceManager* SignificanceManager = GetWorld()->GetSubsystem<UNPCSignificanceManager>())
    {
        SignificanceManager->UnregisterNPC(this);
    }
    
    if (UCrowdManagerSubsystem* CrowdManager = GetWorld()->GetSubsystem<UCrowdManagerSubsystem>())
    {
        CrowdManager->UnregisterAgent(this);
    }
    
    Super::EndPlay(EndPlayReason);
}

void AMedComEnemyCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    // Только на сервере
    if (HasAuthority())
    {
        // Переинициализируем ASC после Possess
        InitializeAbilitySystem();
        
        // Инициализируем FSM
        if (FSMComponent && BehaviorAsset)
        {
            FSMComponent->Initialize(BehaviorAsset, this);
        }
    }
}

void AMedComEnemyCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    // На клиенте тоже нужно инициализировать ASC
    if (!HasAuthority())
    {
        InitializeAbilitySystem();
    }
}

UAbilitySystemComponent* AMedComEnemyCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void AMedComEnemyCharacter::InitializeAbilitySystem()
{
    bIsInitializing = true;

    if (AbilitySystemComponent)
    {
        // Привязка к этому актору
        AbilitySystemComponent->InitAbilityActorInfo(this, this);

        // Получаем/создаём AttributeSet
        AttributeSet = const_cast<UMedComBaseAttributeSet*>(AbilitySystemComponent->GetSet<UMedComBaseAttributeSet>());
        if (!AttributeSet)
        {
            AttributeSet = NewObject<UMedComBaseAttributeSet>(this);
            AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet);
        }

        // Применяем эффект начальных атрибутов
        if (HasAuthority() && InitialAttributesEffect)
        {
            FGameplayEffectContextHandle Ctx = AbilitySystemComponent->MakeEffectContext();
            Ctx.AddSourceObject(this);
            FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(InitialAttributesEffect, 1.f, Ctx);
            if (SpecHandle.IsValid())
            {
                AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            }
        }

        // Подписываемся на изменение здоровья
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
            AttributeSet->GetHealthAttribute()).AddUObject(this, &AMedComEnemyCharacter::OnHealthAttributeChanged);

        // Подписываемся на теги "State.*"
        AbilitySystemComponent->RegisterGameplayTagEvent(
            FGameplayTag::RequestGameplayTag("State"), EGameplayTagEventType::NewOrRemoved
        ).AddUObject(this, &AMedComEnemyCharacter::OnGameplayTagChanged);

        // Если мы на сервере, выдаем стартовые способности
        if (HasAuthority())
        {
            GrantStartupAbilities();
        }

        // Инициализируем LastNotifiedHealth и MinimalLODHealth
        LastNotifiedHealth = GetHealth();
        MinimalLODHealth = GetHealth();

        bIsInitializing = false;
    }
}

void AMedComEnemyCharacter::GrantStartupAbilities()
{
    if (!HasAuthority() || !AbilitySystemComponent) return;

    for (TSubclassOf<UGameplayAbility> GA : StartupAbilities)
    {
        if (!GA) continue;
        
        // Проверяем, что способность еще не выдана
        if (AbilitySystemComponent->FindAbilitySpecFromClass(GA)) continue;   

        // Создаем спецификацию с активным InputID для клиентской активации
        FGameplayAbilitySpec Spec(GA, 1, 0, this);
        AbilitySystemComponent->GiveAbility(Spec);
    }
}

float AMedComEnemyCharacter::TakeDamage(float Damage, FDamageEvent const& DamageEvent,
                                      AController* EventInstigator, AActor* DamageCauser)
{
    #if !UE_BUILD_SHIPPING
    CSV_SCOPED_TIMING_STAT(EnemyTakeDamage, LogMedComEnemy);
    
    UE_LOG(LogMedComEnemy, Log, TEXT("%s: TakeDamage=%.2f from %s"), 
           *GetName(), Damage, *GetNameSafe(DamageCauser));
    #endif
    
    float ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);

    // Упрощенная обработка для дальних ботов (Minimal и Sleep)
    if (CurrentDetailLevel >= EAIDetailLevel::Minimal && AttributeSet)
    {
        // Обновляем локальный кэш здоровья без блокирования репликации
        MinimalLODHealth = FMath::Max(0.f, MinimalLODHealth - ActualDamage);
        
        // Используем Additive вместо Override для правильной репликации
        if (AbilitySystemComponent)
        {
            AbilitySystemComponent->ApplyModToAttribute(
                AttributeSet->GetHealthAttribute(), 
                EGameplayModOp::Additive, 
                -ActualDamage
            );
        }
        
        // Проверка на смерть
        if (MinimalLODHealth <= 0.f && IsAlive())
        {
            AbilitySystemComponent->AddLooseGameplayTag(DeadTag);
            EnableRagdoll();
        }
    }
    else if (AbilitySystemComponent && HasAuthority())
    {
        // Полная обработка GAS для близких ботов
        FGameplayEffectContextHandle Ctx = AbilitySystemComponent->MakeEffectContext();
        Ctx.AddSourceObject(DamageCauser);

        // Добавляем HitResult, если PointDamage
        if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
        {
            const FPointDamageEvent* PDE = static_cast<const FPointDamageEvent*>(&DamageEvent);
            Ctx.AddHitResult(PDE->HitInfo);
        }

        // Применяем эффект урона, если он задан
        if (DamageEffectClass)
        {
            FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(DamageEffectClass, 1.f, Ctx);
            if (SpecHandle.IsValid())
            {
                SpecHandle.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag("Data.Damage"), -ActualDamage);
                AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            }
        }
        else
        {
            // Fallback: прямое уменьшение здоровья
            float NewHealth = FMath::Max(0.f, AttributeSet->GetHealth() - ActualDamage);
            AbilitySystemComponent->ApplyModToAttribute(AttributeSet->GetHealthAttribute(), EGameplayModOp::Additive, -ActualDamage);
        }

        // Если враг ещё жив, генерируем событие FSM
        if (IsAlive() && FSMComponent)
        {
            FSMComponent->ProcessFSMEvent(EEnemyEvent::TookDamage, DamageCauser);
        }
    }

    return ActualDamage;
}

void AMedComEnemyCharacter::OnHealthAttributeChanged(const FOnAttributeChangeData& Data)
{
    if (bIsInitializing)
    {
        return;
    }

    float MaxHealth = AttributeSet ? AttributeSet->GetMaxHealth() : 100.0f;
    float HealthChangeDelta = FMath::Abs(Data.NewValue - LastNotifiedHealth);
    float HealthChangePercent = MaxHealth > 0.f ? HealthChangeDelta / MaxHealth : 0.f;
    
    // Обновляем кэш здоровья для Minimal LOD
    MinimalLODHealth = Data.NewValue;
    
    // Оптимизация: проверяем, достаточно ли изменилось значение или это критичное изменение
    if (HealthChangePercent >= HealthNotificationThreshold || Data.NewValue <= 0.0f)
    {
        LastNotifiedHealth = Data.NewValue;
        
        // Вызов делегата для UI/FX
        OnHealthChanged.Broadcast(Data.NewValue, MaxHealth);
        
        #if !UE_BUILD_SHIPPING
        UE_LOG(LogMedComEnemy, Verbose, TEXT("%s: Health changed to %.1f (%.1f%%)"), 
               *GetName(), Data.NewValue, (Data.NewValue / MaxHealth) * 100.0f);
        #endif
    }

    // Обработка смерти
    if (HasAuthority() && Data.NewValue <= 0.f && IsAlive())
    {
        #if !UE_BUILD_SHIPPING
        UE_LOG(LogMedComEnemy, Log, TEXT("%s: Is now dead"), *GetName());
        #endif
        
        AbilitySystemComponent->AddLooseGameplayTag(DeadTag);
        
        // Сообщаем FSM о смерти
        if (FSMComponent)
        {
            FSMComponent->ProcessFSMEvent(EEnemyEvent::Dead);
        }
        
        // Активация ragdoll
        EnableRagdoll();
    }
}

void AMedComEnemyCharacter::OnGameplayTagChanged(FGameplayTag Tag, int32 NewCount)
{
    // Отслеживаем изменения тегов состояний
    // Например, применяем эффекты при оглушении
    if (Tag == StunnedTag)
    {
        if (NewCount > 0)
        {
            // Оглушен - отключаем движение
            if (EnemyCharacterMovement && EnemyCharacterMovement->IsComponentTickEnabled())
            {
                EnemyCharacterMovement->DisableMovement();
            }
            
            if (FloatingMovementComponent && FloatingMovementComponent->IsComponentTickEnabled())
            {
                FloatingMovementComponent->StopMovementImmediately();
            }
        }
        else
        {
            // Оглушение снято - возвращаем движение
            if (EnemyCharacterMovement && EnemyCharacterMovement->IsComponentTickEnabled())
            {
                EnemyCharacterMovement->SetMovementMode(MOVE_Walking);
            }
        }
    }
}

void AMedComEnemyCharacter::EnableRagdoll()
{
    if (GetMesh())
    {
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        
        if (EnemyCharacterMovement)
        {
            EnemyCharacterMovement->DisableMovement();
        }
        
        if (FloatingMovementComponent)
        {
            FloatingMovementComponent->StopMovementImmediately();
            FloatingMovementComponent->SetComponentTickEnabled(false);
        }
        
        GetMesh()->SetCollisionProfileName("Ragdoll");
        GetMesh()->SetSimulatePhysics(true);
        
        // Прекращаем сетевую репликацию движения
        SetReplicateMovement(false);

        FVector ImpulseDirection = GetActorForwardVector() * -1.f;
        GetMesh()->AddImpulse(ImpulseDirection * RagdollImpulseScale, NAME_None, true);
    }
}

float AMedComEnemyCharacter::GetHealth() const
{
    // Для дальних ботов используем кэш
    if (CurrentDetailLevel >= EAIDetailLevel::Minimal)
    {
        return MinimalLODHealth;
    }
    
    return AttributeSet ? AttributeSet->GetHealth() : 0.f;
}

float AMedComEnemyCharacter::GetMaxHealth() const
{
    return AttributeSet ? AttributeSet->GetMaxHealth() : 0.f;
}

float AMedComEnemyCharacter::GetHealthPercentage() const
{
    if (AttributeSet && AttributeSet->GetMaxHealth() > 0.f)
    {
        return GetHealth() / AttributeSet->GetMaxHealth();
    }
    return 0.f;
}

bool AMedComEnemyCharacter::IsAlive() const
{
    return !(AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(DeadTag));
}

void AMedComEnemyCharacter::SetBehaviorAsset(UEnemyBehaviorDataAsset* NewBehaviorAsset)
{
    if (HasAuthority() && NewBehaviorAsset != BehaviorAsset)
    {
        BehaviorAsset = NewBehaviorAsset;
        
        // Обновляем конфигурацию FSM, если компонент уже создан
        if (FSMComponent)
        {
            FSMComponent->Initialize(BehaviorAsset, this);
        }
    }
}

void AMedComEnemyCharacter::UpdateDetailLevel(float DistanceToPlayer)
{
    EAIDetailLevel NewLevel;
    
    // Используем обновленные пороги на основе аудита
    if (DistanceToPlayer > 20000.f)
        NewLevel = EAIDetailLevel::Sleep;
    else if (DistanceToPlayer > 12000.f)
        NewLevel = EAIDetailLevel::Minimal;
    else if (DistanceToPlayer > 5000.f)
        NewLevel = EAIDetailLevel::Reduced;
    else
        NewLevel = EAIDetailLevel::Full;
    
    if (NewLevel != CurrentDetailLevel)
    {
        EAIDetailLevel OldLevel = CurrentDetailLevel;
        CurrentDetailLevel = NewLevel;
        ApplyDetailLevelSettings(DistanceToPlayer);
        
        #if !UE_BUILD_SHIPPING
        UE_LOG(LogMedComEnemy, Verbose, TEXT("%s: Detail level changed from %d to %d (distance: %.0f)"), 
               *GetName(), (int32)OldLevel, (int32)NewLevel, DistanceToPlayer);
        #endif
    }
}

void AMedComEnemyCharacter::ApplyDetailLevelSettings(float Distance)
{
    // 1. Переключение между типами движения
    switch (CurrentDetailLevel)
    {
    case EAIDetailLevel::Full:
        SwitchMovementComponent(true); // Используем CharacterMovement
        break;
            
    case EAIDetailLevel::Reduced:
    case EAIDetailLevel::Minimal:
    case EAIDetailLevel::Sleep:
        SwitchMovementComponent(false); // Используем FloatingPawnMovement
        break;
    }
    
    // 2. Настройка сетевых обновлений
    switch (CurrentDetailLevel)
    {
    case EAIDetailLevel::Full:
        SetNetUpdateFrequency(10.f);
        SetNetDormancy(DORM_Awake);
        break;
    case EAIDetailLevel::Reduced:
        SetNetUpdateFrequency(4.f);
        SetNetDormancy(DORM_DormantPartial);
        break;
    case EAIDetailLevel::Minimal:
        SetNetUpdateFrequency(1.f);
        SetNetDormancy(DORM_DormantPartial);
        break;
    case EAIDetailLevel::Sleep:
        SetNetUpdateFrequency(0.5f);
        SetNetDormancy(DORM_DormantAll);
        break;
    }
    
    // Форсируем обновление сети для применения изменений
    if (HasAuthority())
    {
        ForceNetUpdate();
    }
    
    // 3. Настройка интервалов тиков компонентов
    ConfigureComponentTickIntervals(CurrentDetailLevel);
    
    // 4. Настройка репликации компонентов
    ManageComponentReplication();
}

void AMedComEnemyCharacter::ConfigureComponentTickIntervals(EAIDetailLevel DetailLevel)
{
    // Настройка PathFollowing
    if (AAIController* AICtrl = Cast<AAIController>(GetController()))
    {
        if (UPathFollowingComponent* PathComp = AICtrl->GetPathFollowingComponent())
        {
            // ИСПРАВЛЕНО: Не ставим слишком большой интервал даже для Minimal LOD
            switch (DetailLevel)
            {
                case EAIDetailLevel::Full:
                    PathComp->SetComponentTickInterval(0.0f); // Каждый кадр
                    break;
                case EAIDetailLevel::Reduced:
                    PathComp->SetComponentTickInterval(0.1f);
                    break;
                case EAIDetailLevel::Minimal:
                    PathComp->SetComponentTickInterval(0.2f);
                    break;
                case EAIDetailLevel::Sleep:
                    PathComp->SetComponentTickInterval(0.5f);
                    break;
            }
        }
    }
    
    // Настройка интервала восприятия
    if (PerceptionComponent)
    {
        float UpdateInterval = 0.1f;
        switch (DetailLevel)
        {
            case EAIDetailLevel::Full:
                UpdateInterval = 0.1f;
                break;
            case EAIDetailLevel::Reduced:
                UpdateInterval = 0.3f;
                break;
            case EAIDetailLevel::Minimal:
                UpdateInterval = 0.6f;
                break;
            case EAIDetailLevel::Sleep:
                UpdateInterval = 1.0f;
                break;
        }
        
        // Устанавливаем интервал обновления
        for (auto ConfigIt = PerceptionComponent->GetSensesConfigIterator(); ConfigIt; ++ConfigIt)
        {
            UAISenseConfig* Config = *ConfigIt;
            if (Config)
            {
                // Настраиваем максимальный возраст стимула для каждого сенсора
                Config->SetMaxAge(UpdateInterval * 3.0f);
            }
        }
    }
    
    // Настройка CharacterMovement и FloatingPawnMovement
    // ИСПРАВЛЕНО: Никогда не устанавливаем интервал для CharacterMovement!
    if (EnemyCharacterMovement)
    {
        // Вместо установки интервала тика, просто включаем/выключаем компонент
        if (DetailLevel == EAIDetailLevel::Full)
        {
            // Полный LOD - CharacterMovement тикает каждый кадр
            EnemyCharacterMovement->SetComponentTickEnabled(true);
            EnemyCharacterMovement->SetComponentTickInterval(0.0f);
        }
        else
        {
            // Для других LOD - отключаем CharacterMovement полностью
            EnemyCharacterMovement->SetComponentTickEnabled(false);
        }
    }
    
    if (FloatingMovementComponent)
    {
        if (DetailLevel != EAIDetailLevel::Full)
        {
            // Для не-Full LOD - FloatingMovementComponent тикает каждый кадр
            FloatingMovementComponent->SetComponentTickEnabled(true);
            FloatingMovementComponent->SetComponentTickInterval(0.0f);
        }
        else
        {
            // Для Full - отключаем FloatingMovementComponent
            FloatingMovementComponent->SetComponentTickEnabled(false);
        }
    }
}

void AMedComEnemyCharacter::SwitchMovementComponent(bool bUseCharacterMovement)
{
    if (!EnemyCharacterMovement || !FloatingMovementComponent)
        return;

    // Сохраняем текущую скорость и направление 
    FVector CurrentVelocity = FVector::ZeroVector;
    FVector MovementDirection = GetActorForwardVector();
    
    if (EnemyCharacterMovement->IsComponentTickEnabled())
    {
        CurrentVelocity = EnemyCharacterMovement->Velocity;
        if (!CurrentVelocity.IsNearlyZero())
        {
            MovementDirection = CurrentVelocity.GetSafeNormal();
        }
    }
    else if (FloatingMovementComponent->IsComponentTickEnabled())
    {
        CurrentVelocity = FloatingMovementComponent->Velocity;
        if (!CurrentVelocity.IsNearlyZero())
        {
            MovementDirection = CurrentVelocity.GetSafeNormal();
        }
    }
    
    const float CurrentSpeed = CurrentVelocity.Size();
    
    if (bUseCharacterMovement)
    {
        // 1. Отключаем FloatingMovementComponent
        FloatingMovementComponent->StopMovementImmediately();
        FloatingMovementComponent->SetComponentTickEnabled(false);
        FloatingMovementComponent->SetUpdatedComponent(nullptr); // Отвязываем от RootComponent

        // 2. Включаем и настраиваем CharacterMovement
        EnemyCharacterMovement->SetComponentTickEnabled(true);
        EnemyCharacterMovement->SetUpdatedComponent(GetRootComponent());
        EnemyCharacterMovement->SetMovementMode(MOVE_Walking);
        EnemyCharacterMovement->bOrientRotationToMovement = true;
        EnemyCharacterMovement->MaxWalkSpeed = 450.f;
        EnemyCharacterMovement->MaxAcceleration = 2048.f;
        EnemyCharacterMovement->BrakingDecelerationWalking = 2048.f;
        EnemyCharacterMovement->GroundFriction = 8.f;
        EnemyCharacterMovement->RotationRate = FRotator(0.f, 300.f, 0.f);
        EnemyCharacterMovement->bRequestedMoveUseAcceleration = true;
        
        // 3. Восстанавливаем скорость и направление, если были в движении
        if (CurrentSpeed > 10.f)
        {
            // Передаем вектор ускорения в том же направлении
            EnemyCharacterMovement->AddInputVector(MovementDirection);
        }
        
        // 4. Настройка контроллера
        if (AAIController* AIC = Cast<AAIController>(GetController()))
        {
            // Отключаем использование контроллера для вращения, 
            // т.к. CharacterMovement будет сам поворачивать персонажа
            this->bUseControllerRotationYaw = false;
        }
        
        UE_LOG(LogMedComEnemy, Verbose, TEXT("%s: Switched to CharacterMovement"), *GetName());
    }
    else // Используем FloatingPawnMovement
    {
        // 1. Отключаем CharacterMovement
        EnemyCharacterMovement->StopMovementImmediately();
        EnemyCharacterMovement->SetMovementMode(MOVE_None);
        EnemyCharacterMovement->SetComponentTickEnabled(false);
        
        // 2. Включаем и настраиваем FloatingPawnMovement
        FloatingMovementComponent->SetUpdatedComponent(GetRootComponent());
        FloatingMovementComponent->SetComponentTickEnabled(true);
        
        // Настройка скорости в зависимости от текущего LOD
        float MaxSpeed = (CurrentDetailLevel == EAIDetailLevel::Reduced) ? 450.0f : 300.0f;
        FloatingMovementComponent->MaxSpeed = MaxSpeed;
        FloatingMovementComponent->Acceleration = 1024.0f;
        FloatingMovementComponent->Deceleration = 1024.0f;
        
        // 3. Восстанавливаем скорость и направление, если были в движении
        if (CurrentSpeed > 10.f)
        {
            // Передаем импульс в том же направлении
            FloatingMovementComponent->AddInputVector(MovementDirection);
        }
        
        // 4. Настройка контроллера
        if (AAIController* AIC = Cast<AAIController>(GetController()))
        {
            // Для FloatingPawnMovement также отключаем вращение контроллером,
            // управление вращением будет через прямую установку ротации
            this->bUseControllerRotationYaw = false;
        }
        
        UE_LOG(LogMedComEnemy, Verbose, TEXT("%s: Switched to FloatingPawnMovement"), *GetName());
    }
}


void AMedComEnemyCharacter::OnRep_BehaviorAsset()
{
    // Клиентская инициализация FSM при репликации Data Asset
    if (!HasAuthority() && FSMComponent && BehaviorAsset)
    {
        FSMComponent->Initialize(BehaviorAsset, this);
        #if !UE_BUILD_SHIPPING
        UE_LOG(LogMedComEnemy, Log, TEXT("%s: Client initialized FSM with replicated behavior asset"), *GetName());
        #endif
    }
}

void AMedComEnemyCharacter::OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{
    // Выводим отладочную информацию
    #if !UE_BUILD_SHIPPING
    UE_LOG(LogMedComEnemy, Verbose, TEXT("%s: Perception updated, actors: %d"), 
           *GetName(), UpdatedActors.Num());
    #endif
    
    // Проверяем, жив ли персонаж
    if (!IsAlive())
        return;
    
    for (AActor* Actor : UpdatedActors)
    {
        // Пропускаем недействительных акторов
        if (!Actor || !IsValid(Actor))
            continue;
            
        // Дополнительная проверка - является ли актор игроком
        APawn* SensedPawn = Cast<APawn>(Actor);
        if (!SensedPawn || !SensedPawn->IsPlayerControlled())
            continue;
        
        FActorPerceptionBlueprintInfo Info;
        if (!PerceptionComponent->GetActorsPerception(Actor, Info))
            continue;
        
        // Более детальная отладка
        #if !UE_BUILD_SHIPPING
        UE_LOG(LogMedComEnemy, Verbose, TEXT("%s: Detected actor %s, stimuli: %d"), 
               *GetName(), *Actor->GetName(), Info.LastSensedStimuli.Num());
        #endif
        
        // Проверяем все стимулы
        bool bPlayerSeen = false;
        bool bPlayerHeard = false;
        
        for (FAIStimulus& Stimulus : Info.LastSensedStimuli)
        {
            // Проверяем, успешно ли обработан стимул
            if (!Stimulus.WasSuccessfullySensed())
                continue;
                
            if (Stimulus.Type == SightConfig->GetSenseID())
            {
                bPlayerSeen = true;
                #if !UE_BUILD_SHIPPING
                UE_LOG(LogMedComEnemy, Log, TEXT("%s: PLAYER SEEN %s at %s"), 
                       *GetName(), *Actor->GetName(), *Stimulus.StimulusLocation.ToString());
                #endif
            }
            else if (Stimulus.Type == HearingConfig->GetSenseID())
            {
                bPlayerHeard = true;
                #if !UE_BUILD_SHIPPING
                UE_LOG(LogMedComEnemy, Log, TEXT("%s: PLAYER HEARD %s at %s"), 
                       *GetName(), *Actor->GetName(), *Stimulus.StimulusLocation.ToString());
                #endif
            }
        }
        
        // Отправляем события в FSM
        if (FSMComponent)
        {
            if (bPlayerSeen)
            {
                FSMComponent->ProcessFSMEvent(EEnemyEvent::PlayerSeen, Actor);
            }
            else if (bPlayerHeard)
            {
                // Для слуха можно также использовать PlayerSeen или отдельное событие
                FSMComponent->ProcessFSMEvent(EEnemyEvent::PlayerSeen, Actor);
            }
        }
    }
}

void AMedComEnemyCharacter::AddGameplayTag(const FGameplayTag& Tag)
{
    if (HasAuthority() && AbilitySystemComponent && Tag.IsValid())
    {
        AbilitySystemComponent->AddLooseGameplayTag(Tag);
    }
}

void AMedComEnemyCharacter::RemoveGameplayTag(const FGameplayTag& Tag)
{
    if (HasAuthority() && AbilitySystemComponent && Tag.IsValid())
    {
        AbilitySystemComponent->RemoveLooseGameplayTag(Tag);
    }
}

bool AMedComEnemyCharacter::HasGameplayTag(const FGameplayTag& Tag) const
{
    return (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(Tag));
}

AWeaponActor* AMedComEnemyCharacter::GetCurrentWeapon() const
{
    // Сначала проверяем кэш
    if (CurrentWeapon)
    {
        return CurrentWeapon;
    }
    
    // Если нет кэша, пробуем получить через EquipmentComponent
    if (EquipmentComponent)
    {
        return EquipmentComponent->GetActiveWeapon();
    }
    
    return nullptr;
}

void AMedComEnemyCharacter::SetCurrentWeaponCache(AWeaponActor* NewWeapon)
{
    if (CurrentWeapon != NewWeapon)
    {
        CurrentWeapon = NewWeapon;
        bHasRifle = (CurrentWeapon != nullptr);
        
        // Оповещаем об изменении оружия
        OnWeaponChanged.Broadcast(CurrentWeapon);
        
        #if !UE_BUILD_SHIPPING
        UE_LOG(LogMedComEnemy, Log, TEXT("%s: Weapon cache updated to %s"), 
               *GetName(), CurrentWeapon ? *CurrentWeapon->GetName() : TEXT("None"));
        #endif
    }
}

FName AMedComEnemyCharacter::GetWeaponAttachSocketName() const
{
    // Пытаемся получить из Data Asset, если есть
    if (BehaviorAsset && !BehaviorAsset->WeaponSocket.IsNone())
    {
        return BehaviorAsset->WeaponSocket;
    }
    
    // Иначе используем значение по умолчанию
    return WeaponAttachSocketName;
}

void AMedComEnemyCharacter::InitializeInventorySystem()
{
    if (!HasAuthority()) return;
    
    // Инициализация инвентаря
    if (InventoryComponent)
    {
        InventoryComponent->InitializeInventoryGrid(6, 4);
        #if !UE_BUILD_SHIPPING
        UE_LOG(LogMedComEnemy, Log, TEXT("%s: Initialized inventory grid 6x4"), *GetName());
        #endif
    }
    
    // Инициализация слотов экипировки
    if (EquipmentComponent)
    {
        TArray<FMCEquipmentSlot> Slots;
        
        // Слот основного оружия
        FMCEquipmentSlot PrimarySlot;
        PrimarySlot.SlotTag = FGameplayTag::RequestGameplayTag("Equipment.Weapon.Primary");
        PrimarySlot.Width = 4;
        PrimarySlot.Height = 2;
        PrimarySlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag("Equipment.Weapon.Primary"));
        Slots.Add(PrimarySlot);
        
        // Слот вторичного оружия
        FMCEquipmentSlot SecondarySlot;
        SecondarySlot.SlotTag = FGameplayTag::RequestGameplayTag("Equipment.Weapon.Secondary");
        SecondarySlot.Width = 3;
        SecondarySlot.Height = 2;
        SecondarySlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag("Equipment.Weapon.Secondary"));
        Slots.Add(SecondarySlot);
        
        // Слот брони
        FMCEquipmentSlot VestSlot;
        VestSlot.SlotTag = FGameplayTag::RequestGameplayTag("Equipment.Vest");
        VestSlot.Width = 2;
        VestSlot.Height = 3;
        VestSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag("Equipment.Vest"));
        Slots.Add(VestSlot);
        
        // Слот шлема
        FMCEquipmentSlot HelmetSlot;
        HelmetSlot.SlotTag = FGameplayTag::RequestGameplayTag("Equipment.Helmet");
        HelmetSlot.Width = 2;
        HelmetSlot.Height = 2;
        HelmetSlot.AllowedItemTypes.AddTag(FGameplayTag::RequestGameplayTag("Equipment.Helmet"));
        Slots.Add(HelmetSlot);
        
        // Инициализируем слоты
        EquipmentComponent->InitializeEquipmentSlots(Slots);
        #if !UE_BUILD_SHIPPING
        UE_LOG(LogMedComEnemy, Log, TEXT("%s: Initialized equipment slots"), *GetName());
        #endif
    }
}

void AMedComEnemyCharacter::OnRep_HasRifle()
{
    // Обработка на клиенте - например, обновление анимаций
    #if !UE_BUILD_SHIPPING
    UE_LOG(LogMedComEnemy, Log, TEXT("%s: Weapon status changed: %s"), 
           *GetName(), bHasRifle ? TEXT("Armed") : TEXT("Unarmed"));
    #endif
}

bool AMedComEnemyCharacter::IsReplicationPausedForConnection(const FNetViewer& ConnectionOwnerNetViewer)
{
    // Полностью останавливаем репликацию для спящих ботов
    return CurrentDetailLevel == EAIDetailLevel::Sleep;
}

void AMedComEnemyCharacter::RegisterWithReplicationGraph()
{
    if (!HasAuthority())
        return;
        
    UWorld* World = GetWorld();
    if (!World)
        return;
        
    // Попытка получить ReplicationGraph без включения заголовочного файла
    UNetDriver* NetDriver = World->GetNetDriver();
    if (!NetDriver)
        return;
        
    // Проверка по имени класса вместо IsA
    FString ClassName = NetDriver->GetClass()->GetName();
    if (!ClassName.Contains(TEXT("ReplicationGraph")))
        return;
    
#if !UE_BUILD_SHIPPING
    UE_LOG(LogMedComEnemy, Verbose, TEXT("%s: Registered with ReplicationGraph"), *GetName());
#endif
}

bool AMedComEnemyCharacter::ShouldReplicateSubObjects() const
{
    // Для Sleep и Minimal уровней не реплицируем дополнительные объекты
    return CurrentDetailLevel <= EAIDetailLevel::Reduced;
}

void AMedComEnemyCharacter::ManageComponentReplication()
{
    if (!HasAuthority())
        return;
        
    // Для Full LOD добавляем компоненты в репликацию
    if (CurrentDetailLevel == EAIDetailLevel::Full)
    {
        if (InventoryComponent && !InventoryComponent->GetIsReplicated())
        {
            AbilitySystemComponent->AddReplicatedSubObject(InventoryComponent);
        }
        
        if (EquipmentComponent && !EquipmentComponent->GetIsReplicated())
        {
            AbilitySystemComponent->AddReplicatedSubObject(EquipmentComponent);
        }
    }
    else
    {
        // Для других LOD уровней убираем компоненты из репликации
        if (InventoryComponent && InventoryComponent->GetIsReplicated())
        {
            AbilitySystemComponent->RemoveReplicatedSubObject(InventoryComponent);
        }
        
        if (EquipmentComponent && EquipmentComponent->GetIsReplicated())
        {
            AbilitySystemComponent->RemoveReplicatedSubObject(EquipmentComponent);
        }
    }
}

void AMedComEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // AIMovementComponent всегда реплицируется
    DOREPLIFETIME(AMedComEnemyCharacter, AIMovementComponent);
    
    // Флаги и состояния - с условиями
    DOREPLIFETIME_CONDITION(AMedComEnemyCharacter, bHasRifle, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(AMedComEnemyCharacter, BehaviorAsset, COND_InitialOnly);
}