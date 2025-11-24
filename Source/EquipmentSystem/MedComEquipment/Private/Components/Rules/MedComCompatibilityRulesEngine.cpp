// MedComCompatibilityRulesEngine.cpp © MedCom Team

#include "Components/Rules/MedComCompatibilityRulesEngine.h"
#include "Components/Validation/MedComEquipmentSlotValidator.h"
#include "Types/Loadout/MedComItemDataTable.h"

DEFINE_LOG_CATEGORY(LogCompatibilityRules);

UMedComCompatibilityRulesEngine::UMedComCompatibilityRulesEngine()
{
}

void UMedComCompatibilityRulesEngine::SetSlotValidator(UMedComEquipmentSlotValidator* InValidator)
{
	SlotValidator = InValidator;
}

void UMedComCompatibilityRulesEngine::SetItemDataProvider(TSharedPtr<IMedComItemDataProvider> InItemProvider)
{
	ItemProvider = MoveTemp(InItemProvider);
}

void UMedComCompatibilityRulesEngine::SetDefaultEquipmentDataProvider(TScriptInterface<IMedComEquipmentDataProvider> InProvider)
{
	DefaultEquipProvider = InProvider;
}

FMedComRuleCheckResult UMedComCompatibilityRulesEngine::Convert(const FSlotValidationResult& R)
{
	if (R.bIsValid)
	{
		FMedComRuleCheckResult Ok = FMedComRuleCheckResult::Success();
		Ok.RuleType = EMedComRuleType::Compatibility;
		Ok.RuleTag = R.ErrorTag.IsValid() ? R.ErrorTag : FGameplayTag::RequestGameplayTag(TEXT("Rule.Compatibility.OK"));
		Ok.Message = NSLOCTEXT("CompatibilityRules", "CompatPass", "Compatible");
		Ok.Severity = EMedComRuleSeverity::Info;
		Ok.ConfidenceScore = 1.0f;
		return Ok;
	}

	EMedComRuleSeverity Sev = EMedComRuleSeverity::Error;
	switch (R.FailureType)
	{
	case EEquipmentValidationFailure::InvalidSlot:
	case EEquipmentValidationFailure::UniqueConstraint:
	case EEquipmentValidationFailure::IncompatibleType:
		Sev = EMedComRuleSeverity::Critical; break;
	case EEquipmentValidationFailure::RequirementsNotMet:
	case EEquipmentValidationFailure::WeightLimit:
	case EEquipmentValidationFailure::LevelRequirement:
		Sev = EMedComRuleSeverity::Error; break;
	default:
		Sev = EMedComRuleSeverity::Error; break;
	}

	FMedComRuleCheckResult Fail = FMedComRuleCheckResult::Failure(
		R.ErrorMessage.IsEmpty() ? NSLOCTEXT("CompatibilityRules", "CompatFail", "Incompatible") : R.ErrorMessage,
		Sev);
	Fail.RuleType = EMedComRuleType::Compatibility;
	Fail.RuleTag = R.ErrorTag.IsValid() ? R.ErrorTag : FGameplayTag::RequestGameplayTag(TEXT("Rule.Compatibility.Fail"));

	// Copy context
	for (const auto& Kvp : R.Context)
	{
		Fail.Context.Add(Kvp.Key, Kvp.Value);
	}
	return Fail;
}

bool UMedComCompatibilityRulesEngine::GetItemData(FName ItemID, FMedComUnifiedItemData& OutData) const
{
	return ItemProvider.IsValid() ? ItemProvider->GetUnifiedItemData(ItemID, OutData) : false;
}

