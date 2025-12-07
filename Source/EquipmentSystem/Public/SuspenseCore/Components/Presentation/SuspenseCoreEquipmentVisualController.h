// Copyright MedCom

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "Curves/CurveFloat.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "Interfaces/Equipment/ISuspenseVisualProvider.h"
#include "Core/Utils/SuspenseEquipmentCacheManager.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "Types/Equipment/SuspenseCoreEquipmentTypes.h"
#include "SuspenseCoreEquipmentVisualController.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UMaterialInstanceDynamic;
class UTexture;
class UTimelineComponent;

/**
 * Visual profile row (DataTable) — без конфликта имени с Factory
 */
USTRUCT(BlueprintType)
struct FSuspenseCoreEquipmentVisualProfile : public FTableRowBase
{
	GENERATED_BODY()

	/** Item type this profile applies to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Profile", meta=(Categories="Item"))
	FGameplayTag ItemType;

	/** Quality level tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Profile", meta=(Categories="Visual.Quality"))
	FGameplayTag QualityTag;

	/** State tag (equipped, holstered, damaged, etc) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Profile", meta=(Categories="Equipment.State"))
	FGameplayTag StateTag;

	/** Material parameters to apply */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Materials")
	TMap<FName, float> ScalarParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Materials")
	TMap<FName, FLinearColor> VectorParameters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Materials")
	TMap<FName, TSoftObjectPtr<UTexture>> TextureParameters;

	/** Visual effects to spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Effects")
	TArray<TSoftObjectPtr<UNiagaraSystem>> NiagaraEffects;

	/** Effect attachment sockets */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Effects")
	TArray<FName> EffectSockets;

	/** Priority for profile selection */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Profile")
	int32 Priority = 0;

	FString GetProfileKey() const
	{
		return FString::Printf(TEXT("%s_%s_%s"),
			*ItemType.ToString(),
			*QualityTag.ToString(),
			*StateTag.ToString());
	}
};

/** Material transition data */
USTRUCT()
struct FSuspenseCoreMaterialTransition
{
	GENERATED_BODY()

	UPROPERTY() UMaterialInstanceDynamic* Material = nullptr;
	UPROPERTY() FName ParameterName;
	UPROPERTY() float StartValue = 0.0f;
	UPROPERTY() float TargetValue = 0.0f;
	UPROPERTY() float Duration = 0.0f;
	UPROPERTY() float ElapsedTime = 0.0f;
	UPROPERTY() UCurveFloat* Curve = nullptr;

	bool IsComplete() const { return ElapsedTime >= Duration; }
	float GetCurrentValue() const
	{
		if (Duration <= 0.0f) return TargetValue;
		float Alpha = FMath::Clamp(ElapsedTime / Duration, 0.0f, 1.0f);
		if (Curve) { Alpha = Curve->GetFloatValue(Alpha); }
		return FMath::Lerp(StartValue, TargetValue, Alpha);
	}
};

/** Enhanced effect pool entry */
USTRUCT()
struct FSuspenseCoreEnhancedVisualEffectPoolEntry
{
	GENERATED_BODY()
	UPROPERTY() UNiagaraComponent* Component = nullptr;
	UPROPERTY() UNiagaraSystem* System = nullptr;
	UPROPERTY() bool bInUse = false;
	UPROPERTY() float LastUsedTime = 0.0f;
	UPROPERTY() FGuid EffectId;
	UPROPERTY() int32 ReuseCount = 0;
	UPROPERTY() FGameplayTag EffectProfile;
};

