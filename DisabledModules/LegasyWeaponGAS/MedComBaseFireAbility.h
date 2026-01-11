#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Equipment/MedComWeaponTypes.h"
#include "Equipment/Base/WeaponActor.h"
#include "NiagaraSystem.h"
#include "MedComBaseFireAbility.generated.h"

class UGameplayEffect;
class UAnimMontage;
class UCameraShakeBase;
class UNiagaraSystem;
class UMedComWeaponAsyncTask_PerformTrace;

/**
 * UMedComBaseFireAbility
 * 
 * Базовый класс для всех способностей стрельбы в системе GAS.
 * Содержит общий функционал и параметры, которые используются во всех режимах стрельбы:
 * - одиночный выстрел (Single)
 * - очередь (Burst)
 * - автоматический огонь (Auto)
 * 
 * Дочерние классы должны реализовать специфичную логику планирования выстрелов.
 */
UCLASS(Abstract)
class MEDCOMCORE_API UMedComBaseFireAbility : public UGameplayAbility
{
    GENERATED_BODY()

public:
    UMedComBaseFireAbility();

    // Переопределение для активации способности
    virtual void ActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    // Обработка входящего ввода (нажатие кнопки)
    virtual void InputPressed(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo) override;

    // Проверка условий активации
    virtual bool CanActivateAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags,
        const FGameplayTagContainer* TargetTags,
        FGameplayTagContainer* OptionalRelevantTags) const override;
    
    // Переопределение для завершения способности
    virtual void EndAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility,
        bool bWasCancelled) override;

    /** Обработчик для результатов асинхронной трассировки */
    UFUNCTION()
    void HandleAsyncTraceResults(const TArray<FHitResult>& HitResults);

