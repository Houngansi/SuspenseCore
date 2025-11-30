// SuspenseCoreCharacterClassData.cpp
// SuspenseCore - Character Class System
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Data/SuspenseCoreCharacterClassData.h"
#include "Abilities/GameplayAbility.h"

USuspenseCoreCharacterClassData::USuspenseCoreCharacterClassData()
{
	// Default identity
	ClassID = NAME_None;
	DisplayName = FText::FromString(TEXT("Unknown Class"));
	ShortDescription = FText::FromString(TEXT("No description available."));
	Role = ESuspenseCoreClassRole::Assault;

	// Default attribute modifiers (all 1.0 = baseline)
	AttributeModifiers = FSuspenseCoreAttributeModifier();

	// Default balance settings
	DifficultyRating = 2;
	bIsStarterClass = true;
	UnlockLevel = 0;
}

FPrimaryAssetId USuspenseCoreCharacterClassData::GetPrimaryAssetId() const
{
	// Format: CharacterClass:ClassID
	// Example: CharacterClass:Assault
	return FPrimaryAssetId(
		FPrimaryAssetType(TEXT("CharacterClass")),
		ClassID.IsNone() ? GetFName() : ClassID
	);
}

TArray<TSubclassOf<UGameplayAbility>> USuspenseCoreCharacterClassData::GetAbilitiesForLevel(int32 Level) const
{
	TArray<TSubclassOf<UGameplayAbility>> Result;

	for (const FSuspenseCoreClassAbilitySlot& Slot : ClassAbilities)
	{
		if (Slot.AbilityClass && Slot.RequiredLevel <= Level)
		{
			Result.Add(Slot.AbilityClass);
		}
	}

	return Result;
}

bool USuspenseCoreCharacterClassData::IsUnlockedForLevel(int32 PlayerLevel) const
{
	if (bIsStarterClass)
	{
		return true;
	}
	return PlayerLevel >= UnlockLevel;
}

#if WITH_EDITOR
void USuspenseCoreCharacterClassData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Auto-generate ClassID from asset name if not set
	if (ClassID.IsNone())
	{
		ClassID = GetFName();
	}

	// Validate ability slots
	for (int32 i = 0; i < ClassAbilities.Num(); ++i)
	{
		FSuspenseCoreClassAbilitySlot& Slot = ClassAbilities[i];

		// Auto-assign slot index if not set
		if (Slot.SlotIndex == 0 && i > 0)
		{
			Slot.SlotIndex = i;
		}
	}
}
#endif
