// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Weapon/SuspenseCoreMagazineTypes.h"
#include "ISuspenseCoreReloadProgressWidget.generated.h"

// ESuspenseCoreReloadType is defined in SuspenseCoreMagazineTypes.h

/**
 * Data structure for reload progress display
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreReloadProgressData
{
	GENERATED_BODY()

	/** Reload type being performed */
	UPROPERTY(BlueprintReadOnly, Category = "Reload")
	ESuspenseCoreReloadType ReloadType = ESuspenseCoreReloadType::None;

	/** Total reload duration in seconds */
	UPROPERTY(BlueprintReadOnly, Category = "Reload")
	float TotalDuration = 0.0f;

	/** Elapsed time since reload started */
	UPROPERTY(BlueprintReadOnly, Category = "Reload")
	float ElapsedTime = 0.0f;

	/** New magazine info (if applicable) */
	UPROPERTY(BlueprintReadOnly, Category = "Reload")
	FName NewMagazineID;

	/** Rounds in new magazine */
	UPROPERTY(BlueprintReadOnly, Category = "Reload")
	int32 NewMagazineRounds = 0;

	/** Can reload be cancelled at this point? */
	UPROPERTY(BlueprintReadOnly, Category = "Reload")
	bool bCanCancel = true;

	/** Is this a quick reload (emergency)? */
	UPROPERTY(BlueprintReadOnly, Category = "Reload")
	bool bIsQuickReload = false;

	/** Get progress as 0-1 */
	float GetProgress() const
	{
		if (TotalDuration <= 0.0f) return 0.0f;
		return FMath::Clamp(ElapsedTime / TotalDuration, 0.0f, 1.0f);
	}

	/** Get remaining time */
	float GetRemainingTime() const
	{
		return FMath::Max(0.0f, TotalDuration - ElapsedTime);
	}

	/** Is reload complete? */
	bool IsComplete() const
	{
		return ElapsedTime >= TotalDuration;
	}

	/** Get reload type display text */
	FText GetReloadTypeText() const
	{
		switch (ReloadType)
		{
		case ESuspenseCoreReloadType::Tactical:
			return NSLOCTEXT("Reload", "Tactical", "Tactical");
		case ESuspenseCoreReloadType::Empty:
			return NSLOCTEXT("Reload", "Empty", "Full Reload");
		case ESuspenseCoreReloadType::Emergency:
			return NSLOCTEXT("Reload", "Emergency", "Emergency");
		case ESuspenseCoreReloadType::ChamberOnly:
			return NSLOCTEXT("Reload", "Chamber", "Chambering");
		default:
			return FText::GetEmpty();
		}
	}
};

UINTERFACE(MinimalAPI, BlueprintType)
class UISuspenseCoreReloadProgressWidget : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for Reload Progress HUD widget
 *
 * Displays:
 * - Reload progress bar
 * - Reload type text (Tactical/Empty/Emergency/Chamber)
 * - Time remaining
 * - Cancel hint (if applicable)
 *
 * EventBus Events (subscribed):
 * - TAG_Equipment_Event_Weapon_ReloadStart
 * - TAG_Equipment_Event_Weapon_ReloadEnd
 * - TAG_Equipment_Event_Reload_Tactical
 * - TAG_Equipment_Event_Reload_Empty
 * - TAG_Equipment_Event_Reload_Emergency
 */
class BRIDGESYSTEM_API ISuspenseCoreReloadProgressWidget
{
	GENERATED_BODY()

public:
	//================================================
	// Reload State
	//================================================

	/**
	 * Show reload progress
	 * @param ReloadData Initial reload data
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Reload")
	void ShowReloadProgress(const FSuspenseCoreReloadProgressData& ReloadData);

	/**
	 * Update reload progress
	 * @param Progress Current progress (0-1)
	 * @param RemainingTime Time remaining in seconds
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Reload")
	void UpdateReloadProgress(float Progress, float RemainingTime);

	/**
	 * Hide reload progress (completed or cancelled)
	 * @param bCompleted True if reload completed successfully
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Reload")
	void HideReloadProgress(bool bCompleted);

	//================================================
	// Phase Indicators
	//================================================

	/**
	 * Notify magazine ejected (visual feedback)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Reload")
	void OnMagazineEjected();

	/**
	 * Notify magazine inserted (visual feedback)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Reload")
	void OnMagazineInserted();

	/**
	 * Notify chambering started (visual feedback)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Reload")
	void OnChambering();

	/**
	 * Notify reload cancelled
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Reload")
	void OnReloadCancelled();

	//================================================
	// Configuration
	//================================================

	/**
	 * Set reload type display text
	 * @param ReloadType Type of reload
	 * @param DisplayText Custom display text (optional)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Reload")
	void SetReloadTypeDisplay(ESuspenseCoreReloadType ReloadType, const FText& DisplayText);

	/**
	 * Show or hide cancel hint
	 * @param bCanCancel Can reload be cancelled?
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Reload")
	void SetCanCancelReload(bool bCanCancel);

	//================================================
	// Visibility
	//================================================

	/**
	 * Check if reload progress is currently showing
	 * @return True if showing
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Reload")
	bool IsReloadProgressVisible() const;

	/**
	 * Get current reload progress
	 * @return Current progress (0-1)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|Reload")
	float GetCurrentReloadProgress() const;
};
