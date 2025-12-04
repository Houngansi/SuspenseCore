// SuspenseCoreSettings.cpp
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Settings/SuspenseCoreSettings.h"
#include "Engine/DataTable.h"
#include "Engine/DataAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreSettings, Log, All);

//========================================================================
// Construction
//========================================================================

USuspenseCoreSettings::USuspenseCoreSettings()
{
	// Default character class (can be overridden in Project Settings)
	DefaultCharacterClass = FGameplayTag::RequestGameplayTag(TEXT("Character.Class.Soldier"), false);
}

//========================================================================
// Static Access
//========================================================================

const USuspenseCoreSettings* USuspenseCoreSettings::Get()
{
	return GetDefault<USuspenseCoreSettings>();
}

USuspenseCoreSettings* USuspenseCoreSettings::GetMutable()
{
	return GetMutableDefault<USuspenseCoreSettings>();
}

//========================================================================
// UDeveloperSettings Interface
//========================================================================

#if WITH_EDITOR
FText USuspenseCoreSettings::GetSectionText() const
{
	return NSLOCTEXT("SuspenseCore", "SettingsSection", "SuspenseCore");
}

FText USuspenseCoreSettings::GetSectionDescription() const
{
	return NSLOCTEXT("SuspenseCore", "SettingsDescription",
		"Configure SuspenseCore system settings including Item DataTables, "
		"Character Classes, Loadouts, and EventBus options.\n\n"
		"This is the SINGLE SOURCE OF TRUTH for all SuspenseCore data configuration.");
}

void USuspenseCoreSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property)
	{
		return;
	}

	const FName PropertyName = PropertyChangedEvent.Property->GetFName();

	// Log configuration changes
	UE_LOG(LogSuspenseCoreSettings, Log, TEXT("SuspenseCore Settings changed: %s"), *PropertyName.ToString());

	// Validate after changes
	TArray<FString> Errors;
	if (!ValidateConfiguration(Errors))
	{
		for (const FString& Error : Errors)
		{
			UE_LOG(LogSuspenseCoreSettings, Warning, TEXT("Configuration warning: %s"), *Error);
		}
	}
}
#endif

//========================================================================
// Validation
//========================================================================

bool USuspenseCoreSettings::ValidateConfiguration(TArray<FString>& OutErrors) const
{
	OutErrors.Empty();
	bool bIsValid = true;

	//========================================================================
	// Item System Validation
	//========================================================================
	if (ItemDataTable.IsNull())
	{
		OutErrors.Add(TEXT("ItemDataTable is not configured! Items will not be available."));
		bIsValid = false;
	}
	else if (ItemDataTable.IsPending())
	{
		// Asset path is set but not loaded - this is OK for soft references
		UE_LOG(LogSuspenseCoreSettings, Verbose, TEXT("ItemDataTable is pending load: %s"),
			*ItemDataTable.ToString());
	}

	//========================================================================
	// Character System Validation
	//========================================================================
	if (CharacterClassesDataAsset.IsNull())
	{
		OutErrors.Add(TEXT("CharacterClassesDataAsset is not configured! Character selection will not work."));
		// Not marking as invalid - character system might be optional
	}

	if (!DefaultCharacterClass.IsValid())
	{
		OutErrors.Add(TEXT("DefaultCharacterClass tag is not valid!"));
	}

	//========================================================================
	// Loadout System Validation
	//========================================================================
	if (LoadoutDataTable.IsNull())
	{
		// Warning only - loadout system might use defaults
		UE_LOG(LogSuspenseCoreSettings, Verbose, TEXT("LoadoutDataTable is not configured"));
	}

	if (DefaultLoadoutID.IsNone())
	{
		OutErrors.Add(TEXT("DefaultLoadoutID is empty!"));
	}

	//========================================================================
	// Animation System Validation
	//========================================================================
	if (WeaponAnimationsTable.IsNull())
	{
		// Warning only - animation system might use defaults
		UE_LOG(LogSuspenseCoreSettings, Verbose, TEXT("WeaponAnimationsTable is not configured"));
	}

	//========================================================================
	// Summary
	//========================================================================
	if (OutErrors.Num() > 0)
	{
		UE_LOG(LogSuspenseCoreSettings, Warning,
			TEXT("SuspenseCore Settings validation found %d issues"),
			OutErrors.Num());
	}
	else
	{
		UE_LOG(LogSuspenseCoreSettings, Log, TEXT("SuspenseCore Settings validation passed"));
	}

	return bIsValid;
}
