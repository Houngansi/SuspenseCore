// Copyright SuspenseCore Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreEquipmentAttachmentComponent.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentMeshComponent.h"
#include "SuspenseCore/Components/SuspenseCoreWeaponStanceComponent.h"
#include "Interfaces/Weapon/ISuspenseWeaponAnimation.h"
#include "Subsystems/WeaponAnimationSubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "ItemSystem/SuspenseItemManager.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

// Static socket priority lists
const TArray<FName> USuspenseCoreEquipmentAttachmentComponent::WeaponSocketPriority = {
    FName("GripPoint"),
    FName("weapon_r"),
    FName("RightHandSocket"),
    FName("hand_r"),
    FName("WeaponSocket")
};

const TArray<FName> USuspenseCoreEquipmentAttachmentComponent::ArmorSocketPriority = {
    FName("spine_03"),
    FName("spine_02"),
    FName("pelvis"),
    FName("root")
};

const TArray<FName> USuspenseCoreEquipmentAttachmentComponent::AccessorySocketPriority = {
    FName("head"),
    FName("neck_01"),
    FName("spine_03"),
    FName("pelvis")
};

USuspenseCoreEquipmentAttachmentComponent::USuspenseCoreEquipmentAttachmentComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f; // Tick every frame for animation updates
    SetIsReplicatedByDefault(true);

    SpawnedEquipmentActor = nullptr;
    AttachTarget = nullptr;
    bIsAttached = false;
    bIsInActiveState = false;
    CurrentSocketName = NAME_None;
    CurrentAttachmentOffset = FTransform::Identity;
    CurrentWeaponType = FGameplayTag();
    bDidSpawnActor = false;
    AttachmentVersion = 1;
    NextAttachmentPredictionKey = 1;
    LastSocketCacheTime = 0.0f;
    LastAnimationInterfaceCacheTime = 0.0f;
    bAutoLinkStanceComponent = true;
}

void USuspenseCoreEquipmentAttachmentComponent::BeginPlay()
{
    Super::BeginPlay();

    // Auto-link stance component if enabled
    if (bAutoLinkStanceComponent)
    {
        if (AActor* Owner = GetOwner())
        {
            // Stance component should be on the character that owns this equipment
            if (APawn* OwnerPawn = Cast<APawn>(Owner))
            {
                if (USuspenseCoreWeaponStanceComponent* StanceComp = OwnerPawn->FindComponentByClass<USuspenseCoreWeaponStanceComponent>())
                {
                    LinkStanceComponent(StanceComp);
                    UE_LOG(LogTemp, Log, TEXT("EquipmentAttachmentComponent: Auto-linked to stance component"));
                }
            }
        }
    }
}

void USuspenseCoreEquipmentAttachmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clear timers
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AnimationCompletionTimer);
    }

    // Clean up any attachments
    if (IsAttached())
    {
        Detach(false);
    }

    // Destroy spawned actor ONLY if we created it
    if (bDidSpawnActor && SpawnedEquipmentActor && SpawnedEquipmentActor != GetOwner())
    {
        DestroyEquipmentActor();
    }

    // Clear references
    LinkedStanceComponent.Reset();
    CachedAnimationInterface = nullptr;

    // Clear caches
    {
        FScopeLock Lock(&SocketCacheCriticalSection);
        SocketCache.Empty();
    }

    Super::EndPlay(EndPlayReason);
}

void USuspenseCoreEquipmentAttachmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update animation state
    if (AnimationState.bIsPlaying)
    {
        UpdateAnimationState(DeltaTime);
    }

    // Clean up expired predictions on clients
    if (!GetOwner()->HasAuthority())
    {
        CleanupExpiredPredictions();
    }
}

void USuspenseCoreEquipmentAttachmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, SpawnedEquipmentActor);
    DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, bIsAttached);
    DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, bIsInActiveState);
    DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, CurrentSocketName);
    DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, CurrentAttachmentOffset);
    DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, CurrentWeaponType);
    DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, bDidSpawnActor);
    DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, AttachmentVersion);
    DOREPLIFETIME(USuspenseCoreEquipmentAttachmentComponent, AnimationState);
}

void USuspenseCoreEquipmentAttachmentComponent::InitializeWithItemInstance(AActor* InOwner, UAbilitySystemComponent* InASC, const FSuspenseInventoryItemInstance& ItemInstance)
{
    // Call base initialization
    Super::InitializeWithItemInstance(InOwner, InASC, ItemInstance);

    if (!IsInitialized())
    {
        EQUIPMENT_LOG(Error, TEXT("Failed to initialize base component"));
        return;
    }

    // Get item data
    FSuspenseUnifiedItemData ItemData;
    if (!GetEquippedItemData(ItemData))
    {
        EQUIPMENT_LOG(Error, TEXT("Failed to get item data for attachment"));
        return;
    }

    // Store weapon type for animations
    CurrentWeaponType = GetWeaponArchetypeFromItem();

    // Check if we are already part of an equipment actor
    AActor* OwnerActor = GetOwner();
    bool bIsPartOfEquipmentActor = false;

    if (OwnerActor)
    {
        // Check if our owner already has mesh component and other equipment components
        if (OwnerActor->FindComponentByClass<USuspenseCoreEquipmentMeshComponent>())
        {
            bIsPartOfEquipmentActor = true;
            EQUIPMENT_LOG(Log, TEXT("AttachmentComponent is part of equipment actor %s"), *OwnerActor->GetName());
        }
    }

    // Only spawn equipment actor if we're NOT already part of one
    if (!bIsPartOfEquipmentActor && GetOwner() && GetOwner()->HasAuthority())
    {
        if (ItemData.bIsEquippable && !ItemData.EquipmentActorClass.IsNull())
        {
            SpawnedEquipmentActor = SpawnEquipmentActor(ItemData);
            if (SpawnedEquipmentActor)
            {
                bDidSpawnActor = true;
                AttachmentVersion++;

                // Force replication update on the owner actor
                if (AActor* Owner = GetOwner())
                {
                    Owner->ForceNetUpdate();
                }
            }
        }
    }
    else if (bIsPartOfEquipmentActor)
    {
        // We are part of the equipment actor, so the actor IS our owner
        SpawnedEquipmentActor = OwnerActor;
        bDidSpawnActor = false;
        EQUIPMENT_LOG(Log, TEXT("Using owner as equipment actor: %s"), *OwnerActor->GetName());
    }

    // Notify stance component of new equipment
    if (LinkedStanceComponent.IsValid())
    {
        LinkedStanceComponent->OnEquipmentChanged(SpawnedEquipmentActor);
    }

    EQUIPMENT_LOG(Log, TEXT("Initialized attachment for item: %s"), *ItemInstance.ItemID.ToString());
}

