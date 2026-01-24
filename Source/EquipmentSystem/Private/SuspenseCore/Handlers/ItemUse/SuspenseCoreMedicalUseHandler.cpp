// SuspenseCoreMedicalUseHandler.cpp
// Handler for medical item usage
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Handlers/ItemUse/SuspenseCoreMedicalUseHandler.h"
#include "SuspenseCore/Data/SuspenseCoreDataManager.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Types/SuspenseCoreTypes.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreItemDataTable.h"
#include "SuspenseCore/Effects/Medical/GE_InstantHeal.h"
#include "SuspenseCore/Effects/Medical/GE_HealOverTime.h"
#include "SuspenseCore/Tags/SuspenseCoreMedicalNativeTags.h"
#include "SuspenseCore/Tags/SuspenseCoreGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayEffect.h"

DEFINE_LOG_CATEGORY_STATIC(LogMedicalUseHandler, Log, All);

#define HANDLER_LOG(Verbosity, Format, ...) \
	UE_LOG(LogMedicalUseHandler, Verbosity, TEXT("[MedicalUse] " Format), ##__VA_ARGS__)

//==================================================================
// Constructor
//==================================================================

USuspenseCoreMedicalUseHandler::USuspenseCoreMedicalUseHandler()
{
	// Duration configuration
	BandageDuration = 3.0f;
	MedkitDuration = 5.0f;
	PainkillerDuration = 2.0f;
	StimulantDuration = 2.0f;
	SplintDuration = 8.0f;
	SurgicalDuration = 15.0f;
	DefaultCooldown = 1.0f;

	// HoT configuration
	MedkitHoTPerTick = 5.0f;
	MedkitHoTDuration = 10.0f;
	SurgicalHoTPerTick = 10.0f;
	SurgicalHoTDuration = 15.0f;

	// Default effect classes (can be overridden in Blueprint/CDO)
	InstantHealEffectClass = UGE_InstantHeal::StaticClass();
	HealOverTimeEffectClass = UGE_HealOverTime::StaticClass();
}

void USuspenseCoreMedicalUseHandler::Initialize(
	USuspenseCoreDataManager* InDataManager,
	USuspenseCoreEventBus* InEventBus)
{
	DataManager = InDataManager;
	EventBus = InEventBus;

	HANDLER_LOG(Log, TEXT("Initialized with DataManager=%s, EventBus=%s"),
		InDataManager ? TEXT("Valid") : TEXT("NULL"),
		InEventBus ? TEXT("Valid") : TEXT("NULL"));
}

//==================================================================
// Handler Identity
//==================================================================

FGameplayTag USuspenseCoreMedicalUseHandler::GetHandlerTag() const
{
	return SuspenseCoreItemUseTags::Handler::TAG_ItemUse_Handler_Medical;
}

ESuspenseCoreHandlerPriority USuspenseCoreMedicalUseHandler::GetPriority() const
{
	return ESuspenseCoreHandlerPriority::Normal;
}

FText USuspenseCoreMedicalUseHandler::GetDisplayName() const
{
	return FText::FromString(TEXT("Use Medical Item"));
}

//==================================================================
// Supported Types
//==================================================================

FGameplayTagContainer USuspenseCoreMedicalUseHandler::GetSupportedSourceTags() const
{
	FGameplayTagContainer Tags;
	Tags.AddTag(FGameplayTag::RequestGameplayTag(FName("Item.Category.Medical"), false));
	Tags.AddTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical"), false));
	return Tags;
}

TArray<ESuspenseCoreItemUseContext> USuspenseCoreMedicalUseHandler::GetSupportedContexts() const
{
	return {
		ESuspenseCoreItemUseContext::DoubleClick,
		ESuspenseCoreItemUseContext::QuickSlot,
		ESuspenseCoreItemUseContext::Hotkey
	};
}

//==================================================================
// Validation
//==================================================================

bool USuspenseCoreMedicalUseHandler::CanHandle(const FSuspenseCoreItemUseRequest& Request) const
{
	// Must be one of supported contexts
	TArray<ESuspenseCoreItemUseContext> Contexts = GetSupportedContexts();
	if (!Contexts.Contains(Request.Context))
	{
		return false;
	}

	// Must have source item
	if (!Request.SourceItem.IsValid())
	{
		return false;
	}

	// Check if item has medical tag by looking up in DataManager
	if (DataManager.IsValid())
	{
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(Request.SourceItem.ItemID, ItemData))
		{
			FGameplayTag MedicalTag = FGameplayTag::RequestGameplayTag(FName("Item.Category.Medical"), false);
			FGameplayTag MedicalTag2 = FGameplayTag::RequestGameplayTag(FName("Item.Medical"), false);

			// Check ItemType tag
			if (ItemData.ItemType.MatchesTag(MedicalTag) || ItemData.ItemType.MatchesTag(MedicalTag2))
			{
				return true;
			}

			// Check ItemTags container
			if (ItemData.ItemTags.HasTag(MedicalTag) || ItemData.ItemTags.HasTag(MedicalTag2))
			{
				return true;
			}
		}
	}

	return false;
}

