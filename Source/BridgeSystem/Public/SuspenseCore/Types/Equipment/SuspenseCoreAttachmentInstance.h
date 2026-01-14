// SuspenseCoreAttachmentInstance.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.
//
// Runtime instance of a weapon attachment.
// Links to SSOT DataTable via AttachmentID.
//
// @see Documentation/Plans/TarkovStyle_Recoil_System_Design.md Section 5.3
// @see FSuspenseCoreAttachmentAttributeRow - SSOT data structure

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreAttachmentInstance.generated.h"

/**
 * FSuspenseCoreAttachmentInstance
 *
 * Runtime instance of an attachment installed on a weapon.
 * This is the "live" instance - SSOT data comes from FSuspenseCoreAttachmentAttributeRow.
 *
 * USAGE:
 * ═══════════════════════════════════════════════════════════════════════════
 * 1. Player installs attachment on weapon
 * 2. Create FSuspenseCoreAttachmentInstance with AttachmentID
 * 3. Store in weapon's TArray<FSuspenseCoreAttachmentInstance>
 * 4. On fire: iterate attachments, lookup SSOT data, multiply modifiers
 *
 * EXAMPLE:
 * ═══════════════════════════════════════════════════════════════════════════
 * // Installing a suppressor
 * FSuspenseCoreAttachmentInstance Suppressor;
 * Suppressor.AttachmentID = FName("PBS1_Suppressor");
 * Suppressor.AttachedSlot = FGameplayTag::RequestGameplayTag(FName("Equipment.Slot.Muzzle"));
 * Suppressor.bIsInstalled = true;
 * Suppressor.InstanceGuid = FGuid::NewGuid();
 * WeaponActor->InstalledAttachments.Add(Suppressor);
 *
 * @see FSuspenseCoreAttachmentAttributeRow - SSOT structure
 * @see USuspenseCoreBaseFireAbility::CalculateAttachmentRecoilModifier()
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreAttachmentInstance
{
	GENERATED_BODY()

	//========================================================================
	// Identity (SSOT Link)
	//========================================================================

	/** SSOT link - row name in DT_AttachmentAttributes DataTable
	 *  This is the key to lookup static attachment data */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
	FName AttachmentID;

	/** Unique runtime instance ID (for inventory/persistence)
	 *  Each physical attachment item has unique GUID */
	UPROPERTY(BlueprintReadOnly, Category = "Identity")
	FGuid InstanceGuid;

	//========================================================================
	// Installation State
	//========================================================================

	/** Which slot this attachment is installed in
	 *  e.g., Equipment.Slot.Muzzle, Equipment.Slot.Stock */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State",
		meta = (Categories = "Equipment.Slot"))
	FGameplayTag AttachedSlot;

	/** Whether this attachment is currently installed on a weapon */
	UPROPERTY(BlueprintReadOnly, Category = "State")
	bool bIsInstalled = false;

	//========================================================================
	// Runtime State
	//========================================================================

	/** Current durability of this specific attachment instance (0-100)
	 *  Attachments can degrade separately from weapon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State",
		meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float CurrentDurability = 100.0f;

	//========================================================================
	// Constructors
	//========================================================================

	FSuspenseCoreAttachmentInstance()
		: AttachmentID(NAME_None)
		, InstanceGuid()
		, AttachedSlot()
		, bIsInstalled(false)
		, CurrentDurability(100.0f)
	{
	}

	FSuspenseCoreAttachmentInstance(const FName& InAttachmentID)
		: AttachmentID(InAttachmentID)
		, InstanceGuid(FGuid::NewGuid())
		, AttachedSlot()
		, bIsInstalled(false)
		, CurrentDurability(100.0f)
	{
	}

	//========================================================================
	// Helper Methods
	//========================================================================

	/** Check if this instance has valid data */
	bool IsValid() const
	{
		return !AttachmentID.IsNone();
	}

	/** Check if this attachment is currently installed and functional */
	bool IsInstalled() const
	{
		return bIsInstalled && IsValid();
	}

	/** Check if attachment is damaged (below 50% durability) */
	bool IsDamaged() const
	{
		return CurrentDurability < 50.0f;
	}

	/** Check if attachment is broken (0% durability) */
	bool IsBroken() const
	{
		return CurrentDurability <= 0.0f;
	}

	/** Apply durability damage to attachment
	 *  @param DamageAmount Amount of durability to remove
	 *  @return New durability value */
	float ApplyDurabilityDamage(float DamageAmount)
	{
		CurrentDurability = FMath::Max(0.0f, CurrentDurability - DamageAmount);
		return CurrentDurability;
	}

	/** Reset to new condition */
	void ResetDurability()
	{
		CurrentDurability = 100.0f;
	}

	/** Install this attachment to a slot
	 *  @param Slot The equipment slot to install to */
	void Install(const FGameplayTag& Slot)
	{
		AttachedSlot = Slot;
		bIsInstalled = true;
	}

	/** Uninstall this attachment from weapon */
	void Uninstall()
	{
		AttachedSlot = FGameplayTag();
		bIsInstalled = false;
	}

	//========================================================================
	// Operators
	//========================================================================

	bool operator==(const FSuspenseCoreAttachmentInstance& Other) const
	{
		return InstanceGuid == Other.InstanceGuid;
	}

	bool operator!=(const FSuspenseCoreAttachmentInstance& Other) const
	{
		return !(*this == Other);
	}
};


