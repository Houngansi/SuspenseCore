// MedComCompatibilityRulesEngine.cpp © MedCom Team

#include "SuspenseCore/Components/Rules/SuspenseCompatibilityRulesEngine.h"
#include "SuspenseCore/Components/Validation/SuspenseEquipmentSlotValidator.h"
#include "Types/Loadout/SuspenseItemDataTable.h"

DEFINE_LOG_CATEGORY(LogCompatibilityRules);

USuspenseCompatibilityRulesEngine::USuspenseCompatibilityRulesEngine()
{
}

void USuspenseCompatibilityRulesEngine::SetSlotValidator(USuspenseEquipmentSlotValidator* InValidator)
{
	SlotValidator = InValidator;
}

void USuspenseCompatibilityRulesEngine::SetItemDataProvider(TSharedPtr<ISuspenseItemDataProvider> InItemProvider)
{
	ItemProvider = MoveTemp(InItemProvider);
}

void USuspenseCompatibilityRulesEngine::SetDefaultEquipmentDataProvider(TScriptInterface<ISuspenseEquipmentDataProvider> InProvider)
{
	DefaultEquipProvider = InProvider;
}

FSuspenseRuleCheckResult USuspenseCompatibilityRulesEngine::Convert(const FSlotValidationResult& R)
{
	if (R.bIsValid)
	{
		FSuspenseRuleCheckResult Ok = FSuspenseRuleCheckResult::Success();
		Ok.RuleType = ESuspenseRuleType::Compatibility;
		Ok.RuleTag = R.ErrorTag.IsValid() ? R.ErrorTag : FGameplayTag::RequestGameplayTag(TEXT("Rule.Compatibility.OK"));
		Ok.Message = NSLOCTEXT("CompatibilityRules", "CompatPass", "Compatible");
		Ok.Severity = ESuspenseRuleSeverity::Info;
		Ok.ConfidenceScore = 1.0f;
		return Ok;
	}

	ESuspenseRuleSeverity Sev = ESuspenseRuleSeverity::Error;
	switch (R.FailureType)
	{
	case EEquipmentValidationFailure::InvalidSlot:
	case EEquipmentValidationFailure::UniqueConstraint:
	case EEquipmentValidationFailure::IncompatibleType:
		Sev = ESuspenseRuleSeverity::Critical; break;
	case EEquipmentValidationFailure::RequirementsNotMet:
	case EEquipmentValidationFailure::WeightLimit:
	case EEquipmentValidationFailure::LevelRequirement:
		Sev = ESuspenseRuleSeverity::Error; break;
	default:
		Sev = ESuspenseRuleSeverity::Error; break;
	}

	FSuspenseRuleCheckResult Fail = FSuspenseRuleCheckResult::Failure(
		R.ErrorMessage.IsEmpty() ? NSLOCTEXT("CompatibilityRules", "CompatFail", "Incompatible") : R.ErrorMessage,
		Sev);
	Fail.RuleType = ESuspenseRuleType::Compatibility;
	Fail.RuleTag = R.ErrorTag.IsValid() ? R.ErrorTag : FGameplayTag::RequestGameplayTag(TEXT("Rule.Compatibility.Fail"));

	// Copy context
	for (const auto& Kvp : R.Context)
	{
		Fail.Context.Add(Kvp.Key, Kvp.Value);
	}
	return Fail;
}

bool USuspenseCompatibilityRulesEngine::GetItemData(FName ItemID, FSuspenseUnifiedItemData& OutData) const
{
	return ItemProvider.IsValid() ? ItemProvider->GetUnifiedItemData(ItemID, OutData) : false;
}

