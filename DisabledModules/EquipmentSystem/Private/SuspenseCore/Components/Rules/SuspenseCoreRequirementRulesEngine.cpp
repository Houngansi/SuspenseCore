// SuspenseCoreRequirementRulesEngine.cpp
// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/Rules/SuspenseCoreRequirementRulesEngine.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayAbilitySpec.h"

DEFINE_LOG_CATEGORY_STATIC(LogRequirementRules, Log, All);

//==================== ctor ====================

USuspenseCoreRequirementRulesEngine::USuspenseCoreRequirementRulesEngine()
{
    // Stateless; nothing to initialize here.
}

//==================== public: aggregate ====================

FSuspenseCoreAggregatedRuleResult USuspenseCoreRequirementRulesEngine::CheckAllRequirements(
    const AActor* Character,
    const FSuspenseCoreItemRequirements& Requirements) const
{
    FSuspenseCoreAggregatedRuleResult Agg;

    if (!IsValid(Character))
    {
        FSuspenseCoreRuleCheckResult R = FSuspenseCoreRuleCheckResult::Failure(
            NSLOCTEXT("RequirementRules", "NoCharacter", "No character supplied"),
            ESuspenseCoreRuleSeverity::Error);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Character.Invalid"));
        Agg.AddResult(R);
        return Agg;
    }

    // Level - strict reading from ASC only
    if (Requirements.RequiredLevel > 0)
    {
        const FSuspenseCoreRuleCheckResult R = CheckCharacterLevel(Character, Requirements.RequiredLevel);
        Agg.AddResult(R);
        if (!R.bPassed && (R.Severity == ESuspenseCoreRuleSeverity::Error || R.Severity == ESuspenseCoreRuleSeverity::Critical))
        {
            return Agg; // short-circuit on hard fail
        }
    }

    // Class tag - strict validation
    if (Requirements.RequiredClass.IsValid())
    {
        const FGameplayTagContainer Owned = GetCharacterTags(Character);
        if (!Owned.HasTag(Requirements.RequiredClass))
        {
            FSuspenseCoreRuleCheckResult R = FSuspenseCoreRuleCheckResult::Failure(
                FText::Format(NSLOCTEXT("RequirementRules", "WrongClass", "Requires class: {0}"),
                    FText::FromString(Requirements.RequiredClass.ToString())),
                ESuspenseCoreRuleSeverity::Error);
            R.RuleType = ESuspenseCoreRuleType::Requirement;
            R.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Class.Mismatch"));
            Agg.AddResult(R);
            return Agg;
        }
    }

    // Required tags
    if (!Requirements.RequiredTags.IsEmpty())
    {
        const FSuspenseCoreRuleCheckResult R = CheckCharacterTags(Character, Requirements.RequiredTags);
        Agg.AddResult(R);
        if (!R.bPassed && (R.Severity == ESuspenseCoreRuleSeverity::Error || R.Severity == ESuspenseCoreRuleSeverity::Critical))
        {
            return Agg;
        }
    }

    // Attribute gates - strict ASC reading
    if (Requirements.AttributeRequirements.Num() > 0)
    {
        const FSuspenseCoreRuleCheckResult R = CheckAttributeRequirements(Character, Requirements.AttributeRequirements);
        Agg.AddResult(R);
        if (!R.bPassed && (R.Severity == ESuspenseCoreRuleSeverity::Error || R.Severity == ESuspenseCoreRuleSeverity::Critical))
        {
            return Agg;
        }
    }

    // Abilities (strict ASC check)
    if (Requirements.RequiredAbilities.Num() > 0)
    {
        const FSuspenseCoreRuleCheckResult R = CheckRequiredAbilities(Character, Requirements.RequiredAbilities);
        Agg.AddResult(R);
        if (!R.bPassed && (R.Severity == ESuspenseCoreRuleSeverity::Error || R.Severity == ESuspenseCoreRuleSeverity::Critical))
        {
            return Agg;
        }
    }

    // External requirements: quests/certifications (informational only)
    if (Requirements.RequiredQuests.Num() > 0)
    {
        FSuspenseCoreRuleCheckResult R = FSuspenseCoreRuleCheckResult::Failure(
            NSLOCTEXT("RequirementRules", "QuestsUnverified", "Quest completion data source is not linked"),
            ESuspenseCoreRuleSeverity::Info);
        R.bCanOverride = true;
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Quests.Unverified"));
        Agg.AddResult(R);
    }

    if (Requirements.RequiredCertifications.Num() > 0)
    {
        FSuspenseCoreRuleCheckResult R = FSuspenseCoreRuleCheckResult::Failure(
            NSLOCTEXT("RequirementRules", "CertsUnverified", "Certification data source is not linked"),
            ESuspenseCoreRuleSeverity::Info);
        R.bCanOverride = true;
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Certifications.Unverified"));
        Agg.AddResult(R);
    }

    return Agg;
}