void USuspenseCoreEquipmentAttachmentComponent::Cleanup()
{
    // Detach first
    Detach(false);

    // Notify stance component
    if (LinkedStanceComponent.IsValid())
    {
        LinkedStanceComponent->OnEquipmentChanged(nullptr);
    }

    // Destroy spawned actor ONLY if we created it
    if (bDidSpawnActor && SpawnedEquipmentActor && SpawnedEquipmentActor != GetOwner())
    {
        DestroyEquipmentActor();
    }

    // Reset state
    SpawnedEquipmentActor = nullptr;
    AttachTarget = nullptr;
    bIsAttached = false;
    bIsInActiveState = false;
    CurrentSocketName = NAME_None;
    CurrentAttachmentOffset = FTransform::Identity;
    CurrentWeaponType = FGameplayTag();
    AttachedCharacter.Reset();
    bDidSpawnActor = false;
    AttachmentPredictions.Empty();
    AnimationState = FAttachmentAnimationState();

    // Update version and force replication
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        AttachmentVersion++;

        if (AActor* Owner = GetOwner())
        {
            Owner->ForceNetUpdate();
        }
    }

    // Call base cleanup
    Super::Cleanup();
}

void USuspenseCoreEquipmentAttachmentComponent::UpdateEquippedItem(const FSuspenseInventoryItemInstance& NewItemInstance)
{
    // Store current attachment state
    TWeakObjectPtr<AActor> CurrentCharacter = AttachedCharacter;
    bool bWasAttached = IsAttached();
    bool bWasActiveSocket = bIsInActiveState;

    // Detach current equipment
    if (bWasAttached)
    {
        Detach(false);
    }

    // Update base item
    Super::UpdateEquippedItem(NewItemInstance);

    // Handle new item
    if (NewItemInstance.IsValid())
    {
        FSuspenseUnifiedItemData ItemData;
        if (GetEquippedItemData(ItemData))
        {
            // Update weapon type
            CurrentWeaponType = GetWeaponArchetypeFromItem();

            // Destroy old actor ONLY if we spawned it
            if (bDidSpawnActor && SpawnedEquipmentActor && SpawnedEquipmentActor != GetOwner())
            {
                DestroyEquipmentActor();
            }

            // Check if we need to spawn new actor
            bool bIsPartOfEquipmentActor = GetOwner() && GetOwner()->FindComponentByClass<USuspenseCoreEquipmentMeshComponent>() != nullptr;

            if (!bIsPartOfEquipmentActor && GetOwner() && GetOwner()->HasAuthority())
            {
                if (ItemData.bIsEquippable && !ItemData.EquipmentActorClass.IsNull())
                {
                    SpawnedEquipmentActor = SpawnEquipmentActor(ItemData);
                    if (SpawnedEquipmentActor)
                    {
                        bDidSpawnActor = true;
                        AttachmentVersion++;
                    }
                }
            }
            else if (bIsPartOfEquipmentActor)
            {
                SpawnedEquipmentActor = GetOwner();
                bDidSpawnActor = false;
            }

            // Notify stance component
            if (LinkedStanceComponent.IsValid())
            {
                LinkedStanceComponent->OnEquipmentChanged(SpawnedEquipmentActor);
            }

            // Reattach to previous character if we were attached
            if (bWasAttached && CurrentCharacter.IsValid())
            {
                AttachToCharacter(CurrentCharacter.Get(), bWasActiveSocket, nullptr);
            }
        }
    }
}

bool USuspenseCoreEquipmentAttachmentComponent::AttachToCharacter(AActor* Character, bool bUseActiveSocket, USceneComponent* ComponentToAttach)
{
    if (!Character)
    {
        EQUIPMENT_LOG(Warning, TEXT("Cannot attach - null character"));
        return false;
    }

    if (!HasEquippedItem())
    {
        EQUIPMENT_LOG(Warning, TEXT("Cannot attach - no item equipped"));
        return false;
    }

    // Get item data for attachment info
    FSuspenseUnifiedItemData ItemData;
    if (!GetEquippedItemData(ItemData))
    {
        EQUIPMENT_LOG(Error, TEXT("Failed to get item data for attachment"));
        return false;
    }

    // Find target mesh on character
    USkeletalMeshComponent* TargetMesh = GetCharacterMesh(Character);
    if (!TargetMesh)
    {
        EQUIPMENT_LOG(Warning, TEXT("No skeletal mesh found on character, using root component"));
    }

    // Select socket and offset based on active state
    FName SocketName = bUseActiveSocket ? ItemData.AttachmentSocket : ItemData.UnequippedSocket;
    FTransform SocketOffset = bUseActiveSocket ? ItemData.AttachmentOffset : ItemData.UnequippedOffset;

    EQUIPMENT_LOG(Log, TEXT("Attaching %s as %s weapon"),
        *ItemData.DisplayName.ToString(),
        bUseActiveSocket ? TEXT("ACTIVE") : TEXT("INACTIVE"));

    // If socket not specified in DataTable, use default
    if (SocketName.IsNone())
    {
        SocketName = GetDefaultSocketForSlot(ItemData.EquipmentSlot, bUseActiveSocket);

        EQUIPMENT_LOG(Warning, TEXT("No %s socket in DataTable for %s, using fallback: %s"),
            bUseActiveSocket ? TEXT("AttachmentSocket") : TEXT("UnequippedSocket"),
            *ItemData.ItemID.ToString(),
            *SocketName.ToString());
    }
    else
    {
        EQUIPMENT_LOG(Log, TEXT("Using %s from DataTable: %s"),
            bUseActiveSocket ? TEXT("AttachmentSocket") : TEXT("UnequippedSocket"),
            *SocketName.ToString());
    }

    // Validate socket exists on mesh
    if (TargetMesh && !TargetMesh->DoesSocketExist(SocketName))
    {
        EQUIPMENT_LOG(Warning, TEXT("Socket %s does not exist on mesh, trying to find alternative"),
            *SocketName.ToString());

        // Try to find alternative socket
        SocketName = FindBestAttachmentSocket(TargetMesh, ItemData, bUseActiveSocket);

        if (SocketName.IsNone())
        {
            EQUIPMENT_LOG(Error, TEXT("No valid socket found for attachment"));
            return false;
        }
    }

    // Handle client prediction
    if (!GetOwner()->HasAuthority())
    {
        // Start prediction
        int32 PredictionKey = PredictAttachment(Character, bUseActiveSocket, SocketName, SocketOffset, CurrentWeaponType);

        // Send request to server
        ServerRequestAttachment(Character, bUseActiveSocket, SocketName, SocketOffset, CurrentWeaponType, PredictionKey);

        return true; // Optimistically return true
    }

    // Server-side attachment
    USceneComponent* AttachComponent = ComponentToAttach;

    // Determine what component to attach
    if (!AttachComponent)
    {
        if (SpawnedEquipmentActor)
        {
            // Try to find mesh component first
            AttachComponent = SpawnedEquipmentActor->FindComponentByClass<USuspenseCoreEquipmentMeshComponent>();
            if (!AttachComponent)
            {
                AttachComponent = SpawnedEquipmentActor->GetRootComponent();
            }

            EQUIPMENT_LOG(Log, TEXT("Using spawned actor's component for attachment"));
        }
    }

    if (!AttachComponent)
    {
        EQUIPMENT_LOG(Error, TEXT("No component to attach"));
        return false;
    }

    // Apply attachment
    USceneComponent* FinalTarget = TargetMesh ? TargetMesh : Character->GetRootComponent();
    ApplyAttachment(AttachComponent, FinalTarget, SocketName, SocketOffset);

    // Update state
    AttachTarget = FinalTarget;
    CurrentSocketName = SocketName;
    CurrentAttachmentOffset = SocketOffset;
    AttachedCharacter = Character;
    bIsAttached = true;
    bIsInActiveState = bUseActiveSocket;

    // Notify stance component
    NotifyStanceOfAttachment(true);

    // Broadcast event
    BroadcastAttachmentEvent(true, Character, SocketName);

    // Update replication
    UpdateReplicatedAttachmentState();

    // Replicate to clients with animation info
    if (SpawnedEquipmentActor)
    {
        MulticastAttachment(SpawnedEquipmentActor, FinalTarget, SocketName, SocketOffset, CurrentWeaponType, false);
    }

    EQUIPMENT_LOG(Log, TEXT("Successfully attached %s to %s at socket %s (State: %s)"),
        *ItemData.DisplayName.ToString(),
        *Character->GetName(),
        *SocketName.ToString(),
        bUseActiveSocket ? TEXT("Active") : TEXT("Inactive"));

    return true;
}