/**
 * FSuspenseCoreInstalledAttachments
 *
 * Container for all attachments installed on a weapon.
 * Provides helper methods to access attachments by slot type.
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreInstalledAttachments
{
	GENERATED_BODY()

	/** All installed attachments */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachments")
	TArray<FSuspenseCoreAttachmentInstance> Attachments;

	//========================================================================
	// Accessors
	//========================================================================

	/** Get attachment installed in specific slot
	 *  @param SlotType Slot to check (e.g., Equipment.Slot.Muzzle)
	 *  @return Pointer to attachment or nullptr if slot empty */
	const FSuspenseCoreAttachmentInstance* GetAttachmentInSlot(const FGameplayTag& SlotType) const
	{
		for (const FSuspenseCoreAttachmentInstance& Attachment : Attachments)
		{
			if (Attachment.bIsInstalled && Attachment.AttachedSlot.MatchesTag(SlotType))
			{
				return &Attachment;
			}
		}
		return nullptr;
	}

	/** Check if slot has an attachment installed */
	bool HasAttachmentInSlot(const FGameplayTag& SlotType) const
	{
		return GetAttachmentInSlot(SlotType) != nullptr;
	}

	/** Get all installed attachment IDs (for SSOT lookup) */
	TArray<FName> GetInstalledAttachmentIDs() const
	{
		TArray<FName> Result;
		for (const FSuspenseCoreAttachmentInstance& Attachment : Attachments)
		{
			if (Attachment.IsInstalled())
			{
				Result.Add(Attachment.AttachmentID);
			}
		}
		return Result;
	}

	/** Count installed attachments */
	int32 GetInstalledCount() const
	{
		int32 Count = 0;
		for (const FSuspenseCoreAttachmentInstance& Attachment : Attachments)
		{
			if (Attachment.IsInstalled())
			{
				Count++;
			}
		}
		return Count;
	}

	/** Check if any attachment is installed */
	bool HasAnyAttachments() const
	{
		return GetInstalledCount() > 0;
	}

	//========================================================================
	// Modification
	//========================================================================

	/** Install attachment to slot (replaces existing if present)
	 *  @param Attachment Attachment to install
	 *  @param SlotType Slot to install to
	 *  @return true if installed successfully */
	bool InstallAttachment(FSuspenseCoreAttachmentInstance& Attachment, const FGameplayTag& SlotType)
	{
		// Remove any existing attachment in this slot
		RemoveAttachmentFromSlot(SlotType);

		// Install new attachment
		Attachment.Install(SlotType);
		Attachments.Add(Attachment);
		return true;
	}

	/** Remove attachment from slot
	 *  @param SlotType Slot to remove from
	 *  @return Removed attachment instance (invalid if slot was empty) */
	FSuspenseCoreAttachmentInstance RemoveAttachmentFromSlot(const FGameplayTag& SlotType)
	{
		for (int32 i = Attachments.Num() - 1; i >= 0; --i)
		{
			if (Attachments[i].AttachedSlot.MatchesTag(SlotType))
			{
				FSuspenseCoreAttachmentInstance Removed = Attachments[i];
				Removed.Uninstall();
				Attachments.RemoveAt(i);
				return Removed;
			}
		}
		return FSuspenseCoreAttachmentInstance();
	}

	/** Clear all attachments */
	void ClearAll()
	{
		Attachments.Empty();
	}
};