FSuspenseCoreAggregatedRuleResult USuspenseCoreRequirementRulesEngine::EvaluateRequirementRules(
    const FSuspenseCoreRuleContext& Context) const
{
    FSuspenseCoreAggregatedRuleResult Agg;

    if (!IsValid(Context.Character))
    {
        FSuspenseCoreRuleCheckResult R = FSuspenseCoreRuleCheckResult::Failure(
            NSLOCTEXT("RequirementRules", "NoCharacterInContext", "Rule context has no character"),
            ESuspenseCoreRuleSeverity::Error);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Context.Invalid"));
        Agg.AddResult(R);
        return Agg;
    }

    // For context-based evaluation, we assume requirements are embedded in item data
    // This is a pass-through that indicates no implicit context-level requirements
    FSuspenseCoreRuleCheckResult OK = FSuspenseCoreRuleCheckResult::Success(
        NSLOCTEXT("RequirementRules", "NoImplicitRequirements", "No implicit requirements in context"));
    OK.RuleType = ESuspenseCoreRuleType::Requirement;
    OK.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Context.None"));
    Agg.AddResult(OK);
    return Agg;
}

//==================== primitives ====================

FSuspenseCoreRuleCheckResult USuspenseCoreRequirementRulesEngine::CheckCharacterLevel(
    const AActor* Character,
    int32 RequiredLevel) const
{
    if (!IsValid(Character))
    {
        FSuspenseCoreRuleCheckResult R = FSuspenseCoreRuleCheckResult::Failure(
            NSLOCTEXT("RequirementRules", "NoCharacter", "No character supplied"),
            ESuspenseCoreRuleSeverity::Error);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Level.InvalidCharacter"));
        return R;
    }

    const int32 Level = GetCharacterLevel(Character);
    
    // Strict validation: if level source is missing and requirement > 0, it's a critical error
    if (Level <= 0 && RequiredLevel > 0)
    {
        FSuspenseCoreRuleCheckResult R = FSuspenseCoreRuleCheckResult::Failure(
            NSLOCTEXT("RequirementRules", "LevelSourceMissing", "Cannot resolve character level from ASC"),
            ESuspenseCoreRuleSeverity::Critical);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Level.SourceMissing"));
        return R;
    }

    if (Level < RequiredLevel)
    {
        FSuspenseCoreRuleCheckResult R = FSuspenseCoreRuleCheckResult::Failure(
            FText::Format(NSLOCTEXT("RequirementRules", "LevelTooLow", "Requires level {0} (current {1})"),
                FText::AsNumber(RequiredLevel), FText::AsNumber(Level)),
            ESuspenseCoreRuleSeverity::Error);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Level.TooLow"));
        return R;
    }

    FSuspenseCoreRuleCheckResult OK = FSuspenseCoreRuleCheckResult::Success(
        NSLOCTEXT("RequirementRules", "LevelOK", "Level requirement met"));
    OK.RuleType = ESuspenseCoreRuleType::Requirement;
    OK.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Level.OK"));
    return OK;
}

