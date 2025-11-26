// Copyright Suspense Team. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SuspenseWeaponStanceComponent.generated.h"

// ✅ Используем существующий у тебя интерфейс (НЕ объявляем свой)
class USuspenseWeaponAnimation;
class ISuspenseWeaponAnimation;

UCLASS(ClassGroup=(Equipment), meta=(BlueprintSpawnableComponent), BlueprintType, Blueprintable)
class EQUIPMENTSYSTEM_API USuspenseWeaponStanceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USuspenseWeaponStanceComponent();

	//~UObject
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -------- API, которое дергает USuspenseEquipmentAttachmentComponent --------
	UFUNCTION(BlueprintCallable, Category="Weapon|Stance")
	void OnEquipmentChanged(AActor* NewEquipmentActor);

	UFUNCTION(BlueprintCallable, Category="Weapon|Stance")
	void SetWeaponStance(const FGameplayTag& WeaponTypeTag, bool bImmediate = false);

	UFUNCTION(BlueprintCallable, Category="Weapon|Stance")
	void ClearWeaponStance(bool bImmediate = false);

	UFUNCTION(BlueprintCallable, Category="Weapon|Stance")
	void SetWeaponDrawnState(bool bDrawn);

	UFUNCTION(BlueprintCallable, Category="Weapon|Stance")
	TScriptInterface<ISuspenseWeaponAnimation> GetAnimationInterface() const;

	// -------- Helpers --------
	UFUNCTION(BlueprintPure, Category="Weapon|Stance")
	FGameplayTag GetCurrentWeaponType() const { return CurrentWeaponType; }

	UFUNCTION(BlueprintPure, Category="Weapon|Stance")
	bool IsWeaponDrawn() const { return bWeaponDrawn; }

	UFUNCTION(BlueprintPure, Category="Weapon|Stance")
	AActor* GetTrackedEquipmentActor() const { return TrackedEquipmentActor.Get(); }

protected:
	// репликационные нотификаторы
	UFUNCTION()
	void OnRep_WeaponType();

	UFUNCTION()
	void OnRep_DrawnState();

	// Протолкнуть текущую стойку в анимационный слой (если он есть)
	void PushToAnimationLayer(bool bSkipIfNoInterface = true) const;

private:
	// -------- реплицируемые данные стойки --------
	UPROPERTY(ReplicatedUsing=OnRep_WeaponType)
	FGameplayTag CurrentWeaponType;

	UPROPERTY(ReplicatedUsing=OnRep_DrawnState)
	bool bWeaponDrawn = false;

	// Тречим актуальный актор экипировки (владелец или спавнённый), weak
	UPROPERTY()
	TWeakObjectPtr<AActor> TrackedEquipmentActor;

	// Кэш интерфейса анимаций (тот же, что ждёт Attachment)
	UPROPERTY(Transient)
	mutable TScriptInterface<ISuspenseWeaponAnimation> CachedAnimationInterface;

	// Настройки кэша
	UPROPERTY(EditDefaultsOnly, Category="Weapon|Stance")
	float AnimationInterfaceCacheLifetime = 0.25f;

	mutable float LastAnimationInterfaceCacheTime = -1000.0f;
};
