// SuspenseCorePlayerState.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Core/SuspenseCorePlayerState.h"
#include "SuspenseCore/Components/SuspenseCoreAbilitySystemComponent.h"
#include "SuspenseCore/Components/SuspenseCoreInventoryComponent.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Subsystems/SuspenseCoreCharacterClassSubsystem.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

// Equipment module includes
#include "Components/Core/SuspenseEquipmentDataStore.h"
#include "Components/Transaction/SuspenseEquipmentTransactionProcessor.h"
#include "Components/Core/SuspenseEquipmentOperationExecutor.h"
#include "Components/Network/SuspenseEquipmentPredictionSystem.h"
#include "Components/Network/SuspenseEquipmentReplicationManager.h"
#include "Components/Network/SuspenseEquipmentNetworkDispatcher.h"
#include "Components/Coordination/SuspenseEquipmentEventDispatcher.h"
#include "Components/Core/SuspenseWeaponStateManager.h"
#include "Components/Core/SuspenseEquipmentInventoryBridge.h"
#include "Components/Validation/SuspenseEquipmentSlotValidator.h"
#include "Types/Loadout/SuspenseLoadoutManager.h"

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════════

ASuspenseCorePlayerState::ASuspenseCorePlayerState()
{
	// Create ASC - lives on PlayerState for persistence across respawns
	AbilitySystemComponent = CreateDefaultSubobject<USuspenseCoreAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// Mixed replication mode - server controls gameplay, client predicts
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	// Create Inventory Component - lives on PlayerState for persistence across respawns
	// Uses EventBus for all notifications (SuspenseCore.Event.Inventory.*)
	InventoryComponent = CreateDefaultSubobject<USuspenseCoreInventoryComponent>(TEXT("InventoryComponent"));
	InventoryComponent->SetIsReplicated(true);

	// ═══════════════════════════════════════════════════════════════════════════════
	// EQUIPMENT MODULE COMPONENTS
	// All equipment components live on PlayerState for persistence across respawns
	// They integrate with the LoadoutManager and work with the InventoryComponent
	// ═══════════════════════════════════════════════════════════════════════════════

	// Core data store - server authoritative source of truth for equipment state
	EquipmentDataStore = CreateDefaultSubobject<USuspenseEquipmentDataStore>(TEXT("EquipmentDataStore"));
	EquipmentDataStore->SetIsReplicated(true);

	// Transaction processor - handles atomic equipment operations (equip/unequip/swap)
	EquipmentTxnProcessor = CreateDefaultSubobject<USuspenseEquipmentTransactionProcessor>(TEXT("EquipmentTxnProcessor"));
	EquipmentTxnProcessor->SetIsReplicated(true);

	// Operation executor - validates and executes equipment operations
	EquipmentOps = CreateDefaultSubobject<USuspenseEquipmentOperationExecutor>(TEXT("EquipmentOps"));
	EquipmentOps->SetIsReplicated(true);

	// Client prediction system - handles optimistic updates for responsive UI
	EquipmentPrediction = CreateDefaultSubobject<USuspenseEquipmentPredictionSystem>(TEXT("EquipmentPrediction"));
	EquipmentPrediction->SetIsReplicated(true);

	// Replication manager - delta-based replication for bandwidth efficiency
	EquipmentReplication = CreateDefaultSubobject<USuspenseEquipmentReplicationManager>(TEXT("EquipmentReplication"));
	EquipmentReplication->SetIsReplicated(true);

	// Network dispatcher - RPC queue and request management
	EquipmentNetworkDispatcher = CreateDefaultSubobject<USuspenseEquipmentNetworkDispatcher>(TEXT("EquipmentNetworkDispatcher"));
	EquipmentNetworkDispatcher->SetIsReplicated(true);

	// Event dispatcher - local event bus for equipment events
	EquipmentEventDispatcher = CreateDefaultSubobject<USuspenseEquipmentEventDispatcher>(TEXT("EquipmentEventDispatcher"));
	EquipmentEventDispatcher->SetIsReplicated(true);

	// Weapon state manager - finite state machine for weapon states (idle, firing, reloading, etc.)
	WeaponStateManager = CreateDefaultSubobject<USuspenseWeaponStateManager>(TEXT("WeaponStateManager"));
	WeaponStateManager->SetIsReplicated(true);

	// Inventory bridge - connects equipment system to inventory component
	EquipmentInventoryBridge = CreateDefaultSubobject<USuspenseEquipmentInventoryBridge>(TEXT("EquipmentInventoryBridge"));
	EquipmentInventoryBridge->SetIsReplicated(true);

	// Network settings - optimized for MMO scale
	// 60Hz is optimal balance between responsiveness and bandwidth for shooters
	// Can be reduced to 30Hz for larger player counts or increased for competitive modes
	SetNetUpdateFrequency(60.0f);
	SetMinNetUpdateFrequency(30.0f);  // Adaptive: reduces when player is idle/not relevant
}