FSuspenseCoreRuleCheckResult USuspenseCoreRequirementRulesEngine::CheckSkillLevel(
    const AActor* Character,
    const FGameplayTag& SkillTag,
    int32 RequiredLevel) const
{
    if (!IsValid(Character) || !SkillTag.IsValid())
    {
        FSuspenseCoreRuleCheckResult R = FSuspenseCoreRuleCheckResult::Failure(
            NSLOCTEXT("RequirementRules", "InvalidSkillInput", "Invalid skill requirement input"),
            ESuspenseCoreRuleSeverity::Error);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Skill.InvalidInput"));
        return R;
    }

    // Resolve attribute name heuristically from tag ("Skill.Marksmanship" -> "MarksmanshipLevel")
    FName AttributeName;
    {
        const FString TagStr = SkillTag.ToString();
        FString Leaf;
        TagStr.Split(TEXT("."), nullptr, &Leaf, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
        AttributeName = FName(*FString::Printf(TEXT("%sLevel"), *Leaf));
    }

    const float SkillValue = GetAttributeValue(Character, AttributeName);
    
    // Strict validation: if skill source is missing and requirement > 0, it's critical
    if (SkillValue <= 0.f && RequiredLevel > 0)
    {
        FSuspenseCoreRuleCheckResult R = FSuspenseCoreRuleCheckResult::Failure(
            FText::Format(NSLOCTEXT("RequirementRules", "SkillSourceMissing",
                "Cannot resolve skill level for {0} from ASC"), FText::FromString(SkillTag.ToString())),
            ESuspenseCoreRuleSeverity::Critical);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Skill.SourceMissing"));
        return R;
    }

    if ((int32)FMath::FloorToInt(SkillValue) < RequiredLevel)
    {
        FSuspenseCoreRuleCheckResult R = FSuspenseCoreRuleCheckResult::Failure(
            FText::Format(NSLOCTEXT("RequirementRules", "SkillTooLow",
                "Requires {0} level {1}"), FText::FromString(SkillTag.ToString()),
                FText::AsNumber(RequiredLevel)),
            ESuspenseCoreRuleSeverity::Error);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Skill.TooLow"));
        return R;
    }

    FSuspenseCoreRuleCheckResult OK = FSuspenseCoreRuleCheckResult::Success(
        NSLOCTEXT("RequirementRules", "SkillOK", "Skill requirement met"));
    OK.RuleType = ESuspenseCoreRuleType::Requirement;
    OK.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Skill.OK"));
    return OK;
}

FSuspenseCoreRuleCheckResult USuspenseCoreRequirementRulesEngine::CheckAttributeRequirements(
    const AActor* Character,
    const TArray<FSuspenseCoreAttributeRequirement>& Requirements) const
{
    bool bAllOk = true;
    FString FailureDetails;
    
    for (const FSuspenseCoreAttributeRequirement& Rq : Requirements)
    {
        const FSuspenseCoreRuleCheckResult R = CheckSingleAttribute(Character, Rq.AttributeName, Rq.RequiredValue, Rq.ComparisonOp);
        if (!R.bPassed && (R.Severity == ESuspenseCoreRuleSeverity::Error || R.Severity == ESuspenseCoreRuleSeverity::Critical))
        {
            bAllOk = false;
            if (!FailureDetails.IsEmpty()) FailureDetails += TEXT("; ");
            FailureDetails += R.Message.ToString();
        }
    }

    if (bAllOk)
    {
        FSuspenseCoreRuleCheckResult OK = FSuspenseCoreRuleCheckResult::Success(
            NSLOCTEXT("RequirementRules", "AttributesOK", "All attribute requirements met"));
        OK.RuleType = ESuspenseCoreRuleType::Requirement;
        OK.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Attributes.OK"));
        return OK;
    }

    FSuspenseCoreRuleCheckResult F = FSuspenseCoreRuleCheckResult::Failure(
        FText::FromString(FailureDetails.IsEmpty() ? TEXT("Attribute requirements not met") : FailureDetails),
        ESuspenseCoreRuleSeverity::Error);
    F.RuleType = ESuspenseCoreRuleType::Requirement;
    F.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Attributes.Failed"));
    return F;
}

FSuspenseCoreRuleCheckResult USuspenseCoreRequirementRulesEngine::CheckSingleAttribute(
    const AActor* Character,
    const FName& AttributeName,
    float RequiredValue,
    ESuspenseCoreComparisonOp Op) const
{
    if (!IsValid(Character) || AttributeName.IsNone())
    {
        FSuspenseCoreRuleCheckResult R = FSuspenseCoreRuleCheckResult::Failure(
            NSLOCTEXT("RequirementRules", "InvalidAttrInput", "Invalid attribute requirement input"),
            ESuspenseCoreRuleSeverity::Error);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Attribute.InvalidInput"));
        return R;
    }

    const float Actual = GetAttributeValue(Character, AttributeName);
    const bool bPass = CompareValues(Actual, RequiredValue, Op);
    
    if (!bPass)
    {
        FText OpStr;
        switch (Op)
        {
        case ESuspenseCoreComparisonOp::Equal:           OpStr = FText::FromString(TEXT("==")); break;
        case ESuspenseCoreComparisonOp::NotEqual:        OpStr = FText::FromString(TEXT("!=")); break;
        case ESuspenseCoreComparisonOp::Greater:         OpStr = FText::FromString(TEXT(">")); break;
        case ESuspenseCoreComparisonOp::GreaterOrEqual:  OpStr = FText::FromString(TEXT(">=")); break;
        case ESuspenseCoreComparisonOp::Less:            OpStr = FText::FromString(TEXT("<")); break;
        case ESuspenseCoreComparisonOp::LessOrEqual:     OpStr = FText::FromString(TEXT("<=")); break;
        default:                                   OpStr = FText::FromString(TEXT("?")); break;
        }

        FSuspenseCoreRuleCheckResult R = FSuspenseCoreRuleCheckResult::Failure(
            FText::Format(NSLOCTEXT("RequirementRules", "AttributeMismatch", "Attribute {0}: {1} (required {2} {3})"),
                FText::FromName(AttributeName), 
                FText::AsNumber(Actual),
                OpStr, 
                FText::AsNumber(RequiredValue)),
            ESuspenseCoreRuleSeverity::Error);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Attribute.Mismatch"));
        R.Context.Add(TEXT("Actual"), FString::SanitizeFloat(Actual));
        R.Context.Add(TEXT("Required"), FString::SanitizeFloat(RequiredValue));
        return R;
    }

    FSuspenseCoreRuleCheckResult OK = FSuspenseCoreRuleCheckResult::Success(
        FText::Format(NSLOCTEXT("RequirementRules", "AttributeOK", "{0} requirement met"),
            FText::FromName(AttributeName)));
    OK.RuleType = ESuspenseCoreRuleType::Requirement;
    OK.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Attribute.OK"));
    return OK;
}

FSuspenseCoreRuleCheckResult USuspenseCoreRequirementRulesEngine::CheckCharacterTags(
    const AActor* Character,
    const FGameplayTagContainer& RequiredTags) const
{
    if (!IsValid(Character))
    {
        FSuspenseCoreRuleCheckResult R = FSuspenseCoreRuleCheckResult::Failure(
            NSLOCTEXT("RequirementRules", "NoCharacter", "No character supplied"),
            ESuspenseCoreRuleSeverity::Error);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Tags.InvalidCharacter"));
        return R;
    }

    const FGameplayTagContainer Owned = GetCharacterTags(Character);
    if (!Owned.HasAll(RequiredTags))
    {
        // Build detailed missing tags list for UI/debugging
        FGameplayTagContainer Missing;
        for (const FGameplayTag& T : RequiredTags)
        {
            if (!Owned.HasTag(T))
            {
                Missing.AddTag(T);
            }
        }

        FSuspenseCoreRuleCheckResult R = FSuspenseCoreRuleCheckResult::Failure(
            FText::Format(NSLOCTEXT("RequirementRules", "TagsMissing", "Missing required tags: {0}"),
                FText::FromString(Missing.ToStringSimple())),
            ESuspenseCoreRuleSeverity::Error);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Tags.Missing"));
        R.Context.Add(TEXT("Missing"), Missing.ToStringSimple());
        R.Context.Add(TEXT("Required"), RequiredTags.ToStringSimple());
        return R;
    }

    FSuspenseCoreRuleCheckResult OK = FSuspenseCoreRuleCheckResult::Success(
        NSLOCTEXT("RequirementRules", "TagsOK", "All required tags present"));
    OK.RuleType = ESuspenseCoreRuleType::Requirement;
    OK.RuleTag = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Tags.OK"));
    return OK;
}

FSuspenseCoreRuleCheckResult USuspenseCoreRequirementRulesEngine::CheckRequiredAbilities(
    const AActor* Character,
    const TArray<TSubclassOf<UGameplayAbility>>& RequiredAbilities) const
{
    if (!IsValid(Character))
    {
        auto R = FSuspenseCoreRuleCheckResult::Failure(
            NSLOCTEXT("RequirementRules", "NoCharacter", "No character supplied"),
            ESuspenseCoreRuleSeverity::Error);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag  = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Ability.InvalidCharacter"));
        return R;
    }

    const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Character);
    if (!ASI)
    {
        auto R = FSuspenseCoreRuleCheckResult::Failure(
            NSLOCTEXT("RequirementRules", "NoASC", "Character does not implement IAbilitySystemInterface"),
            ESuspenseCoreRuleSeverity::Critical);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag  = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Ability.NoASC"));
        return R;
    }

    UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
    if (!ASC)
    {
        auto R = FSuspenseCoreRuleCheckResult::Failure(
            NSLOCTEXT("RequirementRules", "NoASCComponent", "AbilitySystemComponent not found on character"),
            ESuspenseCoreRuleSeverity::Critical);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag  = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Ability.NoASC"));
        return R;
    }

    // В вашей сборке GetActivatableAbilities() -> TArray<FGameplayAbilitySpec>
    TSet<const UClass*> Present;
    {
        const TArray<FGameplayAbilitySpec>& Specs = ASC->GetActivatableAbilities();
        for (const FGameplayAbilitySpec& Spec : Specs)
        {
            if (Spec.Ability)
            {
                Present.Add(Spec.Ability->GetClass());
            }
        }
    }

    TArray<FString> Missing;
    for (const TSubclassOf<UGameplayAbility>& Req : RequiredAbilities)
    {
        const UClass* ReqClass = *Req;
        bool bFound = false;

        if (ReqClass)
        {
            for (const UClass* Have : Present)
            {
                if (Have->IsChildOf(ReqClass))
                {
                    bFound = true;
                    break;
                }
            }
        }

        if (!bFound)
        {
            Missing.Add(GetNameSafe(ReqClass));
        }
    }

    if (Missing.Num() > 0)
    {
        auto R = FSuspenseCoreRuleCheckResult::Failure(
            FText::Format(NSLOCTEXT("RequirementRules", "AbilitiesMissing", "Missing required abilities: {0}"),
                          FText::FromString(FString::Join(Missing, TEXT(", "))))
            , ESuspenseCoreRuleSeverity::Error);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag  = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Ability.Missing"));
        R.Context.Add(TEXT("MissingAbilities"), FString::Join(Missing, TEXT(", ")));
        return R;
    }

    auto OK = FSuspenseCoreRuleCheckResult::Success(
        NSLOCTEXT("RequirementRules", "AbilitiesOK", "All required abilities present"));
    OK.RuleType = ESuspenseCoreRuleType::Requirement;
    OK.RuleTag  = FGameplayTag::RequestGameplayTag(TEXT("Requirement.Ability.OK"));
    return OK;
}


