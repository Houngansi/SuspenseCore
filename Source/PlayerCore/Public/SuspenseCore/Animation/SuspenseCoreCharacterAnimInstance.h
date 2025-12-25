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
class UAnimComposite;
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

	/** Предыдущий тип оружия (для отслеживания смены) */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Weapon")
	FGameplayTag LastWeaponType;

	/** Оружие экипировано (есть в слоте) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	bool bHasWeaponEquipped = false;

	/** Оружие извлечено (в руках) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	bool bIsWeaponDrawn = false;

	/** Персонаж сейчас прицеливается (ADS) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	bool bIsAiming = false;

	/** Alpha для бленда прицеливания (0-1) - интерполируется автоматически */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	float AimingAlpha = 0.0f;

	/** Персонаж сейчас стреляет */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	bool bIsFiring = false;

	/** Персонаж сейчас перезаряжает */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	bool bIsReloading = false;

	/** Персонаж задерживает дыхание (снайпер) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	bool bIsHoldingBreath = false;

	/** Сейчас играет монтаж на оружейном слое */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	bool bIsWeaponMontageActive = false;

	/** Модификатор хвата (0 = обычный, 1 = тактический) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	float GripModifier = 0.0f;

	/** Оружие опущено (0 = нормально, 1 = полностью опущено - у стены и т.д.) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	float WeaponLoweredAlpha = 0.0f;

	/** Текущий recoil alpha (0 = нет отдачи, 1 = максимальная) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	float RecoilAlpha = 0.0f;

	/** Множитель sway (раскачивание оружия, влияет стамина, движение) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	float WeaponSwayMultiplier = 1.0f;

	/** Stored recoil (для процедурной анимации) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	float StoredRecoil = 0.0f;

	/** Additive pitch (отдача, дыхание) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	float AdditivePitch = 0.0f;

	/** Block distance (расстояние до стены) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon")
	float BlockDistance = 0.0f;

	// -------- Compatibility Flags --------

	/** Is Holstered (inverse of bIsWeaponDrawn) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon|State")
	bool bIsHolstered = true;

	/** Modify Grip flag - enables grip modification on aim */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon|State")
	bool bModifyGrip = false;

	/** Create Aim Pose flag - enables custom aim transform */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon|State")
	bool bCreateAimPose = false;

	// -------- Aim Target --------

	/** Sight Distance - расстояние до цели прицеливания (для FABRIK и aim target) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon|Aim")
	float SightDistance = 200.0f;

	// -------- Pose Indices --------

	/** Aim Pose index (from weapon data) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon|Pose")
	int32 AimPose = 0;

	/** Stored Pose index (for transitions) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon|Pose")
	int32 StoredPose = 0;

	/** Grip ID (for hand placement variations) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon|Pose")
	int32 GripID = 0;

	// -------- IK Transforms --------

	/** Aim Transform (weapon positioning) */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon|IK")
	FTransform AimTransform;

	/** Right Hand IK Transform */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon|IK")
	FTransform RightHandTransform;

	/** Left Hand IK Transform */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Weapon|IK")
	FTransform LeftHandTransform;

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
	// LEGACY TRANSFORM VARIABLES (Единственные переменные для AnimGraph!)
	// Имена соответствуют оригинальному MPS AnimBP
	// ═══════════════════════════════════════════════════════════════════════════════

	/** RH Transform - трансформ правой руки для VB_Hand_R */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Animation", meta = (DisplayName = "RH Transform"))
	FTransform RHTransform = FTransform::Identity;

	/** LH Transform - трансформ левой руки для VB_Hand_L */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Animation", meta = (DisplayName = "LH Transform"))
	FTransform LHTransform = FTransform::Identity;

	/** W Transform - трансформ оружия для Weapon bone */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Animation", meta = (DisplayName = "W Transform"))
	FTransform WTransform = FTransform::Identity;

	// ═══════════════════════════════════════════════════════════════════════════════
	// LEGACY DT VARIABLES (Set from Blueprint via legacy DataTable method)
	// These variables are populated by Blueprint from DT_MPS_Anims DataTable
	// ═══════════════════════════════════════════════════════════════════════════════

	/** DT Stance BlendSpace */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	TObjectPtr<UBlendSpace> DTStance = nullptr;

	/** DT Locomotion BlendSpace */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	TObjectPtr<UBlendSpace> DTLocomotion = nullptr;

	/** DT Idle animation */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	TObjectPtr<UAnimSequenceBase> DTIdle = nullptr;

	/** DT Aim Pose animation */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	TObjectPtr<UAnimSequenceBase> DTAimPose = nullptr;

	/** DT Aim In transition animation */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	TObjectPtr<UAnimSequenceBase> DTAimIn = nullptr;

	/** DT Aim Idle animation */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	TObjectPtr<UAnimSequenceBase> DTAimIdle = nullptr;

	/** DT Aim Out transition animation */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	TObjectPtr<UAnimSequenceBase> DTAimOut = nullptr;

	/** DT Slide animation */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	TObjectPtr<UAnimSequenceBase> DTSlide = nullptr;

	/** DT Blocked animation (weapon blocked by wall) */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	TObjectPtr<UAnimSequenceBase> DTBlocked = nullptr;

	/** DT Grip Blocked animation */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	TObjectPtr<UAnimSequenceBase> DTGripBlocked = nullptr;

	/** DT Left Hand Grip transform */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	FTransform DTLeftHandGrip = FTransform::Identity;

	/** DT Grip Poses AnimComposite */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	TObjectPtr<UAnimComposite> DTGripPoses = nullptr;

	/** DT Right Hand Transform */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	FTransform DTRHTransform = FTransform::Identity;

	/** DT Left Hand Transform */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	FTransform DTLHTransform = FTransform::Identity;

	/** DT Weapon Transform */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	FTransform DTWTransform = FTransform::Identity;

	/** DT Left Hand Grip Transform array */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	TArray<FTransform> DTLHGripTransform;

	/** DT Aim Pose Alpha */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Animation|LegacyDT")
	float DTAimPoseAlpha = 0.0f;

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

	/** Camera Shake - интерполированный шейк камеры (читается из Character и интерполируется) */
	UPROPERTY(BlueprintReadWrite, Category = "SuspenseCore|Procedural")
	FVector CameraShake = FVector::ZeroVector;

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

	/**
	 * Высота/сила прыжка (из GAS MovementAttributeSet)
	 * Используется для анимаций прыжка и может быть модифицирована
	 * весом снаряжения через GameplayEffects
	 */
	UPROPERTY(BlueprintReadOnly, Category = "SuspenseCore|Attributes")
	float JumpHeight = 420.0f;

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

	/**
	 * Get animation segment from GripPoses AnimComposite by pose index.
	 *
	 * GripPoses содержит несколько поз (сегментов) для разных состояний:
	 * - Index 0: Base grip pose (базовый хват)
	 * - Index 1: Aim grip pose (хват при прицеливании)
	 * - Index 2: Reload grip pose (хват при перезарядке)
	 * - Index 3+: Weapon-specific poses (специфические для оружия)
	 *
	 * @param PoseIndex Index of pose segment in GripPoses composite
	 * @return AnimSequence for the pose, or nullptr if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Animation|Helpers")
	UAnimSequenceBase* GetGripPoseByIndex(int32 PoseIndex) const;

	/**
	 * Get currently active grip pose based on combat state and GripID.
	 * Automatically selects appropriate pose from GripPoses composite.
	 *
	 * @return Active grip pose animation
	 */
	UFUNCTION(BlueprintCallable, Category = "SuspenseCore|Animation|Helpers")
	UAnimSequenceBase* GetActiveGripPose() const;

	/**
	 * Get legacy DataTable row name from WeaponArchetype tag.
	 * Maps new archetype tags (Weapon.Rifle.Assault) to legacy row names (SMG).
	 *
	 * Mapping:
	 * - Weapon.Rifle.* -> SMG (assault rifles use SMG row for now)
	 * - Weapon.SMG.* -> SMG
	 * - Weapon.Pistol.* -> Pistol
	 * - Weapon.Shotgun.* -> Shotgun
	 * - Weapon.Rifle.Sniper -> Sniper
	 * - Weapon.Melee.Knife -> Knife
	 * - Weapon.Throwable.* -> Frag
	 * - Default -> SMG
	 *
	 * @return FName of the legacy DataTable row
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Animation|Helpers")
	FName GetLegacyRowNameFromArchetype() const;

	/**
	 * Get legacy DataTable row name from any WeaponArchetype tag.
	 * Static version for external use.
	 *
	 * @param WeaponArchetype The weapon archetype GameplayTag
	 * @return FName of the legacy DataTable row
	 */
	UFUNCTION(BlueprintPure, Category = "SuspenseCore|Animation|Helpers")
	static FName GetLegacyRowNameFromArchetypeTag(const FGameplayTag& WeaponArchetype);

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

	// ═══════════════════════════════════════════════════════════════════════════════
	// LEGACY INTERPOLATION (from MPS AnimBP)
	// ═══════════════════════════════════════════════════════════════════════════════

	/** Текущий интерполированный RH Transform (для TInterp To) */
	FTransform InterpolatedRHTransform = FTransform::Identity;

	/** Текущий интерполированный LH Transform (для TInterp To) */
	FTransform InterpolatedLHTransform = FTransform::Identity;

	/** Текущий интерполированный Block Distance */
	float InterpolatedBlockDistance = 0.0f;

	/** Текущий интерполированный Additive Pitch */
	float InterpolatedAdditivePitch = 0.0f;

	/** Скорость интерполяции трансформов (из legacy: 8.0) */
	static constexpr float TransformInterpSpeed = 8.0f;

	/** Скорость интерполяции Block Distance (из legacy: 10.0) */
	static constexpr float BlockDistanceInterpSpeed = 10.0f;

	/** Blend Exponent для Ease функции Additive Pitch (из legacy: 6.0) */
	static constexpr float AdditivePitchBlendExp = 6.0f;

	/** Кэшированный Weapon Actor */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedWeaponActor;

	/** Имя сокета для левой руки на оружии */
	static const FName LHTargetSocketName;

	/**
	 * Получить трансформ сокета LH_Target с оружия.
	 * Реализует логику M LH Offset из legacy Blueprint.
	 *
	 * @param OutTransform Результирующий трансформ
	 * @return true если трансформ получен успешно
	 */
	bool GetWeaponLHTargetTransform(FTransform& OutTransform) const;

	/**
	 * Вычислить итоговый LH Transform с учётом всех условий.
	 * Логика из M LH Offset макро:
	 * - Если !bModifyGrip: использовать socket transform от оружия
	 * - Если bModifyGrip: использовать DT LH Grip Transform[GripID]
	 * - Если bIsMontageActive: пропустить (вернуть Identity)
	 */
	FTransform ComputeLHOffsetTransform() const;
};
