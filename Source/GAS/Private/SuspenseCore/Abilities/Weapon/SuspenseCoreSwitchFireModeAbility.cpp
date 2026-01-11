// SuspenseCoreSwitchFireModeAbility.cpp
// SuspenseCore - Fire Mode Switch Ability Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Abilities/Weapon/SuspenseCoreSwitchFireModeAbility.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeapon.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreFireModeProvider.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponCombatState.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

USuspenseCoreSwitchFireModeAbility::USuspenseCoreSwitchFireModeAbility()
{
	// Network configuration - server only to prevent double-switching
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;

	// Tag configuration
	AbilityTags.AddTag(SuspenseCoreTags::Ability::Weapon::FireModeSwitch);

	// Block during firing/reloading
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Firing);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Reloading);
	ActivationBlockedTags.AddTag(SuspenseCoreTags::State::Dead);
}

bool USuspenseCoreSwitchFireModeAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// Must have weapon with multiple fire modes
	ISuspenseCoreWeapon* Weapon = const_cast<USuspenseCoreSwitchFireModeAbility*>(this)->GetWeaponInterface();
	if (!Weapon)
	{
		return false;
	}

	TArray<FGameplayTag> AvailableModes = Weapon->Execute_GetAvailableFireModes(Cast<UObject>(Weapon));
	if (AvailableModes.Num() <= 1)
	{
		return false;  // Only one mode, can't switch
	}

	return true;
}

void USuspenseCoreSwitchFireModeAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Switch fire mode
	SwitchFireMode();

	// Play effects
	PlaySwitchEffects();

	// End immediately
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

void USuspenseCoreSwitchFireModeAbility::SwitchFireMode()
{
	ISuspenseCoreWeapon* Weapon = GetWeaponInterface();
	if (!Weapon)
	{
		return;
	}

	// Cycle to next fire mode
	bool bSuccess = Weapon->Execute_CycleFireMode(Cast<UObject>(Weapon));

	if (bSuccess)
	{
		// Get new fire mode and publish event
		FGameplayTag NewMode = Weapon->Execute_GetCurrentFireMode(Cast<UObject>(Weapon));
		PublishFireModeChangedEvent(NewMode);
	}
}

ISuspenseCoreWeapon* USuspenseCoreSwitchFireModeAbility::GetWeaponInterface() const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return nullptr;
	}

	// Check attached actors for weapon
	TArray<AActor*> AttachedActors;
	Avatar->GetAttachedActors(AttachedActors);

	for (AActor* Attached : AttachedActors)
	{
		if (ISuspenseCoreWeapon* Weapon = Cast<ISuspenseCoreWeapon>(Attached))
		{
			return Weapon;
		}
	}

	return nullptr;
}

ISuspenseCoreFireModeProvider* USuspenseCoreSwitchFireModeAbility::GetFireModeProvider() const
{
	ISuspenseCoreWeapon* Weapon = const_cast<USuspenseCoreSwitchFireModeAbility*>(this)->GetWeaponInterface();
	if (!Weapon)
	{
		return nullptr;
	}

	AActor* WeaponActor = Cast<AActor>(Cast<UObject>(Weapon));
	if (!WeaponActor)
	{
		return nullptr;
	}

	TArray<UActorComponent*> Components;
	WeaponActor->GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		if (ISuspenseCoreFireModeProvider* Provider = Cast<ISuspenseCoreFireModeProvider>(Comp))
		{
			return Provider;
		}
	}

	return nullptr;
}

void USuspenseCoreSwitchFireModeAbility::PublishFireModeChangedEvent(const FGameplayTag& NewFireMode)
{
	if (USuspenseCoreEventBus* EventBus = GetEventBus())
	{
		FSuspenseCoreEventData EventData = FSuspenseCoreEventData::Create(GetAvatarActorFromActorInfo());
		EventData.SetTag(FName("FireMode"), NewFireMode);

		// Get fire mode name for display
		FString ModeName = NewFireMode.GetTagName().ToString();
		ModeName = ModeName.RightChop(ModeName.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd) + 1);
		EventData.SetString(FName("FireModeName"), ModeName);

		EventBus->Publish(SuspenseCoreTags::Event::Weapon::FireModeChanged, EventData);
	}
}

void USuspenseCoreSwitchFireModeAbility::PlaySwitchEffects()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar)
	{
		return;
	}

	// Play sound
	if (SwitchSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			Avatar,
			SwitchSound,
			Avatar->GetActorLocation()
		);
	}

	// Play montage
	if (SwitchMontage)
	{
		if (ACharacter* Character = Cast<ACharacter>(Avatar))
		{
			if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
			{
				AnimInstance->Montage_Play(SwitchMontage);
			}
		}
	}
}
