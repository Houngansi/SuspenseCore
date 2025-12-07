// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/Presentation/SuspenseCoreEquipmentAttachmentSystem.h"
#include "SuspenseCore/Components/SkeletalMeshComponent.h"
#include "SuspenseCore/Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "SuspenseCore/Components/Coordination/SuspenseCoreEquipmentEventDispatcher.h"
#include "Core/Services/SuspenseCoreEquipmentServiceLocator.h"
#include "SuspenseCore/Services/SuspenseCoreEquipmentServiceMacros.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"

USuspenseCoreEquipmentAttachmentSystem::USuspenseCoreEquipmentAttachmentSystem()
	: SocketConfigCache(50)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // Only tick during transitions
}

void USuspenseCoreEquipmentAttachmentSystem::BeginPlay()
{
	Super::BeginPlay();

	USuspenseCoreEquipmentServiceLocator* Locator = USuspenseCoreEquipmentServiceLocator::Get(this);
	if (Locator)
	{
		const FGameplayTag AttachmentTag = FGameplayTag::RequestGameplayTag(TEXT("Service.AttachmentSystem"));

		if (!Locator->IsServiceRegistered(AttachmentTag))
		{
			Locator->RegisterServiceInstance(AttachmentTag, this);

			UE_LOG(LogEquipmentOperation, Log,
				TEXT("✓ AttachmentSystem registered as service: Service.AttachmentSystem"));
		}
	}
	else
	{
		UE_LOG(LogEquipmentOperation, Error,
			TEXT("❌ Failed to get ServiceLocator - AttachmentSystem NOT registered!"));
	}

	if (SocketMappings.Num() == 0)
	{
		InitializeDefaultMappings();
	}

	for (const FSocketMappingConfig& Mapping : SocketMappings)
	{
		SocketConfigCache.Set(Mapping.ItemType, Mapping, /*TTL*/0.0f);
	}

	LogAttachmentOperation(TEXT("BeginPlay"),
		FString::Printf(TEXT("Initialized with %d socket mappings"), SocketMappings.Num()));
}

void USuspenseCoreEquipmentAttachmentSystem::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	USuspenseCoreEquipmentServiceLocator* Locator = USuspenseCoreEquipmentServiceLocator::Get(this);
	if (Locator)
	{
		const FGameplayTag AttachmentTag = FGameplayTag::RequestGameplayTag(TEXT("Service.AttachmentSystem"));

		if (Locator->IsServiceRegistered(AttachmentTag))
		{
			Locator->UnregisterService(AttachmentTag, /*bForceShutdown=*/false);

			UE_LOG(LogEquipmentOperation, Log,
				TEXT("AttachmentSystem unregistered from ServiceLocator"));
		}
	}

	TArray<AActor*> AttachedActors;
	AttachmentStates.GetKeys(AttachedActors);

	for (AActor* Equipment : AttachedActors)
	{
		DetachEquipment(Equipment, true);
	}

	AttachmentStates.Empty();
	OccupiedSockets.Empty();
	SocketConfigCache.Clear();

	Super::EndPlay(EndPlayReason);
}

void USuspenseCoreEquipmentAttachmentSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ActiveTransitionCount > 0)
	{
		UpdateTransitions(DeltaTime);
	}
}

