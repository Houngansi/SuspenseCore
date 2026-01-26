#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MedComCombatComponent.generated.h"

class UMedComWeaponHandlerComponent;

/**
 * Компонент боевой логики, который может вызываться из AI StateMachine
 * или из игровых Blueprint-скриптов для выполнения атак.
 */
UCLASS(ClassGroup=(MedCom), meta=(BlueprintSpawnableComponent))
class MEDCOMCORE_API UMedComCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMedComCombatComponent();

	/** Проверка, можем ли мы атаковать цель (дистанция, углы, КД) */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool CanAttack(AActor* Target, float& OutDistance) const;

	/** Выполнить атаку (выстрел или ближний бой) по цели */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool PerformAttack(AActor* Target);

	/** Обновлять состояние боя (КД, выбор режима и т.п.) */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void UpdateCombatState(float DeltaTime);

	/** Время перезарядки между атаками (в секундах) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Rates")
	float AttackCooldown;

	/** Дистанция, на которой считаем, что можем стрелять (пример) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Ranges")
	float EffectiveRange;
    
	/** Значения для переключения режимов огня */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Ranges")
	float CloseRange = 200.0f;    // Ближняя дистанция - авто
    
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Ranges")
	float MediumRange = 500.0f;   // Средняя дистанция - очередь

	protected:
	virtual void BeginPlay() override;

private:
	/** Ссылка на компонент оружия (должен находиться на владельце) */
	UPROPERTY()
	UMedComWeaponHandlerComponent* WeaponHandler;

	/** Внутренний счётчик КД */
	float AttackCooldownRemaining;
};