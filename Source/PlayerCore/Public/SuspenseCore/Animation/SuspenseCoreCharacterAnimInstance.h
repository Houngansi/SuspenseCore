// SuspenseCoreCharacterAnimInstance.h
// Copyright Suspense Team. All Rights Reserved.
//
// SINGLE SOURCE OF TRUTH (SSOT) для всех анимационных данных персонажа.
// Этот класс агрегирует данные из всех систем и предоставляет их Animation Blueprint.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Characters/SuspenseCoreCharacter.h"
#include "SuspenseCore/Types/Animation/SuspenseCoreAnimationState.h"
#include "SuspenseCoreCharacterAnimInstance.generated.h"

// Forward declarations
class USuspenseCoreWeaponStanceComponent;
class USuspenseCoreMovementAttributeSet;
class UAbilitySystemComponent;
class UBlendSpace;
class UBlendSpace1D;
class UAnimMontage;
class UAnimSequence;
class UDataTable;

/**
 * USuspenseCoreCharacterAnimInstance
 *
 * SINGLE SOURCE OF TRUTH для Animation Blueprint.
 * Все переменные обновляются в NativeUpdateAnimation каждый кадр.
 *
 * АРХИТЕКТУРА:
 * ┌─────────────────────────────────────────────────────────────────┐
 * │  Character       → MovementState, Speed, Direction             │
 * │  StanceComponent → WeaponType, bDrawn, BlendSpace              │
 * │  GAS Attributes  → SprintSpeed, CrouchSpeed, etc.              │
 * │  DataTable       → Stance BS, Draw/Holster Montages            │
 * │                    ↓                                            │
 * │           AnimInstance (SSOT)                                   │
 * │                    ↓                                            │
 * │           Animation Blueprint                                   │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * ИСПОЛЬЗОВАНИЕ В ABP:
 * - Все переменные доступны напрямую (BlueprintReadOnly)
 * - Не нужно вызывать функции в EventGraph
 * - Используй CurrentStanceBlendSpace прямо как BlendSpace ноду
 */