bool USuspenseCoreEquipmentAttachmentSystem::AttachEquipment(
	AActor* Equipment,
	USceneComponent* Target,
	const FEquipmentAttachmentConfig& Config)
{
	if (!IsValid(Equipment) || !IsValid(Target))
	{
		LogAttachmentOperation(TEXT("AttachEquipment"), TEXT("Invalid equipment or target"));
		return false;
	}

	if (SystemConfig.bValidateSockets && !ValidateSocket(Target, Config.SocketName))
	{
		LogAttachmentOperation(TEXT("AttachEquipment"),
			FString::Printf(TEXT("Socket '%s' not found on target"), *Config.SocketName.ToString()));
		return false;
	}

	if (!SystemConfig.bAllowSocketSharing)
	{
		FEquipmentRWGuard Rw(SocketOccupationLock, ELockType::Read);
		if (AActor** OccupyingActor = OccupiedSockets.Find(Config.SocketName))
		{
			if (*OccupyingActor != Equipment)
			{
				LogAttachmentOperation(TEXT("AttachEquipment"),
					FString::Printf(TEXT("Socket '%s' already occupied by %s"),
						*Config.SocketName.ToString(),
						*(*OccupyingActor)->GetName()));
				return false;
			}
		}
	}

	const bool bSuccess = InternalAttach(
		Equipment,
		Target,
		Config.SocketName,
		Config.RelativeTransform,
		Config.LocationRule,
		Config.RotationRule,
		Config.ScaleRule,
		Config.bWeldSimulatedBodies
	);

	if (bSuccess)
	{
		FEquipmentRWGuard Rw(AttachmentStateLock, ELockType::Write);

		FAttachmentStateData& StateData = AttachmentStates.FindOrAdd(Equipment);
		StateData.CurrentState.bIsAttached = true;
		StateData.CurrentState.AttachedTo = Target;
		StateData.CurrentState.CurrentSocket = Config.SocketName;
		StateData.CurrentState.CurrentOffset = Config.RelativeTransform;
		StateData.CurrentState.bIsActive = true;
		StateData.bIsTransitioning = false;

		MarkSocketOccupied(Config.SocketName, Equipment);

		FSuspenseCoreEquipmentEventData Ev;
		Ev.EventType   = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Attachment.Changed"));
		Ev.Source      = this;
		Ev.Target      = Equipment;
		Ev.Timestamp   = FPlatformTime::Seconds();
		Ev.NumericData = 1.0f; // attached
		Ev.AddMetadata(TEXT("Socket"), Config.SocketName.ToString());
		FSuspenseCoreEquipmentEventBus::Get()->Broadcast(Ev);

		LogAttachmentOperation(TEXT("AttachEquipment"),
			FString::Printf(TEXT("Attached %s to '%s'"),
				*Equipment->GetName(),
				*Config.SocketName.ToString()));
	}

	return bSuccess;
}

bool USuspenseCoreEquipmentAttachmentSystem::DetachEquipment(AActor* Equipment, bool bMaintainWorldTransform)
{
	if (!IsValid(Equipment))
	{
		return false;
	}

	FEquipmentRWGuard Rw(AttachmentStateLock, ELockType::Write);

	FAttachmentStateData* StateData = AttachmentStates.Find(Equipment);
	if (!StateData || !StateData->CurrentState.bIsAttached)
	{
		return false;
	}

	USceneComponent* RootComp = GetEquipmentRootComponent(Equipment);
	if (!RootComp)
	{
		return false;
	}

	ClearSocketOccupation(StateData->CurrentState.CurrentSocket);

	FDetachmentTransformRules DetachRules(
		bMaintainWorldTransform ? EDetachmentRule::KeepWorld : EDetachmentRule::KeepRelative,
		bMaintainWorldTransform ? EDetachmentRule::KeepWorld : EDetachmentRule::KeepRelative,
		bMaintainWorldTransform ? EDetachmentRule::KeepWorld : EDetachmentRule::KeepRelative,
		false
	);

	RootComp->DetachFromComponent(DetachRules);

	const FName OldSocket = StateData->CurrentState.CurrentSocket;
	StateData->CurrentState.bIsAttached = false;
	StateData->CurrentState.AttachedTo = nullptr;
	StateData->CurrentState.CurrentSocket = NAME_None;

	AttachmentStates.Remove(Equipment);

	FSuspenseCoreEquipmentEventData Ev;
	Ev.EventType   = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Attachment.Changed"));
	Ev.Source      = this;
	Ev.Target      = Equipment;
	Ev.Timestamp   = FPlatformTime::Seconds();
	Ev.NumericData = 0.0f;
	Ev.AddMetadata(TEXT("Socket"), OldSocket.ToString());
	FSuspenseCoreEquipmentEventBus::Get()->Broadcast(Ev);

	LogAttachmentOperation(TEXT("DetachEquipment"),
		FString::Printf(TEXT("Detached %s"), *Equipment->GetName()));

	return true;
}