void USuspenseCoreEquipmentAttachmentComponent::Detach(bool bMaintainWorldTransform)
{
    // Handle client prediction
    if (!GetOwner()->HasAuthority())
    {
        // Start prediction
        int32 PredictionKey = PredictDetachment();

        // Send request to server
        ServerRequestDetachment(bMaintainWorldTransform, PredictionKey);

        return;
    }

    // Server-side detachment
    if (!IsAttached())
    {
        return;
    }

    // Stop any ongoing animations
    if (AnimationState.bIsPlaying)
    {
        AnimationState.bIsPlaying = false;
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().ClearTimer(AnimationCompletionTimer);
        }
    }

    // Detach component
    if (SpawnedEquipmentActor)
    {
        FDetachmentTransformRules Rules = bMaintainWorldTransform ?
            FDetachmentTransformRules::KeepWorldTransform :
            FDetachmentTransformRules::KeepRelativeTransform;

        SpawnedEquipmentActor->DetachFromActor(Rules);

        // Replicate to clients
        MulticastDetachment(SpawnedEquipmentActor, bMaintainWorldTransform);
    }

    // Notify stance component
    NotifyStanceOfAttachment(false);

    // Broadcast event before clearing state
    if (AttachedCharacter.IsValid())
    {
        BroadcastAttachmentEvent(false, AttachedCharacter.Get(), CurrentSocketName);
    }

    // Clear state
    AttachTarget = nullptr;
    CurrentSocketName = NAME_None;
    AttachedCharacter.Reset();
    bIsAttached = false;
    bIsInActiveState = false;

    // Update replication
    UpdateReplicatedAttachmentState();

    EQUIPMENT_LOG(Log, TEXT("Detached equipment"));
}

void USuspenseCoreEquipmentAttachmentComponent::UpdateAttachmentState(bool bMakeActive, bool bAnimated)
{
    if (!IsAttached() || !AttachedCharacter.IsValid())
    {
        EQUIPMENT_LOG(Warning, TEXT("UpdateAttachmentState: Not attached to character"));
        return;
    }

    // If already in desired state and not animating, do nothing
    if (bIsInActiveState == bMakeActive && !AnimationState.bIsPlaying)
    {
        EQUIPMENT_LOG(Verbose, TEXT("UpdateAttachmentState: Already in %s state"),
            bMakeActive ? TEXT("active") : TEXT("inactive"));
        return;
    }

    // Get item data
    FSuspenseUnifiedItemData ItemData;
    if (!GetEquippedItemData(ItemData))
    {
        EQUIPMENT_LOG(Error, TEXT("UpdateAttachmentState: Failed to get item data"));
        return;
    }

    // Get new socket and offset
    FName NewSocket = bMakeActive ? ItemData.AttachmentSocket : ItemData.UnequippedSocket;
    FTransform NewOffset = bMakeActive ? ItemData.AttachmentOffset : ItemData.UnequippedOffset;

    // If socket not specified, use default
    if (NewSocket.IsNone())
    {
        NewSocket = GetDefaultSocketForSlot(ItemData.EquipmentSlot, bMakeActive);
    }

    EQUIPMENT_LOG(Log, TEXT("UpdateAttachmentState: Moving %s to %s position (Socket: %s)"),
        *ItemData.DisplayName.ToString(),
        bMakeActive ? TEXT("ACTIVE") : TEXT("INACTIVE"),
        *NewSocket.ToString());

    // Play animation if requested
    if (bAnimated && CurrentWeaponType.IsValid())
    {
        PlayAttachmentAnimation(bMakeActive);
    }

    // Re-attach to new socket
    if (AttachTarget && SpawnedEquipmentActor)
    {
        USceneComponent* ComponentToMove = SpawnedEquipmentActor->FindComponentByClass<USuspenseCoreEquipmentMeshComponent>();
        if (!ComponentToMove)
        {
            ComponentToMove = SpawnedEquipmentActor->GetRootComponent();
        }

        // Apply new attachment
        ApplyAttachment(ComponentToMove, AttachTarget, NewSocket, NewOffset);

        // Update state
        CurrentSocketName = NewSocket;
        CurrentAttachmentOffset = NewOffset;
        bIsInActiveState = bMakeActive;

        // Update stance component
        if (LinkedStanceComponent.IsValid())
        {
            LinkedStanceComponent->SetWeaponDrawnState(bMakeActive);
        }

        // Update replication
        UpdateReplicatedAttachmentState();

        // Replicate change with animation info
        if (GetOwner() && GetOwner()->HasAuthority())
        {
            MulticastAttachment(SpawnedEquipmentActor, AttachTarget, NewSocket, NewOffset, CurrentWeaponType, bAnimated);
        }
    }
}