protected:
    /** Базовый урон, если не используется AttributeSet */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage")
    float DefaultDamage;

    /** Максимальная дистанция трассировки (если 0, берём из AttributeSet) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace")
    float MaxDistanceOverride;

    /** Класс GameplayEffect для урона (обязательно) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Effects")
    TSubclassOf<UGameplayEffect> DamageEffect;

    /** Cooldown GameplayEffect для оружия */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Cooldown")
    TSubclassOf<UGameplayEffect> CooldownEffect;

    /** Анимация выстрела */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visuals")
    UAnimMontage* FireMontage;

    /** Звук выстрела */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visuals")
    USoundBase* FireSound;

    /** Эффект дульного пламени */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visuals")
    UParticleSystem* MuzzleFlash;

    /** Эффект дульного пламени (Niagara) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visuals|Niagara")
    UNiagaraSystem* MuzzleFlashNiagara;

    /** Эффект трассера пули (Niagara) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visuals|Niagara")
    UNiagaraSystem* BulletTracerNiagara;

    /** Эффект попадания пули (Niagara) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visuals|Niagara")
    UNiagaraSystem* ImpactEffectNiagara;

    /** Количество трасс (для дробовика > 1) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace", meta = (ClampMin = "1"))
    int32 NumTraces;

    /** Базовый разброс в градусах */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0"))
    float BaseSpread;

    /** Множитель разброса при прицеливании */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Accuracy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AimingSpreadModifier;

    /** Множитель разброса при движении */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Accuracy", meta = (ClampMin = "1.0"))
    float MovementSpreadModifier;

    /** Тип выстрела (для определения эффектов и звуков) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Type")
    FName ShotType;

    /** Профиль трассировки для выстрела */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace")
    FName TraceProfile;

    /** Показывать отладочную информацию для трассировки */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Debug")
    bool bDebugTrace;

    /** Тег для блокировки способности во время перезарядки */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Gameplay Tags")
    FGameplayTag ReloadingTag;

    /** Тег для состояния прицеливания */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Gameplay Tags")
    FGameplayTag AimingTag;

    /** Тег для кулдауна стрельбы */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Gameplay Tags")
    FGameplayTag WeaponCooldownTag;

    /** Тег состояния стрельбы */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Gameplay Tags")
    FGameplayTag FiringTag;

    // ===== СИСТЕМА ОТДАЧИ =====

    /** Эффект тряски камеры при выстреле */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visuals")
    TSubclassOf<UCameraShakeBase> FireCameraShake;

    /** Множитель прогрессивной отдачи (увеличивается с каждым выстрелом) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil", meta = (ClampMin = "0.0"))
    float ProgressiveRecoilMultiplier = 1.2f;

    /** Максимальный множитель отдачи при непрерывной стрельбе */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil", meta = (ClampMin = "0.0"))
    float MaximumRecoilMultiplier = 3.0f;

    /** Время сброса уровня отдачи после прекращения стрельбы (секунды) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil", meta = (ClampMin = "0.0"))
    float RecoilResetTime = 0.5f;

    /** Коэффициент восстановления отдачи (скорость возврата) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil", meta = (ClampMin = "0.0"))
    float RecoilRecoveryRate = 0.9f;

    /** Задержка перед началом восстановления отдачи (секунды) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil", meta = (ClampMin = "0.0"))
    float RecoilRecoveryDelay = 0.2f;

    /** Интервал между шагами восстановления отдачи (секунды) */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil", meta = (ClampMin = "0.0"))
    float RecoilRecoveryTime = 0.1f;

    /** Получает оружие из ActorInfo через персонажа */
    AWeaponActor* GetWeaponFromActorInfo() const;

    /** Абстрактный метод для выполнения следующего выстрела (должен быть реализован в дочернем классе) */
    virtual void FireNextShot() PURE_VIRTUAL(UMedComBaseFireAbility::FireNextShot, );

    /** Создает и запускает AsyncTask для выполнения трассировки выстрела */
    UMedComWeaponAsyncTask_PerformTrace* CreateWeaponTraceTask(const FMedComShotRequest& ShotRequest);

    /** Применение отдачи (рекойла) */
    void ApplyRecoil(const FGameplayAbilityActorInfo* ActorInfo);

    /** Увеличивает счетчик последовательных выстрелов */
    void IncrementShotCounter();
    
    /** Сбрасывает счетчик выстрелов и множитель отдачи */
    void ResetShotCounter();
    
    /** Запускает восстановление отдачи после выстрела */
    void StartRecoilRecovery();
    
    /** Обрабатывает один шаг восстановления отдачи */
    void ProcessRecoilRecovery();

    /** Локальные эффекты выстрела */
    void PlayLocalFireEffects(const FGameplayAbilityActorInfo* ActorInfo, bool bCreateTracer = true);

    /** Визуальные эффекты в точке попадания */
    void PlayImpactEffects(const FHitResult& HitResult);

    /** Применение кулдауна способности */
    void ApplyCooldownOnAbility(
        const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo, 
        const FGameplayAbilityActivationInfo ActivationInfo);

    /** Проверка, является ли контекст серверным */
    bool IsServer(const FGameplayAbilityActorInfo* ActorInfo) const;

    /** Проверка, является ли контекст локально предсказанным клиентом */
    bool IsLocallyPredicted(const FGameplayAbilityActorInfo* ActorInfo) const;

    // Счетчик ID выстрелов, используем mutable для const-методов
    mutable int32 LastShotID;
    
    // Карта ожидающих подтверждения выстрелов
    TMap<int32, FMedComShotRequest> PendingShots;

    // ===== ВНУТРЕННИЕ ПЕРЕМЕННЫЕ СИСТЕМЫ ОТДАЧИ =====
    
    /** Количество последовательных выстрелов без перерыва */
    int32 ConsecutiveShotsCount;
    
    /** Текущий множитель отдачи */
    float CurrentRecoilMultiplier;
    
    /** Накопленная отдача (X=Pitch, Y=Yaw) */
    FVector2D AccumulatedRecoil;
    
    /** Таймер для сброса счетчика выстрелов */
    FTimerHandle RecoilResetTimerHandle;
    
    /** Таймер для восстановления отдачи */
    FTimerHandle RecoilRecoveryTimerHandle;

    /** Генерирует запрос на выстрел с полными параметрами */
    FMedComShotRequest GenerateShotRequest(const FGameplayAbilityActorInfo* ActorInfo) const;
    
    /** Серверная RPC для обработки запроса */
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerFireShot(FMedComShotRequest ShotRequest);
    
    void ServerFireShot_Implementation(FMedComShotRequest ShotRequest);
    
    bool ServerFireShot_Validate(FMedComShotRequest ShotRequest);

    /** Клиентская RPC для получения результата */
    UFUNCTION(Client, Reliable)
    void ClientReceiveShotResult(FMedComShotResult ShotResult);
    
    void ClientReceiveShotResult_Implementation(FMedComShotResult ShotResult);

    /** Проверка валидности запроса на сервере */
    bool ValidateShotRequest(const FMedComShotRequest& ShotRequest, class AWeaponActor* Weapon) const;

    /** Серверная трассировка */
    void ServerProcessShotTrace(const FMedComShotRequest& ShotRequest, TArray<FHitResult>& OutHits);

    /** Применяем урон */
    void ApplyDamageToTargets(const TArray<FHitResult>& Hits, float InDamage);

    /** Проверка наличия патронов */
    bool HasAmmo(class AWeaponActor* Weapon) const;
    
    /** Расход патронов */
    void ConsumeAmmo(class AWeaponActor* Weapon);
};