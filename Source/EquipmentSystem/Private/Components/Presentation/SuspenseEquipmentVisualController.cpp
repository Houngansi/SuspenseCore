// Copyright MedCom

#include "SuspenseCore/Components/Presentation/SuspenseEquipmentVisualController.h"
#include "Core/Utils/SuspenseEquipmentEventBus.h"
#include "Core/Services/SuspenseEquipmentServiceLocator.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SuspenseCore/Components/MeshComponent.h"
#include "SuspenseCore/Components/SkeletalMeshComponent.h"
#include "SuspenseCore/Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"

//==================== Lifecycle ====================

USuspenseEquipmentVisualController::USuspenseEquipmentVisualController()
	: MaterialInstanceCache(100)
	, EffectSystemCache(50)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.033f;
}

void USuspenseEquipmentVisualController::BeginPlay()
{
	Super::BeginPlay();

	// Регистрируемся в ServiceLocator как Service.VisualController
	if (auto* Locator = USuspenseEquipmentServiceLocator::Get(this))
	{
		const FGameplayTag VisCtlTag = FGameplayTag::RequestGameplayTag(TEXT("Service.VisualController"));
		if (!Locator->IsServiceRegistered(VisCtlTag))
		{
			Locator->RegisterServiceInstance(VisCtlTag, this);
		}
	}

	if (VisualProfileTable) { LoadVisualProfiles(); }
	SetupEventHandlers();

	if (ControllerConfig.PoolCleanupInterval > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			PoolCleanupTimerHandle, this,
			&USuspenseEquipmentVisualController::CleanupEffectPool,
			ControllerConfig.PoolCleanupInterval, true);
	}

	if (ControllerConfig.bEnableBatching)
	{
		GetWorld()->GetTimerManager().SetTimer(
			BatchProcessTimerHandle, this,
			&USuspenseEquipmentVisualController::ProcessBatchQueue,
			0.1f, true);
	}

	// Опциональный прогрев пула (без range-for по TMap)
	if (ControllerConfig.PreWarmEffectCount > 0 && ProfileCache.Num() > 0)
	{
		TArray<UNiagaraSystem*> Systems;
		for (auto It = ProfileCache.CreateConstIterator(); It; ++It)
		{
			const FEquipmentVisualProfile& Row = It.Value();
			const TArray<TSoftObjectPtr<UNiagaraSystem>>& Effects = Row.NiagaraEffects;
			for (int32 i = 0; i < Effects.Num(); ++i)
			{
				if (UNiagaraSystem* Sys = Effects[i].LoadSynchronous())
				{
					Systems.AddUnique(Sys);
				}
			}
		}
		PreWarmEffectPool(Systems, ControllerConfig.PreWarmEffectCount);
	}

	UE_LOG(LogTemp, Log, TEXT("[VisualController] Init: Profiles=%d, Quality=%d"),
		ProfileCache.Num(), CurrentQualityLevel);
}

void USuspenseEquipmentVisualController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Снимаемся из ServiceLocator
	if (auto* Locator = USuspenseEquipmentServiceLocator::Get(this))
	{
		const FGameplayTag VisCtlTag = FGameplayTag::RequestGameplayTag(TEXT("Service.VisualController"));
		if (Locator->IsServiceRegistered(VisCtlTag))
		{
			Locator->UnregisterService(VisCtlTag, /*bForceShutdown=*/false);
		}
	}

	// Отписки (без range-for)
	if (auto Bus = FSuspenseEquipmentEventBus::Get(); Bus.IsValid())
	{
		for (int32 i = 0; i < EventSubscriptions.Num(); ++i)
		{
			Bus->Unsubscribe(EventSubscriptions[i]);
		}
	}
	EventSubscriptions.Empty();

	// Эффекты
	for (auto It = ActiveEffects.CreateIterator(); It; ++It)
	{
		FEnhancedActiveVisualEffect& AE = It.Value();
		if (AE.EffectComponent)
		{
			AE.EffectComponent->Deactivate();
			AE.EffectComponent->DestroyComponent();
		}
	}
	ActiveEffects.Empty();

	// Пул
	for (int32 i = 0; i < EffectPool.Num(); ++i)
	{
		if (EffectPool[i].Component)
		{
			EffectPool[i].Component->DestroyComponent();
		}
	}
	EffectPool.Empty();

	// Кэши
	MaterialInstanceCache.Clear();
	EffectSystemCache.Clear();
	ProfileCache.Empty();

	// Таймеры
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PoolCleanupTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(BatchProcessTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(DebugOverlayTimerHandle);
	}

	LogVisualMetrics();
	Super::EndPlay(EndPlayReason);
}

void USuspenseEquipmentVisualController::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateMaterialTransitions(DeltaTime);
	if (ControllerConfig.bInterpolateWearState) { UpdateWearInterpolation(DeltaTime); }
	UpdateActiveEffects(DeltaTime);

	if (ControllerConfig.bEnableBatching && BatchQueue.Num() >= ControllerConfig.BatchThreshold)
	{
		ProcessBatchQueue();
	}
}

//==================== ISuspenseVisualProvider ====================