FMedComRuleCheckResult UMedComCompatibilityRulesEngine::CheckItemCompatibility(
	const FInventoryItemInstance& ItemInstance,
	const FEquipmentSlotConfig& SlotConfig) const
{
	// Base: delegate to SlotValidator (public API, no access to protected methods)
	if (IsValid(SlotValidator))
	{
		const FSlotValidationResult VR = SlotValidator->CanPlaceItemInSlot(SlotConfig, ItemInstance);
		FMedComRuleCheckResult R = Convert(VR);
		if (!R.bPassed) { return R; } // short-circuit on hard fail
	}

	// Additional soft checks that are not duplicated in the validator
	FMedComUnifiedItemData ItemData;
	if (!GetItemData(ItemInstance.ItemID, ItemData))
	{
		FMedComRuleCheckResult R = FMedComRuleCheckResult::Failure(
			NSLOCTEXT("CompatibilityRules", "ItemDataNotFound", "Item data not found"),
			EMedComRuleSeverity::Error);
		R.RuleType = EMedComRuleType::Compatibility;
		return R;
	}

	// Slot type filter using current config API (Allowed / Disallowed)
	if (!SlotConfig.CanEquipItemType(ItemData.ItemType))
	{
		FMedComRuleCheckResult R = FMedComRuleCheckResult::Failure(
			NSLOCTEXT("CompatibilityRules", "TypeMismatch", "Item type is not allowed in this slot"),
			EMedComRuleSeverity::Error);
		R.RuleType = EMedComRuleType::Compatibility;
		return R;
	}

	// Soft check: broken item is not equippable (override disabled)
	const float Durability = ItemInstance.GetDurabilityPercent();
	if (Durability <= 0.0f)
	{
		FMedComRuleCheckResult R = FMedComRuleCheckResult::Failure(
			NSLOCTEXT("CompatibilityRules", "ItemBroken", "Cannot equip broken items"),
			EMedComRuleSeverity::Error);
		R.RuleType = EMedComRuleType::Compatibility;
		R.bCanOverride = false;
		R.Context.Add(TEXT("Durability"), FString::Printf(TEXT("%.1f%%"), Durability * 100.0f));
		return R;
	}
	else if (Durability < 0.2f)
	{
		FMedComRuleCheckResult W = FMedComRuleCheckResult::Success();
		W.RuleType = EMedComRuleType::Compatibility;
		W.Severity = EMedComRuleSeverity::Warning;
		W.Message = FText::Format(NSLOCTEXT("CompatibilityRules", "LowDurability", "Warning: low durability ({0}%)"),
			FMath::RoundToInt(Durability * 100.0f));
		W.ConfidenceScore = 0.7f;
		W.bCanOverride = true;
		return W;
	}

	FMedComRuleCheckResult Ok = FMedComRuleCheckResult::Success();
	Ok.RuleType = EMedComRuleType::Compatibility;
	Ok.Message = NSLOCTEXT("CompatibilityRules", "Compatible", "Compatible");
	Ok.ConfidenceScore = 1.0f;
	return Ok;
}

FMedComRuleCheckResult UMedComCompatibilityRulesEngine::CheckTypeCompatibility(
	const FGameplayTag& ItemType,
	const FEquipmentSlotConfig& SlotConfig) const
{
	// Simple gate: Allowed/Disallowed sets only (validator covers strict rules)
	const bool bTypeAllowed = SlotConfig.AllowedItemTypes.IsEmpty() || SlotConfig.AllowedItemTypes.HasTag(ItemType);
	if (!bTypeAllowed)
	{
		FMedComRuleCheckResult R = FMedComRuleCheckResult::Failure(
			FText::Format(NSLOCTEXT("CompatibilityRules", "TypeNotAllowed", "Item type {0} is not allowed"),
				FText::FromString(ItemType.ToString())),
			EMedComRuleSeverity::Error);
		R.RuleType = EMedComRuleType::Compatibility;
		return R;
	}

	// Disallowed types (exact match)
	if (!SlotConfig.DisallowedItemTypes.IsEmpty() && SlotConfig.DisallowedItemTypes.HasTagExact(ItemType))
	{
		FMedComRuleCheckResult R = FMedComRuleCheckResult::Failure(
			FText::Format(NSLOCTEXT("CompatibilityRules", "TypeBlocked", "Item type {0} is disallowed"),
				FText::FromString(ItemType.ToString())),
			EMedComRuleSeverity::Error);
		R.RuleType = EMedComRuleType::Compatibility;
		return R;
	}

	FMedComRuleCheckResult Ok = FMedComRuleCheckResult::Success();
	Ok.RuleType = EMedComRuleType::Compatibility;
	Ok.Message = NSLOCTEXT("CompatibilityRules", "TypeCompatible", "Item type is compatible with slot");
	Ok.ConfidenceScore = 1.0f;
	return Ok;
}