//==================== progress / estimation ====================

float USuspenseCoreRequirementRulesEngine::CalculateRequirementProgress(
    const AActor* Character,
    const FSuspenseCoreItemRequirements& Requirements) const
{
    int32 NumChecks = 0;
    int32 NumPassed = 0;

    if (Requirements.RequiredLevel > 0)
    {
        ++NumChecks;
        NumPassed += CheckCharacterLevel(Character, Requirements.RequiredLevel).bPassed ? 1 : 0;
    }

    if (!Requirements.RequiredTags.IsEmpty())
    {
        ++NumChecks;
        NumPassed += CheckCharacterTags(Character, Requirements.RequiredTags).bPassed ? 1 : 0;
    }

    if (Requirements.AttributeRequirements.Num() > 0)
    {
        for (const FSuspenseCoreAttributeRequirement& Rq : Requirements.AttributeRequirements)
        {
            ++NumChecks;
            NumPassed += CheckSingleAttribute(Character, Rq.AttributeName, Rq.RequiredValue, Rq.ComparisonOp).bPassed ? 1 : 0;
        }
    }

    if (Requirements.RequiredAbilities.Num() > 0)
    {
        ++NumChecks;
        NumPassed += CheckRequiredAbilities(Character, Requirements.RequiredAbilities).bPassed ? 1 : 0;
    }

    if (NumChecks == 0)
    {
        return 1.0f; // nothing required = 100% satisfied
    }
    return FMath::Clamp(static_cast<float>(NumPassed) / static_cast<float>(NumChecks), 0.0f, 1.0f);
}

