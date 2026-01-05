// GE_ItemUse_Cooldown.h
// GameplayEffect for item use cooldown
// Copyright Suspense Team. All Rights Reserved.
//
// ARCHITECTURE:
// - Duration set via SetByCaller (Data.ItemUse.Cooldown)
// - Adds State.ItemUse.Cooldown tag to owner
// - Blocks item use ability activation during cooldown
// - Provides UI cooldown indicator
//
// USAGE:
// - Applied by GA_ItemUse after operation completes
// - GA_ItemUse blocked by State.ItemUse.Cooldown tag
// - UI can query for cooldown remaining time

#pragma once

#include "CoreMinimal.h"
#include "SuspenseCore/Effects/SuspenseCoreEffect.h"
#include "GE_ItemUse_Cooldown.generated.h"

/**
 * UGE_ItemUse_Cooldown
 *
 * GameplayEffect applied after item use to enforce cooldown.
 * Blocks concurrent item uses and provides UI feedback.
 *
 * CONFIGURATION:
 * - DurationPolicy: HasDuration (SetByCaller)
 * - Duration Tag: Data.ItemUse.Cooldown
 * - Granted Tag: State.ItemUse.Cooldown
 *
 * BLOCKING:
 * - GA_ItemUse.ActivationBlockedTags includes State.ItemUse.Cooldown
 * - This prevents rapid reuse of items
 *
 * UI INTEGRATION:
 * - UI can query UAbilitySystemComponent for this effect
 * - GetActiveEffectTimeRemaining() for cooldown indicator
 *
 * @see GA_ItemUse
 * @see GE_ItemUse_InProgress
 */
UCLASS()
class GAS_API UGE_ItemUse_Cooldown : public USuspenseCoreEffect
{
	GENERATED_BODY()

public:
	UGE_ItemUse_Cooldown(const FObjectInitializer& ObjectInitializer);
};