bool USuspenseCoreEquipmentAttachmentSystem::UpdateAttachment(
	AActor* Equipment,
	const FEquipmentAttachmentConfig& NewConfig,
	bool bSmooth)
{
	if (!IsValid(Equipment))
	{
		return false;
	}

	FEquipmentRWGuard Rw(AttachmentStateLock, ELockType::Write);

	FAttachmentStateData* StateData = AttachmentStates.Find(Equipment);
	if (!StateData || !StateData->CurrentState.bIsAttached)
	{
		return false;
	}

	if (bSmooth && SystemConfig.bEnableSmoothTransitions)
	{
		StateData->bIsTransitioning   = true;
		StateData->TransitionStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		StateData->TransitionDuration  = SystemConfig.DefaultTransitionDuration;
		StateData->StartTransform      = StateData->CurrentState.CurrentOffset;
		StateData->TargetTransform     = NewConfig.RelativeTransform;
		StateData->PreviousSocket      = StateData->CurrentState.CurrentSocket;

		ActiveTransitionCount++;
		SetComponentTickEnabled(true);

		LogAttachmentOperation(TEXT("UpdateAttachment"),
			FString::Printf(TEXT("Smooth transition for %s"), *Equipment->GetName()));
	}
	else
	{
		USceneComponent* Target = StateData->CurrentState.AttachedTo;

		ClearSocketOccupation(StateData->CurrentState.CurrentSocket);

		const bool bSuccess = InternalAttach(
			Equipment,
			Target,
			NewConfig.SocketName,
			NewConfig.RelativeTransform,
			NewConfig.LocationRule,
			NewConfig.RotationRule,
			NewConfig.ScaleRule,
			NewConfig.bWeldSimulatedBodies
		);

		if (bSuccess)
		{
			StateData->CurrentState.CurrentSocket = NewConfig.SocketName;
			StateData->CurrentState.CurrentOffset = NewConfig.RelativeTransform;

			MarkSocketOccupied(NewConfig.SocketName, Equipment);

			FSuspenseCoreEquipmentEventData Ev;
			Ev.EventType   = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Attachment.Changed"));
			Ev.Source      = this;
			Ev.Target      = Equipment;
			Ev.Timestamp   = FPlatformTime::Seconds();
			Ev.NumericData = 1.0f;
			Ev.AddMetadata(TEXT("Socket"), NewConfig.SocketName.ToString());
			FSuspenseCoreEquipmentEventBus::Get()->Broadcast(Ev);

			LogAttachmentOperation(TEXT("UpdateAttachment"),
				FString::Printf(TEXT("Immediate update for %s"), *Equipment->GetName()));
		}

		return bSuccess;
	}

	return true;
}

FEquipmentAttachmentState USuspenseCoreEquipmentAttachmentSystem::GetAttachmentState(AActor* Equipment) const
{
	FEquipmentRWGuard Rw(AttachmentStateLock, ELockType::Read);

	if (const FAttachmentStateData* StateData = AttachmentStates.Find(Equipment))
	{
		return StateData->CurrentState;
	}
	return FEquipmentAttachmentState();
}

FName USuspenseCoreEquipmentAttachmentSystem::FindBestSocket(
	USkeletalMeshComponent* Target,
	const FGameplayTag& ItemType,
	bool bActiveState) const
{
	if (!Target)
	{
		return NAME_None;
	}

	const FSocketMappingConfig* Mapping = FindSocketMapping(ItemType);
	if (Mapping)
	{
		const FName SocketName = bActiveState ? Mapping->ActiveSocket : Mapping->InactiveSocket;
		if (Target->DoesSocketExist(SocketName))
		{
			return SocketName;
		}
	}

	const FString ItemTypeStr = ItemType.ToString();

	if (ItemTypeStr.Contains(TEXT("Weapon.Rifle")))
	{
		return bActiveState ? TEXT("GripPoint") : TEXT("WeaponBackSocket");
	}
	if (ItemTypeStr.Contains(TEXT("Weapon.Pistol")))
	{
		return bActiveState ? TEXT("GripPoint") : TEXT("HolsterSocket");
	}
	if (ItemTypeStr.Contains(TEXT("Weapon.Melee")))
	{
		return bActiveState ? TEXT("GripPoint") : TEXT("MeleeSocket");
	}

	return NAME_None;
}