// ═══════════════════════════════════════════════════════════════════════════════
// ACTOR LIFECYCLE
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerState::BeginPlay()
{
	Super::BeginPlay();

	// Initialize on server or standalone
	if (HasAuthority())
	{
		InitializeAbilitySystem();
		InitializeInventoryFromLoadout();
		InitializeEquipmentComponents();
	}
}

void ASuspenseCorePlayerState::InitializeInventoryFromLoadout()
{
	if (!InventoryComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCorePlayerState::InitializeInventoryFromLoadout: InventoryComponent is null"));
		return;
	}

	if (DefaultLoadoutID.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCorePlayerState::InitializeInventoryFromLoadout: DefaultLoadoutID not set, using component defaults"));
		return;
	}

	// Initialize inventory from loadout DataTable
	// This reads InventoryWidth, InventoryHeight, MaxWeight from FSuspenseCoreTemplateLoadout
	if (InventoryComponent->InitializeFromLoadout(DefaultLoadoutID))
	{
		UE_LOG(LogTemp, Log, TEXT("SuspenseCorePlayerState: Inventory initialized from loadout '%s'"), *DefaultLoadoutID.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCorePlayerState: Failed to initialize inventory from loadout '%s'"), *DefaultLoadoutID.ToString());
	}
}

void ASuspenseCorePlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear equipment wiring retry timer
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(EquipmentWireRetryHandle);
	}

	CleanupAttributeCallbacks();

	// Publish player left event
	PublishPlayerStateEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.Left")),
		FString::Printf(TEXT("{\"playerId\":\"%s\"}"), *GetPlayerName())
	);

	Super::EndPlay(EndPlayReason);
}

void ASuspenseCorePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Core Components
	DOREPLIFETIME(ASuspenseCorePlayerState, AbilitySystemComponent);
	DOREPLIFETIME(ASuspenseCorePlayerState, InventoryComponent);

	// Equipment Module Components
	DOREPLIFETIME(ASuspenseCorePlayerState, EquipmentDataStore);
	DOREPLIFETIME(ASuspenseCorePlayerState, EquipmentTxnProcessor);
	DOREPLIFETIME(ASuspenseCorePlayerState, EquipmentOps);
	DOREPLIFETIME(ASuspenseCorePlayerState, EquipmentPrediction);
	DOREPLIFETIME(ASuspenseCorePlayerState, EquipmentReplication);
	DOREPLIFETIME(ASuspenseCorePlayerState, EquipmentNetworkDispatcher);
	DOREPLIFETIME(ASuspenseCorePlayerState, EquipmentEventDispatcher);
	DOREPLIFETIME(ASuspenseCorePlayerState, WeaponStateManager);
	DOREPLIFETIME(ASuspenseCorePlayerState, EquipmentInventoryBridge);

	// State
	DOREPLIFETIME_CONDITION_NOTIFY(ASuspenseCorePlayerState, PlayerLevel, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ASuspenseCorePlayerState, TeamId, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(ASuspenseCorePlayerState, CharacterClassId, COND_None, REPNOTIFY_Always);
}

// ═══════════════════════════════════════════════════════════════════════════════
// IABILITYSYSTEMINTERFACE
// ═══════════════════════════════════════════════════════════════════════════════