/** Active effect metadata */
USTRUCT()
struct FSuspenseCoreEnhancedActiveVisualEffect
{
	GENERATED_BODY()
	UPROPERTY() FGuid EffectId;
	UPROPERTY() AActor* TargetActor = nullptr;
	UPROPERTY() UNiagaraComponent* EffectComponent = nullptr;
	UPROPERTY() FGameplayTag EffectType;
	UPROPERTY() FGameplayTag ProfileTag;
	UPROPERTY() float StartTime = 0.0f;
	UPROPERTY() float Duration = 0.0f;
	UPROPERTY() bool bIsLooping = false;
	UPROPERTY() int32 Priority = 0;
	UPROPERTY() TMap<FString, FString> Metadata;
};

/** Material state tracking */
USTRUCT()
struct FSuspenseCoreEnhancedMaterialState
{
	GENERATED_BODY()
	UPROPERTY() TArray<UMaterialInterface*> OriginalMaterials;
	UPROPERTY() TArray<UMaterialInstanceDynamic*> DynamicMaterials;
	UPROPERTY() FSuspenseCoreEquipmentVisualProfile ActiveProfile;
	UPROPERTY() bool bHasOverride = false;
	UPROPERTY() float WearLevel = 0.0f;
	UPROPERTY() bool bIsHighlighted = false;
	UPROPERTY() FLinearColor HighlightColor = FLinearColor::White;
	UPROPERTY() TArray<FSuspenseCoreMaterialTransition> ActiveTransitions;
};

/** Batch visual request */
USTRUCT()
struct FSuspenseCoreBatchVisualRequest
{
	GENERATED_BODY()
	enum class EOperationType : uint8
	{
		ApplyEffect,
		RemoveEffect,
		UpdateMaterial,
		UpdateWear,
		SetHighlight
	};

	EOperationType Operation = EOperationType::ApplyEffect;
	UPROPERTY() AActor* TargetActor = nullptr;
	UPROPERTY() FGameplayTag ProfileTag;
	UPROPERTY() float FloatParam = 0.0f;
	UPROPERTY() FLinearColor ColorParam = FLinearColor::White;
	UPROPERTY() int32 Priority = 0;
};

/** Controller config (добавлен троттлинг эффектов) */
USTRUCT(BlueprintType)
struct FSuspenseCoreVisualControllerConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Pool") int32 MaxEffectPoolSize = 30;
	UPROPERTY(EditAnywhere, Category="Pool") int32 PreWarmEffectCount = 5;
	UPROPERTY(EditAnywhere, Category="Pool") float PoolCleanupInterval = 30.0f;
	UPROPERTY(EditAnywhere, Category="Pool") float EffectIdleTimeout = 60.0f;

	UPROPERTY(EditAnywhere, Category="Performance") bool bCacheMaterialInstances = true;
	UPROPERTY(EditAnywhere, Category="Performance") int32 MaxCachedMaterials = 100;

	UPROPERTY(EditAnywhere, Category="Visual") bool bInterpolateWearState = true;
	UPROPERTY(EditAnywhere, Category="Visual") float WearStateInterpSpeed = 2.0f;

	UPROPERTY(EditAnywhere, Category="Performance") bool bEnableBatching = true;
	UPROPERTY(EditAnywhere, Category="Performance") int32 BatchThreshold = 5;

	UPROPERTY(EditAnywhere, Category="Debug") bool bEnableDebugOverlay = false;
	UPROPERTY(EditAnywhere, Category="Debug") float DebugOverlayUpdateRate = 0.5f;

	/** ====== Новое: дефолтный троттлинг эффектов (мс) ====== */
	UPROPERTY(EditAnywhere, Category="Effects|Throttle", meta=(ClampMin="0.0", ClampMax="1000.0"))
	float DefaultEffectThrottleMs = 33.0f;

	/** Индивидуальный троттлинг по тегу эффектов */
	UPROPERTY(EditAnywhere, Category="Effects|Throttle")
	TMap<FGameplayTag, float> PerTagEffectThrottleMs;
};

/**
 * Equipment Visual Controller — SRP: только визуал (материалы/эффекты/анимации).
 */
UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent))
class EQUIPMENTSYSTEM_API USuspenseCoreEquipmentVisualController : public UActorComponent, public ISuspenseCoreVisualProvider
{
	GENERATED_BODY()

public:
	USuspenseCoreEquipmentVisualController();

	//~ UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent

	//~ ISuspenseCoreVisualProvider
	virtual FGuid ApplyVisualEffect(AActor* Equipment, const FEquipmentVisualEffect& Effect) override;
	virtual bool RemoveVisualEffect(const FGuid& EffectId) override;
	virtual bool ApplyMaterialOverride(AActor* Equipment, const FEquipmentMaterialOverride& Override) override;
	virtual void ResetMaterials(AActor* Equipment) override;
	virtual void UpdateWearState(AActor* Equipment, float WearPercent) override;
	virtual void SetHighlighted(AActor* Equipment, bool bHighlighted, const FLinearColor& HighlightColor = FLinearColor::White) override;
	virtual bool PlayEquipmentAnimation(AActor* Equipment, const FGameplayTag& AnimationTag) override;
	//~ End ISuspenseCoreVisualProvider

	/** Применить профиль визуала по StateTag (качество берётся из текущего уровня качества контроллера) */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Visual")
	bool ApplyVisualProfile(AActor* Equipment, const FGameplayTag& ProfileTag, bool bSmooth = true);

	/** Пакет визуальных операций */
	//UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Visual")
	int32 BatchProcessVisualRequests(const TArray<FSuspenseCoreBatchVisualRequest>& Requests);

	/** Прогрев пула эффектов */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Visual")
	void PreWarmEffectPool(const TArray<UNiagaraSystem*>& EffectSystems, int32 Count = 3);

	/** Плавные переходы параметров материалов */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Visual")
	void StartMaterialTransition(AActor* Equipment, const FName& ParameterName, float TargetValue, float Duration = 1.0f, UCurveFloat* Curve = nullptr);

	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Visual")
	void StartColorTransition(AActor* Equipment, const FName& ParameterName, const FLinearColor& TargetColor, float Duration = 1.0f);

	/** Очистить все эффекты предмета */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Visual")
	void ClearAllEffectsForEquipment(AActor* Equipment, bool bImmediate = false);

	/** Установить уровень качества (0..3) */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Visual")
	void SetVisualQualityLevel(int32 QualityLevel);

	/** Загрузить таблицу профилей */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Visual")
	void LoadVisualProfileTable(UDataTable* ProfileTable);

	/** Диагностика */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Visual")
	FString GetVisualStatistics() const;

	/** Debug Overlay toggle */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Visual|Debug")
	void ToggleDebugOverlay();

	/** Обновить конфиг контроллера (включая троттлинг эффектов) */
	UFUNCTION(BlueprintCallable, Category="SuspenseCoreCore|Equipment|Visual")
	void SetControllerConfiguration(const FSuspenseCoreVisualControllerConfig& NewConfig);

protected:
	UPROPERTY(EditAnywhere, Category="Visual Config")
	FSuspenseCoreVisualControllerConfig ControllerConfig;

	UPROPERTY(EditAnywhere, Category="Visual Config")
	UDataTable* VisualProfileTable = nullptr;

	UPROPERTY() int32 CurrentQualityLevel = 2;

	UPROPERTY() TArray<FSuspenseCoreEnhancedVisualEffectPoolEntry> EffectPool;
	UPROPERTY() TMap<FGuid, FSuspenseCoreEnhancedActiveVisualEffect> ActiveEffects;
	UPROPERTY() TMap<AActor*, FSuspenseCoreEnhancedMaterialState> MaterialStates;

	TArray<FSuspenseCoreBatchVisualRequest> BatchQueue;

	/** Профили из таблицы */
	TMap<FString, FSuspenseCoreEquipmentVisualProfile> ProfileCache;

	/** Кэши */
	FSuspenseEquipmentCacheManager<FName, UMaterialInstanceDynamic*> MaterialInstanceCache;
	FSuspenseEquipmentCacheManager<FName, UNiagaraSystem*> EffectSystemCache;

