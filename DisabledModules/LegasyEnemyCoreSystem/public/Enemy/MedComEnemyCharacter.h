#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "Perception/PawnSensingComponent.h"
#include "MedComEnemyCharacter.generated.h"

// Forward declarations
class AController;
class UAbilitySystemComponent;
class UMedComBaseAttributeSet;
class UMedComEnemyFSMComponent;
class UMedComInventoryComponent;
class UMedComEquipmentComponent;
class UMedComWeaponHandlerComponent;
class UMedComAIMovementComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class AWeaponActor;
class UEnemyBehaviorDataAsset;
class UFloatingPawnMovement;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEnemyHealthChanged, float, CurrentHealth, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponChanged, AWeaponActor*, NewWeapon);

/**
 * Уровни детализации для ИИ ботов
 * Переопределены пороги согласно рекомендациям аудита
 */
UENUM(BlueprintType)
enum class EAIDetailLevel : uint8
{
    Full,     // До 5000 юнитов - полная детализация (вместо 2000)
    Reduced,  // 5000-12000 юнитов - сниженная детализация (вместо 2000-5000)
    Minimal,  // 12000-20000 юнитов - минимальная детализация
    Sleep     // Свыше 20000 юнитов - спящий режим (новый уровень)
};

/**
 * Базовый класс врага в сетевом шутере с поддержкой FSM и Gameplay Ability System.
 * Использует событийно-ориентированную архитектуру и минимизирует тики для оптимальной
 * производительности при большом количестве врагов.
 */