UAbilitySystemComponent* ASuspenseCorePlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API - GAS
// ═══════════════════════════════════════════════════════════════════════════════

bool ASuspenseCorePlayerState::GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 Level)
{
	if (!AbilitySystemComponent || !AbilityClass)
	{
		return false;
	}

	if (!HasAuthority())
	{
		return false;
	}

	AbilitySystemComponent->GiveAbilityOfClass(AbilityClass, Level);
	return true;
}

bool ASuspenseCorePlayerState::RemoveAbility(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilitySystemComponent || !AbilityClass)
	{
		return false;
	}

	if (!HasAuthority())
	{
		return false;
	}

	AbilitySystemComponent->RemoveAbilitiesOfClass(AbilityClass);
	return true;
}

bool ASuspenseCorePlayerState::ApplyEffect(TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
	if (!AbilitySystemComponent || !EffectClass)
	{
		return false;
	}

	FActiveGameplayEffectHandle Handle = AbilitySystemComponent->ApplyEffectToSelf(EffectClass, Level);
	return Handle.IsValid();
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API - STATE
// ═══════════════════════════════════════════════════════════════════════════════

bool ASuspenseCorePlayerState::IsAlive() const
{
	return GetHealth() > 0.0f;
}

void ASuspenseCorePlayerState::SetPlayerLevel(int32 NewLevel)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 OldLevel = PlayerLevel;
	PlayerLevel = FMath::Max(1, NewLevel);

	if (OldLevel != PlayerLevel)
	{
		PublishPlayerStateEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.LevelChanged")),
			FString::Printf(TEXT("{\"oldLevel\":%d,\"newLevel\":%d}"), OldLevel, PlayerLevel)
		);
	}
}

void ASuspenseCorePlayerState::SetTeamId(int32 NewTeamId)
{
	if (!HasAuthority())
	{
		return;
	}

	const int32 OldTeamId = TeamId;
	TeamId = NewTeamId;

	if (OldTeamId != TeamId)
	{
		PublishPlayerStateEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.TeamChanged")),
			FString::Printf(TEXT("{\"oldTeam\":%d,\"newTeam\":%d}"), OldTeamId, TeamId)
		);
	}
}

bool ASuspenseCorePlayerState::ApplyCharacterClass(FName ClassId)
{
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCorePlayerState: ApplyCharacterClass called on non-authority"));
		return false;
	}

	if (ClassId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCorePlayerState: ApplyCharacterClass called with empty ClassId"));
		return false;
	}

	// Get the CharacterClassSubsystem
	USuspenseCoreCharacterClassSubsystem* ClassSubsystem = USuspenseCoreCharacterClassSubsystem::Get(this);
	if (!ClassSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCorePlayerState: CharacterClassSubsystem not available"));
		return false;
	}

	// Apply the class to this actor (PlayerState implements IAbilitySystemInterface)
	bool bSuccess = ClassSubsystem->ApplyClassToActor(this, ClassId, PlayerLevel);

	if (bSuccess)
	{
		CharacterClassId = ClassId;

		UE_LOG(LogTemp, Log, TEXT("SuspenseCorePlayerState: Applied class '%s' to player %s"),
			*ClassId.ToString(), *GetPlayerName());

		// Publish event via EventBus
		PublishPlayerStateEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.ClassChanged")),
			FString::Printf(TEXT("{\"classId\":\"%s\"}"), *ClassId.ToString())
		);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCorePlayerState: Failed to apply class '%s'"),
			*ClassId.ToString());
	}

	return bSuccess;
}

// ═══════════════════════════════════════════════════════════════════════════════
// PUBLIC API - ATTRIBUTES
// ═══════════════════════════════════════════════════════════════════════════════

float ASuspenseCorePlayerState::GetHealth() const
{
	if (AttributeSet)
	{
		return AttributeSet->GetHealth();
	}
	return 0.0f;
}

float ASuspenseCorePlayerState::GetMaxHealth() const
{
	if (AttributeSet)
	{
		return AttributeSet->GetMaxHealth();
	}
	return 0.0f;
}

