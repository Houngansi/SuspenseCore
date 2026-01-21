// SuspenseCoreAnimationState.h
// Copyright Suspense Team. All Rights Reserved.
//
// SINGLE SOURCE OF TRUTH (SSOT) for weapon animation data.
// This structure is the DataTable row format for all weapon animations.
//
// ARCHITECTURE:
// ═══════════════════════════════════════════════════════════════════════════
// - DataTable is configured in Project Settings → Game → SuspenseCore
// - AnimInstance reads from this DT via GetAnimationDataForWeaponType()
// - Row names match legacy format: SMG, Pistol, Shotgun, Sniper, Knife, Special, Frag
// - Mapping from WeaponArchetype GameplayTag to Row Name is handled by helper functions
//
// ROW NAMES (SSOT):
// ═══════════════════════════════════════════════════════════════════════════
// | Row Name | GameplayTag Mapping                          |
// |----------|----------------------------------------------|
// | SMG      | Weapon.Rifle.*, Weapon.SMG.*                 |
// | Pistol   | Weapon.Pistol.*                              |
// | Shotgun  | Weapon.Shotgun.*                             |
// | Sniper   | Weapon.Rifle.Sniper, *Sniper*                |
// | Knife    | Weapon.Melee.Knife                           |
// | Special  | Weapon.Melee.*, Weapon.Heavy.*               |
// | Frag     | Weapon.Throwable.*                           |
//
// POSE INDEX CONVENTION (for LHGripTransform array):
// ═══════════════════════════════════════════════════════════════════════════
// Index 0: Base grip pose (Hip fire)
// Index 1: Aim grip pose (ADS)
// Index 2: Reload grip pose
// Index 3+: Weapon-specific custom poses
//
// SEQUENCE EVALUATOR FORMULA:
// ═══════════════════════════════════════════════════════════════════════════
// ExplicitTime = 0.02 + (PoseIndex * 0.03)
//
// @see USuspenseCoreSettings::WeaponAnimationsTable - DataTable reference
// @see USuspenseCoreCharacterAnimInstance - Consumes this data
// @see USuspenseCoreWeaponStanceComponent - Provides combat state context

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimComposite.h"
#include "SuspenseCoreAnimationState.generated.h"

