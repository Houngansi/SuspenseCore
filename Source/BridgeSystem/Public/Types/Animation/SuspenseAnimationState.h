#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimComposite.h"
#include "SuspenseAnimationState.generated.h"

/**
 * Структура анимационных данных для оружия
 * Содержит все анимации и трансформации для конкретного типа оружия
 * Использует прямые ссылки на анимационные ресурсы для безопасности потоков
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FAnimationStateData : public FTableRowBase
{
	GENERATED_BODY()

	FAnimationStateData()
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
		, AimPoseAlpha(1.0f)
	{
		// Инициализируем массив трансформаций хвата с одним элементом по умолчанию
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

	// Альфа для позы прицеливания - Float
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimPoseAlpha;

	/**
	 * Получает трансформацию хвата левой руки по индексу
	 * @param Index индекс позиции хвата (0 = базовая позиция)
	 * @return трансформация или Identity если индекс неверный
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
	 * Устанавливает трансформацию хвата левой руки по индексу
	 * @param Index индекс позиции хвата
	 * @param Transform новая трансформация
	 */
	FORCEINLINE void SetLeftHandGripTransform(int32 Index, const FTransform& Transform)
	{
		// Расширяем массив если необходимо
		while (LHGripTransform.Num() <= Index)
		{
			LHGripTransform.Add(FTransform::Identity);
		}
		LHGripTransform[Index] = Transform;
	}
};