FGuid USuspenseEquipmentVisualController::ApplyVisualEffect(AActor* Equipment, const FEquipmentVisualEffect& Effect)
{
	if (!IsValid(Equipment) || (!Effect.NiagaraEffect && !Effect.CascadeEffect))
	{
		return FGuid();
	}

	// Throttle
	float CooldownMs = 0.0f;
	if (!ShouldPlayEffect(Equipment, Effect.EffectType, CooldownMs))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[VisualController] Effect '%s' throttled (%.1f ms) on %s"),
			*Effect.EffectType.ToString(), CooldownMs, *Equipment->GetName());
		return FGuid();
	}

	UNiagaraSystem* SystemToUse = Effect.NiagaraEffect;
	if (!SystemToUse && Effect.CascadeEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VisualController] Cascade not supported here, use Niagara"));
		return FGuid();
	}

	UNiagaraComponent* Comp = GetPooledEffectComponent(SystemToUse, Effect.EffectType);
	if (!Comp)
	{
		Comp = CreateEffectComponent(SystemToUse);
		if (!Comp)
		{
			UE_LOG(LogTemp, Warning, TEXT("[VisualController] Failed to create NiagaraComponent"));
			return FGuid();
		}
		EffectPoolMisses++;
	}
	else
	{
		EffectPoolHits++;
	}

	// Привязка
	Comp->AttachToComponent(
		Equipment->GetRootComponent(),
		FAttachmentTransformRules::SnapToTargetIncludingScale,
		Effect.AttachSocket);

	Comp->SetRelativeTransform(Effect.RelativeTransform);
	Comp->Activate(true);
	Comp->SetVisibility(true);

	// Учёт
	const FGuid Id = GenerateEffectId();
	{
		EQUIPMENT_WRITE_LOCK(EffectLock);
		FEnhancedActiveVisualEffect& AE = ActiveEffects.Add(Id);
		AE.EffectId = Id;
		AE.TargetActor = Equipment;
		AE.EffectComponent = Comp;
		AE.EffectType = Effect.EffectType;
		AE.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		AE.Duration = Effect.Duration;
		AE.bIsLooping = Effect.bLooping;
	}
	TotalEffectsApplied++;

	// Отметка троттлинга
	MarkEffectPlayed(Equipment, Effect.EffectType);

	// EventBus
	FSuspenseEquipmentEventData Ev;
	Ev.EventType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Visual.EffectApplied"));
	Ev.Target = Equipment;
	Ev.AddMetadata(TEXT("EffectType"), Effect.EffectType.ToString());
	Ev.AddMetadata(TEXT("EffectId"), Id.ToString());
	FSuspenseEquipmentEventBus::Get()->Broadcast(Ev);

	return Id;
}

bool USuspenseEquipmentVisualController::RemoveVisualEffect(const FGuid& EffectId)
{
	EQUIPMENT_WRITE_LOCK(EffectLock);

	if (FEnhancedActiveVisualEffect* AE = ActiveEffects.Find(EffectId))
	{
		if (IsValid(AE->EffectComponent))
		{
			AE->EffectComponent->Deactivate();
			AE->EffectComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			ReturnEffectToPool(AE->EffectComponent);
		}

		AActor* Target = AE->TargetActor;
		const FGameplayTag Type = AE->EffectType;
		ActiveEffects.Remove(EffectId);
		TotalEffectsRemoved++;

		FSuspenseEquipmentEventData Ev;
		Ev.EventType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Visual.EffectRemoved"));
		Ev.Target = Target;
		Ev.AddMetadata(TEXT("EffectType"), Type.ToString());
		Ev.AddMetadata(TEXT("EffectId"), EffectId.ToString());
		FSuspenseEquipmentEventBus::Get()->Broadcast(Ev);

		return true;
	}
	return false;
}

bool USuspenseEquipmentVisualController::ApplyMaterialOverride(AActor* Equipment, const FEquipmentMaterialOverride& Override)
{
	if (!IsValid(Equipment)) return false;

	TArray<UMeshComponent*> MeshComps = GetMeshComponents(Equipment);
	if (MeshComps.Num() == 0) return false;

	EQUIPMENT_WRITE_LOCK(MaterialLock);
	FEnhancedMaterialState& MS = MaterialStates.FindOrAdd(Equipment);

	if (!MS.bHasOverride)
	{
		for (UMeshComponent* MC : MeshComps)
		{
			for (int32 i = 0; i < MC->GetNumMaterials(); ++i)
			{
				MS.OriginalMaterials.Add(MC->GetMaterial(i));
			}
		}
	}

	MS.DynamicMaterials.Empty();

	for (UMeshComponent* MC : MeshComps)
	{
		const int32 StartIdx = Override.MaterialSlot >= 0 ? Override.MaterialSlot : 0;
		const int32 EndIdx = Override.MaterialSlot >= 0 ? Override.MaterialSlot + 1 : MC->GetNumMaterials();

		for (int32 i = StartIdx; i < EndIdx && i < MC->GetNumMaterials(); ++i)
		{
			UMaterialInterface* Base = Override.OverrideMaterial ? Override.OverrideMaterial : MC->GetMaterial(i);
			FName CacheKey(*FString::Printf(TEXT("%s_%d_override"), *Equipment->GetName(), i));
			UMaterialInstanceDynamic* Dyn = GetOrCreateDynamicMaterial(Base, CacheKey);
			if (!Dyn) continue;

			for (const auto& P : Override.ScalarParameters)  { Dyn->SetScalarParameterValue(P.Key, P.Value); }
			for (const auto& P : Override.VectorParameters)  { Dyn->SetVectorParameterValue(P.Key, P.Value); }
			for (const auto& P : Override.TextureParameters) { Dyn->SetTextureParameterValue(P.Key, P.Value); }

			if (MS.WearLevel > 0.0f) { ApplyWearToMaterial(Dyn, MS.WearLevel); }
			if (MS.bIsHighlighted)    { ApplyHighlightToMaterial(Dyn, true, MS.HighlightColor); }

			MC->SetMaterial(i, Dyn);
			MS.DynamicMaterials.Add(Dyn);
		}
	}

	MS.bHasOverride = true;
	TotalMaterialsCreated += MS.DynamicMaterials.Num();
	return true;
}