float ASuspenseCorePlayerState::GetHealthPercent() const
{
	const float MaxHP = GetMaxHealth();
	if (MaxHP > 0.0f)
	{
		return GetHealth() / MaxHP;
	}
	return 0.0f;
}

float ASuspenseCorePlayerState::GetStamina() const
{
	if (AttributeSet)
	{
		return AttributeSet->GetStamina();
	}
	return 0.0f;
}

float ASuspenseCorePlayerState::GetMaxStamina() const
{
	if (AttributeSet)
	{
		return AttributeSet->GetMaxStamina();
	}
	return 0.0f;
}

float ASuspenseCorePlayerState::GetStaminaPercent() const
{
	const float MaxStam = GetMaxStamina();
	if (MaxStam > 0.0f)
	{
		return GetStamina() / MaxStam;
	}
	return 0.0f;
}

// ═══════════════════════════════════════════════════════════════════════════════
// REPLICATION CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerState::OnRep_PlayerLevel(int32 OldPlayerLevel)
{
	// Broadcast change to clients via EventBus
	if (OldPlayerLevel != PlayerLevel)
	{
		PublishPlayerStateEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.LevelChanged")),
			FString::Printf(TEXT("{\"oldLevel\":%d,\"newLevel\":%d}"), OldPlayerLevel, PlayerLevel)
		);
	}
}

void ASuspenseCorePlayerState::OnRep_TeamId(int32 OldTeamId)
{
	// Broadcast change to clients via EventBus
	if (OldTeamId != TeamId)
	{
		PublishPlayerStateEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.TeamChanged")),
			FString::Printf(TEXT("{\"oldTeam\":%d,\"newTeam\":%d}"), OldTeamId, TeamId)
		);
	}
}

void ASuspenseCorePlayerState::OnRep_CharacterClassId(FName OldClassId)
{
	// Broadcast change to clients via EventBus
	if (OldClassId != CharacterClassId)
	{
		PublishPlayerStateEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.ClassChanged")),
			FString::Printf(TEXT("{\"oldClassId\":\"%s\",\"newClassId\":\"%s\"}"), *OldClassId.ToString(), *CharacterClassId.ToString())
		);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL - INITIALIZATION
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerState::InitializeAbilitySystem()
{
	if (bAbilitySystemInitialized || !AbilitySystemComponent)
	{
		return;
	}

	// Initialize ASC with owner info
	AbilitySystemComponent->InitAbilityActorInfo(this, GetPawn());

	// Create attribute set
	if (AttributeSetClass)
	{
		AttributeSet = NewObject<USuspenseCoreAttributeSet>(this, AttributeSetClass);
		AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet);
	}
	else
	{
		// Use default attribute set
		AttributeSet = NewObject<USuspenseCoreAttributeSet>(this, USuspenseCoreAttributeSet::StaticClass());
		AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet);
	}

	// Setup callbacks before applying effects
	SetupAttributeCallbacks();

	// Apply initial effects
	ApplyInitialEffects();

	// Grant abilities
	GrantStartupAbilities();

	bAbilitySystemInitialized = true;

	// Publish initialization complete
	PublishPlayerStateEvent(
		FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Player.Initialized")),
		FString::Printf(TEXT("{\"playerId\":\"%s\",\"level\":%d}"), *GetPlayerName(), PlayerLevel)
	);
}

void ASuspenseCorePlayerState::GrantStartupAbilities()
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return;
	}

	for (const FSuspenseCoreAbilityEntry& Entry : StartupAbilities)
	{
		if (Entry.AbilityClass)
		{
			AbilitySystemComponent->GiveAbilityOfClass(Entry.AbilityClass, Entry.AbilityLevel);
		}
	}
}

