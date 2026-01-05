// GE_ItemUse_InProgress.h
// GameplayEffect for item use in-progress state
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// - Duration set via SetByCaller (Data.ItemUse.Duration)
// - Adds State.ItemUse.InProgress tag to owner
// - Provides cooldown-like UI feedback
// - Removed when operation completes/cancels
//
// USAGE:
// - Applied by GA_ItemUse when time-based operation starts
// - UI can query this effect for progress bar
// - Effect duration = operation duration

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Effects/SuspenseCoreEffect.h"
#include "GE_ItemUse_InProgress.generated.h"

/**
 * UGE_ItemUse_InProgress
 *
 * GameplayEffect applied during item use operations.
 * Provides visual feedback and blocks concurrent item uses.
 *
 * CONFIGURATION:
 * - DurationPolicy: HasDuration (SetByCaller)
 * - Duration Tag: Data.ItemUse.Duration
 * - Granted Tag: State.ItemUse.InProgress
 *
 * UI INTEGRATION:
 * - UI can query UAbilitySystemComponent for this effect
 * - GetActiveEffectTimeRemaining() for progress bar
 * - Effect stack count = concurrent operations (typically 1)
 *
 * @see GA_ItemUse
 * @see GE_ItemUse_Cooldown
 */
UCLASS()
class GAS_API UGE_ItemUse_InProgress : public USuspenseCoreEffect
{
	GENERATED_BODY()

public:
	UGE_ItemUse_InProgress(const FObjectInitializer& ObjectInitializer);
};