float USuspenseCoreRequirementRulesEngine::EstimateTimeToMeetRequirements(
    const AActor* /*Character*/,
    const FSuspenseCoreItemRequirements& /*Requirements*/) const
{
    // Estimation depends on external progression systems that are not available to this engine
    return -1.0f;
}

//==================== custom validators ====================

void USuspenseCoreRequirementRulesEngine::RegisterCustomRequirement(
    const FGameplayTag& RequirementTag,
    FCustomRequirementValidator Validator)
{
    if (!RequirementTag.IsValid() || !Validator.IsBound())
    {
        return;
    }
    FScopeLock Lock(&ValidatorLock);
    CustomValidators.Add(RequirementTag, MoveTemp(Validator));
}

void USuspenseCoreRequirementRulesEngine::UnregisterCustomRequirement(const FGameplayTag& RequirementTag)
{
    FScopeLock Lock(&ValidatorLock);
    CustomValidators.Remove(RequirementTag);
}

FSuspenseCoreRuleCheckResult USuspenseCoreRequirementRulesEngine::CheckCustomRequirement(
    const AActor* Character,
    const FGameplayTag& RequirementTag,
    const FString& Parameters) const
{
    FScopeLock Lock(&ValidatorLock);
    if (const FCustomRequirementValidator* Found = CustomValidators.Find(RequirementTag))
    {
        const bool bOk = Found->Execute(Character, Parameters);
        FSuspenseCoreRuleCheckResult R = bOk ? FSuspenseCoreRuleCheckResult::Success(
                                            NSLOCTEXT("RequirementRules", "CustomOK", "Custom requirement satisfied"))
                                       : FSuspenseCoreRuleCheckResult::Failure(
                                            NSLOCTEXT("RequirementRules", "CustomFailed", "Custom requirement failed"),
                                            ESuspenseCoreRuleSeverity::Error);
        R.RuleType = ESuspenseCoreRuleType::Requirement;
        R.RuleTag = RequirementTag;
        return R;
    }

    FSuspenseCoreRuleCheckResult F = FSuspenseCoreRuleCheckResult::Failure(
        NSLOCTEXT("RequirementRules", "NoCustomValidator", "No validator registered for custom requirement"),
        ESuspenseCoreRuleSeverity::Info);
    F.bCanOverride = true;
    F.RuleType = ESuspenseCoreRuleType::Requirement;
    F.RuleTag = RequirementTag.IsValid() ? RequirementTag
                                         : FGameplayTag::RequestGameplayTag(TEXT("Requirement.Custom.Unknown"));
    return F;
}

