// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "ISuspenseCoreAmmoCounterWidget.generated.h"

/**
 * Enum for reload types display
 */
UENUM(BlueprintType)
enum class ESuspenseCoreReloadTypeDisplay : uint8
{
	None			UMETA(DisplayName = "None"),
	Tactical		UMETA(DisplayName = "Tactical"),
	Empty			UMETA(DisplayName = "Empty"),
	Emergency		UMETA(DisplayName = "Emergency"),
	ChamberOnly		UMETA(DisplayName = "Chamber Only")
};

/**
 * Data structure for ammo counter display (Tarkov-style)
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreAmmoCounterData
{
	GENERATED_BODY()

	//================================================
	// Magazine State
	//================================================

	/** Current rounds in magazine */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo|Magazine")
	int32 MagazineRounds = 0;

	/** Magazine max capacity */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo|Magazine")
	int32 MagazineCapacity = 0;

	/** Magazine ID (for display name lookup) */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo|Magazine")
	FName MagazineID;

	/** Is magazine inserted? */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo|Magazine")
	bool bHasMagazine = false;

	//================================================
	// Chamber State
	//================================================

	/** Is there a round in chamber? */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo|Chamber")
	bool bHasChamberedRound = false;

	/** Chambered round ammo type */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo|Chamber")
	FName ChamberedAmmoType;

	//================================================
	// Loaded Ammo
	//================================================

	/** Ammo type loaded in magazine */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo|Type")
	FName LoadedAmmoType;

	/** Ammo type display name */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo|Type")
	FText AmmoDisplayName;

	/** Ammo caliber tag */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo|Type")
	FGameplayTag AmmoCaliberTag;

	//================================================
	// Reserve Ammo
	//================================================

	/** Total reserve rounds (same caliber, in inventory + quickslots) */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo|Reserve")
	int32 ReserveRounds = 0;

	/** Available magazines count (same caliber) */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo|Reserve")
	int32 AvailableMagazines = 0;

	//================================================
	// Fire Mode
	//================================================

	/** Current fire mode tag */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo|FireMode")
	FGameplayTag FireModeTag;

	/** Fire mode display text */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo|FireMode")
	FText FireModeText;

	//================================================
	// Weapon Info
	//================================================

	/** Weapon ID */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo|Weapon")
	FName WeaponID;

	/** Weapon display name */
	UPROPERTY(BlueprintReadOnly, Category = "Ammo|Weapon")
	FText WeaponDisplayName;

	//================================================
	// Helpers
	//================================================

	/** Get total ready rounds (mag + chamber) */
	int32 GetTotalReadyRounds() const
	{
		return MagazineRounds + (bHasChamberedRound ? 1 : 0);
	}

	/** Is weapon ready to fire? */
	bool IsReadyToFire() const
	{
		return bHasChamberedRound;
	}

	/** Is magazine empty? */
	bool IsMagazineEmpty() const
	{
		return bHasMagazine && MagazineRounds <= 0;
	}

	/** Get magazine fill percentage (0-1) */
	float GetMagazineFillPercent() const
	{
		if (MagazineCapacity <= 0) return 0.0f;
		return FMath::Clamp((float)MagazineRounds / (float)MagazineCapacity, 0.0f, 1.0f);
	}

	/** Is ammo low? (below 25%) */
	bool IsAmmoLow() const
	{
		return bHasMagazine && GetMagazineFillPercent() < 0.25f;
	}

	/** Is ammo critical? (below 10% or <= 3 rounds) */
	bool IsAmmoCritical() const
	{
		return bHasMagazine && (GetMagazineFillPercent() < 0.1f || MagazineRounds <= 3);
	}
};

UINTERFACE(MinimalAPI, BlueprintType)
class USuspenseCoreAmmoCounterWidget : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface for Ammo Counter HUD widget (Tarkov-style)
 *
 * Displays:
 * - Magazine rounds / capacity (e.g., "30/30")
 * - Chamber indicator (+1 when chambered)
 * - Reserve ammo count
 * - Loaded ammo type (e.g., "5.45 PS")
 * - Fire mode indicator
 *
 * EventBus Events (subscribed):
 * - TAG_Equipment_Event_Magazine_Inserted
 * - TAG_Equipment_Event_Magazine_Ejected
 * - TAG_Equipment_Event_Magazine_RoundsChanged
 * - TAG_Equipment_Event_Weapon_AmmoChanged
 * - TAG_Equipment_Event_Weapon_FireModeChanged
 */
class BRIDGESYSTEM_API ISuspenseCoreAmmoCounterWidget
{
	GENERATED_BODY()

public:
	//================================================
	// Initialization
	//================================================

	/**
	 * Initialize with weapon actor
	 * @param WeaponActor The currently equipped weapon
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|AmmoCounter")
	void InitializeWithWeapon(AActor* WeaponActor);

	/**
	 * Clear weapon reference (holstered/unequipped)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|AmmoCounter")
	void ClearWeapon();

	//================================================
	// State Updates
	//================================================

	/**
	 * Update full ammo counter state
	 * @param AmmoData Complete ammo state data
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|AmmoCounter")
	void UpdateAmmoCounter(const FSuspenseCoreAmmoCounterData& AmmoData);

	/**
	 * Update magazine state only
	 * @param CurrentRounds Rounds in magazine
	 * @param MaxRounds Magazine capacity
	 * @param bChambered Is there a round chambered?
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|AmmoCounter")
	void UpdateMagazineState(int32 CurrentRounds, int32 MaxRounds, bool bChambered);

	/**
	 * Update reserve ammo display
	 * @param ReserveRounds Total reserve rounds
	 * @param AvailableMags Number of available magazines
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|AmmoCounter")
	void UpdateReserveAmmo(int32 ReserveRounds, int32 AvailableMags);

	/**
	 * Update fire mode display
	 * @param FireModeTag Fire mode gameplay tag
	 * @param DisplayText Fire mode display text
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|AmmoCounter")
	void UpdateFireMode(FGameplayTag FireModeTag, const FText& DisplayText);

	/**
	 * Update ammo type display
	 * @param AmmoID Ammo type ID
	 * @param DisplayName Ammo display name (e.g., "5.45x39 PS")
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|AmmoCounter")
	void UpdateAmmoType(FName AmmoID, const FText& DisplayName);

	//================================================
	// Visual States
	//================================================

	/**
	 * Set low ammo warning state
	 * @param bLowAmmo Is ammo low?
	 * @param bCritical Is ammo critical?
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|AmmoCounter")
	void SetLowAmmoWarning(bool bLowAmmo, bool bCritical);

	/**
	 * Show no magazine state
	 * @param bNoMagazine True when no magazine is inserted
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|AmmoCounter")
	void SetNoMagazineState(bool bNoMagazine);

	/**
	 * Play ammo consumed animation (on fire)
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|AmmoCounter")
	void PlayAmmoConsumedAnimation();

	/**
	 * Play magazine swap animation
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|AmmoCounter")
	void PlayMagazineSwapAnimation();

	//================================================
	// Visibility
	//================================================

	/**
	 * Show or hide the ammo counter
	 * @param bVisible Should be visible?
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|AmmoCounter")
	void SetAmmoCounterVisible(bool bVisible);

	/**
	 * Check if ammo counter is visible
	 * @return True if visible
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "UI|AmmoCounter")
	bool IsAmmoCounterVisible() const;
};