bool USuspenseCoreEquipmentAttachmentSystem::SwitchAttachmentState(
	AActor* Equipment,
	bool bMakeActive,
	float /*Duration*/)
{
	if (!IsValid(Equipment))
	{
		return false;
	}

	FEquipmentRWGuard Rw(AttachmentStateLock, ELockType::Write);

	FAttachmentStateData* StateData = AttachmentStates.Find(Equipment);
	if (!StateData || !StateData->CurrentState.bIsAttached)
	{
		return false;
	}

	if (StateData->CurrentState.bIsActive == bMakeActive)
	{
		return true;
	}

	StateData->CurrentState.bIsActive = bMakeActive;

	FSuspenseCoreEquipmentEventData Ev;
	Ev.EventType   = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Attachment.StateChanged"));
	Ev.Source      = this;
	Ev.Target      = Equipment;
	Ev.Timestamp   = FPlatformTime::Seconds();
	Ev.NumericData = bMakeActive ? 1.0f : 0.0f;
	Ev.AddMetadata(TEXT("IsActive"), bMakeActive ? TEXT("1") : TEXT("0"));
	FSuspenseCoreEquipmentEventBus::Get()->Broadcast(Ev);

	LogAttachmentOperation(TEXT("SwitchAttachmentState"),
		FString::Printf(TEXT("Switched %s to %s"),
			*Equipment->GetName(), bMakeActive ? TEXT("active") : TEXT("inactive")));

	return true;
}

FEquipmentAttachmentConfig USuspenseCoreEquipmentAttachmentSystem::GetSlotAttachmentConfig(
	int32 /*SlotIndex*/,
	bool /*bIsActive*/) const
{
	FEquipmentAttachmentConfig Config;
	Config.LocationRule = EAttachmentRule::SnapToTarget;
	Config.RotationRule = EAttachmentRule::SnapToTarget;
	Config.ScaleRule    = EAttachmentRule::KeepRelative;
	Config.bWeldSimulatedBodies = true;
	return Config;
}

bool USuspenseCoreEquipmentAttachmentSystem::ValidateSocket(
	USceneComponent* Target,
	const FName& SocketName) const
{
	if (!Target || SocketName.IsNone())
	{
		return false;
	}

	if (USkeletalMeshComponent* SkelMesh = Cast<USkeletalMeshComponent>(Target))
	{
		return SkelMesh->DoesSocketExist(SocketName);
	}
	if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Target))
	{
		return StaticMesh->DoesSocketExist(SocketName);
	}
	return false;
}

USkeletalMeshComponent* USuspenseCoreEquipmentAttachmentSystem::FindPrimarySkelMesh(AActor* Character) const
{
	if (ACharacter* C = Cast<ACharacter>(Character))
	{
		if (USkeletalMeshComponent* Mesh = C->GetMesh())
		{
			if (Mesh->IsRegistered()) return Mesh;
		}
	}

	TArray<USkeletalMeshComponent*> Skels;
	Character->GetComponents(Skels);
	for (USkeletalMeshComponent* S : Skels)
	{
		if (S && S->IsRegistered()) return S;
	}
	return nullptr;
}

USceneComponent* USuspenseCoreEquipmentAttachmentSystem::GetAttachmentTarget(AActor* Character) const
{
	if (!Character) return nullptr;
	if (USkeletalMeshComponent* Skel = FindPrimarySkelMesh(Character)) return Skel;
	return Character->GetRootComponent();
}

bool USuspenseCoreEquipmentAttachmentSystem::AttachToCharacter(AActor* Equipment, AActor* TargetCharacter,
                                                         FName Socket, FTransform Offset,
                                                         bool /*bSmooth*/, float /*BlendTime*/)
{
	if (!Equipment || !TargetCharacter) return false;

	USceneComponent* Target = GetAttachmentTarget(TargetCharacter);
	if (!Target) return false;

	// Автоподбор сокета, если не задан
	if (Socket.IsNone())
	{
		if (USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Target))
		{
			const FName Best = FindBestSocket(Skel, /*ItemType*/FGameplayTag(), /*bIsActive*/true);
			if (!Best.IsNone()) { Socket = Best; }
		}
	}

	// Базовый конфиг + оверрайды (используем только поля интерфейса)
	FEquipmentAttachmentConfig Cfg = GetSlotAttachmentConfig(/*SlotIndex*/0, /*bIsActive*/true);
	Cfg.SocketName        = Socket;
	Cfg.RelativeTransform = Offset;

	// Валидация
	if (SystemConfig.bValidateSockets && !ValidateSocket(Target, Cfg.SocketName))
	{
		UE_LOG(LogEquipmentOperation, Warning, TEXT("[AttachToCharacter] Invalid socket '%s' on %s"),
			*Cfg.SocketName.ToString(), *GetNameSafe(Target));
		Cfg.SocketName = NAME_None;
	}

	return AttachEquipment(Equipment, Target, Cfg);
}