UCLASS()
class PLAYERCORE_API USuspenseCoreCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	USuspenseCoreCharacterAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	// ═══════════════════════════════════════════════════════════════════════════════
	// MOVEMENT STATE (из Character)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Текущее состояние движения (Idle, Walking, Sprinting, Crouching, Jumping, Falling) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement")
	ESuspenseCoreMovementState MovementState = ESuspenseCoreMovementState::Idle;

	/** Персонаж сейчас спринтит */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement")
	bool bIsSprinting = false;

	/** Персонаж сейчас в присяде */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement")
	bool bIsCrouching = false;

	/** Персонаж сейчас в воздухе (прыжок или падение) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement")
	bool bIsInAir = false;

	/** Персонаж сейчас падает */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement")
	bool bIsFalling = false;

	/** Персонаж сейчас прыгает */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement")
	bool bIsJumping = false;

	/** Персонаж двигается (есть input) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement")
	bool bHasMovementInput = false;

	/** Персонаж находится на земле */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement")
	bool bIsOnGround = true;

	/** Персонаж сейчас скользит (sliding) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Movement")
	bool bIsSliding = false;

	// ═══════════════════════════════════════════════════════════════════════════════
	// POSE STATES / TURN IN PLACE (для поворота корпуса)
	// ═══════════════════════════════════════════════════════════════════════════════

	/**
	 * Lean (Roll) - наклон персонажа при поворотах
	 * Интерполируется с скоростью 10.0
	 * Получается из Character или вычисляется из velocity
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|PoseStates")
	float Lean = 0.0f;

	/**
	 * Body Pitch - наклон корпуса вперёд/назад
	 * Используется для aim offset корпуса
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|PoseStates")
	float BodyPitch = 0.0f;

	/**
	 * Yaw Offset для Turn In Place
	 * Накапливается когда персонаж стоит, интерполируется к 0 при движении
	 * Кламп: -120..120 градусов
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|PoseStates")
	float YawOffset = 0.0f;

	/**
	 * Rotation Curve - значение из анимационной кривой "Rotation"
	 * Используется для синхронизации поворота с анимацией
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|PoseStates")
	float RotationCurve = 0.0f;

	/**
	 * Is Turning - значение из анимационной кривой "IsTurning"
	 * 1.0 когда проигрывается анимация поворота
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|PoseStates")
	float IsTurningCurve = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// VELOCITY & DIRECTION (для BlendSpaces)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Текущая скорость (длина вектора velocity) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Velocity")
	float Speed = 0.0f;

	/** Скорость в Ground Plane (без вертикальной компоненты) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Velocity")
	float GroundSpeed = 0.0f;

	/** Нормализованная скорость (0-1 относительно MaxWalkSpeed) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Velocity")
	float NormalizedSpeed = 0.0f;

	/** Направление движения вперёд (-1..1) для BlendSpace */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Direction")
	float MoveForward = 0.0f;

	/** Направление движения вправо (-1..1) для BlendSpace */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Direction")
	float MoveRight = 0.0f;

	/** Угол направления движения в градусах (-180..180) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Direction")
	float MovementDirection = 0.0f;

	/**
	 * Величина движения для State Machine (0-2)
	 * = Clamp(ABS(Forward) + ABS(Right), 0, Max)
	 * Max = 1.0 (walk), 2.0 (sprint)
	 * Используется для переходов между Idle/Walk/Run
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Direction")
	float Movement = 0.0f;

	/** Вертикальная скорость (для Jump/Fall бленда) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Velocity")
	float VerticalVelocity = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// WEAPON & STANCE (из StanceComponent)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Тип текущего оружия (GameplayTag) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	FGameplayTag CurrentWeaponType;

	/** Оружие экипировано (есть в слоте) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	bool bHasWeaponEquipped = false;

	/** Оружие извлечено (в руках) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	bool bIsWeaponDrawn = false;

	/** Персонаж сейчас прицеливается (ADS) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	bool bIsAiming = false;

	/** Alpha для бленда прицеливания (0-1) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	float AimingAlpha = 0.0f;

	/** Персонаж сейчас стреляет */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	bool bIsFiring = false;

	/** Персонаж сейчас перезаряжает */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	bool bIsReloading = false;

	// ═══════════════════════════════════════════════════════════════════════════════
	// ANIMATION ASSETS FROM DATATABLE (SSOT)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Текущий BlendSpace стойки (из DataTable!) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Animation|Assets")
	TObjectPtr<UBlendSpace> CurrentStanceBlendSpace = nullptr;

	/** Текущий BlendSpace локомоции (из DataTable!) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Animation|Assets")
	TObjectPtr<UBlendSpace1D> CurrentLocomotionBlendSpace = nullptr;

	/** Текущая Idle анимация (из DataTable!) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Animation|Assets")
	TObjectPtr<UAnimSequence> CurrentIdleAnimation = nullptr;

	/** Текущая поза прицеливания (из DataTable!) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Animation|Assets")
	TObjectPtr<UAnimSequence> CurrentAimPose = nullptr;

	// ═══════════════════════════════════════════════════════════════════════════════
	// FULL ANIMATION DATA (Полные данные из DataTable)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Полная структура анимационных данных текущего оружия */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Animation|Data")
	FAnimationStateData CurrentAnimationData;

	/** DataTable с анимациями оружия (референс) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Animation|Data")
	TObjectPtr<UDataTable> WeaponAnimationsTable = nullptr;

	// ═══════════════════════════════════════════════════════════════════════════════
	// IK TRANSFORMS (Inverse Kinematics)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Трансформ левой руки для IK */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|IK")
	FTransform LeftHandIKTransform = FTransform::Identity;

	/** Трансформ правой руки для IK */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|IK")
	FTransform RightHandIKTransform = FTransform::Identity;

	/** Трансформ оружия */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|IK")
	FTransform WeaponTransform = FTransform::Identity;

	/** Alpha для Left Hand IK (0 = выкл, 1 = вкл) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|IK")
	float LeftHandIKAlpha = 0.0f;

	/** Alpha для Right Hand IK */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|IK")
	float RightHandIKAlpha = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// AIM OFFSET & LOOK (Будущее расширение)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Yaw для AimOffset (-180..180) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|AimOffset")
	float AimYaw = 0.0f;

	/** Pitch для AimOffset (-90..90) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|AimOffset")
	float AimPitch = 0.0f;

	/** Delta rotation для поворота корпуса */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|AimOffset")
	float TurnInPlaceAngle = 0.0f;

	/** Нужен ли поворот корпуса */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|AimOffset")
	bool bShouldTurnInPlace = false;

	// ═══════════════════════════════════════════════════════════════════════════════
	// RECOIL & PROCEDURAL (Будущее расширение)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Текущий отдача (pitch добавка) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Recoil")
	float RecoilPitch = 0.0f;

	/** Текущий отдача (yaw добавка) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Recoil")
	float RecoilYaw = 0.0f;

	/** Alpha для процедурного дыхания */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Procedural")
	float BreathingAlpha = 0.0f;

	/** Alpha для sway оружия */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Procedural")
	float WeaponSwayAlpha = 0.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// GAS ATTRIBUTES (Скорости из AttributeSet)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Максимальная скорость ходьбы (из GAS) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Attributes")
	float MaxWalkSpeed = 400.0f;

	/** Максимальная скорость спринта (из GAS) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Attributes")
	float MaxSprintSpeed = 600.0f;

	/** Максимальная скорость в присяде (из GAS) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Attributes")
	float MaxCrouchSpeed = 200.0f;

	/** Максимальная скорость при прицеливании (из GAS) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Attributes")
	float MaxAimSpeed = 250.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// HELPER METHODS (Вспомогательные методы для ABP)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Получить владельца как SuspenseCoreCharacter */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Animation")
	ASuspenseCoreCharacter* GetSuspenseCoreCharacter() const;

	/** Проверить валидность анимационных данных */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Animation")
	bool HasValidAnimationData() const;

	/** Получить монтаж доставания оружия */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Animation")
	UAnimMontage* GetDrawMontage(bool bFirstDraw = false) const;

	/** Получить монтаж убирания оружия */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Animation")
	UAnimMontage* GetHolsterMontage() const;

	/** Получить монтаж перезарядки */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Animation")
	UAnimMontage* GetReloadMontage(bool bTactical = true) const;

	/** Получить монтаж выстрела */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Animation")
	UAnimMontage* GetFireMontage(bool bAiming = false) const;