void USuspenseCoreEquipmentAttachmentComponent::PlayAttachmentAnimation(bool bToActive, float Duration)
{
    // Get animation interface
    TScriptInterface<ISuspenseWeaponAnimation> AnimInterface = GetAnimationInterface();
    if (!AnimInterface.GetInterface())
    {
        OnAttachmentAnimationComplete();
        return;
    }

    // Get appropriate montage
    UAnimMontage* Montage = nullptr;
    if (bToActive)
    {
        Montage = ISuspenseWeaponAnimation::Execute_GetDrawMontage(
            AnimInterface.GetObject(), CurrentWeaponType, false);
    }
    else
    {
        Montage = ISuspenseWeaponAnimation::Execute_GetHolsterMontage(
            AnimInterface.GetObject(), CurrentWeaponType);
    }

    if (!Montage)
    {
        OnAttachmentAnimationComplete();
        return;
    }

    // Set animation state
    AnimationState.CurrentMontage = Montage;
    AnimationState.PlayRate = 1.0f;
    AnimationState.bIsPlaying = true;
    AnimationState.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Calculate duration
    float AnimDuration = Duration > 0.0f ? Duration : Montage->GetPlayLength() / AnimationState.PlayRate;

    // Set timer for completion
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            AnimationCompletionTimer,
            this,
            &USuspenseCoreEquipmentAttachmentComponent::OnAttachmentAnimationComplete,
            AnimDuration,
            false
        );
    }

    // Play on character mesh if available
    if (AttachedCharacter.IsValid())
    {
        if (ACharacter* Character = Cast<ACharacter>(AttachedCharacter.Get()))
        {
            if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
            {
                AnimInstance->Montage_Play(Montage, AnimationState.PlayRate);
            }
        }
    }
}

TScriptInterface<ISuspenseWeaponAnimation> USuspenseCoreEquipmentAttachmentComponent::GetAnimationInterface() const
{
    // Check cache
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if ((CurrentTime - LastAnimationInterfaceCacheTime) < AnimationInterfaceCacheLifetime)
    {
        if (CachedAnimationInterface.GetInterface())
        {
            return CachedAnimationInterface;
        }
    }

    // Try stance component first
    if (LinkedStanceComponent.IsValid())
    {
        TScriptInterface<ISuspenseWeaponAnimation> StanceInterface = LinkedStanceComponent->GetAnimationInterface();
        if (StanceInterface.GetInterface())
        {
            CachedAnimationInterface = StanceInterface;
            LastAnimationInterfaceCacheTime = CurrentTime;
            return CachedAnimationInterface;
        }
    }

    // Get from subsystem
    if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UWeaponAnimationSubsystem* AnimSubsystem = GameInstance->GetSubsystem<UWeaponAnimationSubsystem>())
        {
            CachedAnimationInterface.SetObject(AnimSubsystem);
            CachedAnimationInterface.SetInterface(AnimSubsystem);
            LastAnimationInterfaceCacheTime = CurrentTime;

            return CachedAnimationInterface;
        }
    }

    return nullptr;
}

void USuspenseCoreEquipmentAttachmentComponent::OnAttachmentAnimationComplete()
{
    // Clear animation state
    AnimationState.bIsPlaying = false;
    AnimationState.CurrentMontage = nullptr;

    // Clear timer
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AnimationCompletionTimer);
    }

    EQUIPMENT_LOG(Verbose, TEXT("Attachment animation completed"));
}

void USuspenseCoreEquipmentAttachmentComponent::LinkStanceComponent(USuspenseCoreWeaponStanceComponent* StanceComponent)
{
    LinkedStanceComponent = StanceComponent;

    if (StanceComponent)
    {
        // Notify stance of current equipment
        if (SpawnedEquipmentActor)
        {
            StanceComponent->OnEquipmentChanged(SpawnedEquipmentActor);
        }

        // Set weapon stance based on current type
        if (CurrentWeaponType.IsValid())
        {
            StanceComponent->SetWeaponStance(CurrentWeaponType, true);
        }

        EQUIPMENT_LOG(Log, TEXT("Linked to stance component"));
    }
}

void USuspenseCoreEquipmentAttachmentComponent::NotifyStanceOfAttachment(bool bAttached)
{
    if (LinkedStanceComponent.IsValid())
    {
        if (bAttached)
        {
            LinkedStanceComponent->SetWeaponStance(CurrentWeaponType, false);
            LinkedStanceComponent->SetWeaponDrawnState(bIsInActiveState);
        }
        else
        {
            LinkedStanceComponent->ClearWeaponStance(false);
        }
    }
}

FGameplayTag USuspenseCoreEquipmentAttachmentComponent::GetWeaponTypeTag() const
{
    return CurrentWeaponType;
}

FName USuspenseCoreEquipmentAttachmentComponent::GetAttachmentSocketName(bool bActive) const
{
    if (!HasEquippedItem())
    {
        return NAME_None;
    }

    FSuspenseUnifiedItemData ItemData;
    if (GetEquippedItemData(ItemData))
    {
        return bActive ? ItemData.AttachmentSocket : ItemData.UnequippedSocket;
    }

    return NAME_None;
}

FTransform USuspenseCoreEquipmentAttachmentComponent::GetAttachmentOffset(bool bActive) const
{
    if (!HasEquippedItem())
    {
        return FTransform::Identity;
    }

    FSuspenseUnifiedItemData ItemData;
    if (GetEquippedItemData(ItemData))
    {
        return bActive ? ItemData.AttachmentOffset : ItemData.UnequippedOffset;
    }

    return FTransform::Identity;
}

//================================================
// Client Prediction Implementation
//================================================

int32 USuspenseCoreEquipmentAttachmentComponent::PredictAttachment(AActor* Character, bool bUseActiveSocket, const FName& SocketName,
                                                      const FTransform& Offset, const FGameplayTag& WeaponType)
{
    if (GetOwner()->HasAuthority())
    {
        return 0; // No prediction on server
    }

    // Create prediction
    FAttachmentPredictionData Prediction;
    Prediction.PredictionKey = NextAttachmentPredictionKey++;
    Prediction.bPredictedAttached = true;
    Prediction.bPredictedActive = bUseActiveSocket;
    Prediction.PredictedSocketName = SocketName;
    Prediction.PredictedOffset = Offset;
    Prediction.PredictedCharacter = Character;
    Prediction.WeaponTypeTag = WeaponType;
    Prediction.PredictionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Apply prediction locally
    ApplyPredictedAttachment(Prediction);

    // Store prediction
    AttachmentPredictions.Add(Prediction);

    EQUIPMENT_LOG(Verbose, TEXT("Started attachment prediction %d"), Prediction.PredictionKey);

    return Prediction.PredictionKey;
}