FMedComAggregatedRuleResult UMedComCompatibilityRulesEngine::EvaluateCompatibilityRules(
	const FMedComRuleContext& Context) const
{
	FMedComAggregatedRuleResult Agg;

	// Resolve equipment data provider ONLY from default DI
	TScriptInterface<IMedComEquipmentDataProvider> EquipProvider = DefaultEquipProvider;
	if (!EquipProvider.GetInterface())
	{
		FMedComRuleCheckResult R = FMedComRuleCheckResult::Failure(
			NSLOCTEXT("CompatibilityRules", "NoDataProvider", "No equipment data provider"),
			EMedComRuleSeverity::Error);
		R.RuleType = EMedComRuleType::Compatibility;
		Agg.AddResult(R);
		return Agg;
	}

	// Resolve SlotConfig by index
	if (Context.TargetSlotIndex == INDEX_NONE)
	{
		FMedComRuleCheckResult R = FMedComRuleCheckResult::Failure(
			NSLOCTEXT("CompatibilityRules", "NoTargetSlot", "No target slot specified"),
			EMedComRuleSeverity::Error);
		R.RuleType = EMedComRuleType::Compatibility;
		Agg.AddResult(R);
		return Agg;
	}
	const FEquipmentSlotConfig SlotCfg = EquipProvider.GetInterface()->GetSlotConfiguration(Context.TargetSlotIndex);

	// Base hard checks via SlotValidator (short-circuit on fail)
	if (IsValid(SlotValidator))
	{
		const FSlotValidationResult VR = SlotValidator->CanPlaceItemInSlot(SlotCfg, Context.ItemInstance);
		FMedComRuleCheckResult R = Convert(VR);
		Agg.AddResult(R);
		if (!R.bPassed)
		{
			return Agg; // stop on hard fail
		}
	}

	// Soft/UX checks that do not require extra fields in SlotConfig (durability only)
	{
		const float Durability = Context.ItemInstance.GetDurabilityPercent();
		if (Durability < 0.2f && Durability > 0.0f)
		{
			FMedComRuleCheckResult W = FMedComRuleCheckResult::Success();
			W.RuleType = EMedComRuleType::Compatibility;
			W.Severity = EMedComRuleSeverity::Warning;
			W.Message = FText::Format(NSLOCTEXT("CompatibilityRules", "LowDurability", "Warning: low durability ({0}%)"),
				FMath::RoundToInt(Durability * 100.0f));
			W.ConfidenceScore = 0.7f;
			W.bCanOverride = true;
			Agg.AddResult(W);
		}
	}

	return Agg;
}

TArray<int32> UMedComCompatibilityRulesEngine::FindCompatibleSlots(
	const FInventoryItemInstance& ItemInstance,
	const TArray<FEquipmentSlotConfig>& AvailableSlots) const
{
	TArray<int32> Out;

	for (int32 i = 0; i < AvailableSlots.Num(); ++i)
	{
		const FEquipmentSlotConfig& SlotCfg = AvailableSlots[i];

		if (IsValid(SlotValidator))
		{
			const FSlotValidationResult VR = SlotValidator->CanPlaceItemInSlot(SlotCfg, ItemInstance);
			if (VR.bIsValid) { Out.Add(i); }
		}
		else
		{
			// Fallback: light gate using Allowed/Disallowed against item type
			FMedComUnifiedItemData ItemData;
			if (GetItemData(ItemInstance.ItemID, ItemData))
			{
				if (SlotCfg.CanEquipItemType(ItemData.ItemType))
				{
					Out.Add(i);
				}
			}
		}
	}
	return Out;
}

float UMedComCompatibilityRulesEngine::GetCompatibilityScore(
	const FInventoryItemInstance& ItemInstance,
	const FEquipmentSlotConfig& SlotConfig) const
{
	// Start from base check confidence
	float Score = 1.0f;
	if (IsValid(SlotValidator))
	{
		const FSlotValidationResult VR = SlotValidator->CanPlaceItemInSlot(SlotConfig, ItemInstance);
		if (!VR.bIsValid) { return 0.0f; }
	}

	FMedComUnifiedItemData ItemData;
	if (!GetItemData(ItemInstance.ItemID, ItemData))
	{
		return 0.5f; // uncertain without item meta
	}

	// If slot cannot equip this item type by config — zero score
	if (!SlotConfig.CanEquipItemType(ItemData.ItemType))
	{
		return 0.0f;
	}

	// Bonus for exact slot tag match with item-required slot
	if (ItemData.EquipmentSlot == SlotConfig.SlotTag)
	{
		Score *= 1.15f;
	}

	// Durability factor
	const float Durability = ItemInstance.GetDurabilityPercent();
	Score *= FMath::Lerp(0.6f, 1.0f, FMath::Clamp(Durability, 0.0f, 1.0f));

	return FMath::Clamp(Score, 0.0f, 1.0f);
}