protected:
	// ═══════════════════════════════════════════════════════════════════════════════
	// CACHED REFERENCES
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Кэшированный Character */
	UPROPERTY(Transient)
	TWeakObjectPtr<ASuspenseCoreCharacter> CachedCharacter;

	/** Кэшированный StanceComponent */
	UPROPERTY(Transient)
	TWeakObjectPtr<USuspenseCoreWeaponStanceComponent> CachedStanceComponent;

	/** Кэшированный CharacterMovementComponent */
	UPROPERTY(Transient)
	TWeakObjectPtr<UCharacterMovementComponent> CachedMovementComponent;

	/** Кэшированный AbilitySystemComponent */
	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> CachedASC;

	/** Кэшированный MovementAttributeSet */
	UPROPERTY(Transient)
	TWeakObjectPtr<const USuspenseCoreMovementAttributeSet> CachedMovementAttributes;

	// ═══════════════════════════════════════════════════════════════════════════════
	// INTERNAL UPDATE METHODS
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Обновить кэш компонентов */
	void UpdateCachedReferences();

	/** Обновить данные о движении из Character */
	void UpdateMovementData(float DeltaSeconds);

	/** Обновить данные о скорости и направлении */
	void UpdateVelocityData(float DeltaSeconds);

	/** Обновить данные об оружии из StanceComponent */
	void UpdateWeaponData(float DeltaSeconds);

	/** Обновить анимационные ассеты из DataTable */
	void UpdateAnimationAssets();

	/** Обновить IK трансформы */
	void UpdateIKData(float DeltaSeconds);

	/** Обновить AimOffset данные */
	void UpdateAimOffsetData(float DeltaSeconds);

	/** Обновить Pose States (Turn In Place, Lean, etc.) */
	void UpdatePoseStates(float DeltaSeconds);

	/** Обновить GAS атрибуты */
	void UpdateGASAttributes();

	/** Загрузить DataTable из настроек */
	void LoadWeaponAnimationsTable();

	/** Получить строку из DataTable по WeaponType */
	const FAnimationStateData* GetAnimationDataForWeaponType(const FGameplayTag& WeaponType) const;

private:
	/** Время последнего обновления кэша */
	float LastCacheUpdateTime = 0.0f;

	/** Интервал обновления кэша (секунды) */
	static constexpr float CacheUpdateInterval = 1.0f;

	/** Предыдущее значение для интерполяции */
	float PreviousAimingAlpha = 0.0f;

	/** Скорость интерполяции для AimAlpha */
	static constexpr float AimInterpSpeed = 10.0f;

	// ═══════════════════════════════════════════════════════════════════════════════
	// POSE STATES INTERNAL
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Yaw на прошлом тике для вычисления дельты */
	float LastTickYaw = 0.0f;

	/** Текущий Yaw актора */
	float CurrentYaw = 0.0f;

	/** Target Lean для интерполяции */
	float TargetLean = 0.0f;

	/** Скорость интерполяции Lean */
	static constexpr float LeanInterpSpeed = 10.0f;

	/** Скорость интерполяции YawOffset к 0 при движении */
	static constexpr float YawOffsetInterpSpeed = 4.0f;

	/** Скорость интерполяции YawOffset из RotationCurve */
	static constexpr float YawOffsetCurveInterpSpeed = 2.0f;

	/** Максимальный YawOffset */
	static constexpr float MaxYawOffset = 120.0f;
};