//==================== cache and statistics (required by coordinator) ====================

void USuspenseCoreRequirementRulesEngine::ClearCache()
{
    // Requirements engine is stateless - no cache to clear
    UE_LOG(LogRequirementRules, Log, TEXT("Cache cleared (no cache maintained)"));
}

void USuspenseCoreRequirementRulesEngine::ResetStatistics()
{
    // Requirements engine doesn't maintain internal statistics
    UE_LOG(LogRequirementRules, Log, TEXT("Statistics reset (no statistics maintained)"));
}

//==================== data access helpers (ASC-only, no world access) ====================

int32 USuspenseCoreRequirementRulesEngine::GetCharacterLevel(const AActor* Character) const
{
    const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Character);
    if (!ASI) { return 0; }
    const UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
    if (!ASC) { return 0; }

    // Try common level attribute names in order of preference
    static const FName LevelNames[] = {
        FName(TEXT("Level")),
        FName(TEXT("CharacterLevel")),
        FName(TEXT("PlayerLevel")),
        FName(TEXT("CurrentLevel"))
    };
    
    for (const FName& N : LevelNames)
    {
        const float V = GetAttributeValue(Character, N);
        if (V > 0.f)
        {
            return FMath::FloorToInt(V);
        }
    }
    
    // No level found in ASC - this is not an error per se, but means we can't validate level requirements
    return 0;
}

