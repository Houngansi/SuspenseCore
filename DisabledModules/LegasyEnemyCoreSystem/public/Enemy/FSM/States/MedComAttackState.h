// Copyright (c) 2025 MedCom
#pragma once

#include "CoreMinimal.h"
#include "Core/Enemy/FSM/MedComEnemyState.h"
#include "GameplayTagContainer.h"
#include "MedComAttackState.generated.h"

class AAIController;
class AMedComEnemyCharacter;

/**
 *  Стойка «Attack».
 *  ─ Бот неподвижен (или двигается минимально), стреляет по цели,
 *    выводит события PlayerLost / TargetOutOfRange в FSM
 *  ─ Поддерживает принцип «0-1 состояний»: всё, что не относится к атаке,
 *    отдаём наружу через FSM-события (погоня, смерть, оглушение и т.д.)
 *
 *  **Внутри оставлены только лёгкие проверки и логи —
 *  тяжёлая логика добавляется позже.**
 */
UCLASS()
class MEDCOMCORE_API UMedComAttackState : public UMedComEnemyState
{
	GENERATED_BODY()

public:
	UMedComAttackState();

	// ≡ MedComEnemyState
	virtual void OnEnter (AMedComEnemyCharacter* Owner) override;
	virtual void OnExit  (AMedComEnemyCharacter* Owner) override;
	virtual void ProcessTick(AMedComEnemyCharacter* Owner, float DeltaTime) override;
	virtual void OnEvent (AMedComEnemyCharacter* Owner,
	                      EEnemyEvent          Event,
	                      AActor*              Instigator) override;

private:

	/* === runtime caches === */
	TWeakObjectPtr<AAIController> CachedController;
	TWeakObjectPtr<APawn>         CachedTarget;

	/* === editable params (можно читать из DataAsset, здесь жёстко заложены) === */
	float AttackRange        = 1200.f;   // 12 м
	float MinAttackDistance  = 150.f;    // 1.5 м  (не подпускать ближе — включим «отскок» в будущем)
	float FireCooldown       = 0.6f;     // сек. до следующего «выстрела»
	float SightGraceTime     = 2.f;      // сколько можно не видеть цель, прежде чем «потерять»

	/* === timers === */
	float NextAllowedShotTime = 0.f;     // world-seconds
	float TimeSinceLastSight  = 0.f;     // счётчик потери цели

	/* === helpers === */
	bool  HasValidTarget   (AMedComEnemyCharacter* Owner) const;
	bool  HasLineOfSight   (AMedComEnemyCharacter* Owner) const;
	void  SelectFireMode   (AMedComEnemyCharacter* Owner, float DistM);
	void  FireWeapon       (AMedComEnemyCharacter* Owner);
	void  ForceRotationToTarget(AMedComEnemyCharacter* Owner, float DeltaTime) const;
};