FSuspenseRuleCheckResult USuspenseCompatibilityRulesEngine::CheckItemCompatibility(
	const FSuspenseInventoryItemInstance& ItemInstance,
	const FEquipmentSlotConfig& SlotConfig) const
{
	// Base: delegate to SlotValidator (public API, no access to protected methods)
	if (IsValid(SlotValidator))
	{
		const FSlotValidationResult VR = SlotValidator->CanPlaceItemInSlot(SlotConfig, ItemInstance);
		FSuspenseRuleCheckResult R = Convert(VR);
		if (!R.bPassed) { return R; } // short-circuit on hard fail
	}

	// Additional soft checks that are not duplicated in the validator
	FSuspenseUnifiedItemData ItemData;
	if (!GetItemData(ItemInstance.ItemID, ItemData))
	{
		FSuspenseRuleCheckResult R = FSuspenseRuleCheckResult::Failure(
			NSLOCTEXT("CompatibilityRules", "ItemDataNotFound", "Item data not found"),
			ESuspenseRuleSeverity::Error);
		R.RuleType = ESuspenseRuleType::Compatibility;
		return R;
	}

	// Slot type filter using current config API (Allowed / Disallowed)
	if (!SlotConfig.CanEquipItemType(ItemData.ItemType))
	{
		FSuspenseRuleCheckResult R = FSuspenseRuleCheckResult::Failure(
			NSLOCTEXT("CompatibilityRules", "TypeMismatch", "Item type is not allowed in this slot"),
			ESuspenseRuleSeverity::Error);
		R.RuleType = ESuspenseRuleType::Compatibility;
		return R;
	}

	// Soft check: broken item is not equippable (override disabled)
	const float Durability = ItemInstance.GetDurabilityPercent();
	if (Durability <= 0.0f)
	{
		FSuspenseRuleCheckResult R = FSuspenseRuleCheckResult::Failure(
			NSLOCTEXT("CompatibilityRules", "ItemBroken", "Cannot equip broken items"),
			ESuspenseRuleSeverity::Error);
		R.RuleType = ESuspenseRuleType::Compatibility;
		R.bCanOverride = false;
		R.Context.Add(TEXT("Durability"), FString::Printf(TEXT("%.1f%%"), Durability * 100.0f));
		return R;
	}
	else if (Durability < 0.2f)
	{
		FSuspenseRuleCheckResult W = FSuspenseRuleCheckResult::Success();
		W.RuleType = ESuspenseRuleType::Compatibility;
		W.Severity = ESuspenseRuleSeverity::Warning;
		W.Message = FText::Format(NSLOCTEXT("CompatibilityRules", "LowDurability", "Warning: low durability ({0}%)"),
			FMath::RoundToInt(Durability * 100.0f));
		W.ConfidenceScore = 0.7f;
		W.bCanOverride = true;
		return W;
	}

	FSuspenseRuleCheckResult Ok = FSuspenseRuleCheckResult::Success();
	Ok.RuleType = ESuspenseRuleType::Compatibility;
	Ok.Message = NSLOCTEXT("CompatibilityRules", "Compatible", "Compatible");
	Ok.ConfidenceScore = 1.0f;
	return Ok;
}

FSuspenseRuleCheckResult USuspenseCompatibilityRulesEngine::CheckTypeCompatibility(
	const FGameplayTag& ItemType,
	const FEquipmentSlotConfig& SlotConfig) const
{
	// Simple gate: Allowed/Disallowed sets only (validator covers strict rules)
	const bool bTypeAllowed = SlotConfig.AllowedItemTypes.IsEmpty() || SlotConfig.AllowedItemTypes.HasTag(ItemType);
	if (!bTypeAllowed)
	{
		FSuspenseRuleCheckResult R = FSuspenseRuleCheckResult::Failure(
			FText::Format(NSLOCTEXT("CompatibilityRules", "TypeNotAllowed", "Item type {0} is not allowed"),
				FText::FromString(ItemType.ToString())),
			ESuspenseRuleSeverity::Error);
		R.RuleType = ESuspenseRuleType::Compatibility;
		return R;
	}

	// Disallowed types (exact match)
	if (!SlotConfig.DisallowedItemTypes.IsEmpty() && SlotConfig.DisallowedItemTypes.HasTagExact(ItemType))
	{
		FSuspenseRuleCheckResult R = FSuspenseRuleCheckResult::Failure(
			FText::Format(NSLOCTEXT("CompatibilityRules", "TypeBlocked", "Item type {0} is disallowed"),
				FText::FromString(ItemType.ToString())),
			ESuspenseRuleSeverity::Error);
		R.RuleType = ESuspenseRuleType::Compatibility;
		return R;
	}

	FSuspenseRuleCheckResult Ok = FSuspenseRuleCheckResult::Success();
	Ok.RuleType = ESuspenseRuleType::Compatibility;
	Ok.Message = NSLOCTEXT("CompatibilityRules", "TypeCompatible", "Item type is compatible with slot");
	Ok.ConfidenceScore = 1.0f;
	return Ok;
}