void USuspenseEquipmentVisualController::ResetMaterials(AActor* Equipment)
{
	if (!IsValid(Equipment)) return;

	EQUIPMENT_WRITE_LOCK(MaterialLock);
	FEnhancedMaterialState* MS = MaterialStates.Find(Equipment);
	if (!MS || !MS->bHasOverride) return;

	MS->ActiveTransitions.Empty();

	TArray<UMeshComponent*> MeshComps = GetMeshComponents(Equipment);
	int32 Idx = 0;
	for (UMeshComponent* MC : MeshComps)
	{
		for (int32 i = 0; i < MC->GetNumMaterials() && Idx < MS->OriginalMaterials.Num(); ++i)
		{
			MC->SetMaterial(i, MS->OriginalMaterials[Idx++]);
		}
	}
	MaterialStates.Remove(Equipment);
}

void USuspenseEquipmentVisualController::UpdateWearState(AActor* Equipment, float WearPercent)
{
	if (!IsValid(Equipment)) return;

	WearPercent = FMath::Clamp(WearPercent, 0.0f, 1.0f);

	if (ControllerConfig.bInterpolateWearState)
	{
		TargetWearStates.Add(Equipment, WearPercent);
		if (!CurrentWearStates.Contains(Equipment)) { CurrentWearStates.Add(Equipment, WearPercent); }
	}
	else
	{
		CurrentWearStates.Add(Equipment, WearPercent);
		TArray<UMeshComponent*> MeshComps = GetMeshComponents(Equipment);
		for (UMeshComponent* MC : MeshComps)
		{
			for (int32 i = 0; i < MC->GetNumMaterials(); ++i)
			{
				UMaterialInstanceDynamic* Dyn = Cast<UMaterialInstanceDynamic>(MC->GetMaterial(i));
				if (!Dyn)
				{
					if (UMaterialInterface* Base = MC->GetMaterial(i))
					{
						FName CacheKey(*FString::Printf(TEXT("%s_wear_%d"), *Equipment->GetName(), i));
						Dyn = GetOrCreateDynamicMaterial(Base, CacheKey);
						MC->SetMaterial(i, Dyn);
					}
				}
				if (Dyn) { ApplyWearToMaterial(Dyn, WearPercent); }
			}
		}
	}

	EQUIPMENT_WRITE_LOCK(MaterialLock);
	if (FEnhancedMaterialState* MS = MaterialStates.Find(Equipment))
	{
		MS->WearLevel = WearPercent;
	}
}

void USuspenseEquipmentVisualController::SetHighlighted(AActor* Equipment, bool bHighlighted, const FLinearColor& HighlightColor)
{
	if (!IsValid(Equipment)) return;

	EQUIPMENT_WRITE_LOCK(MaterialLock);
	FEnhancedMaterialState& MS = MaterialStates.FindOrAdd(Equipment);
	MS.bIsHighlighted = bHighlighted;
	MS.HighlightColor = HighlightColor;

	TArray<UMeshComponent*> MeshComps = GetMeshComponents(Equipment);
	for (UMeshComponent* MC : MeshComps)
	{
		for (int32 i = 0; i < MC->GetNumMaterials(); ++i)
		{
			UMaterialInstanceDynamic* Dyn = Cast<UMaterialInstanceDynamic>(MC->GetMaterial(i));
			if (!Dyn)
			{
				if (UMaterialInterface* Base = MC->GetMaterial(i))
				{
					FName CacheKey(*FString::Printf(TEXT("%s_highlight_%d"), *Equipment->GetName(), i));
					Dyn = GetOrCreateDynamicMaterial(Base, CacheKey);
					MC->SetMaterial(i, Dyn);
				}
			}
			if (Dyn) { ApplyHighlightToMaterial(Dyn, bHighlighted, HighlightColor); }
		}
	}
}

bool USuspenseEquipmentVisualController::PlayEquipmentAnimation(AActor* Equipment, const FGameplayTag& AnimationTag)
{
	if (!IsValid(Equipment)) return false;

	if (const FEquipmentVisualProfile* Profile = FindBestVisualProfile(
		FGameplayTag::RequestGameplayTag(TEXT("Item.Equipment")), AnimationTag))
	{
		ApplyProfileEffects(Equipment, *Profile);
		if (Profile->ScalarParameters.Num() || Profile->VectorParameters.Num() || Profile->TextureParameters.Num())
		{
			ApplyProfileToMaterials(Equipment, *Profile, true);
		}
	}

	FSuspenseEquipmentEventData Ev;
	Ev.EventType = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Visual.AnimationPlayed"));
	Ev.Target = Equipment;
	Ev.AddMetadata(TEXT("AnimationTag"), AnimationTag.ToString());
	FSuspenseEquipmentEventBus::Get()->Broadcast(Ev);
	return true;
}

//==================== Profiles / Effects ====================