UCLASS()
class MEDCOMCORE_API AMedComEnemyCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    /** Конструктор - инициализирует компоненты и настраивает сетевую репликацию */
    AMedComEnemyCharacter();

    //------------------------------------------------
    // IAbilitySystemInterface
    //------------------------------------------------
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    //------------------------------------------------
    // Жизненный цикл
    //------------------------------------------------
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;
    virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent,
                          AController* EventInstigator, AActor* DamageCauser) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual bool IsReplicationPausedForConnection(const FNetViewer& ConnectionOwnerNetViewer) override;
    
    //------------------------------------------------
    // Интеграция с ReplicationGraph
    //------------------------------------------------
    /**
     * Используется ReplicationGraph для динамического контроля репликации
     * Обязательно для эффективной работы на серверах со 100+ игроками
     */
    void RegisterWithReplicationGraph();
    bool ShouldReplicateSubObjects() const;

    //------------------------------------------------
    // Атрибуты и состояние
    //------------------------------------------------
    /** Возвращает текущее здоровье */
    UFUNCTION(BlueprintPure, Category = "Enemy|Attributes")
    float GetHealth() const;

    /** Возвращает максимальное здоровье */
    UFUNCTION(BlueprintPure, Category = "Enemy|Attributes")
    float GetMaxHealth() const;

    /** Возвращает процент здоровья (0.0-1.0) */
    UFUNCTION(BlueprintPure, Category = "Enemy|Attributes")
    float GetHealthPercentage() const;

    /** Проверяет, жив ли враг */
    UFUNCTION(BlueprintPure, Category = "Enemy|State")
    bool IsAlive() const;

    /** Активирует ragdoll физику при смерти */
    UFUNCTION(BlueprintCallable, Category = "Enemy|Physics")
    void EnableRagdoll();

    /** Устанавливает конфигурацию поведения (Data Asset) */
    UFUNCTION(BlueprintCallable, Category = "Enemy|AI")
    void SetBehaviorAsset(UEnemyBehaviorDataAsset* NewBehaviorAsset);

    /** Получает текущую конфигурацию поведения */
    UFUNCTION(BlueprintPure, Category = "Enemy|AI")
    UEnemyBehaviorDataAsset* GetBehaviorAsset() const { return BehaviorAsset; }
    
    /** Обновляет уровень детализации в зависимости от дистанции до игрока */
    UFUNCTION(BlueprintCallable, Category = "Enemy|Optimization")
    void UpdateDetailLevel(float DistanceToPlayer);
    
    /** Применяет настройки в соответствии с текущим уровнем детализации */
    void ApplyDetailLevelSettings(float Distance);
    
    /** Возвращает текущий уровень детализации */
    UFUNCTION(BlueprintPure, Category = "Enemy|Optimization")
    EAIDetailLevel GetCurrentDetailLevel() const { return CurrentDetailLevel; }
    
    /** Переключает компонент движения между Character и Floating */
    UFUNCTION(BlueprintCallable, Category = "Enemy|Movement")
    void SwitchMovementComponent(bool bUseCharacterMovement);

    //------------------------------------------------
    // Доступ к компонентам
    //------------------------------------------------
    /** Возвращает компонент FSM */
    UFUNCTION(BlueprintPure, Category = "Enemy|Components")
    UMedComEnemyFSMComponent* GetFsmComponent() const { return FSMComponent; }
    
    /** Возвращает компонент инвентаря */
    UFUNCTION(BlueprintPure, Category = "Enemy|Components")
    UMedComInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

    /** Возвращает компонент экипировки */
    UFUNCTION(BlueprintPure, Category = "Enemy|Components")
    UMedComEquipmentComponent* GetEquipmentComponent() const { return EquipmentComponent; }

    /** Возвращает компонент управления оружием */
    UFUNCTION(BlueprintPure, Category = "Enemy|Components")
    UMedComWeaponHandlerComponent* GetWeaponHandlerComponent() const { return WeaponHandlerComponent; }

    /** Возвращает компонент AI движения */
    UFUNCTION(BlueprintPure, Category = "Enemy|Components")
    UMedComAIMovementComponent* GetAIMovementComponent() const { return AIMovementComponent; }
    
    /** Возвращает компонент плавающего движения */
    UFUNCTION(BlueprintPure, Category = "Enemy|Components") 
    UFloatingPawnMovement* GetFloatingMovementComponent() const { return FloatingMovementComponent; }

    /** Возвращает компонент AI восприятия */
    UFUNCTION(BlueprintPure, Category = "Enemy|Components")
    UAIPerceptionComponent* GetPerceptionComponent() const { return PerceptionComponent; }
    
    /** Возвращает текущее активное оружие */
    UFUNCTION(BlueprintPure, Category = "Enemy|Weapon")
    AWeaponActor* GetCurrentWeapon() const;

    /** Устанавливает текущее оружие (для внутреннего использования) */
    void SetCurrentWeaponCache(AWeaponActor* NewWeapon);

    //------------------------------------------------
    // Управление тегами Gameplay
    //------------------------------------------------
    /** Добавляет тег Gameplay */
    UFUNCTION(BlueprintCallable, Category = "Enemy|Tags")
    void AddGameplayTag(const FGameplayTag& Tag);

    /** Удаляет тег Gameplay */
    UFUNCTION(BlueprintCallable, Category = "Enemy|Tags")
    void RemoveGameplayTag(const FGameplayTag& Tag);

    /** Проверяет наличие тега Gameplay */
    UFUNCTION(BlueprintCallable, Category = "Enemy|Tags")
    bool HasGameplayTag(const FGameplayTag& Tag) const;

    //------------------------------------------------
    // Геттеры для параметров поведения
    //------------------------------------------------
    /** Возвращает начальную позицию */
    UFUNCTION(BlueprintPure, Category = "Enemy|AI")
    const FVector& GetInitialPosition() const { return InitialPosition; }

    /** Возвращает сокет для крепления оружия */
    UFUNCTION(BlueprintPure, Category = "Enemy|Weapon")
    FName GetWeaponAttachSocketName() const;

    //------------------------------------------------
    // События
    //------------------------------------------------
    /** Событие изменения здоровья - для UI и FX */
    UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
    FOnEnemyHealthChanged OnHealthChanged;

    /** Событие изменения оружия */
    UPROPERTY(BlueprintAssignable, Category = "Enemy|Events")
    FOnWeaponChanged OnWeaponChanged;

