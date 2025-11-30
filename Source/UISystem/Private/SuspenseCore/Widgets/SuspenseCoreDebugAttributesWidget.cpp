// SuspenseCoreDebugAttributesWidget.cpp
// SuspenseCore - Debug Attribute Display
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCoreDebugAttributesWidget.h"
#include "SuspenseCore/Core/SuspenseCorePlayerState.h"
#include "SuspenseCore/Subsystems/SuspenseCoreCharacterClassSubsystem.h"
#include "SuspenseCore/Data/SuspenseCoreCharacterClassData.h"
#include "SuspenseCore/Components/SuspenseCoreAbilitySystemComponent.h"
#include "SuspenseCore/Attributes/SuspenseCoreAttributeSet.h"
#include "SuspenseCore/Attributes/SuspenseCoreShieldAttributeSet.h"
#include "SuspenseCore/Attributes/SuspenseCoreMovementAttributeSet.h"
#include "SuspenseCore/Attributes/SuspenseCoreProgressionAttributeSet.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreDebugUI, Log, All);

USuspenseCoreDebugAttributesWidget::USuspenseCoreDebugAttributesWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCoreDebugAttributesWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetupButtonBindings();
	PopulateClassSelector();

	if (TitleText)
	{
		TitleText->SetText(FText::FromString(TEXT("ATTRIBUTE DEBUG")));
	}

	AddLogMessage(TEXT("Debug widget initialized"));
	RefreshDisplay();
}

void USuspenseCoreDebugAttributesWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

void USuspenseCoreDebugAttributesWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateTimer += InDeltaTime;
	if (UpdateTimer >= UpdateInterval)
	{
		UpdateTimer = 0.0f;
		RefreshDisplay();
	}
}

void USuspenseCoreDebugAttributesWidget::SetupButtonBindings()
{
	if (ApplyClassButton)
	{
		ApplyClassButton->OnClicked.AddDynamic(this, &USuspenseCoreDebugAttributesWidget::OnApplyClassClicked);
	}

	if (DamageButton)
	{
		DamageButton->OnClicked.AddDynamic(this, &USuspenseCoreDebugAttributesWidget::OnDamageClicked);
	}

	if (HealButton)
	{
		HealButton->OnClicked.AddDynamic(this, &USuspenseCoreDebugAttributesWidget::OnHealClicked);
	}

	if (StaminaConsumeButton)
	{
		StaminaConsumeButton->OnClicked.AddDynamic(this, &USuspenseCoreDebugAttributesWidget::OnStaminaConsumeClicked);
	}

	if (ResetButton)
	{
		ResetButton->OnClicked.AddDynamic(this, &USuspenseCoreDebugAttributesWidget::OnResetClicked);
	}

	if (ClearLogButton)
	{
		ClearLogButton->OnClicked.AddDynamic(this, &USuspenseCoreDebugAttributesWidget::OnClearLogClicked);
	}

	if (ClassSelector)
	{
		ClassSelector->OnSelectionChanged.AddDynamic(this, &USuspenseCoreDebugAttributesWidget::OnClassSelectionChanged);
	}
}

void USuspenseCoreDebugAttributesWidget::PopulateClassSelector()
{
	if (!ClassSelector)
	{
		return;
	}

	ClassSelector->ClearOptions();

	USuspenseCoreCharacterClassSubsystem* ClassSystem = USuspenseCoreCharacterClassSubsystem::Get(this);
	if (!ClassSystem)
	{
		ClassSelector->AddOption(TEXT("No Class System"));
		return;
	}

	CachedClassSubsystem = ClassSystem;

	TArray<USuspenseCoreCharacterClassData*> AllClasses = ClassSystem->GetAllClasses();

	if (AllClasses.Num() == 0)
	{
		ClassSelector->AddOption(TEXT("No Classes Found"));
		AddLogMessage(TEXT("WARNING: No character classes loaded"));
		return;
	}

	for (USuspenseCoreCharacterClassData* ClassData : AllClasses)
	{
		if (ClassData)
		{
			FString OptionText = FString::Printf(TEXT("%s (%s)"),
				*ClassData->DisplayName.ToString(),
				*ClassData->ClassID.ToString());
			ClassSelector->AddOption(OptionText);
		}
	}

	AddLogMessage(FString::Printf(TEXT("Loaded %d character classes"), AllClasses.Num()));
}