/**
 * FSuspenseCoreAnimationData
 *
 * SINGLE SOURCE OF TRUTH for weapon animation data.
 * This is the DataTable row structure for weapon animations.
 *
 * IMPORTANT: Pose indices (GripID, AimPose, StoredPose) are stored in WeaponActor,
 * NOT in this structure. This allows different weapons to use different poses
 * from the same animation set.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreAnimationData : public FTableRowBase
{
	GENERATED_BODY()

	FSuspenseCoreAnimationData()
		: Stance(nullptr)
		, Locomotion(nullptr)
		, Idle(nullptr)
		, AimPose(nullptr)
		, AimIn(nullptr)
		, AimIdle(nullptr)
		, AimOut(nullptr)
		, Slide(nullptr)
		, Blocked(nullptr)
		, GripBlocked(nullptr)
		, LeftHandGrip(nullptr)
		, FirstDraw(nullptr)
		, Draw(nullptr)
		, Holster(nullptr)
		, Firemode(nullptr)
		, Shoot(nullptr)
		, AimShoot(nullptr)
		, ReloadShort(nullptr)
		, ReloadLong(nullptr)
		, Melee(nullptr)
		, Throw(nullptr)
		, GripPoses(nullptr)
		, RHTransform(FTransform::Identity)
		, LHTransform(FTransform::Identity)
		, WTransform(FTransform::Identity)
	{
		// Initialize grip transform array with default poses
		// Index 0: Base (Hip fire)
		// Index 1: Aim (ADS)
		LHGripTransform.Add(FTransform::Identity);
		LHGripTransform.Add(FTransform::Identity);
	}

	// Стойка - Blend Space
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UBlendSpace> Stance;

	// Передвижение - Blend Space 1D
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UBlendSpace1D> Locomotion;

	// Состояние покоя - Anim Sequence
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimSequence> Idle;

	// Поза прицеливания - Anim Composite
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimComposite> AimPose;

	// Вход в прицеливание - Anim Sequence
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimSequence> AimIn;

	// Удержание прицела - Anim Sequence
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimSequence> AimIdle;

	// Выход из прицеливания - Anim Sequence
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimSequence> AimOut;

	// Скольжение - Anim Sequence
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimSequence> Slide;

	// Блокированное состояние - Anim Sequence
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimSequence> Blocked;

	// Блокированный хват - Anim Sequence
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimSequence> GripBlocked;

	// Хват левой рукой - Anim Sequence
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimSequence> LeftHandGrip;

	// Первое доставание оружия - Anim Montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimMontage> FirstDraw;

	// Доставание оружия - Anim Montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimMontage> Draw;

	// Убирание оружия в кобуру - Anim Montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimMontage> Holster;

	// Переключение режима огня - Anim Montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimMontage> Firemode;

	// Выстрел - Anim Montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimMontage> Shoot;

	// Выстрел с прицела - Anim Montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimMontage> AimShoot;

	// Быстрая перезарядка - Anim Montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimMontage> ReloadShort;

	// Полная перезарядка - Anim Montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimMontage> ReloadLong;

	// Удар прикладом - Anim Montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimMontage> Melee;

	// Бросок - Anim Montage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimMontage> Throw;

	// Позы хвата - Anim Composite
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	TObjectPtr<UAnimComposite> GripPoses;

	// Трансформация правой руки - Transform
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms")
	FTransform RHTransform;

	// Трансформация левой руки - Transform
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms")
	FTransform LHTransform;

	// Массив трансформаций хвата левой руки - Transform Array
	// Индекс 0: базовая позиция хвата
	// Индекс 1: позиция при прицеливании
	// Индекс 2: позиция при перезарядке
	// Дополнительные индексы для специфических состояний
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms")
	TArray<FTransform> LHGripTransform;

	// Трансформация оружия - Transform
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Transforms")
	FTransform WTransform;

	// ═══════════════════════════════════════════════════════════════════════════
	// HELPER METHODS
	// ═══════════════════════════════════════════════════════════════════════════

	/**
	 * Get left hand grip transform by pose index.
	 *
	 * @param Index Pose index (0 = Base, 1 = Aim, 2 = Reload, 3+ = Custom)
	 * @return Transform for the pose, or Identity if index is invalid
	 */
	FORCEINLINE FTransform GetLeftHandGripTransform(int32 Index = 0) const
	{
		if (LHGripTransform.IsValidIndex(Index))
		{
			return LHGripTransform[Index];
		}
		return FTransform::Identity;
	}

	/**
	 * Set left hand grip transform by pose index.
	 * Automatically expands array if needed.
	 *
	 * @param Index Pose index
	 * @param Transform New transform value
	 */
	FORCEINLINE void SetLeftHandGripTransform(int32 Index, const FTransform& Transform)
	{
		// Expand array if necessary
		while (LHGripTransform.Num() <= Index)
		{
			LHGripTransform.Add(FTransform::Identity);
		}
		LHGripTransform[Index] = Transform;
	}

	/**
	 * Get blended left hand grip transform between two poses.
	 * Useful for smooth transitions (e.g., Hip -> ADS).
	 *
	 * @param FromIndex Starting pose index
	 * @param ToIndex Target pose index
	 * @param Alpha Blend alpha (0 = From, 1 = To)
	 * @return Blended transform
	 */
	FORCEINLINE FTransform GetBlendedGripTransform(int32 FromIndex, int32 ToIndex, float Alpha) const
	{
		const FTransform FromTransform = GetLeftHandGripTransform(FromIndex);
		const FTransform ToTransform = GetLeftHandGripTransform(ToIndex);

		FTransform Result;
		Result.Blend(FromTransform, ToTransform, Alpha);
		return Result;
	}

	/**
	 * Calculate Explicit Time for Sequence Evaluator from pose index.
	 *
	 * Formula: ExplicitTime = 0.02 + (PoseIndex * 0.03)
	 *
	 * @param PoseIndex Index of the pose in GripPoses composite
	 * @return Explicit time value for Sequence Evaluator
	 */
	FORCEINLINE static float GetExplicitTimeFromPoseIndex(int32 PoseIndex)
	{
		return 0.02f + (static_cast<float>(PoseIndex) * 0.03f);
	}

	/**
	 * Check if this animation data is valid (has at least Stance).
	 */
	FORCEINLINE bool IsValid() const
	{
		return Stance != nullptr;
	}
};