protected:
    //------------------------------------------------
    // Компоненты
    //------------------------------------------------
    /** Компонент Ability System для GAS */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
    UAbilitySystemComponent* AbilitySystemComponent;

    /** Набор атрибутов (здоровье, скорость и т.д.) */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Attributes")
    UMedComBaseAttributeSet* AttributeSet;

    /** Компонент FSM для управления состояниями ИИ */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UMedComEnemyFSMComponent* FSMComponent;

    /** Компонент инвентаря для хранения предметов */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    UMedComInventoryComponent* InventoryComponent;

    /** Компонент экипировки для управления оружием и снаряжением */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
    UMedComEquipmentComponent* EquipmentComponent;

    /** Компонент для управления оружием */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    UMedComWeaponHandlerComponent* WeaponHandlerComponent;

    /** Компонент AI движения для оптимизированных маневров */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI", Replicated)
    UMedComAIMovementComponent* AIMovementComponent;
    
    /** Альтернативный компонент плавающего движения для оптимизации */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    UFloatingPawnMovement* FloatingMovementComponent;
    
    /** Кэш ссылки на компонент CharacterMovement */
    UPROPERTY()
    UCharacterMovementComponent* EnemyCharacterMovement;

    /** Компонент AI восприятия, заменяющий устаревший PawnSensing */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAIPerceptionComponent* PerceptionComponent;
    
    /** Конфигурация сенсора зрения */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAISenseConfig_Sight* SightConfig;
    
    /** Конфигурация сенсора слуха */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI")
    UAISenseConfig_Hearing* HearingConfig;
    
    //------------------------------------------------
    // Конфигурация
    //------------------------------------------------
    /** Data Asset с настройками поведения */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", ReplicatedUsing = OnRep_BehaviorAsset)
    UEnemyBehaviorDataAsset* BehaviorAsset;

    /** Начальный эффект для атрибутов (задает здоровье и др.) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
    TSubclassOf<class UGameplayEffect> InitialAttributesEffect;

    /** Базовые способности, выдаваемые при спавне */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities")
    TArray<TSubclassOf<class UGameplayAbility>> StartupAbilities;

    /** Эффект для обработки урона */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Abilities|Damage")
    TSubclassOf<class UGameplayEffect> DamageEffectClass;

    /** Набор основных тегов (жив/мёртв/атакует/и т.д.) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
    FGameplayTag DeadTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
    FGameplayTag IdleTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
    FGameplayTag PatrolTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
    FGameplayTag ChaseTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
    FGameplayTag AttackTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
    FGameplayTag StunnedTag;

    /** Сила импульса для ragdoll при смерти */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Physics")
    float RagdollImpulseScale;

    /** Сокет для крепления оружия */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon")
    FName WeaponAttachSocketName;

    /** Флаг наличия оружия (для анимаций и логики) */
    UPROPERTY(ReplicatedUsing = OnRep_HasRifle, BlueprintReadOnly, Category = "Weapon")
    bool bHasRifle;
    
    /** Текущий уровень детализации */
    UPROPERTY()
    EAIDetailLevel CurrentDetailLevel;
    
    /** Локальный кэш здоровья для Minimal LOD */
    float MinimalLODHealth;

    /** Обработчик репликации флага наличия оружия */
    UFUNCTION()
    void OnRep_HasRifle();

    /** Обработчик репликации поведения */
    UFUNCTION()
    void OnRep_BehaviorAsset();

    /** Начальная позиция для возврата патрулирования */
    UPROPERTY(BlueprintReadOnly, Category = "AI")
    FVector InitialPosition;

    /** Минимальное изменение здоровья для уведомления (процент) */
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    float HealthNotificationThreshold;

    UFUNCTION()
    void OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors);

private:
    //------------------------------------------------
    // Внутренние методы
    //------------------------------------------------
    /** Инициализация AbilitySystem и атрибутов */
    void InitializeAbilitySystem();

    /** Выдача стартовых способностей */
    void GrantStartupAbilities();

    /** Обработчик изменения здоровья */
    void OnHealthAttributeChanged(const FOnAttributeChangeData& Data);

    /** Обработчик изменения тегов состояния */
    void OnGameplayTagChanged(FGameplayTag Tag, int32 NewCount);

    /** Инициализация системы инвентаря и экипировки */
    void InitializeInventorySystem();

    /** Флаг инициализации (для предотвращения рекурсивных вызовов) */
    bool bIsInitializing;

    /** Текущее оружие (кэш для производительности) */
    UPROPERTY()
    AWeaponActor* CurrentWeapon;

    /** Последнее значение здоровья (для оптимизации обновлений UI) */
    float LastNotifiedHealth;
    
    /** 
     * Настройки интервалов для компонентов на основе LOD
     * Вынесено в отдельный метод для применения к различным компонентам
     */
    void ConfigureComponentTickIntervals(EAIDetailLevel DetailLevel);
    
    /** Управление репликацией компонентов */
    void ManageComponentReplication();
    
    /** Форсирует обновление сети после изменения важных параметров */
    inline void SafeForceNetUpdate()
    {
        if (!IsNetMode(NM_Client))
        {
            ForceNetUpdate();
        }
    }
};