int32 USuspenseCoreEquipmentAttachmentComponent::PredictDetachment()
{
    if (GetOwner()->HasAuthority())
    {
        return 0; // No prediction on server
    }

    // Create prediction
    FAttachmentPredictionData Prediction;
    Prediction.PredictionKey = NextAttachmentPredictionKey++;
    Prediction.bPredictedAttached = false;
    Prediction.PredictionTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Apply prediction locally (detach)
    if (SpawnedEquipmentActor)
    {
        SpawnedEquipmentActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    }

    // Store prediction
    AttachmentPredictions.Add(Prediction);

    EQUIPMENT_LOG(Verbose, TEXT("Started detachment prediction %d"), Prediction.PredictionKey);

    return Prediction.PredictionKey;
}

void USuspenseCoreEquipmentAttachmentComponent::ConfirmAttachmentPrediction(int32 PredictionKey, bool bSuccess)
{
    // Find prediction
    int32 PredictionIndex = AttachmentPredictions.IndexOfByPredicate(
        [PredictionKey](const FAttachmentPredictionData& Data) { return Data.PredictionKey == PredictionKey; }
    );

    if (PredictionIndex == INDEX_NONE)
    {
        return;
    }

    FAttachmentPredictionData Prediction = AttachmentPredictions[PredictionIndex];
    AttachmentPredictions.RemoveAt(PredictionIndex);

    if (!bSuccess)
    {
        // Revert prediction
        RevertPredictedAttachment(Prediction);
        EQUIPMENT_LOG(Warning, TEXT("Attachment prediction %d failed - reverting"), PredictionKey);
    }
    else
    {
        EQUIPMENT_LOG(Verbose, TEXT("Attachment prediction %d confirmed"), PredictionKey);
    }
}

void USuspenseCoreEquipmentAttachmentComponent::ApplyPredictedAttachment(const FAttachmentPredictionData& Prediction)
{
    if (!Prediction.bPredictedAttached || !SpawnedEquipmentActor)
    {
        return;
    }

    if (AActor* Character = Prediction.PredictedCharacter.Get())
    {
        USkeletalMeshComponent* TargetMesh = GetCharacterMesh(Character);
        USceneComponent* FinalTarget = TargetMesh ? TargetMesh : Character->GetRootComponent();

        USceneComponent* AttachComponent = SpawnedEquipmentActor->GetRootComponent();
        ApplyAttachment(AttachComponent, FinalTarget, Prediction.PredictedSocketName, Prediction.PredictedOffset);

        // Play animation if weapon type is valid
        if (Prediction.WeaponTypeTag.IsValid())
        {
            PlayAttachmentAnimation(Prediction.bPredictedActive);
        }
    }
}

void USuspenseCoreEquipmentAttachmentComponent::RevertPredictedAttachment(const FAttachmentPredictionData& Prediction)
{
    // Revert to last confirmed state
    if (LastConfirmedState.bPredictedAttached && LastConfirmedState.PredictedCharacter.IsValid())
    {
        ApplyPredictedAttachment(LastConfirmedState);
    }
    else if (SpawnedEquipmentActor)
    {
        SpawnedEquipmentActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    }
}

void USuspenseCoreEquipmentAttachmentComponent::CleanupExpiredPredictions()
{
    if (!GetWorld())
    {
        return;
    }

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    const float TimeoutSeconds = 2.0f;

    AttachmentPredictions.RemoveAll([CurrentTime, TimeoutSeconds](const FAttachmentPredictionData& Data)
    {
        return (CurrentTime - Data.PredictionTime) > TimeoutSeconds;
    });
}

void USuspenseCoreEquipmentAttachmentComponent::UpdateAnimationState(float DeltaTime)
{
    if (!AnimationState.bIsPlaying)
    {
        return;
    }

    // Update animation progress
    // This could be used to sync animation states across network
    // or to control attachment interpolation
}

FGameplayTag USuspenseCoreEquipmentAttachmentComponent::GetWeaponArchetypeFromItem() const
{
    FSuspenseUnifiedItemData ItemData;
    if (GetEquippedItemData(ItemData) && ItemData.bIsWeapon)
    {
        return ItemData.WeaponArchetype;
    }

    return FGameplayTag();
}

//================================================
// Socket Management Implementation
//================================================

TArray<FSocketSearchResult> USuspenseCoreEquipmentAttachmentComponent::GetValidSocketsForItem(const FSuspenseUnifiedItemData& ItemData, USkeletalMeshComponent* TargetMesh) const
{
    TArray<FSocketSearchResult> Results;

    if (!TargetMesh)
    {
        return Results;
    }

    // Check primary sockets from DataTable FIRST
    if (ItemData.AttachmentSocket != NAME_None)
    {
        bool bExists = TargetMesh->DoesSocketExist(ItemData.AttachmentSocket);
        Results.Add(FSocketSearchResult(ItemData.AttachmentSocket, 100, bExists));
    }

    if (ItemData.UnequippedSocket != NAME_None)
    {
        bool bExists = TargetMesh->DoesSocketExist(ItemData.UnequippedSocket);
        Results.Add(FSocketSearchResult(ItemData.UnequippedSocket, 95, bExists));
    }

    // Check slot defaults
    FName DefaultActive = GetDefaultSocketForSlot(ItemData.EquipmentSlot, true);
    FName DefaultInactive = GetDefaultSocketForSlot(ItemData.EquipmentSlot, false);

    if (DefaultActive != NAME_None && DefaultActive != ItemData.AttachmentSocket)
    {
        bool bExists = TargetMesh->DoesSocketExist(DefaultActive);
        Results.Add(FSocketSearchResult(DefaultActive, 90, bExists));
    }

    if (DefaultInactive != NAME_None && DefaultInactive != ItemData.UnequippedSocket)
    {
        bool bExists = TargetMesh->DoesSocketExist(DefaultInactive);
        Results.Add(FSocketSearchResult(DefaultInactive, 85, bExists));
    }

    // Check priority lists
    const TArray<FName>* PriorityList = nullptr;

    if (ItemData.bIsWeapon)
    {
        PriorityList = &WeaponSocketPriority;
    }
    else if (ItemData.bIsArmor)
    {
        PriorityList = &ArmorSocketPriority;
    }
    else
    {
        PriorityList = &AccessorySocketPriority;
    }

    if (PriorityList)
    {
        int32 Score = 80;
        for (const FName& SocketName : *PriorityList)
        {
            // Skip if already added
            bool bAlreadyAdded = Results.ContainsByPredicate([&SocketName](const FSocketSearchResult& Result)
            {
                return Result.SocketName == SocketName;
            });

            if (!bAlreadyAdded)
            {
                bool bExists = TargetMesh->DoesSocketExist(SocketName);
                Results.Add(FSocketSearchResult(SocketName, Score--, bExists));
            }
        }
    }

    // Sort by score
    Results.Sort([](const FSocketSearchResult& A, const FSocketSearchResult& B)
    {
        return A.QualityScore > B.QualityScore;
    });

    return Results;
}