// ═══════════════════════════════════════════════════════════════════════════
// HELPER FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

/**
 * FSuspenseCoreAnimationHelpers
 *
 * Static helper functions for animation system.
 * Provides mapping from WeaponArchetype GameplayTag to DataTable row name.
 */
struct BRIDGESYSTEM_API FSuspenseCoreAnimationHelpers
{
	/**
	 * Map WeaponArchetype GameplayTag to DataTable row name.
	 *
	 * Mapping:
	 * - Weapon.Rifle.* (except Sniper) → SMG
	 * - Weapon.SMG.* → SMG
	 * - Weapon.Pistol.* → Pistol
	 * - Weapon.Shotgun.* → Shotgun
	 * - *Sniper* → Sniper
	 * - Weapon.Melee.Knife → Knife
	 * - Weapon.Melee.* → Special
	 * - Weapon.Heavy.* → Special
	 * - Weapon.Grenade.Frag → Frag
	 * - Weapon.Grenade.Smoke → Frag (can be customized)
	 * - Weapon.Grenade.Flash → Frag (can be customized)
	 * - Weapon.Grenade.Incendiary → Frag (can be customized)
	 * - Weapon.Grenade.* → Frag
	 * - Weapon.Throwable.* → Frag (legacy)
	 * - Item.Throwable.* → Frag (item tags)
	 * - Default → SMG
	 *
	 * @param WeaponArchetype The weapon archetype GameplayTag
	 * @return Row name for DataTable lookup
	 */
	static FName GetRowNameFromWeaponArchetype(const FGameplayTag& WeaponArchetype)
	{
		if (!WeaponArchetype.IsValid())
		{
			return NAME_None;
		}

		const FString TagString = WeaponArchetype.ToString();

		// Order matters! Sniper check BEFORE general Rifle check
		if (TagString.Contains(TEXT("Sniper")))
		{
			return FName("Sniper");
		}

		if (TagString.Contains(TEXT("Weapon.Rifle")))
		{
			return FName("SMG");
		}

		if (TagString.Contains(TEXT("Weapon.SMG")))
		{
			return FName("SMG");
		}

		if (TagString.Contains(TEXT("Weapon.Pistol")))
		{
			return FName("Pistol");
		}

		if (TagString.Contains(TEXT("Weapon.Shotgun")))
		{
			return FName("Shotgun");
		}

		if (TagString.Contains(TEXT("Weapon.Melee.Knife")))
		{
			return FName("Knife");
		}

		if (TagString.Contains(TEXT("Weapon.Melee")))
		{
			return FName("Special");
		}

		if (TagString.Contains(TEXT("Weapon.Heavy")))
		{
			return FName("Special");
		}

		// Grenade types - check specific types first for future extensibility
		// Tags: Weapon.Grenade.Frag, Weapon.Grenade.Smoke, Weapon.Grenade.Flash, etc.
		if (TagString.Contains(TEXT("Weapon.Grenade.Frag")))
		{
			return FName("Frag");
		}

		if (TagString.Contains(TEXT("Weapon.Grenade.Smoke")))
		{
			return FName("Frag");  // Can be changed to "Smoke" if separate animations needed
		}

		if (TagString.Contains(TEXT("Weapon.Grenade.Flash")))
		{
			return FName("Frag");  // Can be changed to "Flash" if separate animations needed
		}

		if (TagString.Contains(TEXT("Weapon.Grenade.Incendiary")))
		{
			return FName("Frag");  // Can be changed to "Incendiary" if separate animations needed
		}

		// Fallback for any grenade/throwable type
		// Covers: Weapon.Grenade.*, Weapon.Throwable.*, Item.Throwable.*
		if (TagString.Contains(TEXT("Weapon.Grenade")) ||
			TagString.Contains(TEXT("Weapon.Throwable")) ||
			TagString.Contains(TEXT("Item.Throwable")))
		{
			return FName("Frag");
		}

		// Default fallback
		return FName("SMG");
	}

	/**
	 * Get all valid row names for the animation DataTable.
	 */
	static TArray<FName> GetAllValidRowNames()
	{
		return TArray<FName>{
			FName("SMG"),
			FName("Pistol"),
			FName("Shotgun"),
			FName("Sniper"),
			FName("Knife"),
			FName("Special"),
			FName("Frag")
		};
	}
};


