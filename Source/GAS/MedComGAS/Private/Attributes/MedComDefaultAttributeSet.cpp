// Copyright MedCom Team. All Rights Reserved.

#include "Attributes/MedComDefaultAttributeSet.h"

UMedComDefaultAttributeSet::UMedComDefaultAttributeSet()
{
	// IMPORTANT: Do NOT set initial values here!
	// All initial values should be set through UMedComInitialAttributesEffect
	// This ensures a single source of truth for attribute initialization
    
	// Setting values to 0 here, they will be properly initialized by effect
	Health = 50.0f;
	MaxHealth = 50.0f;
	HealthRegen = 0.0f;
	Armor = 0.0f;
	AttackPower = 0.0f;
	MovementSpeed = 0.0f;
	Stamina = 0.0f;
	MaxStamina = 0.0f;
	StaminaRegen = 0.0f;
    
	UE_LOG(LogTemp, Log, TEXT("UMedComDefaultAttributeSet constructed - all values set to 0, waiting for InitialAttributesEffect"));
}