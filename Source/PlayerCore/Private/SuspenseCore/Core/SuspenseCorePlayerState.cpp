// SuspenseCorePlayerState.cpp
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Core/SuspenseCorePlayerState.h"
#include "SuspenseCore/Components/SuspenseCoreAbilitySystemComponent.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Subsystems/SuspenseCoreCharacterClassSubsystem.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Net/UnrealNetwork.h"

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
	}
}

void ASuspenseCorePlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

	DOREPLIFETIME(ASuspenseCorePlayerState, AbilitySystemComponent);
	DOREPLIFETIME(ASuspenseCorePlayerState, PlayerLevel);
	DOREPLIFETIME(ASuspenseCorePlayerState, TeamId);
	DOREPLIFETIME(ASuspenseCorePlayerState, CharacterClassId);
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