bool USuspenseCoreEquipmentAttachmentComponent::ValidateSocket(const FName& SocketName, USkeletalMeshComponent* TargetMesh) const
{
    if (SocketName.IsNone() || !TargetMesh)
    {
        return false;
    }

    return TargetMesh->DoesSocketExist(SocketName);
}

//================================================
// Protected Methods Implementation
//================================================

AActor* USuspenseCoreEquipmentAttachmentComponent::SpawnEquipmentActor(const FSuspenseUnifiedItemData& ItemData)
{
    if (!GetOwner() || !GetWorld())
    {
        EQUIPMENT_LOG(Error, TEXT("Cannot spawn equipment actor - invalid owner or world"));
        return nullptr;
    }

    // Load actor class
    UClass* ActorClass = ItemData.EquipmentActorClass.LoadSynchronous();
    if (!ActorClass)
    {
        EQUIPMENT_LOG(Error, TEXT("Failed to load equipment actor class"));
        return nullptr;
    }

    // Spawn actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = GetOwner();
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* NewActor = GetWorld()->SpawnActor<AActor>(ActorClass, FTransform::Identity, SpawnParams);
    if (!NewActor)
    {
        EQUIPMENT_LOG(Error, TEXT("Failed to spawn equipment actor"));
        return nullptr;
    }

    // Initialize mesh component if exists
    if (USuspenseCoreEquipmentMeshComponent* MeshComp = NewActor->FindComponentByClass<USuspenseCoreEquipmentMeshComponent>())
    {
        MeshComp->InitializeFromItemInstance(EquippedItemInstance);
    }

    EQUIPMENT_LOG(Log, TEXT("Spawned equipment actor: %s"), *NewActor->GetName());
    return NewActor;
}

void USuspenseCoreEquipmentAttachmentComponent::DestroyEquipmentActor()
{
    if (SpawnedEquipmentActor && IsValid(SpawnedEquipmentActor) && SpawnedEquipmentActor != GetOwner())
    {
        EQUIPMENT_LOG(Log, TEXT("Destroying equipment actor: %s"), *SpawnedEquipmentActor->GetName());
        SpawnedEquipmentActor->Destroy();
        SpawnedEquipmentActor = nullptr;
    }
}

FName USuspenseCoreEquipmentAttachmentComponent::FindBestAttachmentSocket(USkeletalMeshComponent* TargetMesh, const FSuspenseUnifiedItemData& ItemData, bool bForActive) const
{
    if (!TargetMesh)
    {
        return NAME_None;
    }

    // Check socket cache first
    {
        FScopeLock Lock(&SocketCacheCriticalSection);

        const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        if ((CurrentTime - LastSocketCacheTime) < SocketCacheLifetime)
        {
            FString CacheKey = FString::Printf(TEXT("%s_%s_%s"),
                *ItemData.ItemID.ToString(),
                *TargetMesh->GetName(),
                bForActive ? TEXT("Active") : TEXT("Inactive"));

            if (FSocketSearchResult* CachedResult = SocketCache.Find(CacheKey))
            {
                if (CachedResult->bSocketExists)
                {
                    return CachedResult->SocketName;
                }
            }
        }
    }

    // Get all valid sockets
    TArray<FSocketSearchResult> ValidSockets = GetValidSocketsForItem(ItemData, TargetMesh);

    // Find first existing socket
    for (const FSocketSearchResult& Result : ValidSockets)
    {
        if (Result.bSocketExists)
        {
            // Cache the result
            {
                FScopeLock Lock(&SocketCacheCriticalSection);
                FString CacheKey = FString::Printf(TEXT("%s_%s_%s"),
                    *ItemData.ItemID.ToString(),
                    *TargetMesh->GetName(),
                    bForActive ? TEXT("Active") : TEXT("Inactive"));

                SocketCache.Add(CacheKey, Result);
                LastSocketCacheTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
            }

            return Result.SocketName;
        }
    }

    // No suitable socket found
    return NAME_None;
}