float USuspenseCoreRequirementRulesEngine::GetAttributeValue(const AActor* Character, const FName& AttributeName) const
{
    const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Character);
    if (!ASI) { return 0.0f; }
    const UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent();
    if (!ASC) { return 0.0f; }

    // Iterate through all spawned attribute sets and use reflection to find the attribute
    const TArray<UAttributeSet*>& Sets = ASC->GetSpawnedAttributes();

    for (const UAttributeSet* Set : Sets)
    {
        if (!Set) { continue; }
        
        if (FProperty* Prop = Set->GetClass()->FindPropertyByName(AttributeName))
        {
            // Support both float and int properties
            if (const FFloatProperty* FP = CastField<FFloatProperty>(Prop))
            {
                const void* Ptr = FP->ContainerPtrToValuePtr<void>(Set);
                const float* Val = static_cast<const float*>(Ptr);
                return Val ? *Val : 0.0f;
            }
            if (const FIntProperty* IP = CastField<FIntProperty>(Prop))
            {
                const void* Ptr = IP->ContainerPtrToValuePtr<void>(Set);
                const int32* Val = static_cast<const int32*>(Ptr);
                return Val ? static_cast<float>(*Val) : 0.0f;
            }
        }
    }
    
    // Attribute not found - return 0 (this allows for optional attributes)
    return 0.0f;
}

FGameplayTagContainer USuspenseCoreRequirementRulesEngine::GetCharacterTags(const AActor* Character) const
{
    FGameplayTagContainer Out;
    if (!IsValid(Character)) { return Out; }

    // Try IGameplayTagAssetInterface first (more direct)
    if (const IGameplayTagAssetInterface* GTAI = Cast<IGameplayTagAssetInterface>(Character))
    {
        GTAI->GetOwnedGameplayTags(Out);
        return Out;
    }

    // Fallback to ASC tags
    if (const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Character))
    {
        if (const UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
        {
            ASC->GetOwnedGameplayTags(Out);
        }
    }
    
    return Out;
}

bool USuspenseCoreRequirementRulesEngine::CompareValues(float Value1, float Value2, ESuspenseCoreComparisonOp Op) const
{
    switch (Op)
    {
    case ESuspenseCoreComparisonOp::Equal:           return FMath::IsNearlyEqual(Value1, Value2);
    case ESuspenseCoreComparisonOp::NotEqual:        return !FMath::IsNearlyEqual(Value1, Value2);
    case ESuspenseCoreComparisonOp::Greater:         return Value1 > Value2;
    case ESuspenseCoreComparisonOp::GreaterOrEqual:  return Value1 >= Value2;
    case ESuspenseCoreComparisonOp::Less:            return Value1 < Value2;
    case ESuspenseCoreComparisonOp::LessOrEqual:     return Value1 <= Value2;
    default:                                   return false;
    }
}