	/** Подписки на EventBus */
	TArray<FEventSubscriptionHandle> EventSubscriptions;

	/** Синхронизация: RWLocks под макросы из EquipmentServiceMacros.h */
	mutable FEquipmentRWLock EffectLock;
	mutable FEquipmentRWLock MaterialLock;
	mutable FEquipmentRWLock BatchLock;

	/** Таймеры */
	FTimerHandle PoolCleanupTimerHandle;
	FTimerHandle BatchProcessTimerHandle;
	FTimerHandle DebugOverlayTimerHandle;

	/** Wear interpolation */
	TMap<AActor*, float> TargetWearStates;
	TMap<AActor*, float> CurrentWearStates;

	/** Debug overlay */
	TMap<AActor*, FString> DebugOverlayData;
	bool bDebugOverlayEnabled = false;

	/** Метрики */
	mutable int32 TotalEffectsApplied = 0;
	mutable int32 TotalEffectsRemoved = 0;
	mutable int32 TotalMaterialsCreated = 0;
	mutable int32 TotalTransitionsStarted = 0;
	mutable int32 EffectPoolHits = 0;
	mutable int32 EffectPoolMisses = 0;

private:
	void SetupEventHandlers();
	void OnEquipmentStateChanged(const FSuspenseEquipmentEventData& EventData);
	void OnWeaponFired(const FSuspenseEquipmentEventData& EventData);
	void OnWeaponReload(const FSuspenseEquipmentEventData& EventData);
	void OnQuickSwitch(const FSuspenseEquipmentEventData& EventData);

	void ProcessBatchQueue();
	void UpdateMaterialTransitions(float DeltaTime);
	void UpdateWearInterpolation(float DeltaTime);
	void UpdateActiveEffects(float DeltaTime);
	void UpdateDebugOverlay();

	UNiagaraComponent* GetPooledEffectComponent(UNiagaraSystem* System, const FGameplayTag& ProfileTag);
	bool ReturnEffectToPool(UNiagaraComponent* Component);
	UNiagaraComponent* CreateEffectComponent(UNiagaraSystem* System);
	void CleanupEffectPool();

	const FSuspenseCoreEquipmentVisualProfile* FindBestVisualProfile(const FGameplayTag& ItemType, const FGameplayTag& StateTag) const;
	void LoadVisualProfiles();

	void ApplyProfileToMaterials(AActor* Equipment, const FSuspenseCoreEquipmentVisualProfile& Profile, bool bSmooth);
	void ApplyProfileEffects(AActor* Equipment, const FSuspenseCoreEquipmentVisualProfile& Profile);

	UMaterialInstanceDynamic* GetOrCreateDynamicMaterial(UMaterialInterface* BaseMaterial, const FName& CacheKey);
	void ApplyWearToMaterial(UMaterialInstanceDynamic* Material, float WearPercent);
	void ApplyHighlightToMaterial(UMaterialInstanceDynamic* Material, bool bHighlight, const FLinearColor& Color);
	TArray<UMeshComponent*> GetMeshComponents(AActor* Actor) const;
	FGuid GenerateEffectId() const;
	FGameplayTag GetQualityTag() const;

	void DrawDebugInfoForActor(AActor* Actor, const FString& Info) const;
	void LogVisualMetrics() const;

	/** ====== Новое: троттлинг эффектов ====== */
	bool ShouldPlayEffect(AActor* Equipment, const FGameplayTag& EffectTag, float& OutCooldownMs) const;
	void MarkEffectPlayed(AActor* Equipment, const FGameplayTag& EffectTag);

	/** Последнее время запуска эффектов по актёру и тегу */
	mutable TMap<TWeakObjectPtr<AActor>, TMap<FGameplayTag, double>> LastEffectTimeByActor;
};