ASuspenseCorePlayerState* USuspenseCoreDebugAttributesWidget::GetLocalPlayerState() const
{
	if (CachedPlayerState.IsValid())
	{
		return CachedPlayerState.Get();
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return nullptr;
	}

	ASuspenseCorePlayerState* PS = Cast<ASuspenseCorePlayerState>(PC->PlayerState);
	if (PS)
	{
		const_cast<USuspenseCoreDebugAttributesWidget*>(this)->CachedPlayerState = PS;
	}

	return PS;
}

void USuspenseCoreDebugAttributesWidget::RefreshDisplay()
{
	UpdateHealthDisplay();
	UpdateStaminaDisplay();
	UpdateShieldDisplay();
	UpdateCombatDisplay();
	UpdateMovementDisplay();
	UpdateProgressionDisplay();
	UpdateClassDisplay();
}

void USuspenseCoreDebugAttributesWidget::UpdateHealthDisplay()
{
	ASuspenseCorePlayerState* PS = GetLocalPlayerState();
	if (!PS)
	{
		return;
	}

	USuspenseCoreAbilitySystemComponent* ASC = PS->GetSuspenseCoreASC();
	if (!ASC)
	{
		return;
	}

	const USuspenseCoreAttributeSet* Attribs = ASC->GetSet<USuspenseCoreAttributeSet>();
	if (!Attribs)
	{
		return;
	}

	if (HealthValueText)
	{
		HealthValueText->SetText(FText::FromString(
			FormatPercent(Attribs->GetHealth(), Attribs->GetMaxHealth())));
	}

	if (MaxHealthValueText)
	{
		MaxHealthValueText->SetText(FText::FromString(FormatValue(Attribs->GetMaxHealth(), 0)));
	}

	if (HealthRegenValueText)
	{
		HealthRegenValueText->SetText(FText::FromString(FormatValue(Attribs->GetHealthRegen()) + TEXT("/s")));
	}

	if (ArmorValueText)
	{
		ArmorValueText->SetText(FText::FromString(FormatValue(Attribs->GetArmor(), 0)));
	}
}

void USuspenseCoreDebugAttributesWidget::UpdateStaminaDisplay()
{
	ASuspenseCorePlayerState* PS = GetLocalPlayerState();
	if (!PS)
	{
		return;
	}

	USuspenseCoreAbilitySystemComponent* ASC = PS->GetSuspenseCoreASC();
	if (!ASC)
	{
		return;
	}

	const USuspenseCoreAttributeSet* Attribs = ASC->GetSet<USuspenseCoreAttributeSet>();
	if (!Attribs)
	{
		return;
	}

	if (StaminaValueText)
	{
		StaminaValueText->SetText(FText::FromString(
			FormatPercent(Attribs->GetStamina(), Attribs->GetMaxStamina())));
	}

	if (MaxStaminaValueText)
	{
		MaxStaminaValueText->SetText(FText::FromString(FormatValue(Attribs->GetMaxStamina(), 0)));
	}

	if (StaminaRegenValueText)
	{
		StaminaRegenValueText->SetText(FText::FromString(FormatValue(Attribs->GetStaminaRegen()) + TEXT("/s")));
	}
}