bool USuspenseCoreMedicalUseHandler::ValidateRequest(
	const FSuspenseCoreItemUseRequest& Request,
	FSuspenseCoreItemUseResponse& OutResponse) const
{
	// Check source is valid
	if (!Request.SourceItem.IsValid())
	{
		OutResponse = FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_NotUsable,
			FText::FromString(TEXT("Invalid medical item")));
		return false;
	}

	// Check quantity
	if (Request.SourceItem.Quantity <= 0)
	{
		OutResponse = FSuspenseCoreItemUseResponse::Failure(
			Request.RequestID,
			ESuspenseCoreItemUseResult::Failed_MissingRequirement,
			FText::FromString(TEXT("No medical items available")));
		return false;
	}

	// Could add health checks here (e.g., already at full health for medkit)

	return true;
}

//==================================================================
// Execution
//==================================================================

FSuspenseCoreItemUseResponse USuspenseCoreMedicalUseHandler::Execute(
	const FSuspenseCoreItemUseRequest& Request,
	AActor* OwnerActor)
{
	// Get item tags from DataManager
	FGameplayTagContainer ItemTags;
	if (DataManager.IsValid())
	{
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(Request.SourceItem.ItemID, ItemData))
		{
			ItemTags = ItemData.ItemTags;
		}
	}

	ESuspenseCoreMedicalType MedType = GetMedicalType(ItemTags);

	HANDLER_LOG(Log, TEXT("Execute: Using medical item %s (type=%d)"),
		*Request.SourceItem.ItemID.ToString(),
		static_cast<int32>(MedType));

	// Get duration for this operation
	float Duration = GetDuration(Request);

	// Return InProgress response - actual healing happens in OnOperationComplete
	FSuspenseCoreItemUseResponse Response = FSuspenseCoreItemUseResponse::Success(Request.RequestID, Duration);
	Response.HandlerTag = GetHandlerTag();
	Response.Cooldown = GetCooldown(Request);

	// Publish started event
	PublishMedicalEvent(Request, Response, OwnerActor);

	return Response;
}

float USuspenseCoreMedicalUseHandler::GetDuration(const FSuspenseCoreItemUseRequest& Request) const
{
	// Get item tags from DataManager
	FGameplayTagContainer ItemTags;
	if (DataManager.IsValid())
	{
		FSuspenseCoreUnifiedItemData ItemData;
		if (DataManager->GetUnifiedItemData(Request.SourceItem.ItemID, ItemData))
		{
			ItemTags = ItemData.ItemTags;

			// If UseTime is defined in item data, use that
			if (ItemData.bIsConsumable && ItemData.UseTime > 0.0f)
			{
				return ItemData.UseTime;
			}
		}
	}

	ESuspenseCoreMedicalType MedType = GetMedicalType(ItemTags);
	return GetMedicalDuration(MedType);
}

float USuspenseCoreMedicalUseHandler::GetCooldown(const FSuspenseCoreItemUseRequest& Request) const
{
	return DefaultCooldown;
}