bool USuspenseEquipmentVisualController::ApplyVisualProfile(AActor* Equipment, const FGameplayTag& ProfileTag, bool bSmooth)
{
	if (!IsValid(Equipment)) return false;

	const FEquipmentVisualProfile* Profile = nullptr;
	for (auto It = ProfileCache.CreateConstIterator(); It; ++It)
	{
		const FEquipmentVisualProfile& Row = It.Value();
		if (Row.StateTag.MatchesTagExact(ProfileTag))
		{
			Profile = &Row;
			break;
		}
	}
	if (!Profile)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[VisualController] Profile '%s' not found"), *ProfileTag.ToString());
		return false;
	}

	ApplyProfileToMaterials(Equipment, *Profile, bSmooth);
	ApplyProfileEffects(Equipment, *Profile);

	EQUIPMENT_WRITE_LOCK(MaterialLock);
	if (FEnhancedMaterialState* MS = MaterialStates.Find(Equipment)) { MS->ActiveProfile = *Profile; }
	return true;
}


int32 USuspenseEquipmentVisualController::BatchProcessVisualRequests(const TArray<FBatchVisualRequest>& Requests)
{
	if (!ControllerConfig.bEnableBatching || Requests.Num() < ControllerConfig.BatchThreshold)
	{
		int32 Success = 0;
		for (const FBatchVisualRequest& R : Requests)
		{
			switch (R.Operation)
			{
				case FBatchVisualRequest::EOperationType::ApplyEffect:
				{
					FEquipmentVisualEffect E; E.EffectType = R.ProfileTag;
					if (ApplyVisualEffect(R.TargetActor, E).IsValid()) { ++Success; }
					break;
				}
				case FBatchVisualRequest::EOperationType::UpdateWear:
					UpdateWearState(R.TargetActor, R.FloatParam); ++Success; break;
				case FBatchVisualRequest::EOperationType::SetHighlight:
					SetHighlighted(R.TargetActor, R.FloatParam > 0.0f, R.ColorParam); ++Success; break;
				default: break;
			}
		}
		return Success;
	}

	EQUIPMENT_WRITE_LOCK(BatchLock);
	for (const FBatchVisualRequest& R : Requests) { BatchQueue.Add(R); }
	if (BatchQueue.Num() >= ControllerConfig.BatchThreshold * 2) { ProcessBatchQueue(); }
	return Requests.Num();
}

void USuspenseEquipmentVisualController::PreWarmEffectPool(const TArray<UNiagaraSystem*>& EffectSystems, int32 Count)
{
	for (UNiagaraSystem* Sys : EffectSystems)
	{
		if (!Sys) continue;
		for (int32 i = 0; i < Count; ++i)
		{
			if (UNiagaraComponent* C = CreateEffectComponent(Sys)) { ReturnEffectToPool(C); }
		}
	}
}

void USuspenseEquipmentVisualController::StartMaterialTransition(AActor* Equipment, const FName& ParameterName, float TargetValue, float Duration, UCurveFloat* Curve)
{
	if (!IsValid(Equipment) || Duration <= 0.0f) return;

	EQUIPMENT_WRITE_LOCK(MaterialLock);
	FEnhancedMaterialState& MS = MaterialStates.FindOrAdd(Equipment);

	for (UMaterialInstanceDynamic* Dyn : MS.DynamicMaterials)
	{
		if (!Dyn) continue;

		float Current = 0.0f; Dyn->GetScalarParameterValue(ParameterName, Current);

		FMaterialTransition* Existing = nullptr;
		for (FMaterialTransition& T : MS.ActiveTransitions)
		{
			if (T.Material == Dyn && T.ParameterName == ParameterName) { Existing = &T; break; }
		}
		if (Existing)
		{
			Existing->StartValue = Existing->GetCurrentValue();
			Existing->TargetValue = TargetValue;
			Existing->Duration = Duration;
			Existing->ElapsedTime = 0.0f;
			Existing->Curve = Curve;
		}
		else
		{
			FMaterialTransition NewT;
			NewT.Material = Dyn;
			NewT.ParameterName = ParameterName;
			NewT.StartValue = Current;
			NewT.TargetValue = TargetValue;
			NewT.Duration = Duration;
			NewT.ElapsedTime = 0.0f;
			NewT.Curve = Curve;
			MS.ActiveTransitions.Add(NewT);
		}
	}
	TotalTransitionsStarted++;
}

void USuspenseEquipmentVisualController::StartColorTransition(AActor* Equipment, const FName& ParameterName, const FLinearColor& TargetColor, float Duration)
{
	if (!IsValid(Equipment) || Duration <= 0.0f) return;
	StartMaterialTransition(Equipment, FName(*(ParameterName.ToString()+TEXT(".R"))), TargetColor.R, Duration);
	StartMaterialTransition(Equipment, FName(*(ParameterName.ToString()+TEXT(".G"))), TargetColor.G, Duration);
	StartMaterialTransition(Equipment, FName(*(ParameterName.ToString()+TEXT(".B"))), TargetColor.B, Duration);
	StartMaterialTransition(Equipment, FName(*(ParameterName.ToString()+TEXT(".A"))), TargetColor.A, Duration);
}

void USuspenseEquipmentVisualController::ClearAllEffectsForEquipment(AActor* Equipment, bool bImmediate)
{
	if (!IsValid(Equipment)) return;

	TArray<FGuid> ToRemove;
	{
		EQUIPMENT_READ_LOCK(EffectLock);
		for (const auto& P : ActiveEffects)
		{
			if (P.Value.TargetActor == Equipment) { ToRemove.Add(P.Key); }
		}
	}
	for (const FGuid& Id : ToRemove) { RemoveVisualEffect(Id); }
	if (bImmediate) { ResetMaterials(Equipment); }

	if (bDebugOverlayEnabled) { DebugOverlayData.Remove(Equipment); }
}

void USuspenseEquipmentVisualController::SetVisualQualityLevel(int32 QualityLevel)
{
	CurrentQualityLevel = FMath::Clamp(QualityLevel, 0, 3);
	UE_LOG(LogTemp, Log, TEXT("[VisualController] Quality=%d"), CurrentQualityLevel);
}