void USuspenseCoreDebugAttributesWidget::UpdateShieldDisplay()
{
	ASuspenseCorePlayerState* PS = GetLocalPlayerState();
	if (!PS)
	{
		return;
	}

	USuspenseCoreAbilitySystemComponent* ASC = PS->GetSuspenseCoreASC();
	if (!ASC)
	{
		return;
	}

	const USuspenseCoreShieldAttributeSet* Attribs = ASC->GetSet<USuspenseCoreShieldAttributeSet>();
	if (!Attribs)
	{
		// Shield attributes not available
		if (ShieldValueText)
		{
			ShieldValueText->SetText(FText::FromString(TEXT("N/A")));
		}
		return;
	}

	if (ShieldValueText)
	{
		ShieldValueText->SetText(FText::FromString(
			FormatPercent(Attribs->GetShield(), Attribs->GetMaxShield())));
	}

	if (MaxShieldValueText)
	{
		MaxShieldValueText->SetText(FText::FromString(FormatValue(Attribs->GetMaxShield(), 0)));
	}

	if (ShieldRegenValueText)
	{
		ShieldRegenValueText->SetText(FText::FromString(FormatValue(Attribs->GetShieldRegen()) + TEXT("/s")));
	}
}

void USuspenseCoreDebugAttributesWidget::UpdateCombatDisplay()
{
	ASuspenseCorePlayerState* PS = GetLocalPlayerState();
	if (!PS)
	{
		return;
	}

	USuspenseCoreAbilitySystemComponent* ASC = PS->GetSuspenseCoreASC();
	if (!ASC)
	{
		return;
	}

	const USuspenseCoreAttributeSet* Attribs = ASC->GetSet<USuspenseCoreAttributeSet>();
	if (!Attribs)
	{
		return;
	}

	if (AttackPowerValueText)
	{
		AttackPowerValueText->SetText(FText::FromString(FormatValue(Attribs->GetAttackPower(), 2)));
	}

	if (MovementSpeedValueText)
	{
		MovementSpeedValueText->SetText(FText::FromString(FormatValue(Attribs->GetMovementSpeed(), 2) + TEXT("x")));
	}
}

void USuspenseCoreDebugAttributesWidget::UpdateMovementDisplay()
{
	ASuspenseCorePlayerState* PS = GetLocalPlayerState();
	if (!PS)
	{
		return;
	}

	USuspenseCoreAbilitySystemComponent* ASC = PS->GetSuspenseCoreASC();
	if (!ASC)
	{
		return;
	}

	const USuspenseCoreMovementAttributeSet* Attribs = ASC->GetSet<USuspenseCoreMovementAttributeSet>();
	if (!Attribs)
	{
		if (WalkSpeedValueText)
		{
			WalkSpeedValueText->SetText(FText::FromString(TEXT("N/A")));
		}
		return;
	}

	if (WalkSpeedValueText)
	{
		WalkSpeedValueText->SetText(FText::FromString(FormatValue(Attribs->GetWalkSpeed(), 0)));
	}

	if (SprintSpeedValueText)
	{
		SprintSpeedValueText->SetText(FText::FromString(FormatValue(Attribs->GetSprintSpeed(), 0)));
	}

	if (JumpHeightValueText)
	{
		JumpHeightValueText->SetText(FText::FromString(FormatValue(Attribs->GetJumpHeight(), 0)));
	}
}

void USuspenseCoreDebugAttributesWidget::UpdateProgressionDisplay()
{
	ASuspenseCorePlayerState* PS = GetLocalPlayerState();
	if (!PS)
	{
		return;
	}

	USuspenseCoreAbilitySystemComponent* ASC = PS->GetSuspenseCoreASC();
	if (!ASC)
	{
		return;
	}

	const USuspenseCoreProgressionAttributeSet* Attribs = ASC->GetSet<USuspenseCoreProgressionAttributeSet>();
	if (!Attribs)
	{
		if (LevelValueText)
		{
			LevelValueText->SetText(FText::FromString(FString::Printf(TEXT("%d"), PS->GetPlayerLevel())));
		}
		return;
	}

	if (LevelValueText)
	{
		LevelValueText->SetText(FText::FromString(FormatValue(Attribs->GetLevel(), 0)));
	}

	if (ExperienceValueText)
	{
		ExperienceValueText->SetText(FText::FromString(
			FString::Printf(TEXT("%.0f / %.0f"),
				Attribs->GetExperience(),
				Attribs->GetExperienceToNextLevel())));
	}
}