bool USuspenseCoreMedicalUseHandler::CancelOperation(const FGuid& RequestID)
{
	HANDLER_LOG(Log, TEXT("CancelOperation: %s"), *RequestID.ToString().Left(8));
	// Cancellation is handled by GAS ability
	// No healing is applied if cancelled
	return true;
}

FSuspenseCoreItemUseResponse USuspenseCoreMedicalUseHandler::OnOperationComplete(
	const FSuspenseCoreItemUseRequest& Request,
	AActor* OwnerActor)
{
	HANDLER_LOG(Log, TEXT("OnOperationComplete: Applying healing for %s"),
		*Request.RequestID.ToString().Left(8));

	FSuspenseCoreItemUseResponse Response = FSuspenseCoreItemUseResponse::Success(Request.RequestID);
	Response.HandlerTag = GetHandlerTag();
	Response.Progress = 1.0f;

	// Get medical capabilities for this item
	bool bCanCureLightBleed = false;
	bool bCanCureHeavyBleed = false;
	bool bCanCureFracture = false;
	float HoTAmount = 0.0f;
	float HoTDuration = 0.0f;

	GetMedicalCapabilities(
		Request.SourceItem.ItemID,
		bCanCureLightBleed,
		bCanCureHeavyBleed,
		bCanCureFracture,
		HoTAmount,
		HoTDuration);

	// Get instant heal amount from item data
	float InstantHealAmount = GetHealAmount(Request.SourceItem.ItemID);

	if (OwnerActor)
	{
		// 1. Apply instant healing
		if (InstantHealAmount > 0.0f)
		{
			bool bHealed = ApplyHealing(OwnerActor, InstantHealAmount);
			if (bHealed)
			{
				Response.Metadata.Add(TEXT("InstantHeal"), FString::SanitizeFloat(InstantHealAmount));
				HANDLER_LOG(Log, TEXT("Applied %.1f instant healing"), InstantHealAmount);
			}
		}

		// 2. Apply HoT if applicable (Medkits, Surgical Kits)
		if (HoTAmount > 0.0f && HoTDuration > 0.0f)
		{
			bool bHoTApplied = ApplyHealOverTime(OwnerActor, HoTAmount, HoTDuration);
			if (bHoTApplied)
			{
				Response.Metadata.Add(TEXT("HoTPerTick"), FString::SanitizeFloat(HoTAmount));
				Response.Metadata.Add(TEXT("HoTDuration"), FString::SanitizeFloat(HoTDuration));
				HANDLER_LOG(Log, TEXT("Applied HoT: %.1f/tick for %.1fs"), HoTAmount, HoTDuration);
			}
		}

		// 3. Cure bleeding effects
		if (bCanCureLightBleed || bCanCureHeavyBleed)
		{
			int32 BleedsCured = CureBleedingEffect(OwnerActor, bCanCureLightBleed, bCanCureHeavyBleed);
			if (BleedsCured > 0)
			{
				Response.Metadata.Add(TEXT("BleedingCured"), FString::FromInt(BleedsCured));
				HANDLER_LOG(Log, TEXT("Cured %d bleeding effect(s)"), BleedsCured);
			}
		}

		// 4. Cure fractures (Splints, Surgical Kits)
		if (bCanCureFracture)
		{
			int32 FracturesCured = CureFractureEffect(OwnerActor);
			if (FracturesCured > 0)
			{
				Response.Metadata.Add(TEXT("FracturesCured"), FString::FromInt(FracturesCured));
				HANDLER_LOG(Log, TEXT("Cured %d fracture(s)"), FracturesCured);
			}
		}
	}

	// Update modified items - consume one
	Response.ModifiedSourceItem = Request.SourceItem;
	Response.ModifiedSourceItem.Quantity -= 1;

	Response.Metadata.Add(TEXT("RemainingQuantity"),
		FString::FromInt(Response.ModifiedSourceItem.Quantity));

	// Publish completion event
	PublishMedicalEvent(Request, Response, OwnerActor);

	return Response;
}

//==================================================================
// Internal Methods
//==================================================================

