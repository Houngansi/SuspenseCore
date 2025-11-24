// Copyright MedCom Team. All Rights Reserved.

#include "Abilities/MedComCharacterSprintAbility.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitAttributeChangeThreshold.h"
#include "Attributes/MedComBaseAttributeSet.h"
#include "Interfaces/Core/IMedComMovementInterface.h"
#include "Interfaces/Core/IMedComCharacterInterface.h"
#include "GameplayEffectTypes.h"
#include "Engine/World.h"
#include "AbilitySystemLog.h"

UMedComCharacterSprintAbility::UMedComCharacterSprintAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    
    // Ability tags
    FGameplayTag SprintTag = FGameplayTag::RequestGameplayTag("Ability.Input.Sprint");
    SetAssetTags(FGameplayTagContainer(SprintTag));
    
    // Important! Set AbilityTags for activity check
    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag("Ability.Active.Sprint"));
    
    // Blocking tags
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Dead"));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Stunned"));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Disabled.Movement"));
    ActivationBlockedTags.AddTag(FGameplayTag::RequestGameplayTag("State.Crouching"));
    
    // Sprint parameters
    SprintSpeedMultiplier = 1.5f;
    StaminaCostPerSecond = 10.0f;
    MinimumStaminaToSprint = 10.0f;
}

bool UMedComCharacterSprintAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, 
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, 
    const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
    {
        return false;
    }
    
    // Check through movement interface
    if (ActorInfo && ActorInfo->AvatarActor.IsValid())
    {
        AActor* Avatar = ActorInfo->AvatarActor.Get();
        
        // Check if actor supports movement interface
        if (!Avatar->GetClass()->ImplementsInterface(UMedComMovementInterface::StaticClass()))
        {
            ABILITY_LOG(Warning, TEXT("[Sprint] Actor doesn't support IMedComMovementInterface"));
            return false;
        }
        
        // Check sprint capability through interface
        if (!IMedComMovementInterface::Execute_CanSprint(Avatar))
        {
            ABILITY_LOG(Warning, TEXT("[Sprint] Character cannot sprint"));
            return false;
        }
        
        // Check if already sprinting
        if (IMedComMovementInterface::Execute_IsSprinting(Avatar))
        {
            ABILITY_LOG(Warning, TEXT("[Sprint] Character is already sprinting"));
            return false;
        }
    }
    
    // Check stamina through ASC
    if (const UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
    {
        if (const UMedComBaseAttributeSet* Attributes = ASC->GetSet<UMedComBaseAttributeSet>())
        {
            if (Attributes->GetStamina() < MinimumStaminaToSprint)
            {
                ABILITY_LOG(Warning, TEXT("[Sprint] Not enough stamina: %.1f/%.1f"), 
                    Attributes->GetStamina(), MinimumStaminaToSprint);
                return false;
            }
        }
    }
    
    return true;
}

void UMedComCharacterSprintAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    if (!HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
    {
        ABILITY_LOG(Error, TEXT("[Sprint] No authority or prediction key"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        ABILITY_LOG(Error, TEXT("[Sprint] Failed to commit ability"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    
    // Save activation parameters
    CurrentSpecHandle = Handle;
    CurrentActorInfo = ActorInfo;
    CurrentActivationInfo = ActivationInfo;
    
    ABILITY_LOG(Display, TEXT("[Sprint] Activating sprint ability"));
    
    // Get avatar and ASC
    AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    if (!Avatar)
    {
        ABILITY_LOG(Error, TEXT("[Sprint] No valid avatar"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }
    
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC)
    {
        ABILITY_LOG(Error, TEXT("[Sprint] No ASC"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }
    
    // Apply speed buff effect FIRST
    if (SprintBuffEffectClass)
    {
        FGameplayEffectContextHandle BuffContext = ASC->MakeEffectContext();
        BuffContext.AddSourceObject(Avatar);
        
        FGameplayEffectSpecHandle BuffSpecHandle = ASC->MakeOutgoingSpec(
            SprintBuffEffectClass, GetAbilityLevel(), BuffContext);
            
        if (BuffSpecHandle.IsValid())
        {
            SprintBuffEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*BuffSpecHandle.Data.Get());
            
            if (SprintBuffEffectHandle.IsValid())
            {
                ABILITY_LOG(Display, TEXT("[Sprint] Speed buff effect applied successfully"));
            }
            else
            {
                ABILITY_LOG(Error, TEXT("[Sprint] Failed to apply speed buff effect"));
                EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
                return;
            }
        }
    }
    else
    {
        ABILITY_LOG(Warning, TEXT("[Sprint] SprintBuffEffectClass not configured!"));
    }
    
    // Apply stamina cost effect SECOND
    if (SprintCostEffectClass && StaminaCostPerSecond > 0.0f)
    {
        FGameplayEffectContextHandle CostContext = ASC->MakeEffectContext();
        CostContext.AddSourceObject(Avatar);
        
        FGameplayEffectSpecHandle CostSpecHandle = ASC->MakeOutgoingSpec(
            SprintCostEffectClass, GetAbilityLevel(), CostContext);
            
        if (CostSpecHandle.IsValid())
        {
            SprintCostEffectHandle = ASC->ApplyGameplayEffectSpecToSelf(*CostSpecHandle.Data.Get());
            
            if (SprintCostEffectHandle.IsValid())
            {
                ABILITY_LOG(Display, TEXT("[Sprint] Stamina cost effect applied successfully"));
            }
            else
            {
                ABILITY_LOG(Warning, TEXT("[Sprint] Failed to apply stamina cost effect"));
                // Not critical, we can sprint without stamina drain
            }
        }
    }
    else
    {
        ABILITY_LOG(Warning, TEXT("[Sprint] SprintCostEffectClass not configured or stamina cost is 0"));
    }
    
    // Monitor stamina depletion
    if (const UMedComBaseAttributeSet* Attributes = ASC->GetSet<UMedComBaseAttributeSet>())
    {
        UAbilityTask_WaitAttributeChangeThreshold* StaminaTask = 
            UAbilityTask_WaitAttributeChangeThreshold::WaitForAttributeChangeThreshold(
                this, 
                Attributes->GetStaminaAttribute(),
                EWaitAttributeChangeComparison::LessThanOrEqualTo,
                0.1f,
                false
            );
            
        if (StaminaTask)
        {
            StaminaTask->OnChange.AddDynamic(this, &UMedComCharacterSprintAbility::OnStaminaBelowThreshold);
            StaminaTask->ReadyForActivation();
            ABILITY_LOG(Display, TEXT("[Sprint] Stamina monitoring task activated"));
        }
    }
    
    // Setup input release monitoring
    UAbilityTask_WaitInputRelease* WaitReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
    if (WaitReleaseTask)
    {
        WaitReleaseTask->OnRelease.AddDynamic(this, &UMedComCharacterSprintAbility::OnSprintInputReleased);
        WaitReleaseTask->ReadyForActivation();
        ABILITY_LOG(Display, TEXT("[Sprint] Input release task activated"));
    }
    
    ABILITY_LOG(Display, TEXT("[Sprint] Ability activated successfully"));
    ABILITY_LOG(Display, TEXT("  - Speed buff: %s"), SprintBuffEffectHandle.IsValid() ? TEXT("Active") : TEXT("Failed"));
    ABILITY_LOG(Display, TEXT("  - Stamina drain: %s"), SprintCostEffectHandle.IsValid() ? TEXT("Active") : TEXT("None"));
}

void UMedComCharacterSprintAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    ABILITY_LOG(Display, TEXT("[Sprint] Ending ability (Cancelled: %s)"), 
        bWasCancelled ? TEXT("Yes") : TEXT("No"));
    
    // Get ASC for effect removal
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    
    // Remove speed buff effect
    if (ASC && SprintBuffEffectHandle.IsValid())
    {
        bool bRemoved = ASC->RemoveActiveGameplayEffect(SprintBuffEffectHandle);
        ABILITY_LOG(Display, TEXT("[Sprint] Speed buff effect removed: %s"), 
            bRemoved ? TEXT("Yes") : TEXT("No"));
        
        SprintBuffEffectHandle.Invalidate();
    }
    
    // Remove stamina cost effect
    if (ASC && SprintCostEffectHandle.IsValid())
    {
        bool bRemoved = ASC->RemoveActiveGameplayEffect(SprintCostEffectHandle);
        ABILITY_LOG(Display, TEXT("[Sprint] Stamina cost effect removed: %s"), 
            bRemoved ? TEXT("Yes") : TEXT("No"));
        
        SprintCostEffectHandle.Invalidate();
    }
    
    // Clear saved activation parameters
    CurrentSpecHandle = FGameplayAbilitySpecHandle();
    CurrentActorInfo = nullptr;
    CurrentActivationInfo = FGameplayAbilityActivationInfo();
    
    // Call parent implementation last
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UMedComCharacterSprintAbility::InputReleased(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputReleased(Handle, ActorInfo, ActivationInfo);
    
    ABILITY_LOG(Display, TEXT("[Sprint] InputReleased called"));
    
    // Check if ability is active
    if (IsActive())
    {
        ABILITY_LOG(Display, TEXT("[Sprint] Ability is active, ending it"));
        
        // End ability when button is released
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
    }
}

void UMedComCharacterSprintAbility::OnSprintInputReleased(float TimeHeld)
{
    ABILITY_LOG(Display, TEXT("[Sprint] Button released (held for %.2f sec)"), TimeHeld);
    
    // Use saved parameters
    if (CurrentSpecHandle.IsValid() && CurrentActorInfo)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}

void UMedComCharacterSprintAbility::OnStaminaBelowThreshold(bool bMatched, float CurrentValue)
{
    if (bMatched)
    {
        ABILITY_LOG(Display, TEXT("[Sprint] Stamina dropped below threshold: %.1f"), CurrentValue);
        
        // Use saved parameters
        if (CurrentSpecHandle.IsValid() && CurrentActorInfo)
        {
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        }
    }
}

void UMedComCharacterSprintAbility::InputPressed(const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
    Super::InputPressed(Handle, ActorInfo, ActivationInfo);
    
    // Log press for debugging
    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        const FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
        if (Spec)
        {
            ABILITY_LOG(Display, TEXT("[Sprint] InputPressed with InputID: %d"), Spec->InputID);
        }
    }
}