FSuspenseAggregatedRuleResult USuspenseCompatibilityRulesEngine::EvaluateCompatibilityRules(
	const FSuspenseRuleContext& Context) const
{
	FSuspenseAggregatedRuleResult Agg;

	// Resolve equipment data provider ONLY from default DI
	TScriptInterface<ISuspenseEquipmentDataProvider> EquipProvider = DefaultEquipProvider;
	if (!EquipProvider.GetInterface())
	{
		FSuspenseRuleCheckResult R = FSuspenseRuleCheckResult::Failure(
			NSLOCTEXT("CompatibilityRules", "NoDataProvider", "No equipment data provider"),
			ESuspenseRuleSeverity::Error);
		R.RuleType = ESuspenseRuleType::Compatibility;
		Agg.AddResult(R);
		return Agg;
	}

	// Resolve SlotConfig by index
	if (Context.TargetSlotIndex == INDEX_NONE)
	{
		FSuspenseRuleCheckResult R = FSuspenseRuleCheckResult::Failure(
			NSLOCTEXT("CompatibilityRules", "NoTargetSlot", "No target slot specified"),
			ESuspenseRuleSeverity::Error);
		R.RuleType = ESuspenseRuleType::Compatibility;
		Agg.AddResult(R);
		return Agg;
	}
	const FEquipmentSlotConfig SlotCfg = EquipProvider.GetInterface()->GetSlotConfiguration(Context.TargetSlotIndex);

	// Base hard checks via SlotValidator (short-circuit on fail)
	if (IsValid(SlotValidator))
	{
		const FSlotValidationResult VR = SlotValidator->CanPlaceItemInSlot(SlotCfg, Context.ItemInstance);
		FSuspenseRuleCheckResult R = Convert(VR);
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
			FSuspenseRuleCheckResult W = FSuspenseRuleCheckResult::Success();
			W.RuleType = ESuspenseRuleType::Compatibility;
			W.Severity = ESuspenseRuleSeverity::Warning;
			W.Message = FText::Format(NSLOCTEXT("CompatibilityRules", "LowDurability", "Warning: low durability ({0}%)"),
				FMath::RoundToInt(Durability * 100.0f));
			W.ConfidenceScore = 0.7f;
			W.bCanOverride = true;
			Agg.AddResult(W);
		}
	}

	return Agg;
}

TArray<int32> USuspenseCompatibilityRulesEngine::FindCompatibleSlots(
	const FSuspenseInventoryItemInstance& ItemInstance,
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
			FSuspenseUnifiedItemData ItemData;
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

float USuspenseCompatibilityRulesEngine::GetCompatibilityScore(
	const FSuspenseInventoryItemInstance& ItemInstance,
	const FEquipmentSlotConfig& SlotConfig) const
{
	// Start from base check confidence
	float Score = 1.0f;
	if (IsValid(SlotValidator))
	{
		const FSlotValidationResult VR = SlotValidator->CanPlaceItemInSlot(SlotConfig, ItemInstance);
		if (!VR.bIsValid) { return 0.0f; }
	}

	FSuspenseUnifiedItemData ItemData;
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