void USuspenseCoreDebugAttributesWidget::UpdateClassDisplay()
{
	ASuspenseCorePlayerState* PS = GetLocalPlayerState();
	if (!PS || !CurrentClassText)
	{
		return;
	}

	USuspenseCoreCharacterClassSubsystem* ClassSystem = CachedClassSubsystem.Get();
	if (!ClassSystem)
	{
		ClassSystem = USuspenseCoreCharacterClassSubsystem::Get(this);
		CachedClassSubsystem = ClassSystem;
	}

	if (!ClassSystem)
	{
		CurrentClassText->SetText(FText::FromString(TEXT("No Class System")));
		return;
	}

	USuspenseCoreCharacterClassData* CurrentClass = ClassSystem->GetPlayerCurrentClass(PS);
	if (CurrentClass)
	{
		CurrentClassText->SetText(FText::FromString(
			FString::Printf(TEXT("Current: %s"), *CurrentClass->DisplayName.ToString())));
	}
	else
	{
		CurrentClassText->SetText(FText::FromString(TEXT("Current: None")));
	}
}

FString USuspenseCoreDebugAttributesWidget::FormatValue(float Value, int32 Decimals) const
{
	if (Decimals == 0)
	{
		return FString::Printf(TEXT("%.0f"), Value);
	}
	else if (Decimals == 1)
	{
		return FString::Printf(TEXT("%.1f"), Value);
	}
	else
	{
		return FString::Printf(TEXT("%.2f"), Value);
	}
}

FString USuspenseCoreDebugAttributesWidget::FormatPercent(float Current, float Max) const
{
	float Percent = (Max > 0.0f) ? (Current / Max * 100.0f) : 0.0f;
	return FString::Printf(TEXT("%.0f / %.0f (%.0f%%)"), Current, Max, Percent);
}

void USuspenseCoreDebugAttributesWidget::ApplySelectedClass()
{
	if (SelectedClassId.IsNone())
	{
		AddLogMessage(TEXT("ERROR: No class selected"));
		return;
	}

	ASuspenseCorePlayerState* PS = GetLocalPlayerState();
	if (!PS)
	{
		AddLogMessage(TEXT("ERROR: No player state"));
		return;
	}

	USuspenseCoreCharacterClassSubsystem* ClassSystem = CachedClassSubsystem.Get();
	if (!ClassSystem)
	{
		AddLogMessage(TEXT("ERROR: No class system"));
		return;
	}

	if (ClassSystem->ApplyClassToPlayer(PS, SelectedClassId))
	{
		AddLogMessage(FString::Printf(TEXT("Applied class: %s"), *SelectedClassId.ToString()));
	}
	else
	{
		AddLogMessage(FString::Printf(TEXT("ERROR: Failed to apply class: %s"), *SelectedClassId.ToString()));
	}
}

void USuspenseCoreDebugAttributesWidget::TestDamage(float Amount)
{
	ASuspenseCorePlayerState* PS = GetLocalPlayerState();
	if (!PS)
	{
		return;
	}

	USuspenseCoreAbilitySystemComponent* ASC = PS->GetSuspenseCoreASC();
	if (!ASC)
	{
		return;
	}

	// Apply damage via IncomingDamage meta attribute
	ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetIncomingDamageAttribute(), Amount);

	AddLogMessage(FString::Printf(TEXT("Applied %.0f damage"), Amount));
}

