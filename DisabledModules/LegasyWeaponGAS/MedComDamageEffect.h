#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "MedComDamageEffect.generated.h"

/**
 * Эффект для нанесения урона через GAS.
 * Использует SetByCaller (тег "Data.Damage") для передачи величины урона.
 * ВНИМАНИЕ: сигнатуры методов работы с тегами могут измениться в следующей версии.
 */
UCLASS()
class MEDCOMCORE_API UMedComDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UMedComDamageEffect();

private:
	// Вспомогательный метод для получения изменяемой ссылки на контейнер тегов.
	FGameplayTagContainer& GetMutableAssetTags() { return CachedAssetTags; }
};