ESuspenseCoreMedicalType USuspenseCoreMedicalUseHandler::GetMedicalType(
	const FGameplayTagContainer& ItemTags) const
{
	// Check for specific medical type tags
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Bandage"), false)))
	{
		return ESuspenseCoreMedicalType::Bandage;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Medkit"), false)))
	{
		return ESuspenseCoreMedicalType::Medkit;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Painkiller"), false)))
	{
		return ESuspenseCoreMedicalType::Painkiller;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Stimulant"), false)))
	{
		return ESuspenseCoreMedicalType::Stimulant;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Splint"), false)))
	{
		return ESuspenseCoreMedicalType::Splint;
	}
	if (ItemTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Item.Medical.Surgical"), false)))
	{
		return ESuspenseCoreMedicalType::Surgical;
	}

	// Default to bandage
	return ESuspenseCoreMedicalType::Bandage;
}

float USuspenseCoreMedicalUseHandler::GetMedicalDuration(ESuspenseCoreMedicalType Type) const
{
	switch (Type)
	{
		case ESuspenseCoreMedicalType::Bandage:
			return BandageDuration;
		case ESuspenseCoreMedicalType::Medkit:
			return MedkitDuration;
		case ESuspenseCoreMedicalType::Painkiller:
			return PainkillerDuration;
		case ESuspenseCoreMedicalType::Stimulant:
			return StimulantDuration;
		case ESuspenseCoreMedicalType::Splint:
			return SplintDuration;
		case ESuspenseCoreMedicalType::Surgical:
			return SurgicalDuration;
		default:
			return BandageDuration;
	}
}

float USuspenseCoreMedicalUseHandler::GetHealAmount(FName ItemID) const
{
	// SSOT Integration - Read from ConsumableAttributesDataTable
	// @see Content/Data/ItemDatabase/SuspenseCoreConsumableAttributes.json
	FSuspenseCoreConsumableAttributeRow ConsumableData;
	if (DataManager.IsValid() && DataManager->GetConsumableAttributes(ItemID, ConsumableData))
	{
		HANDLER_LOG(Verbose, TEXT("GetHealAmount (SSOT): %s = %.0f HP"), *ItemID.ToString(), ConsumableData.HealAmount);
		return ConsumableData.HealAmount;
	}

	// FALLBACK: Hardcoded heal amounts if SSOT not available
	HANDLER_LOG(Warning, TEXT("GetHealAmount: SSOT lookup failed for %s, using fallback"), *ItemID.ToString());

	FString ItemName = ItemID.ToString();
	if (ItemName.Contains(TEXT("Grizzly")))
	{
		return 1800.0f;
	}
	if (ItemName.Contains(TEXT("Salewa")))
	{
		return 400.0f;
	}
	if (ItemName.Contains(TEXT("CarMedkit")))
	{
		return 220.0f;
	}
	if (ItemName.Contains(TEXT("IFAK")))
	{
		return 150.0f;
	}
	if (ItemName.Contains(TEXT("Bandage")) || ItemName.Contains(TEXT("Splint")) ||
		ItemName.Contains(TEXT("Morphine")) || ItemName.Contains(TEXT("Painkiller")))
	{
		return 0.0f; // These don't heal HP
	}

	return 0.0f;
}

bool USuspenseCoreMedicalUseHandler::ApplyHealing(AActor* Actor, float HealAmount) const
{
	if (!Actor || HealAmount <= 0.0f)
	{
		return false;
	}

	// Try to get AbilitySystemComponent
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
	if (!ASC)
	{
		HANDLER_LOG(Warning, TEXT("ApplyHealing: No ASC found on actor"));
		return false;
	}

	// Apply healing via GE_InstantHeal GameplayEffect
	if (!InstantHealEffectClass)
	{
		HANDLER_LOG(Warning, TEXT("ApplyHealing: InstantHealEffectClass not set"));
		return false;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(Actor);

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
		InstantHealEffectClass,
		1.0f,
		Context);

	if (!SpecHandle.IsValid())
	{
		HANDLER_LOG(Warning, TEXT("ApplyHealing: Failed to create effect spec"));
		return false;
	}

	// Set heal amount via SetByCaller
	SpecHandle.Data->SetSetByCallerMagnitude(
		SuspenseCoreMedicalTags::Data::TAG_Data_Medical_InstantHeal,
		HealAmount);

	FActiveGameplayEffectHandle ActiveHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);

	if (ActiveHandle.IsValid())
	{
		HANDLER_LOG(Log, TEXT("ApplyHealing: Applied %.1f instant healing to %s"),
			HealAmount, *Actor->GetName());
		return true;
	}

	HANDLER_LOG(Warning, TEXT("ApplyHealing: Failed to apply effect"));
	return false;
}

bool USuspenseCoreMedicalUseHandler::ApplyHealOverTime(AActor* Actor, float HealPerTick, float Duration) const
{
	if (!Actor || HealPerTick <= 0.0f || Duration <= 0.0f)
	{
		return false;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
	if (!ASC)
	{
		HANDLER_LOG(Warning, TEXT("ApplyHealOverTime: No ASC found on actor"));
		return false;
	}

	if (!HealOverTimeEffectClass)
	{
		HANDLER_LOG(Warning, TEXT("ApplyHealOverTime: HealOverTimeEffectClass not set"));
		return false;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(Actor);

	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
		HealOverTimeEffectClass,
		1.0f,
		Context);

	if (!SpecHandle.IsValid())
	{
		HANDLER_LOG(Warning, TEXT("ApplyHealOverTime: Failed to create effect spec"));
		return false;
	}

	// Set HoT parameters via SetByCaller
	SpecHandle.Data->SetSetByCallerMagnitude(
		SuspenseCoreMedicalTags::Data::TAG_Data_Medical_HealPerTick,
		HealPerTick);

	SpecHandle.Data->SetSetByCallerMagnitude(
		SuspenseCoreMedicalTags::Data::TAG_Data_Medical_HoTDuration,
		Duration);

	FActiveGameplayEffectHandle ActiveHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);

	if (ActiveHandle.IsValid())
	{
		HANDLER_LOG(Log, TEXT("ApplyHealOverTime: Applied %.1f/tick for %.1fs to %s"),
			HealPerTick, Duration, *Actor->GetName());

		// Publish HoT started event
		if (EventBus.IsValid())
		{
			FSuspenseCoreEventData EventData;
			EventData.Source = Actor;
			EventData.Timestamp = FPlatformTime::Seconds();
			EventData.FloatPayload.Add(TEXT("HealPerTick"), HealPerTick);
			EventData.FloatPayload.Add(TEXT("Duration"), Duration);
			EventBus->Publish(SuspenseCoreMedicalTags::Event::TAG_Event_Medical_HoTStarted, EventData);
		}

		return true;
	}

	HANDLER_LOG(Warning, TEXT("ApplyHealOverTime: Failed to apply effect"));
	return false;
}

int32 USuspenseCoreMedicalUseHandler::CureBleedingEffect(AActor* Actor, bool bCanCureLightBleed, bool bCanCureHeavyBleed) const
{
	HANDLER_LOG(Log, TEXT("CureBleedingEffect: Starting (LightBleed=%d, HeavyBleed=%d)"),
		bCanCureLightBleed, bCanCureHeavyBleed);

	if (!Actor)
	{
		HANDLER_LOG(Warning, TEXT("CureBleedingEffect: Actor is null"));
		return 0;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
	if (!ASC)
	{
		HANDLER_LOG(Warning, TEXT("CureBleedingEffect: No ASC found on actor"));
		return 0;
	}

	int32 TotalRemoved = 0;

	// Remove light bleeding effects
	if (bCanCureLightBleed)
	{
		// Try with native tag first
		FGameplayTagContainer LightBleedTags;
		LightBleedTags.AddTag(SuspenseCoreTags::State::Health::BleedingLight);

		HANDLER_LOG(Verbose, TEXT("CureBleedingEffect: Looking for light bleed tag: %s"),
			*SuspenseCoreTags::State::Health::BleedingLight.ToString());

		int32 Removed = ASC->RemoveActiveEffectsWithGrantedTags(LightBleedTags);

		// Also try with the exact tag string used in bleeding effect
		if (Removed == 0)
		{
			FGameplayTagContainer AlternateTags;
			AlternateTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Bleeding.Light"), false));
			Removed = ASC->RemoveActiveEffectsWithGrantedTags(AlternateTags);
		}

		TotalRemoved += Removed;

		HANDLER_LOG(Log, TEXT("CureBleedingEffect: Light bleed - removed %d effect(s)"), Removed);
	}

	// Remove heavy bleeding effects
	if (bCanCureHeavyBleed)
	{
		FGameplayTagContainer HeavyBleedTags;
		HeavyBleedTags.AddTag(SuspenseCoreTags::State::Health::BleedingHeavy);

		HANDLER_LOG(Verbose, TEXT("CureBleedingEffect: Looking for heavy bleed tag: %s"),
			*SuspenseCoreTags::State::Health::BleedingHeavy.ToString());

		int32 Removed = ASC->RemoveActiveEffectsWithGrantedTags(HeavyBleedTags);

		// Also try with the exact tag string used in bleeding effect
		if (Removed == 0)
		{
			FGameplayTagContainer AlternateTags;
			AlternateTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Health.Bleeding.Heavy"), false));
			Removed = ASC->RemoveActiveEffectsWithGrantedTags(AlternateTags);
		}

		TotalRemoved += Removed;

		HANDLER_LOG(Log, TEXT("CureBleedingEffect: Heavy bleed - removed %d effect(s)"), Removed);
	}

	// Publish bleeding cured event
	if (TotalRemoved > 0 && EventBus.IsValid())
	{
		FSuspenseCoreEventData EventData;
		EventData.Source = Actor;
		EventData.Timestamp = FPlatformTime::Seconds();
		EventData.IntPayload.Add(TEXT("BleedingsCured"), TotalRemoved);
		EventData.BoolPayload.Add(TEXT("LightBleed"), bCanCureLightBleed);
		EventData.BoolPayload.Add(TEXT("HeavyBleed"), bCanCureHeavyBleed);
		EventBus->Publish(SuspenseCoreMedicalTags::Event::TAG_Event_Medical_BleedingCured, EventData);
	}

	return TotalRemoved;
}

int32 USuspenseCoreMedicalUseHandler::CureFractureEffect(AActor* Actor) const
{
	if (!Actor)
	{
		return 0;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor);
	if (!ASC)
	{
		HANDLER_LOG(Warning, TEXT("CureFractureEffect: No ASC found on actor"));
		return 0;
	}

	int32 TotalRemoved = 0;

	// Remove all fracture effects
	FGameplayTagContainer FractureTags;
	FractureTags.AddTag(SuspenseCoreTags::State::Health::Fracture);
	FractureTags.AddTag(SuspenseCoreTags::State::Health::FractureLeg);
	FractureTags.AddTag(SuspenseCoreTags::State::Health::FractureArm);

	TotalRemoved = ASC->RemoveActiveEffectsWithGrantedTags(FractureTags);

	if (TotalRemoved > 0)
	{
		HANDLER_LOG(Log, TEXT("CureFractureEffect: Removed %d fracture effect(s)"), TotalRemoved);

		// Publish status cured event
		if (EventBus.IsValid())
		{
			FSuspenseCoreEventData EventData;
			EventData.Source = Actor;
			EventData.Timestamp = FPlatformTime::Seconds();
			EventData.IntPayload.Add(TEXT("FracturesCured"), TotalRemoved);
			EventBus->Publish(SuspenseCoreMedicalTags::Event::TAG_Event_Medical_StatusCured, EventData);
		}
	}

	return TotalRemoved;
}

void USuspenseCoreMedicalUseHandler::GetMedicalCapabilities(
	FName ItemID,
	bool& OutCanCureLightBleed,
	bool& OutCanCureHeavyBleed,
	bool& OutCanCureFracture,
	float& OutHoTAmount,
	float& OutHoTDuration) const
{
	// Reset outputs
	OutCanCureLightBleed = false;
	OutCanCureHeavyBleed = false;
	OutCanCureFracture = false;
	OutHoTAmount = 0.0f;
	OutHoTDuration = 0.0f;

	// SSOT Integration - Read from ConsumableAttributesDataTable
	// @see Content/Data/ItemDatabase/SuspenseCoreConsumableAttributes.json
	FSuspenseCoreConsumableAttributeRow ConsumableData;
	if (DataManager.IsValid() && DataManager->GetConsumableAttributes(ItemID, ConsumableData))
	{
		OutCanCureLightBleed = ConsumableData.bCanHealLightBleed;
		OutCanCureHeavyBleed = ConsumableData.bCanHealHeavyBleed;
		OutCanCureFracture = ConsumableData.bCanHealFracture;

		// HoT calculation: HealRate is HP per second
		// For post-use HoT, calculate duration from total heal / rate
		if (ConsumableData.HealRate > 0.0f && ConsumableData.HealAmount > 0.0f)
		{
			OutHoTAmount = ConsumableData.HealRate;
			OutHoTDuration = ConsumableData.HealAmount / ConsumableData.HealRate;
		}

		HANDLER_LOG(Log, TEXT("GetMedicalCapabilities (SSOT): %s -> LightBleed=%d, HeavyBleed=%d, Fracture=%d, HoT=%.1f/%.1fs"),
			*ItemID.ToString(),
			OutCanCureLightBleed,
			OutCanCureHeavyBleed,
			OutCanCureFracture,
			OutHoTAmount,
			OutHoTDuration);
		return;
	}

	// FALLBACK: Hardcoded capabilities if SSOT not available
	HANDLER_LOG(Warning, TEXT("GetMedicalCapabilities: SSOT lookup failed for %s, using fallback"), *ItemID.ToString());

	FString ItemName = ItemID.ToString();

	if (ItemName.Contains(TEXT("Bandage")))
	{
		OutCanCureLightBleed = true;
	}
	else if (ItemName.Contains(TEXT("Medkit")) || ItemName.Contains(TEXT("IFAK")) ||
			 ItemName.Contains(TEXT("AFAK")) || ItemName.Contains(TEXT("Salewa")))
	{
		OutCanCureLightBleed = true;
		OutCanCureHeavyBleed = true;
		OutHoTAmount = MedkitHoTPerTick;
		OutHoTDuration = MedkitHoTDuration;
	}
	else if (ItemName.Contains(TEXT("Splint")))
	{
		OutCanCureFracture = true;
	}
	else if (ItemName.Contains(TEXT("Grizzly")) || ItemName.Contains(TEXT("Surgical")))
	{
		OutCanCureLightBleed = true;
		OutCanCureHeavyBleed = true;
		OutCanCureFracture = true;
		OutHoTAmount = SurgicalHoTPerTick;
		OutHoTDuration = SurgicalHoTDuration;
	}
}

void USuspenseCoreMedicalUseHandler::PublishMedicalEvent(
	const FSuspenseCoreItemUseRequest& Request,
	const FSuspenseCoreItemUseResponse& Response,
	AActor* OwnerActor)
{
	if (!EventBus.IsValid())
	{
		return;
	}

	FSuspenseCoreEventData EventData;
	EventData.Source = OwnerActor;
	EventData.Timestamp = FPlatformTime::Seconds();
	EventData.StringPayload.Add(TEXT("RequestID"), Request.RequestID.ToString());
	EventData.StringPayload.Add(TEXT("ItemID"), Request.SourceItem.ItemID.ToString());
	EventData.IntPayload.Add(TEXT("Result"), static_cast<int32>(Response.Result));
	EventData.FloatPayload.Add(TEXT("Duration"), Response.Duration);

	if (Response.Metadata.Contains(TEXT("HealAmount")))
	{
		EventData.FloatPayload.Add(TEXT("HealAmount"),
			FCString::Atof(*Response.Metadata[TEXT("HealAmount")]));
	}

	FGameplayTag EventTag;
	if (Response.IsInProgress())
	{
		EventTag = SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Started;
	}
	else if (Response.IsSuccess())
	{
		EventTag = SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Completed;
	}
	else
	{
		EventTag = SuspenseCoreItemUseTags::Event::TAG_ItemUse_Event_Failed;
	}

	EventBus->Publish(EventTag, EventData);
}