void USuspenseEquipmentVisualController::LoadVisualProfileTable(UDataTable* ProfileTable)
{
	VisualProfileTable = ProfileTable;
	LoadVisualProfiles();
}

//==================== Events / subscriptions ====================

void USuspenseEquipmentVisualController::SetupEventHandlers()
{
	auto Bus = FSuspenseEquipmentEventBus::Get();
	if (!Bus.IsValid()) return;

	EventSubscriptions.Add(Bus->Subscribe(
		FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.StateChanged")),
		FEventHandlerDelegate::CreateUObject(this, &USuspenseEquipmentVisualController::OnEquipmentStateChanged),
		EEventPriority::Normal, EEventExecutionContext::GameThread, this));

	EventSubscriptions.Add(Bus->Subscribe(
		FGameplayTag::RequestGameplayTag(TEXT("Weapon.Event.Fired")),
		FEventHandlerDelegate::CreateUObject(this, &USuspenseEquipmentVisualController::OnWeaponFired),
		EEventPriority::High, EEventExecutionContext::GameThread, this));

	EventSubscriptions.Add(Bus->Subscribe(
		FGameplayTag::RequestGameplayTag(TEXT("Weapon.Event.Reload")),
		FEventHandlerDelegate::CreateUObject(this, &USuspenseEquipmentVisualController::OnWeaponReload),
		EEventPriority::Normal, EEventExecutionContext::GameThread, this));

	EventSubscriptions.Add(Bus->Subscribe(
		FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.QuickSwitch")),
		FEventHandlerDelegate::CreateUObject(this, &USuspenseEquipmentVisualController::OnQuickSwitch),
		EEventPriority::High, EEventExecutionContext::GameThread, this));
}

void USuspenseEquipmentVisualController::OnEquipmentStateChanged(const FSuspenseEquipmentEventData& EventData)
{
	if (AActor* Eq = EventData.GetTargetAs<AActor>())
	{
		const FGameplayTag StateTag = FGameplayTag::RequestGameplayTag(
			*EventData.GetMetadata(TEXT("NewState"), TEXT("Equipment.State.Idle")));
		ApplyVisualProfile(Eq, StateTag, true);
	}
}

void USuspenseEquipmentVisualController::OnWeaponFired(const FSuspenseEquipmentEventData& EventData)
{
	if (AActor* Wep = EventData.GetTargetAs<AActor>())
	{
		FEquipmentVisualEffect Fx;
		Fx.EffectType = FGameplayTag::RequestGameplayTag(TEXT("Effect.Weapon.MuzzleFlash"));
		Fx.AttachSocket = TEXT("Muzzle");
		Fx.Duration = 0.1f;
		ApplyVisualEffect(Wep, Fx);

		StartMaterialTransition(Wep, TEXT("HeatAmount"), 1.0f, 0.2f);
		StartMaterialTransition(Wep, TEXT("HeatAmount"), 0.0f, 3.0f);
	}
}