void ASuspenseCorePlayerState::ApplyInitialEffects()
{
	if (!AbilitySystemComponent || !HasAuthority())
	{
		return;
	}

	// Apply initial attributes effect
	if (InitialAttributesEffect)
	{
		AbilitySystemComponent->ApplyEffectToSelf(InitialAttributesEffect, static_cast<float>(PlayerLevel));
	}

	// Apply passive effects
	for (const TSubclassOf<UGameplayEffect>& EffectClass : PassiveEffects)
	{
		if (EffectClass)
		{
			AbilitySystemComponent->ApplyEffectToSelf(EffectClass, static_cast<float>(PlayerLevel));
		}
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL - ATTRIBUTE CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerState::SetupAttributeCallbacks()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	// Health callback
	FDelegateHandle HealthHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		AttributeSet->GetHealthAttribute()
	).AddLambda([this](const FOnAttributeChangeData& Data)
	{
		HandleAttributeChange(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Health")),
			Data.NewValue,
			Data.OldValue
		);
	});
	AttributeCallbackHandles.Add(HealthHandle);

	// Stamina callback
	FDelegateHandle StaminaHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		AttributeSet->GetStaminaAttribute()
	).AddLambda([this](const FOnAttributeChangeData& Data)
	{
		HandleAttributeChange(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Stamina")),
			Data.NewValue,
			Data.OldValue
		);
	});
	AttributeCallbackHandles.Add(StaminaHandle);
}

void ASuspenseCorePlayerState::CleanupAttributeCallbacks()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	// Clear all callback handles
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		AttributeSet->GetHealthAttribute()
	).RemoveAll(this);

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		AttributeSet->GetStaminaAttribute()
	).RemoveAll(this);

	AttributeCallbackHandles.Empty();
}

void ASuspenseCorePlayerState::HandleAttributeChange(const FGameplayTag& AttributeTag, float NewValue, float OldValue)
{
	// Broadcast local delegate
	OnAttributeChanged.Broadcast(AttributeTag, NewValue, OldValue);

	// Publish to EventBus
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
		EventData.SetString(FName("Attribute"), AttributeTag.ToString());
		EventData.SetFloat(FName("NewValue"), NewValue);
		EventData.SetFloat(FName("OldValue"), OldValue);

		EventBus->Publish(FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.GAS.Attribute.Changed")), EventData);
	}
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL - EVENTBUS
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerState::PublishPlayerStateEvent(const FGameplayTag& EventTag, const FString& Payload)
{
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(this);
		if (!Payload.IsEmpty())
		{
			EventData.SetString(FName("Payload"), Payload);
		}
		EventBus->Publish(EventTag, EventData);
	}
}

USuspenseCoreEventBus* ASuspenseCorePlayerState::GetEventBus() const
{
	if (CachedEventBus.IsValid())
	{
		return CachedEventBus.Get();
	}

	// Get from EventManager (CachedEventBus is mutable for const caching)
	if (USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(this))
	{
		USuspenseCoreEventBus* EventBus = Manager->GetEventBus();
		if (EventBus)
		{
			CachedEventBus = EventBus;
		}
		return EventBus;
	}

	return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTERNAL - EQUIPMENT MODULE
// ═══════════════════════════════════════════════════════════════════════════════

void ASuspenseCorePlayerState::InitializeEquipmentComponents()
{
	if (bEquipmentModuleInitialized)
	{
		return;
	}

	// Try to wire equipment module immediately
	// If global services aren't ready yet, this will start the retry mechanism
	if (!TryWireEquipmentModuleOnce())
	{
		UE_LOG(LogTemp, Log, TEXT("SuspenseCorePlayerState: Equipment wiring deferred, starting retry timer"));
	}
}

bool ASuspenseCorePlayerState::TryWireEquipmentModuleOnce()
{
	// Attempt to wire
	if (WireEquipmentModule(nullptr, NAME_None))
	{
		bEquipmentModuleInitialized = true;
		EquipmentWireRetryCount = 0;

		// Clear retry timer if running
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(EquipmentWireRetryHandle);
		}

		UE_LOG(LogTemp, Log, TEXT("SuspenseCorePlayerState: Equipment module wired successfully"));

		// Publish equipment initialized event
		PublishPlayerStateEvent(
			FGameplayTag::RequestGameplayTag(FName("SuspenseCore.Event.Equipment.Initialized")),
			TEXT("{}")
		);

		return true;
	}

	// Wiring failed, schedule retry
	EquipmentWireRetryCount++;

	if (EquipmentWireRetryCount >= MaxEquipmentWireRetries)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCorePlayerState: Equipment wiring failed after %d attempts"),
			MaxEquipmentWireRetries);
		return false;
	}

	// Schedule next retry using lambda wrapper (timer callbacks must return void)
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			EquipmentWireRetryHandle,
			FTimerDelegate::CreateLambda([this]()
			{
				TryWireEquipmentModuleOnce();
			}),
			EquipmentWireRetryInterval,
			false  // Don't loop, we'll reschedule manually if needed
		);
	}

	return false;
}

