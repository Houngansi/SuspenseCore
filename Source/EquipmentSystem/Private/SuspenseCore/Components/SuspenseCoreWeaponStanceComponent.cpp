// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreWeaponStanceComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "SuspenseCore/Interfaces/Weapon/ISuspenseCoreWeaponAnimation.h"

USuspenseCoreWeaponStanceComponent::USuspenseCoreWeaponStanceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	CurrentWeaponType = FGameplayTag(); // none
	bWeaponDrawn = false;
	LastAnimationInterfaceCacheTime = -1000.0f;
}

void USuspenseCoreWeaponStanceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USuspenseCoreWeaponStanceComponent, CurrentWeaponType);
	DOREPLIFETIME(USuspenseCoreWeaponStanceComponent, bWeaponDrawn);
}

void USuspenseCoreWeaponStanceComponent::OnEquipmentChanged(AActor* NewEquipmentActor)
{
	TrackedEquipmentActor = NewEquipmentActor;
	PushToAnimationLayer(/*bSkipIfNoInterface=*/true);
}

void USuspenseCoreWeaponStanceComponent::SetWeaponStance(const FGameplayTag& WeaponTypeTag, bool bImmediate)
{
	if (CurrentWeaponType == WeaponTypeTag)
	{
		return;
	}

	CurrentWeaponType = WeaponTypeTag;

	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			Owner->ForceNetUpdate();
		}
	}

	if (bImmediate)
	{
		PushToAnimationLayer(/*bSkipIfNoInterface=*/false);
	}
}

void USuspenseCoreWeaponStanceComponent::ClearWeaponStance(bool bImmediate)
{
	SetWeaponStance(FGameplayTag(), bImmediate);
}

void USuspenseCoreWeaponStanceComponent::SetWeaponDrawnState(bool bDrawn)
{
	if (bWeaponDrawn == bDrawn)
	{
		return;
	}

	bWeaponDrawn = bDrawn;

	if (AActor* Owner = GetOwner())
	{
		if (Owner->HasAuthority())
		{
			Owner->ForceNetUpdate();
		}
	}

	PushToAnimationLayer(/*bSkipIfNoInterface=*/true);
}

TScriptInterface<ISuspenseCoreWeaponAnimation> USuspenseCoreWeaponStanceComponent::GetAnimationInterface() const
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// кэш свежий?
	if (CachedAnimationInterface.GetInterface() &&
		(Now - LastAnimationInterfaceCacheTime) < AnimationInterfaceCacheLifetime)
	{
		return CachedAnimationInterface;
	}

	// Вариант 1: владелец реализует интерфейс
	if (AActor* OwnerActor = GetOwner())
	{
		if (OwnerActor->GetClass()->ImplementsInterface(USuspenseCoreWeaponAnimation::StaticClass()))
		{
			CachedAnimationInterface.SetObject(OwnerActor);
			CachedAnimationInterface.SetInterface(Cast<ISuspenseCoreWeaponAnimation>(OwnerActor));
			LastAnimationInterfaceCacheTime = Now;
			return CachedAnimationInterface;
		}
	}

	// Вариант 2: (осознанно) НЕ полезем в сабсистему здесь — аттачмент сам делает fallback на субсистему.
	// Это позволяет не плодить зависимостей и избежать лишних include.

	// Кэша нет — возвращаем пусто
	CachedAnimationInterface = nullptr;
	LastAnimationInterfaceCacheTime = Now;
	return CachedAnimationInterface;
}

void USuspenseCoreWeaponStanceComponent::OnRep_WeaponType()
{
	PushToAnimationLayer(/*bSkipIfNoInterface=*/true);
}

void USuspenseCoreWeaponStanceComponent::OnRep_DrawnState()
{
	PushToAnimationLayer(/*bSkipIfNoInterface=*/true);
}

void USuspenseCoreWeaponStanceComponent::PushToAnimationLayer(bool bSkipIfNoInterface) const
{
	const TScriptInterface<ISuspenseCoreWeaponAnimation> AnimI = GetAnimationInterface();
	if (!AnimI.GetInterface())
	{
		if (!bSkipIfNoInterface)
		{
			// нет интерфейса — ничего не делаем
		}
		return;
	}

	// Здесь можно добавить активные вызовы в твой аним-интерфейс
	// (например, ApplyStance(CurrentWeaponType, bWeaponDrawn)),
	// когда определим контракт. Пока отдаём интерфейс наверх.
}