void USuspenseEquipmentVisualController::OnWeaponReload(const FSuspenseEquipmentEventData& EventData)
{
	if (AActor* Wep = EventData.GetTargetAs<AActor>())
	{
		ApplyVisualProfile(Wep, FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Reloading")), true);
	}
}

void USuspenseEquipmentVisualController::OnQuickSwitch(const FSuspenseEquipmentEventData& EventData)
{
	if (AActor* OldW = Cast<AActor>(EventData.Source.Get())) { ClearAllEffectsForEquipment(OldW, false); }
	if (AActor* NewW = EventData.GetTargetAs<AActor>())
	{
		ApplyVisualProfile(NewW, FGameplayTag::RequestGameplayTag(TEXT("Equipment.State.Active")), true);
	}
}

//==================== Batch / updates ====================

void USuspenseEquipmentVisualController::ProcessBatchQueue()
{
	EQUIPMENT_WRITE_LOCK(BatchLock);
	if (BatchQueue.Num() == 0) return;

	BatchQueue.Sort([](const FBatchVisualRequest& A, const FBatchVisualRequest& B)
	{
		return A.Priority > B.Priority;
	});

	int32 Processed = 0;
	for (const FBatchVisualRequest& R : BatchQueue)
	{
		switch (R.Operation)
		{
			case FBatchVisualRequest::EOperationType::ApplyEffect:
			{
				FEquipmentVisualEffect Fx; Fx.EffectType = R.ProfileTag;
				ApplyVisualEffect(R.TargetActor, Fx);
				break;
			}
			case FBatchVisualRequest::EOperationType::UpdateWear:
				UpdateWearState(R.TargetActor, R.FloatParam);
				break;
			case FBatchVisualRequest::EOperationType::SetHighlight:
				SetHighlighted(R.TargetActor, R.FloatParam > 0.0f, R.ColorParam);
				break;
			default: break;
		}
		if (++Processed >= ControllerConfig.BatchThreshold * 2) { break; }
	}
	BatchQueue.RemoveAt(0, Processed);
}

void USuspenseEquipmentVisualController::UpdateMaterialTransitions(float DeltaTime)
{
	EQUIPMENT_WRITE_LOCK(MaterialLock);
	for (auto& P : MaterialStates)
	{
		FEnhancedMaterialState& MS = P.Value;
		for (int32 i = MS.ActiveTransitions.Num() - 1; i >= 0; --i)
		{
			FMaterialTransition& T = MS.ActiveTransitions[i];
			T.ElapsedTime += DeltaTime;
			if (T.Material)
			{
				T.Material->SetScalarParameterValue(T.ParameterName, T.GetCurrentValue());
				if (T.IsComplete()) { MS.ActiveTransitions.RemoveAt(i); }
			}
		}
	}
}

void USuspenseEquipmentVisualController::UpdateWearInterpolation(float DeltaTime)
{
	for (auto& P : CurrentWearStates)
	{
		AActor* Eq = P.Key;
		float& Cur = P.Value;

		if (float* Target = TargetWearStates.Find(Eq))
		{
			if (!FMath::IsNearlyEqual(Cur, *Target))
			{
				Cur = FMath::FInterpTo(Cur, *Target, DeltaTime, ControllerConfig.WearStateInterpSpeed);
				TArray<UMeshComponent*> MeshComps = GetMeshComponents(Eq);
				for (UMeshComponent* MC : MeshComps)
				{
					for (int32 i = 0; i < MC->GetNumMaterials(); ++i)
					{
						if (UMaterialInstanceDynamic* Dyn = Cast<UMaterialInstanceDynamic>(MC->GetMaterial(i)))
						{
							ApplyWearToMaterial(Dyn, Cur);
						}
					}
				}
			}
		}
	}
}

void USuspenseEquipmentVisualController::UpdateActiveEffects(float DeltaTime)
{
	EQUIPMENT_WRITE_LOCK(EffectLock);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	TArray<FGuid> Remove;
	for (auto& P : ActiveEffects)
	{
		FEnhancedActiveVisualEffect& E = P.Value;
		if ((!E.bIsLooping && E.Duration > 0.0f && (Now - E.StartTime) >= E.Duration) || !IsValid(E.TargetActor))
		{
			Remove.Add(P.Key);
		}
	}
	for (const FGuid& Id : Remove) { RemoveVisualEffect(Id); }
}

//==================== Helpers / profiles ====================

UNiagaraComponent* USuspenseEquipmentVisualController::GetPooledEffectComponent(UNiagaraSystem* System, const FGameplayTag& /*ProfileTag*/)
{
	EQUIPMENT_WRITE_LOCK(EffectLock);
	for (FEnhancedVisualEffectPoolEntry& E : EffectPool)
	{
		if (E.System == System && !E.bInUse && IsValid(E.Component))
		{
			E.bInUse = true;
			E.LastUsedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			E.ReuseCount++;
			return E.Component;
		}
	}
	return nullptr;
}

bool USuspenseEquipmentVisualController::ReturnEffectToPool(UNiagaraComponent* Component)
{
	if (!IsValid(Component)) return false;

	EQUIPMENT_WRITE_LOCK(EffectLock);

	if (EffectPool.Num() >= ControllerConfig.MaxEffectPoolSize)
	{
		Component->DestroyComponent();
		return false;
	}

	for (FEnhancedVisualEffectPoolEntry& E : EffectPool)
	{
		if (E.Component == Component)
		{
			E.bInUse = false;
			E.LastUsedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			Component->SetVisibility(false);
			return true;
		}
	}

	FEnhancedVisualEffectPoolEntry NewE;
	NewE.Component = Component;
	NewE.System = Component->GetAsset();
	NewE.bInUse = false;
	NewE.LastUsedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	EffectPool.Add(NewE);
	Component->SetVisibility(false);
	return true;
}

UNiagaraComponent* USuspenseEquipmentVisualController::CreateEffectComponent(UNiagaraSystem* System)
{
	if (!System || !GetOwner()) return nullptr;
	UNiagaraComponent* C = NewObject<UNiagaraComponent>(GetOwner());
	C->SetAsset(System);
	C->SetAutoActivate(false);
	C->RegisterComponent();
	return C;
}

void USuspenseEquipmentVisualController::CleanupEffectPool()
{
	EQUIPMENT_WRITE_LOCK(EffectLock);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	for (int32 i = EffectPool.Num()-1; i >= 0; --i)
	{
		FEnhancedVisualEffectPoolEntry& E = EffectPool[i];
		if (!E.bInUse && (Now - E.LastUsedTime) > ControllerConfig.EffectIdleTimeout)
		{
			if (IsValid(E.Component)) { E.Component->DestroyComponent(); }
			EffectPool.RemoveAt(i);
		}
	}
}

const FEquipmentVisualProfile* USuspenseEquipmentVisualController::FindBestVisualProfile(const FGameplayTag& ItemType, const FGameplayTag& StateTag) const
{
	const FEquipmentVisualProfile* Best = nullptr;
	int32 BestScore = -1;
	const FGameplayTag Quality = GetQualityTag();

	for (auto It = ProfileCache.CreateConstIterator(); It; ++It)
	{
		const FEquipmentVisualProfile& R = It.Value();
		int32 Score = 0;

		if (R.ItemType.MatchesTagExact(ItemType))        { Score += 100; }
		else if (R.ItemType.MatchesTag(ItemType))        { Score += 50; }

		if (R.StateTag.MatchesTagExact(StateTag))        { Score += 100; }
		else if (R.StateTag.MatchesTag(StateTag))        { Score += 50; }

		if (R.QualityTag.MatchesTagExact(Quality))       { Score += 50; }

		Score += R.Priority;

		if (Score > BestScore) { Best = &R; BestScore = Score; }
	}
	return Best;
}

void USuspenseEquipmentVisualController::LoadVisualProfiles()
{
	if (!VisualProfileTable) return;

	ProfileCache.Empty();
	const FString Ctx(TEXT("LoadVisualProfiles"));
	TArray<FEquipmentVisualProfile*> Rows;
	VisualProfileTable->GetAllRows<FEquipmentVisualProfile>(Ctx, Rows);

	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		const FEquipmentVisualProfile* R = Rows[i];
		if (R)
		{
			ProfileCache.Add(R->GetProfileKey(), *R);
		}
	}
}