FName USuspenseCoreEquipmentAttachmentComponent::GetDefaultSocketForSlot(const FGameplayTag& SlotType, bool bForActive) const
{
    if (bForActive)
    {
        // Active sockets (weapon in use)
        if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.PrimaryWeapon"))))
            return FName("GripPoint");
        else if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.SecondaryWeapon"))))
            return FName("GripPoint");
        else if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Sidearm"))))
            return FName("GripPoint");
        else if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.MeleeWeapon"))))
            return FName("GripPoint");
        else if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Helmet"))))
            return FName("head");
        else if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Chest"))))
            return FName("spine_03");
        else if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Backpack"))))
            return FName("spine_02");
    }
    else
    {
        // Inactive sockets (weapon stored)
        if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.PrimaryWeapon"))))
            return FName("WeaponBackSocket");
        else if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.SecondaryWeapon"))))
            return FName("WeaponBackSocket_Secondary");
        else if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Sidearm"))))
            return FName("HolsterSocket");
        else if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.MeleeWeapon"))))
            return FName("MeleeSocket");
        else if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Grenade"))))
            return FName("GrenadeSocket");
        // Armor always uses same socket
        else if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Helmet"))))
            return FName("head");
        else if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Chest"))))
            return FName("spine_03");
        else if (SlotType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Equipment.Slot.Backpack"))))
            return FName("spine_02");
    }

    return NAME_None;
}

void USuspenseCoreEquipmentAttachmentComponent::ApplyAttachment(USceneComponent* ComponentToAttach, USceneComponent* TargetComponent,
                                                   const FName& SocketName, const FTransform& AttachmentOffset)
{
    if (!ComponentToAttach || !TargetComponent)
    {
        EQUIPMENT_LOG(Error, TEXT("ApplyAttachment: Invalid components"));
        return;
    }

    // First, ensure the component is not simulating physics
    if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(ComponentToAttach))
    {
        // Disable physics simulation to prevent "floating" behavior
        PrimComp->SetSimulatePhysics(false);
        PrimComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        EQUIPMENT_LOG(Log, TEXT("ApplyAttachment: Disabled physics simulation on component"));
    }

    // Log attachment details for debugging
    EQUIPMENT_LOG(Log, TEXT("ApplyAttachment: Attaching %s to %s (Socket: %s)"),
        *ComponentToAttach->GetName(),
        *TargetComponent->GetName(),
        *SocketName.ToString());

    // Detach first to ensure clean state
    ComponentToAttach->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

    // Use explicit attachment rules for all transform components
    FAttachmentTransformRules AttachRules(
        EAttachmentRule::SnapToTarget,      // Location - snap to target
        EAttachmentRule::SnapToTarget,      // Rotation - snap to target
        EAttachmentRule::SnapToTarget,      // Scale - snap to target
        true                                // Weld simulated bodies
    );

    // Attach to socket or component
    bool bAttachSuccess = false;
    if (SocketName != NAME_None)
    {
        bAttachSuccess = ComponentToAttach->AttachToComponent(TargetComponent, AttachRules, SocketName);
        if (!bAttachSuccess)
        {
            EQUIPMENT_LOG(Warning, TEXT("ApplyAttachment: Failed to attach to socket %s, trying without socket"),
                *SocketName.ToString());
            bAttachSuccess = ComponentToAttach->AttachToComponent(TargetComponent, AttachRules);
        }
    }
    else
    {
        bAttachSuccess = ComponentToAttach->AttachToComponent(TargetComponent, AttachRules);
    }

    if (!bAttachSuccess)
    {
        EQUIPMENT_LOG(Error, TEXT("ApplyAttachment: Failed to attach component"));
        return;
    }

    // Apply transform offset AFTER attachment
    if (!AttachmentOffset.Equals(FTransform::Identity))
    {
        // Set relative transform directly
        ComponentToAttach->SetRelativeTransform(AttachmentOffset);

        EQUIPMENT_LOG(Log, TEXT("ApplyAttachment: Applied relative transform offset"));
    }

    // Ensure the component updates its transform
    ComponentToAttach->UpdateComponentToWorld();

    // Double-check physics is disabled on the entire actor
    if (AActor* AttachedActor = ComponentToAttach->GetOwner())
    {
        AttachedActor->DisableComponentsSimulatePhysics();
        AttachedActor->SetActorEnableCollision(false);

        EQUIPMENT_LOG(Log, TEXT("ApplyAttachment: Disabled physics on entire actor %s"),
            *AttachedActor->GetName());
    }

    EQUIPMENT_LOG(Log, TEXT("ApplyAttachment: Successfully attached with final world location: %s"),
        *ComponentToAttach->GetComponentLocation().ToString());
}

void USuspenseCoreEquipmentAttachmentComponent::BroadcastAttachmentEvent(bool bAttached, AActor* Character, const FName& SocketName)
{
    if (!HasEquippedItem() || !Character)
    {
        return;
    }

    // Create event data
    FString EventData = FString::Printf(
        TEXT("Character:%s,Socket:%s,ItemID:%s,InstanceID:%s,Active:%s,WeaponType:%s"),
        *Character->GetName(),
        *SocketName.ToString(),
        *EquippedItemInstance.ItemID.ToString(),
        *EquippedItemInstance.InstanceID.ToString(),
        bIsInActiveState ? TEXT("true") : TEXT("false"),
        *CurrentWeaponType.ToString()
    );

    // Broadcast appropriate event
    FGameplayTag EventTag = bAttached ?
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Attached")) :
        FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.Detached"));

    BroadcastEquipmentEvent(EventTag, EventData);
}

USkeletalMeshComponent* USuspenseCoreEquipmentAttachmentComponent::GetCharacterMesh(AActor* Character) const
{
    if (!Character)
    {
        return nullptr;
    }

    // Try character class first
    if (ACharacter* CharacterPawn = Cast<ACharacter>(Character))
    {
        return CharacterPawn->GetMesh();
    }

    // Look for first skeletal mesh component
    TArray<USkeletalMeshComponent*> MeshComponents;
    Character->GetComponents<USkeletalMeshComponent>(MeshComponents);

    // Return first valid mesh that's not an equipment mesh
    for (USkeletalMeshComponent* Mesh : MeshComponents)
    {
        if (Mesh && !Mesh->IsA<USuspenseCoreEquipmentMeshComponent>())
        {
            return Mesh;
        }
    }

    return nullptr;
}

void USuspenseCoreEquipmentAttachmentComponent::UpdateReplicatedAttachmentState()
{
    if (!GetOwner() || !GetOwner()->HasAuthority())
    {
        return;
    }

    // Update version to force replication
    AttachmentVersion++;

    // Force net update on the owner actor
    if (AActor* Owner = GetOwner())
    {
        Owner->ForceNetUpdate();
    }
}

//================================================
// Replication Callbacks
//================================================

void USuspenseCoreEquipmentAttachmentComponent::OnRep_AttachmentState()
{
    EQUIPMENT_LOG(Verbose, TEXT("OnRep_AttachmentState: Attached=%s, Active=%s, Socket=%s, Version=%d"),
        bIsAttached ? TEXT("true") : TEXT("false"),
        bIsInActiveState ? TEXT("true") : TEXT("false"),
        *CurrentSocketName.ToString(),
        AttachmentVersion);

    // Update last confirmed state for predictions
    LastConfirmedState.bPredictedAttached = bIsAttached;
    LastConfirmedState.bPredictedActive = bIsInActiveState;
    LastConfirmedState.PredictedSocketName = CurrentSocketName;
    LastConfirmedState.PredictedOffset = CurrentAttachmentOffset;
    LastConfirmedState.PredictedCharacter = AttachedCharacter;
    LastConfirmedState.WeaponTypeTag = CurrentWeaponType;

    // Update stance component
    if (LinkedStanceComponent.IsValid())
    {
        if (bIsAttached)
        {
            LinkedStanceComponent->SetWeaponStance(CurrentWeaponType, true);
            LinkedStanceComponent->SetWeaponDrawnState(bIsInActiveState);
        }
        else
        {
            LinkedStanceComponent->ClearWeaponStance(true);
        }
    }
}