void USuspenseCoreDebugAttributesWidget::TestHealing(float Amount)
{
	ASuspenseCorePlayerState* PS = GetLocalPlayerState();
	if (!PS)
	{
		return;
	}

	USuspenseCoreAbilitySystemComponent* ASC = PS->GetSuspenseCoreASC();
	if (!ASC)
	{
		return;
	}

	// Apply healing via IncomingHealing meta attribute
	ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetIncomingHealingAttribute(), Amount);

	AddLogMessage(FString::Printf(TEXT("Applied %.0f healing"), Amount));
}

void USuspenseCoreDebugAttributesWidget::TestStaminaConsume(float Amount)
{
	ASuspenseCorePlayerState* PS = GetLocalPlayerState();
	if (!PS)
	{
		return;
	}

	USuspenseCoreAbilitySystemComponent* ASC = PS->GetSuspenseCoreASC();
	if (!ASC)
	{
		return;
	}

	const USuspenseCoreAttributeSet* Attribs = ASC->GetSet<USuspenseCoreAttributeSet>();
	if (!Attribs)
	{
		return;
	}

	float NewStamina = FMath::Max(0.0f, Attribs->GetStamina() - Amount);
	ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetStaminaAttribute(), NewStamina);

	AddLogMessage(FString::Printf(TEXT("Consumed %.0f stamina"), Amount));
}

void USuspenseCoreDebugAttributesWidget::ResetAttributes()
{
	ASuspenseCorePlayerState* PS = GetLocalPlayerState();
	if (!PS)
	{
		return;
	}

	USuspenseCoreAbilitySystemComponent* ASC = PS->GetSuspenseCoreASC();
	if (!ASC)
	{
		return;
	}

	// Reset to base values
	ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetHealthAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetMaxHealthAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetStaminaAttribute(), 100.0f);
	ASC->SetNumericAttributeBase(USuspenseCoreAttributeSet::GetMaxStaminaAttribute(), 100.0f);

	AddLogMessage(TEXT("Attributes reset to defaults"));
}

void USuspenseCoreDebugAttributesWidget::AddLogMessage(const FString& Message)
{
	FString TimestampedMessage = FString::Printf(TEXT("[%.2f] %s"),
		GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f, *Message);

	LogEntries.Add(TimestampedMessage);

	// Trim to max entries
	while (LogEntries.Num() > MaxLogEntries)
	{
		LogEntries.RemoveAt(0);
	}

	// Update log container if available
	// (Would need to create text widgets dynamically or use a ListView)

	UE_LOG(LogSuspenseCoreDebugUI, Log, TEXT("%s"), *Message);
}

void USuspenseCoreDebugAttributesWidget::OnApplyClassClicked()
{
	ApplySelectedClass();
}

void USuspenseCoreDebugAttributesWidget::OnDamageClicked()
{
	TestDamage(25.0f);
}

void USuspenseCoreDebugAttributesWidget::OnHealClicked()
{
	TestHealing(25.0f);
}

void USuspenseCoreDebugAttributesWidget::OnStaminaConsumeClicked()
{
	TestStaminaConsume(20.0f);
}

void USuspenseCoreDebugAttributesWidget::OnResetClicked()
{
	ResetAttributes();
}

void USuspenseCoreDebugAttributesWidget::OnClearLogClicked()
{
	LogEntries.Empty();
	AddLogMessage(TEXT("Log cleared"));
}

void USuspenseCoreDebugAttributesWidget::OnClassSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	// Parse class ID from selection
	// Format: "DisplayName (ClassID)"
	int32 StartIndex = SelectedItem.Find(TEXT("("));
	int32 EndIndex = SelectedItem.Find(TEXT(")"));

	if (StartIndex != INDEX_NONE && EndIndex != INDEX_NONE)
	{
		FString ClassIdStr = SelectedItem.Mid(StartIndex + 1, EndIndex - StartIndex - 1);
		SelectedClassId = FName(*ClassIdStr);
		AddLogMessage(FString::Printf(TEXT("Selected class: %s"), *ClassIdStr));
	}
}