void USuspenseEquipmentVisualController::ApplyProfileToMaterials(AActor* Equipment, const FEquipmentVisualProfile& Profile, bool bSmooth)
{
	if (!IsValid(Equipment)) return;

	TArray<UMeshComponent*> MeshComps = GetMeshComponents(Equipment);
	for (UMeshComponent* MC : MeshComps)
	{
		for (int32 i = 0; i < MC->GetNumMaterials(); ++i)
		{
			UMaterialInstanceDynamic* Dyn = Cast<UMaterialInstanceDynamic>(MC->GetMaterial(i));
			if (!Dyn)
			{
				if (UMaterialInterface* Base = MC->GetMaterial(i))
				{
					FName CacheKey(*FString::Printf(TEXT("%s_profile_%d"), *Equipment->GetName(), i));
					Dyn = GetOrCreateDynamicMaterial(Base, CacheKey);
					MC->SetMaterial(i, Dyn);
				}
			}
			if (!Dyn) continue;

			for (const auto& P : Profile.ScalarParameters)
			{
				if (bSmooth) { StartMaterialTransition(Equipment, P.Key, P.Value, 0.5f); }
				else         { Dyn->SetScalarParameterValue(P.Key, P.Value); }
			}
			for (const auto& P : Profile.VectorParameters)
			{
				if (bSmooth) { StartColorTransition(Equipment, P.Key, P.Value, 0.5f); }
				else         { Dyn->SetVectorParameterValue(P.Key, P.Value); }
			}
			for (const auto& P : Profile.TextureParameters)
			{
				if (UTexture* T = P.Value.LoadSynchronous()) { Dyn->SetTextureParameterValue(P.Key, T); }
			}
		}
	}
}

void USuspenseEquipmentVisualController::ApplyProfileEffects(AActor* Equipment, const FEquipmentVisualProfile& Profile)
{
	if (!IsValid(Equipment)) return;

	for (int32 i = 0; i < Profile.NiagaraEffects.Num(); ++i)
	{
		if (UNiagaraSystem* S = Profile.NiagaraEffects[i].LoadSynchronous())
		{
			FEquipmentVisualEffect Fx;
			Fx.NiagaraEffect = S;
			Fx.EffectType = Profile.StateTag;
			Fx.AttachSocket = Profile.EffectSockets.IsValidIndex(i) ? Profile.EffectSockets[i] : NAME_None;
			Fx.bLooping = true;
			ApplyVisualEffect(Equipment, Fx);
		}
	}
}

//==================== Materials / utils ====================

UMaterialInstanceDynamic* USuspenseEquipmentVisualController::GetOrCreateDynamicMaterial(UMaterialInterface* BaseMaterial, const FName& CacheKey)
{
	if (!BaseMaterial) return nullptr;

	if (ControllerConfig.bCacheMaterialInstances)
	{
		UMaterialInstanceDynamic* Cached = nullptr;
		if (MaterialInstanceCache.Get(CacheKey, Cached) && IsValid(Cached)) { return Cached; }
	}
	UMaterialInstanceDynamic* Dyn = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (Dyn && ControllerConfig.bCacheMaterialInstances)
	{
		MaterialInstanceCache.Set(CacheKey, Dyn, 300.0f);
	}
	TotalMaterialsCreated++;
	return Dyn;
}

void USuspenseEquipmentVisualController::ApplyWearToMaterial(UMaterialInstanceDynamic* Material, float WearPercent)
{
	if (!Material) return;
	Material->SetScalarParameterValue(TEXT("WearAmount"), WearPercent);
	Material->SetScalarParameterValue(TEXT("DirtAmount"), WearPercent * 0.7f);
	Material->SetScalarParameterValue(TEXT("ScratchAmount"), WearPercent * 0.5f);
	Material->SetScalarParameterValue(TEXT("RustAmount"), WearPercent * 0.3f);

	const FLinearColor Tint = FLinearColor::LerpUsingHSV(FLinearColor::White, FLinearColor(0.7f, 0.6f, 0.5f), WearPercent);
	Material->SetVectorParameterValue(TEXT("WearTint"), Tint);
}

void USuspenseEquipmentVisualController::ApplyHighlightToMaterial(UMaterialInstanceDynamic* Material, bool bHighlight, const FLinearColor& Color)
{
	if (!Material) return;
	Material->SetScalarParameterValue(TEXT("HighlightIntensity"), bHighlight ? 1.0f : 0.0f);
	Material->SetVectorParameterValue(TEXT("HighlightColor"), Color);
	Material->SetScalarParameterValue(TEXT("EmissiveBoost"), bHighlight ? 2.0f : 1.0f);
	Material->SetScalarParameterValue(TEXT("FresnelExponent"), bHighlight ? 3.0f : 5.0f);
	Material->SetScalarParameterValue(TEXT("FresnelIntensity"), bHighlight ? 1.5f : 0.0f);
}

TArray<UMeshComponent*> USuspenseEquipmentVisualController::GetMeshComponents(AActor* Actor) const
{
	TArray<UMeshComponent*> Out;
	if (Actor) { Actor->GetComponents<UMeshComponent>(Out); }
	return Out;
}

FGuid USuspenseEquipmentVisualController::GenerateEffectId() const { return FGuid::NewGuid(); }