bool ASuspenseCorePlayerState::WireEquipmentModule(USuspenseLoadoutManager* LoadoutManager, const FName& AppliedLoadoutID)
{
	// Basic validation
	if (!EquipmentDataStore || !EquipmentTxnProcessor || !EquipmentOps)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCorePlayerState::WireEquipmentModule: Core equipment components not created"));
		return false;
	}

	// Initialize DataStore (primary data layer)
	// DataStore doesn't have external dependencies, just needs owner
	if (EquipmentDataStore)
	{
		// DataStore initializes itself in BeginPlay, just validate it's ready
		UE_LOG(LogTemp, Verbose, TEXT("SuspenseCorePlayerState: EquipmentDataStore ready"));
	}

	// Initialize Transaction Processor
	// It coordinates with DataStore for atomic operations
	if (EquipmentTxnProcessor && EquipmentDataStore)
	{
		// Transaction processor wires to data store
		UE_LOG(LogTemp, Verbose, TEXT("SuspenseCorePlayerState: EquipmentTxnProcessor ready"));
	}

	// Initialize Operation Executor
	// Validates and executes through transaction processor
	if (EquipmentOps && EquipmentTxnProcessor && EquipmentDataStore)
	{
		UE_LOG(LogTemp, Verbose, TEXT("SuspenseCorePlayerState: EquipmentOps ready"));
	}

	// Initialize Inventory Bridge
	// Connects equipment system to inventory component
	if (EquipmentInventoryBridge && InventoryComponent)
	{
		// Bridge connects inventory to equipment data store
		UE_LOG(LogTemp, Verbose, TEXT("SuspenseCorePlayerState: EquipmentInventoryBridge ready"));
	}

	// Initialize Prediction System (client-side)
	if (EquipmentPrediction && EquipmentDataStore)
	{
		UE_LOG(LogTemp, Verbose, TEXT("SuspenseCorePlayerState: EquipmentPrediction ready"));
	}

	// Initialize Replication Manager
	if (EquipmentReplication && EquipmentDataStore)
	{
		UE_LOG(LogTemp, Verbose, TEXT("SuspenseCorePlayerState: EquipmentReplication ready"));
	}

	// Initialize Network Dispatcher
	if (EquipmentNetworkDispatcher)
	{
		UE_LOG(LogTemp, Verbose, TEXT("SuspenseCorePlayerState: EquipmentNetworkDispatcher ready"));
	}

	// Initialize Event Dispatcher
	if (EquipmentEventDispatcher)
	{
		UE_LOG(LogTemp, Verbose, TEXT("SuspenseCorePlayerState: EquipmentEventDispatcher ready"));
	}

	// Initialize Weapon State Manager
	if (WeaponStateManager)
	{
		UE_LOG(LogTemp, Verbose, TEXT("SuspenseCorePlayerState: WeaponStateManager ready"));
	}

	// Create Slot Validator (not a component, just UObject)
	if (!EquipmentSlotValidator)
	{
		EquipmentSlotValidator = NewObject<USuspenseEquipmentSlotValidator>(this, TEXT("EquipmentSlotValidator"));
		UE_LOG(LogTemp, Verbose, TEXT("SuspenseCorePlayerState: EquipmentSlotValidator created"));
	}

	UE_LOG(LogTemp, Log, TEXT("SuspenseCorePlayerState: Equipment module wiring complete for %s"),
		*GetPlayerName());

	return true;
}