bool USuspenseCoreEquipmentAttachmentSystem::DetachFromCharacter(AActor* Equipment, AActor* /*TargetCharacter*/, bool bSmooth)
{
	return DetachEquipment(Equipment, /*bMaintainWorldTransform=*/bSmooth);
}

bool USuspenseCoreEquipmentAttachmentSystem::InternalAttach(
	AActor* Equipment,
	USceneComponent* Target,
	const FName& SocketName,
	const FTransform& RelativeTransform,
	EAttachmentRule LocationRule,
	EAttachmentRule RotationRule,
	EAttachmentRule ScaleRule,
	bool bWeldBodies)
{
	USceneComponent* RootComp = GetEquipmentRootComponent(Equipment);
	if (!RootComp)
	{
		return false;
	}

	FAttachmentTransformRules AttachRules(
		LocationRule,
		RotationRule,
		ScaleRule,
		bWeldBodies
	);

	const bool bSuccess = RootComp->AttachToComponent(Target, AttachRules, SocketName);

	if (bSuccess)
	{
		RootComp->SetRelativeTransform(RelativeTransform);
	}

	return bSuccess;
}

void USuspenseCoreEquipmentAttachmentSystem::UpdateTransitions(float DeltaTime)
{
	FEquipmentRWGuard Rw(AttachmentStateLock, ELockType::Write);

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	int32 CompletedTransitions = 0;

	for (auto& StatePair : AttachmentStates)
	{
		FAttachmentStateData& StateData = StatePair.Value;

		if (StateData.bIsTransitioning)
		{
			const float Progress = FMath::Clamp(
				(CurrentTime - StateData.TransitionStartTime) / StateData.TransitionDuration,
				0.0f, 1.0f);

			if (Progress >= 1.0f)
			{
				StateData.CurrentState.CurrentOffset = StateData.TargetTransform;
				StateData.bIsTransitioning = false;
				CompletedTransitions++;

				if (USceneComponent* RootComp = GetEquipmentRootComponent(StatePair.Key))
				{
					RootComp->SetRelativeTransform(StateData.TargetTransform);
				}

				BroadcastAttachmentEvent(
					FGameplayTag::RequestGameplayTag(TEXT("Equipment.Attachment.TransitionComplete")),
					StatePair.Key,
					true
				);
			}
			else
			{
				FTransform InterpolatedTransform;
				InterpolatedTransform.Blend(StateData.StartTransform, StateData.TargetTransform, Progress);

				if (USceneComponent* RootComp = GetEquipmentRootComponent(StatePair.Key))
				{
					RootComp->SetRelativeTransform(InterpolatedTransform);
				}
			}
		}
	}

	ActiveTransitionCount -= CompletedTransitions;

	if (ActiveTransitionCount <= 0)
	{
		ActiveTransitionCount = 0;
		SetComponentTickEnabled(false);
	}
}

const FSocketMappingConfig* USuspenseCoreEquipmentAttachmentSystem::FindSocketMapping(const FGameplayTag& ItemType) const
{
	FSocketMappingConfig Cached;
	if (SocketConfigCache.Get(ItemType, Cached))
	{
		static FSocketMappingConfig Snapshot;
		Snapshot = Cached;
		return &Snapshot;
	}

	const FSocketMappingConfig* Best = nullptr;
	int32 BestPriority = -1;

	for (const FSocketMappingConfig& Mapping : SocketMappings)
	{
		if (Mapping.ItemType.MatchesTag(ItemType) && Mapping.Priority > BestPriority)
		{
			Best = &Mapping;
			BestPriority = Mapping.Priority;
		}
	}

	if (Best)
	{
		SocketConfigCache.Set(ItemType, *Best, /*TTL*/0.0f);
		static FSocketMappingConfig Snapshot;
		Snapshot = *Best;
		return &Snapshot;
	}

	return nullptr;
}

void USuspenseCoreEquipmentAttachmentSystem::MarkSocketOccupied(const FName& SocketName, AActor* Equipment)
{
	FEquipmentRWGuard Rw(SocketOccupationLock, ELockType::Write);
	OccupiedSockets.Add(SocketName, Equipment);
}