FGameplayTag USuspenseEquipmentVisualController::GetQualityTag() const
{
	switch (CurrentQualityLevel)
	{
		case 0: return FGameplayTag::RequestGameplayTag(TEXT("Visual.Quality.Low"));
		case 1: return FGameplayTag::RequestGameplayTag(TEXT("Visual.Quality.Medium"));
		case 3: return FGameplayTag::RequestGameplayTag(TEXT("Visual.Quality.Ultra"));
		default: return FGameplayTag::RequestGameplayTag(TEXT("Visual.Quality.High"));
	}
}

//==================== Debug / stats ====================

FString USuspenseEquipmentVisualController::GetVisualStatistics() const
{
	EQUIPMENT_READ_LOCK(EffectLock);
	const float HitRate = (EffectPoolHits + EffectPoolMisses) > 0
		? (float)EffectPoolHits / (EffectPoolHits + EffectPoolMisses) * 100.0f
		: 100.0f;

	return FString::Printf(
		TEXT("VC: Active=%d, Pool=%d, HitRate=%.2f%%, Effects{+%d,-%d}, Materials=%d, Transitions=%d, Profiles=%d, Q=%d"),
		ActiveEffects.Num(), EffectPool.Num(), HitRate,
		TotalEffectsApplied, TotalEffectsRemoved, TotalMaterialsCreated, TotalTransitionsStarted,
		ProfileCache.Num(), CurrentQualityLevel);
}

void USuspenseEquipmentVisualController::ToggleDebugOverlay()
{
	bDebugOverlayEnabled = !bDebugOverlayEnabled;
	if (bDebugOverlayEnabled)
	{
		GetWorld()->GetTimerManager().SetTimer(
			DebugOverlayTimerHandle, this, &USuspenseEquipmentVisualController::UpdateDebugOverlay,
			ControllerConfig.DebugOverlayUpdateRate, true);
	}
	else
	{
		GetWorld()->GetTimerManager().ClearTimer(DebugOverlayTimerHandle);
		DebugOverlayData.Empty();
	}
}

void USuspenseEquipmentVisualController::UpdateDebugOverlay()
{
	for (auto It = DebugOverlayData.CreateConstIterator(); It; ++It)
	{
		AActor* const Actor = It.Key();
		if (IsValid(Actor))
		{
			DrawDebugInfoForActor(Actor, It.Value());
		}
	}
}

void USuspenseEquipmentVisualController::DrawDebugInfoForActor(AActor* Actor, const FString& Info) const
{
	if (!GetWorld() || !Actor) return;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			static_cast<int32>(Actor->GetUniqueID()),
			ControllerConfig.DebugOverlayUpdateRate,
			FColor::Green,
			FString::Printf(TEXT("%s:%s"), *Actor->GetName(), *Info),
			true,
			FVector2D(1.2f, 1.2f));
	}
	DrawDebugSphere(GetWorld(), Actor->GetActorLocation(), 20.0f, 8, FColor::Yellow, false, ControllerConfig.DebugOverlayUpdateRate);
}

void USuspenseEquipmentVisualController::LogVisualMetrics() const
{
	UE_LOG(LogTemp, Log, TEXT("[VisualController] Metrics: Effects +%d/-%d, Materials %d, Transitions %d, PoolHit %.2f%%"),
		TotalEffectsApplied, TotalEffectsRemoved, TotalMaterialsCreated, TotalTransitionsStarted,
		(EffectPoolHits + EffectPoolMisses) > 0 ? (float)EffectPoolHits / (EffectPoolMisses + EffectPoolHits) * 100.0f : 0.0f);
}

void USuspenseEquipmentVisualController::SetControllerConfiguration(const FVisualControllerConfig& NewConfig)
{
	ControllerConfig = NewConfig;

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PoolCleanupTimerHandle);
		if (ControllerConfig.PoolCleanupInterval > 0.0f)
		{
			GetWorld()->GetTimerManager().SetTimer(
				PoolCleanupTimerHandle, this, &USuspenseEquipmentVisualController::CleanupEffectPool,
				ControllerConfig.PoolCleanupInterval, true);
		}
	}
}

//==================== Effect throttling ====================

bool USuspenseEquipmentVisualController::ShouldPlayEffect(AActor* Equipment, const FGameplayTag& EffectTag, float& OutCooldownMs) const
{
	OutCooldownMs = 0.0f;
	if (!Equipment) return false;

	// Выберем окно троттлинга
	float Ms = ControllerConfig.DefaultEffectThrottleMs;
	if (const float* OverrideMs = ControllerConfig.PerTagEffectThrottleMs.Find(EffectTag))
	{
		Ms = *OverrideMs;
	}
	OutCooldownMs = Ms;
	if (Ms <= 0.0f) return true;

	const double Now = FPlatformTime::Seconds();
	const double WindowSec = Ms / 1000.0;

	// Проверим последнюю отметку
	const TWeakObjectPtr<AActor> Key = Equipment;
	if (const TMap<FGameplayTag, double>* ForActor = LastEffectTimeByActor.Find(Key))
	{
		if (const double* Last = ForActor->Find(EffectTag))
		{
			return (Now - *Last) >= WindowSec;
		}
	}
	return true;
}

void USuspenseEquipmentVisualController::MarkEffectPlayed(AActor* Equipment, const FGameplayTag& EffectTag)
{
	const TWeakObjectPtr<AActor> Key = Equipment;
	TMap<FGameplayTag, double>& Map = LastEffectTimeByActor.FindOrAdd(Key);
	Map.Add(EffectTag, FPlatformTime::Seconds());
}