void USuspenseCoreEquipmentAttachmentComponent::OnRep_SpawnedEquipmentActor()
{
    EQUIPMENT_LOG(Verbose, TEXT("OnRep_SpawnedEquipmentActor: %s"),
        SpawnedEquipmentActor ? *SpawnedEquipmentActor->GetName() : TEXT("None"));

    // Initialize mesh component on clients
    if (SpawnedEquipmentActor && HasEquippedItem())
    {
        if (USuspenseCoreEquipmentMeshComponent* MeshComp = SpawnedEquipmentActor->FindComponentByClass<USuspenseCoreEquipmentMeshComponent>())
        {
            MeshComp->InitializeFromItemInstance(EquippedItemInstance);
        }
    }

    // Notify stance component
    if (LinkedStanceComponent.IsValid())
    {
        LinkedStanceComponent->OnEquipmentChanged(SpawnedEquipmentActor);
    }
}

void USuspenseCoreEquipmentAttachmentComponent::OnRep_AnimationState()
{
    EQUIPMENT_LOG(Verbose, TEXT("OnRep_AnimationState: Playing=%s, PlayRate=%.2f"),
        AnimationState.bIsPlaying ? TEXT("true") : TEXT("false"),
        AnimationState.PlayRate);

    // Play animation on client if needed
    if (AnimationState.bIsPlaying && AnimationState.CurrentMontage)
    {
        if (AttachedCharacter.IsValid())
        {
            if (ACharacter* Character = Cast<ACharacter>(AttachedCharacter.Get()))
            {
                if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
                {
                    AnimInstance->Montage_Play(AnimationState.CurrentMontage, AnimationState.PlayRate);
                }
            }
        }
    }
}

//================================================
// Server RPC Implementation
//================================================

void USuspenseCoreEquipmentAttachmentComponent::ServerRequestAttachment_Implementation(AActor* Character, bool bUseActiveSocket, FName RequestedSocket,
                                                                         FTransform RequestedOffset, FGameplayTag WeaponType, int32 PredictionKey)
{
    bool bSuccess = false;
    FName ActualSocket = RequestedSocket;
    FTransform ActualOffset = RequestedOffset;

    if (Character && HasEquippedItem())
    {
        // Update weapon type
        CurrentWeaponType = WeaponType;

        // Validate and perform attachment
        bSuccess = AttachToCharacter(Character, bUseActiveSocket, nullptr);

        if (bSuccess)
        {
            ActualSocket = CurrentSocketName;
            ActualOffset = CurrentAttachmentOffset;
        }
    }

    // Send confirmation to client
    ClientConfirmAttachment(PredictionKey, bSuccess, ActualSocket, ActualOffset);
}

bool USuspenseCoreEquipmentAttachmentComponent::ServerRequestAttachment_Validate(AActor* Character, bool bUseActiveSocket, FName RequestedSocket,
                                                                    FTransform RequestedOffset, FGameplayTag WeaponType, int32 PredictionKey)
{
    return Character != nullptr && PredictionKey > 0;
}

void USuspenseCoreEquipmentAttachmentComponent::ServerRequestDetachment_Implementation(bool bMaintainTransform, int32 PredictionKey)
{
    Detach(bMaintainTransform);

    // Send confirmation to client
    ClientConfirmAttachment(PredictionKey, true, NAME_None, FTransform::Identity);
}

bool USuspenseCoreEquipmentAttachmentComponent::ServerRequestDetachment_Validate(bool bMaintainTransform, int32 PredictionKey)
{
    return PredictionKey > 0;
}

void USuspenseCoreEquipmentAttachmentComponent::ClientConfirmAttachment_Implementation(int32 PredictionKey, bool bSuccess, FName ActualSocket, FTransform ActualOffset)
{
    ConfirmAttachmentPrediction(PredictionKey, bSuccess);

    if (bSuccess)
    {
        // Update confirmed state
        LastConfirmedState.PredictedSocketName = ActualSocket;
        LastConfirmedState.PredictedOffset = ActualOffset;
    }
}

//================================================
// Multicast RPC Implementation
//================================================

void USuspenseCoreEquipmentAttachmentComponent::MulticastAttachment_Implementation(AActor* Actor, USceneComponent* Parent, FName Socket,
                                                                      FTransform Offset, FGameplayTag WeaponType, bool bAnimated)
{
    if (!Actor || !Parent)
    {
        return;
    }

    // Skip on server as it's already done
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        return;
    }

    // Update weapon type
    CurrentWeaponType = WeaponType;

    // Find component to attach
    USceneComponent* ComponentToAttach = Actor->FindComponentByClass<USuspenseCoreEquipmentMeshComponent>();
    if (!ComponentToAttach)
    {
        ComponentToAttach = Actor->GetRootComponent();
    }

    // Apply attachment
    ApplyAttachment(ComponentToAttach, Parent, Socket, Offset);

    // Play animation if requested
    if (bAnimated && WeaponType.IsValid())
    {
        PlayAttachmentAnimation(bIsInActiveState);
    }

    // Update stance component
    if (LinkedStanceComponent.IsValid())
    {
        LinkedStanceComponent->SetWeaponStance(WeaponType, !bAnimated);
        LinkedStanceComponent->SetWeaponDrawnState(bIsInActiveState);
    }
}

void USuspenseCoreEquipmentAttachmentComponent::MulticastDetachment_Implementation(AActor* Actor, bool bMaintainTransform)
{
    if (!Actor)
    {
        return;
    }

    // Skip on server as it's already done
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        return;
    }

    // Detach actor
    FDetachmentTransformRules Rules = bMaintainTransform ?
        FDetachmentTransformRules::KeepWorldTransform :
        FDetachmentTransformRules::KeepRelativeTransform;

    Actor->DetachFromActor(Rules);

    // Update stance component
    if (LinkedStanceComponent.IsValid())
    {
        LinkedStanceComponent->ClearWeaponStance(true);
    }
}

//================================================
// Equipment Component Base Overrides
//================================================

void USuspenseCoreEquipmentAttachmentComponent::OnEquipmentInitialized()
{
    Super::OnEquipmentInitialized();

    // Update weapon type when equipment is initialized
    CurrentWeaponType = GetWeaponArchetypeFromItem();
}

void USuspenseCoreEquipmentAttachmentComponent::OnEquippedItemChanged(const FSuspenseInventoryItemInstance& OldItem, const FSuspenseInventoryItemInstance& NewItem)
{
    Super::OnEquippedItemChanged(OldItem, NewItem);

    // Update weapon type when item changes
    CurrentWeaponType = GetWeaponArchetypeFromItem();

    // Notify stance component
    if (LinkedStanceComponent.IsValid())
    {
        if (NewItem.IsValid())
        {
            LinkedStanceComponent->SetWeaponStance(CurrentWeaponType, false);
        }
        else
        {
            LinkedStanceComponent->ClearWeaponStance(false);
        }
    }
}