void USuspenseCoreEquipmentAttachmentSystem::ClearSocketOccupation(const FName& SocketName)
{
	FEquipmentRWGuard Rw(SocketOccupationLock, ELockType::Write);
	OccupiedSockets.Remove(SocketName);
}

USceneComponent* USuspenseCoreEquipmentAttachmentSystem::GetEquipmentRootComponent(AActor* Equipment) const
{
	return Equipment ? Equipment->GetRootComponent() : nullptr;
}

void USuspenseCoreEquipmentAttachmentSystem::InitializeDefaultMappings()
{
	{
		FSocketMappingConfig RifleMapping;
		RifleMapping.ItemType = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Rifle"));
		RifleMapping.ActiveSocket = TEXT("GripPoint");
		RifleMapping.InactiveSocket = TEXT("WeaponBackSocket");
		RifleMapping.Priority = 10;
		SocketMappings.Add(RifleMapping);
	}

	{
		FSocketMappingConfig PistolMapping;
		PistolMapping.ItemType = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Pistol"));
		PistolMapping.ActiveSocket = TEXT("GripPoint");
		PistolMapping.InactiveSocket = TEXT("HolsterSocket");
		PistolMapping.Priority = 10;
		SocketMappings.Add(PistolMapping);
	}

	{
		FSocketMappingConfig MeleeMapping;
		MeleeMapping.ItemType = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon.Melee"));
		MeleeMapping.ActiveSocket = TEXT("GripPoint");
		MeleeMapping.InactiveSocket = TEXT("MeleeSocket");
		MeleeMapping.Priority = 10;
		SocketMappings.Add(MeleeMapping);
	}
}

void USuspenseCoreEquipmentAttachmentSystem::BroadcastAttachmentEvent(const FGameplayTag& EventTag, AActor* Equipment, bool bSuccess)
{
	FSuspenseCoreEquipmentEventData Ev;
	Ev.EventType   = EventTag;
	Ev.Source      = this;
	Ev.Target      = Equipment;
	Ev.Timestamp   = FPlatformTime::Seconds();
	Ev.NumericData = bSuccess ? 1.0f : 0.0f;
	FSuspenseCoreEquipmentEventBus::Get()->Broadcast(Ev);
}

void USuspenseCoreEquipmentAttachmentSystem::LogAttachmentOperation(const FString& Operation, const FString& Details) const
{
	UE_LOG(LogEquipmentOperation, Verbose, TEXT("[AttachmentSystem] %s: %s"), *Operation, *Details);
}

void USuspenseCoreEquipmentAttachmentSystem::RemoveSocketMapping(const FGameplayTag& ItemType)
{
	for (int32 i = SocketMappings.Num()-1; i >= 0; --i)
	{
		if (SocketMappings[i].ItemType.MatchesTag(ItemType))
		{
			SocketMappings.RemoveAt(i);
		}
	}
}

TArray<AActor*> USuspenseCoreEquipmentAttachmentSystem::GetAllAttachedEquipment() const
{
	TArray<AActor*> Out;
	FEquipmentRWGuard Rw(AttachmentStateLock, ELockType::Read);
	AttachmentStates.GetKeys(Out);
	return Out;
}

bool USuspenseCoreEquipmentAttachmentSystem::IsEquipmentAttached(AActor* Equipment) const
{
	FEquipmentRWGuard Rw(AttachmentStateLock, ELockType::Read);
	if (const FAttachmentStateData* State = AttachmentStates.Find(Equipment))
	{
		return State->CurrentState.bIsAttached;
	}
	return false;
}

void USuspenseCoreEquipmentAttachmentSystem::ForceUpdateAllAttachments()
{
	FEquipmentRWGuard Rw(AttachmentStateLock, ELockType::Read);
	for (const auto& Pair : AttachmentStates)
	{
		if (USceneComponent* RootComp = GetEquipmentRootComponent(Pair.Key))
		{
			RootComp->SetRelativeTransform(Pair.Value.CurrentState.CurrentOffset);
		}
	}
}

FString USuspenseCoreEquipmentAttachmentSystem::GetAttachmentStatistics() const
{
	FEquipmentRWGuard Rw(AttachmentStateLock, ELockType::Read);
	return FString::Printf(TEXT("Attached=%d, Transitions=%d"),
		AttachmentStates.Num(), ActiveTransitionCount);
}
