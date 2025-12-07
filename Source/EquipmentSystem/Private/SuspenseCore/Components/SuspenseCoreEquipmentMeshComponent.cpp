// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Components/SuspenseCoreEquipmentMeshComponent.h"
#include "SuspenseCore/Components/SuspenseCoreEquipmentComponentBase.h"
#include "ItemSystem/SuspenseCoreItemManager.h"
#include "Delegates/SuspenseCoreEventManager.h"
#include "Camera/CameraComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "SuspenseCore/Components/AudioComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameplayTagsManager.h"
#include "TimerManager.h"

// Static socket name definitions
const FName USuspenseCoreEquipmentMeshComponent::DefaultMuzzleSocket = FName("Muzzle");
const FName USuspenseCoreEquipmentMeshComponent::DefaultScopeSocket = FName("Scope");
const FName USuspenseCoreEquipmentMeshComponent::DefaultMagazineSocket = FName("Magazine");

USuspenseCoreEquipmentMeshComponent::USuspenseCoreEquipmentMeshComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // For prediction cleanup
    bWantsInitializeComponent = true;
    SetCollisionProfileName(TEXT("NoCollision"));

    // Initialize state
    AdditionalOffset = FTransform::Identity;
    bVisualsInitialized = false;
    LastCacheValidationTime = 0.0f;
    NextPredictionKey = 1;

    // Visual components
    ScopeCamera = nullptr;
    MuzzleFlashComponent = nullptr;
    AudioComponent = nullptr;
}

void USuspenseCoreEquipmentMeshComponent::BeginPlay()
{
    Super::BeginPlay();

    // Apply any initial offset
    ApplyOffsetTransform(AdditionalOffset);

    // Pre-allocate effect pool
    for (int32 i = 0; i < MaxPooledEffects; ++i)
    {
        UNiagaraComponent* EffectComp = NewObject<UNiagaraComponent>(this, NAME_None, RF_Transient);
        EffectComp->SetAutoActivate(false);
        EffectComp->RegisterComponent();
        PooledEffectComponents.Add(EffectComp);
    }
}

void USuspenseCoreEquipmentMeshComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CleanupVisuals();

    // Clear pooled effects
    for (UNiagaraComponent* EffectComp : PooledEffectComponents)
    {
        if (IsValid(EffectComp))
        {
            EffectComp->DestroyComponent();
        }
    }
    PooledEffectComponents.Empty();

    // Clear cached references
    CachedItemManager.Reset();
    CachedDelegateManager.Reset();

    Super::EndPlay(EndPlayReason);
}

void USuspenseCoreEquipmentMeshComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Clean up expired predictions
    CleanupExpiredPredictions();

    // Check for visual state changes
    if (bVisualsInitialized && HasVisualStateChanged(PreviousVisualState))
    {
        NotifyVisualStateChanged();
        PreviousVisualState = CurrentVisualState;
    }
}

bool USuspenseCoreEquipmentMeshComponent::InitializeFromItemInstance(const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    if (!ItemInstance.IsValid())
    {
        UE_LOG(LogMedComEquipment, Warning, TEXT("InitializeFromItemInstance: Invalid item instance"));
        return false;
    }

    // Store current instance
    CurrentItemInstance = ItemInstance;

    // Get item data from DataTable
    USuspenseCoreItemManager* ItemManager = GetItemManager();
    if (!ItemManager)
    {
        UE_LOG(LogMedComEquipment, Error, TEXT("InitializeFromItemInstance: ItemManager not available"));
        return false;
    }

    if (!ItemManager->GetUnifiedItemData(ItemInstance.ItemID, CachedItemData))
    {
        UE_LOG(LogMedComEquipment, Error, TEXT("InitializeFromItemInstance: Failed to get item data for %s"),
            *ItemInstance.ItemID.ToString());
        return false;
    }

    // Load mesh from item data
    bool bMeshLoaded = LoadMeshFromItemData(CachedItemData);

    // Initialize visual components
    InitializeVisualComponents(CachedItemData);

    // Create dynamic materials
    CreateDynamicMaterials();

    // Apply initial visual state
    UpdateVisualState(ItemInstance);

    // Mark as initialized
    bVisualsInitialized = true;

    // Request initial state sync
    RequestStateSync();

    UE_LOG(LogMedComEquipment, Log, TEXT("Initialized mesh for item: %s (Mesh loaded: %s)"),
        *CachedItemData.DisplayName.ToString(),
        bMeshLoaded ? TEXT("Yes") : TEXT("No"));

    return bMeshLoaded;
}

void USuspenseCoreEquipmentMeshComponent::UpdateVisualState(const FSuspenseCoreInventoryItemInstance& ItemInstance)
{
    if (!bVisualsInitialized)
    {
        return;
    }

    FScopeLock Lock(&VisualStateCriticalSection);

    CurrentItemInstance = ItemInstance;

    // Update condition visual
    float Durability = ItemInstance.GetRuntimeProperty(TEXT("Durability"), 100.0f);
    float MaxDurability = ItemInstance.GetRuntimeProperty(TEXT("MaxDurability"), 100.0f);
    float ConditionPercent = MaxDurability > 0 ? Durability / MaxDurability : 1.0f;
    SetConditionVisual(ConditionPercent);

    // Update rarity visual
    SetRarityVisual(CachedItemData.Rarity);

    // Update weapon-specific visuals
    if (CachedItemData.bIsWeapon)
    {
        // Update ammo counter display if exists
        float CurrentAmmo = ItemInstance.GetRuntimeProperty(TEXT("CurrentAmmo"), 0.0f);
        float MaxAmmo = ItemInstance.GetRuntimeProperty(TEXT("MaxAmmo"), 30.0f);
        SetMaterialParameter(TEXT("AmmoPercent"), MaxAmmo > 0 ? CurrentAmmo / MaxAmmo : 0.0f);

        // Update heat/overheat visual
        float HeatLevel = ItemInstance.GetRuntimeProperty(TEXT("HeatLevel"), 0.0f);
        SetMaterialParameter(TEXT("HeatLevel"), HeatLevel);

        // ПРАВИЛЬНЫЙ СПОСОБ: Получаем режим огня как числовой индекс
        // 0 = Single, 1 = Burst, 2 = Auto
        float FireModeIndex = ItemInstance.GetRuntimeProperty(TEXT("CurrentFireModeIndex"), 0.0f);

        // Преобразуем в значение для материала (1.0, 2.0, 3.0)
        SetMaterialParameter(TEXT("FireModeIndicator"), FireModeIndex + 1.0f);
    }

    // Increment state version
    CurrentVisualState.StateVersion++;
}

void USuspenseCoreEquipmentMeshComponent::CleanupVisuals()
{
    // Stop all active effects
    for (const FSuspenseCoreVisualEffectPrediction& Prediction : ActivePredictions)
    {
        StopPredictedEffect(Prediction);
    }
    ActivePredictions.Empty();

    // Return active effects to pool
    for (UNiagaraComponent* EffectComp : ActiveEffectComponents)
    {
        if (IsValid(EffectComp))
        {
            EffectComp->Deactivate();
            if (PooledEffectComponents.Num() < MaxPooledEffects)
            {
                PooledEffectComponents.Add(EffectComp);
            }
            else
            {
                EffectComp->DestroyComponent();
            }
        }
    }
    ActiveEffectComponents.Empty();

    // Destroy visual components
    if (ScopeCamera)
    {
        ScopeCamera->DestroyComponent();
        ScopeCamera = nullptr;
    }

    if (MuzzleFlashComponent)
    {
        MuzzleFlashComponent->DestroyComponent();
        MuzzleFlashComponent = nullptr;
    }

    if (AudioComponent)
    {
        AudioComponent->DestroyComponent();
        AudioComponent = nullptr;
    }

    // Clear dynamic materials
    DynamicMaterials.Empty();

    bVisualsInitialized = false;
}

//================================================
// Visual State Synchronization
//================================================

void USuspenseCoreEquipmentMeshComponent::ApplyVisualState(const FSuspenseCoreEquipmentVisualState& NewState, bool bForceUpdate)
{
    FScopeLock Lock(&VisualStateCriticalSection);

    // Check if update needed
    if (!bForceUpdate && CurrentVisualState == NewState)
    {
        return;
    }

    // Store previous state
    PreviousVisualState = CurrentVisualState;
    CurrentVisualState = NewState;

    // Apply condition
    if (FMath::Abs(CurrentVisualState.ConditionPercent - PreviousVisualState.ConditionPercent) > 0.01f)
    {
        SetMaterialParameter(TEXT("Condition"), CurrentVisualState.ConditionPercent);
        SetMaterialParameter(TEXT("WearAmount"), 1.0f - CurrentVisualState.ConditionPercent);
    }

    // Apply rarity
    if (!CurrentVisualState.RarityColor.Equals(PreviousVisualState.RarityColor) ||
        FMath::Abs(CurrentVisualState.RarityGlowIntensity - PreviousVisualState.RarityGlowIntensity) > 0.01f)
    {
        SetMaterialColorParameter(TEXT("RarityGlow"), CurrentVisualState.RarityColor);
        SetMaterialParameter(TEXT("RarityIntensity"), CurrentVisualState.RarityGlowIntensity);
    }

    // Apply material parameters
    for (const auto& Pair : CurrentVisualState.MaterialScalarParams)
    {
        SetMaterialParameter(Pair.Key, Pair.Value);
    }

    for (const auto& Pair : CurrentVisualState.MaterialVectorParams)
    {
        SetMaterialColorParameter(Pair.Key, Pair.Value);
    }

    // Apply active effects
    FGameplayTagContainer AddedEffects = CurrentVisualState.ActiveEffects;
    FGameplayTagContainer RemovedEffects = PreviousVisualState.ActiveEffects;

    // Find effects to add
    AddedEffects.RemoveTags(PreviousVisualState.ActiveEffects);

    // Find effects to remove
    RemovedEffects.RemoveTags(CurrentVisualState.ActiveEffects);

    // Apply effect changes
    for (const FGameplayTag& EffectTag : AddedEffects)
    {
        PlayEquipmentEffect(EffectTag);
    }

    for (const FGameplayTag& EffectTag : RemovedEffects)
    {
        // Stop effects with this tag
        ActivePredictions.RemoveAll([EffectTag](const FSuspenseCoreVisualEffectPrediction& Pred)
        {
            return Pred.EffectType == EffectTag;
        });
    }

    UE_LOG(LogMedComEquipment, VeryVerbose, TEXT("Applied visual state v%d"), CurrentVisualState.StateVersion);
}

bool USuspenseCoreEquipmentMeshComponent::HasVisualStateChanged(const FSuspenseCoreEquipmentVisualState& OtherState) const
{
    FScopeLock Lock(&VisualStateCriticalSection);
    return CurrentVisualState != OtherState;
}

void USuspenseCoreEquipmentMeshComponent::NotifyVisualStateChanged()
{
    // Early exit if no owner
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // Get all equipment components directly using templated method
    TArray<USuspenseCoreEquipmentComponentBase*> EquipmentComponents;
    Owner->GetComponents<USuspenseCoreEquipmentComponentBase>(EquipmentComponents);

    // If no equipment components found, nothing to notify
    if (EquipmentComponents.Num() == 0)
    {
        return;
    }

    // Prepare event data once to avoid string construction in loop
    const FString EventData = FString::Printf(TEXT("StateVersion:%d"), CurrentVisualState.StateVersion);
    const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.VisualStateChanged"));

    // Notify all equipment components
    for (USuspenseCoreEquipmentComponentBase* EquipComp : EquipmentComponents)
    {
        // Additional safety check (GetComponents should return only valid components)
        if (IsValid(EquipComp))
        {
            EquipComp->BroadcastEquipmentEvent(EventTag, EventData);
        }
    }

    // Log for debugging if needed
    UE_LOG(LogTemp, VeryVerbose, TEXT("USuspenseCoreEquipmentMeshComponent: Notified %d equipment components about visual state change (version: %d)"),
        EquipmentComponents.Num(), CurrentVisualState.StateVersion);
}

void USuspenseCoreEquipmentMeshComponent::RequestStateSync()
{
    // Broadcast request for state sync
    if (USuspenseCoreEventManager* Manager = GetDelegateManager())
    {
        FString EventData = FString::Printf(TEXT("Component:%s,ItemID:%s"),
            *GetName(),
            *CurrentItemInstance.ItemID.ToString());

        Manager->NotifyEquipmentEvent(
            GetOwner(),
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.RequestVisualSync")),
            EventData
        );
    }
}

//================================================
// Visual Components Implementation
//================================================

void USuspenseCoreEquipmentMeshComponent::InitializeVisualComponents(const FSuspenseCoreUnifiedItemData& ItemData)
{
    // Create audio component
    if (!AudioComponent)
    {
        AudioComponent = NewObject<UAudioComponent>(this, TEXT("EquipmentAudio"));
        AudioComponent->SetupAttachment(this);
        AudioComponent->SetAutoActivate(false);
        AudioComponent->RegisterComponent();
    }

    // Setup weapon-specific components
    if (ItemData.bIsWeapon)
    {
        SetupWeaponVisuals(ItemData);
    }
}

bool USuspenseCoreEquipmentMeshComponent::LoadMeshFromItemData(const FSuspenseCoreUnifiedItemData& ItemData)
{
    // Equipment typically uses skeletal meshes
    // The WorldMesh in item data is a static mesh reference
    // In a real implementation, you'd have a separate skeletal mesh reference
    // or a conversion system

    if (!ItemData.WorldMesh.IsNull())
    {
        // Log that we have a mesh reference but it's static mesh
        UE_LOG(LogMedComEquipment, Warning,
            TEXT("Item %s has WorldMesh but it's a static mesh reference. Equipment typically uses skeletal meshes."),
            *ItemData.DisplayName.ToString());
    }

    // For now, we'll assume the skeletal mesh is already set in the actor blueprint
    // or loaded through other means

    return GetSkeletalMeshAsset() != nullptr;
}

void USuspenseCoreEquipmentMeshComponent::CreateDynamicMaterials()
{
    FScopeLock Lock(&VisualStateCriticalSection);

    DynamicMaterials.Empty();

    int32 NumMaterials = GetNumMaterials();
    for (int32 i = 0; i < NumMaterials; i++)
    {
        UMaterialInterface* BaseMaterial = GetMaterial(i);
        if (BaseMaterial)
        {
            UMaterialInstanceDynamic* DynMaterial = CreateDynamicMaterialInstance(i, BaseMaterial);
            if (DynMaterial)
            {
                DynamicMaterials.Add(DynMaterial);
            }
        }
    }
}

void USuspenseCoreEquipmentMeshComponent::UpdateDynamicMaterials()
{
    FScopeLock Lock(&VisualStateCriticalSection);

    // Apply all current state parameters to materials
    for (UMaterialInstanceDynamic* DynMat : DynamicMaterials)
    {
        if (!IsValid(DynMat))
        {
            continue;
        }

        // Apply scalar parameters
        for (const auto& Pair : CurrentVisualState.MaterialScalarParams)
        {
            DynMat->SetScalarParameterValue(Pair.Key, Pair.Value);
        }

        // Apply vector parameters
        for (const auto& Pair : CurrentVisualState.MaterialVectorParams)
        {
            DynMat->SetVectorParameterValue(Pair.Key, Pair.Value);
        }
    }
}

void USuspenseCoreEquipmentMeshComponent::SetupWeaponVisuals(const FSuspenseCoreUnifiedItemData& WeaponData)
{
    // Create muzzle flash component
    if (!MuzzleFlashComponent)
    {
        MuzzleFlashComponent = NewObject<UNiagaraComponent>(this, TEXT("MuzzleFlash"));
        MuzzleFlashComponent->SetupAttachment(this, WeaponData.MuzzleSocket);
        MuzzleFlashComponent->SetAutoActivate(false);
        MuzzleFlashComponent->RegisterComponent();
    }

    // Setup scope if weapon has one
    bool bHasScope = WeaponData.ItemTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Weapon.Feature.Scope")));
    if (bHasScope)
    {
        SetupScopeCamera(40.0f, false);
    }
}

//================================================
// Socket Management
//================================================

FVector USuspenseCoreEquipmentMeshComponent::GetSocketLocationSafe(const FName& SocketName) const
{
    if (DoesSocketExist(SocketName))
    {
        return GetSocketLocation(SocketName);
    }

    UE_LOG(LogMedComEquipment, VeryVerbose, TEXT("Socket %s not found, using component location"),
           *SocketName.ToString());
    return GetComponentLocation();
}

FRotator USuspenseCoreEquipmentMeshComponent::GetSocketRotationSafe(const FName& SocketName) const
{
    if (DoesSocketExist(SocketName))
    {
        return GetSocketRotation(SocketName);
    }

    return GetComponentRotation();
}

FTransform USuspenseCoreEquipmentMeshComponent::GetSocketTransformSafe(const FName& SocketName) const
{
    if (DoesSocketExist(SocketName))
    {
        return GetSocketTransform(SocketName);
    }

    return GetComponentTransform();
}

void USuspenseCoreEquipmentMeshComponent::ApplyOffsetTransform(const FTransform& Offset)
{
    AdditionalOffset = Offset;
    SetRelativeTransform(AdditionalOffset);
}

//================================================
// Weapon Features
//================================================

FVector USuspenseCoreEquipmentMeshComponent::GetMuzzleLocation() const
{
    FName MuzzleSocket = CachedItemData.MuzzleSocket.IsNone() ? DefaultMuzzleSocket : CachedItemData.MuzzleSocket;
    return GetSocketLocationSafe(MuzzleSocket);
}

FVector USuspenseCoreEquipmentMeshComponent::GetMuzzleDirection() const
{
    FName MuzzleSocket = CachedItemData.MuzzleSocket.IsNone() ? DefaultMuzzleSocket : CachedItemData.MuzzleSocket;
    FRotator MuzzleRotation = GetSocketRotationSafe(MuzzleSocket);
    return MuzzleRotation.Vector();
}

int32 USuspenseCoreEquipmentMeshComponent::PlayMuzzleFlash()
{
    // Create prediction
    FSuspenseCoreVisualEffectPrediction Prediction;
    Prediction.PredictionKey = NextPredictionKey++;
    Prediction.EffectType = FGameplayTag::RequestGameplayTag(TEXT("Effect.Weapon.MuzzleFlash"));
    Prediction.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Prediction.Duration = 0.1f; // Muzzle flash duration

    // Play effect immediately for responsiveness
    if (MuzzleFlashComponent)
    {
        MuzzleFlashComponent->Activate(true);
        Prediction.EffectComponent = MuzzleFlashComponent;
    }

    // Store prediction
    ActivePredictions.Add(Prediction);

    // Also play fire sound if available
    if (AudioComponent && !CachedItemData.FireSound.IsNull())
    {
        if (USoundBase* Sound = CachedItemData.FireSound.LoadSynchronous())
        {
            AudioComponent->SetSound(Sound);
            AudioComponent->Play();
        }
    }

    // Update visual state
    {
        FScopeLock Lock(&VisualStateCriticalSection);
        CurrentVisualState.ActiveEffects.AddTag(Prediction.EffectType);
        CurrentVisualState.StateVersion++;
    }

    return Prediction.PredictionKey;
}

void USuspenseCoreEquipmentMeshComponent::SetupScopeCamera(float FOV, bool bShouldAutoActivate)
{
    if (!ScopeCamera)
    {
        ScopeCamera = NewObject<UCameraComponent>(this, TEXT("ScopeCamera"));
        ScopeCamera->SetupAttachment(this, CachedItemData.SightSocket.IsNone() ? DefaultScopeSocket : CachedItemData.SightSocket);
        ScopeCamera->SetFieldOfView(FOV);
        ScopeCamera->bAutoActivate = bShouldAutoActivate;
        ScopeCamera->RegisterComponent();
    }
    else
    {
        ScopeCamera->SetFieldOfView(FOV);
        if (bShouldAutoActivate)
        {
            ScopeCamera->Activate();
        }
    }
}

void USuspenseCoreEquipmentMeshComponent::SetScopeCameraActive(bool bActivate)
{
    if (ScopeCamera)
    {
        ScopeCamera->SetActive(bActivate);

        // Update visual state
        FScopeLock Lock(&VisualStateCriticalSection);
        if (bActivate)
        {
            CurrentVisualState.ActiveEffects.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Weapon.ScopeActive")));
        }
        else
        {
            CurrentVisualState.ActiveEffects.RemoveTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Weapon.ScopeActive")));
        }
        CurrentVisualState.StateVersion++;
    }
}

//================================================
// Visual State and Effects
//================================================

void USuspenseCoreEquipmentMeshComponent::SetConditionVisual(float ConditionPercent)
{
    FScopeLock Lock(&VisualStateCriticalSection);

    CurrentVisualState.ConditionPercent = FMath::Clamp(ConditionPercent, 0.0f, 1.0f);

    // Update material parameters for wear and tear
    SetMaterialParameter(TEXT("Condition"), CurrentVisualState.ConditionPercent);
    SetMaterialParameter(TEXT("WearAmount"), 1.0f - CurrentVisualState.ConditionPercent);

    // Add damage decals or scratches based on condition
    if (CurrentVisualState.ConditionPercent < 0.3f)
    {
        SetMaterialParameter(TEXT("DamageIntensity"), 1.0f - CurrentVisualState.ConditionPercent);
    }
}

void USuspenseCoreEquipmentMeshComponent::SetRarityVisual(const FGameplayTag& RarityTag)
{
    FScopeLock Lock(&VisualStateCriticalSection);

    // Get rarity color from item data helper
    FLinearColor RarityColor = CachedItemData.GetRarityColor();

    CurrentVisualState.RarityColor = RarityColor;
    CurrentVisualState.RarityGlowIntensity = 1.0f;

    // Apply to materials
    SetMaterialColorParameter(TEXT("RarityGlow"), RarityColor);
    SetMaterialParameter(TEXT("RarityIntensity"), 1.0f);
}

int32 USuspenseCoreEquipmentMeshComponent::PlayEquipmentEffect(const FGameplayTag& EffectType)
{
    // Create prediction
    FSuspenseCoreVisualEffectPrediction Prediction;
    Prediction.PredictionKey = NextPredictionKey++;
    Prediction.EffectType = EffectType;
    Prediction.StartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Determine effect duration based on type
    if (EffectType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Use"))))
    {
        Prediction.Duration = 1.0f;
    }
    else if (EffectType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Reload"))))
    {
        Prediction.Duration = 2.0f;
    }
    else
    {
        Prediction.Duration = 0.5f;
    }

    // Play effect immediately
    ApplyPredictedEffect(Prediction);

    // Store prediction
    ActivePredictions.Add(Prediction);

    // Update visual state
    {
        FScopeLock Lock(&VisualStateCriticalSection);
        CurrentVisualState.ActiveEffects.AddTag(EffectType);
        CurrentVisualState.StateVersion++;
    }

    // Broadcast effect event
    if (USuspenseCoreEventManager* Manager = GetDelegateManager())
    {
        FString EventData = FString::Printf(TEXT("EffectType:%s,PredictionKey:%d"),
            *EffectType.ToString(), Prediction.PredictionKey);
        Manager->NotifyEquipmentEvent(GetOwner(),
            FGameplayTag::RequestGameplayTag(TEXT("Equipment.Event.VisualEffect")),
            EventData);
    }

    return Prediction.PredictionKey;
}

void USuspenseCoreEquipmentMeshComponent::ConfirmEffectPrediction(int32 PredictionKey, bool bSuccess)
{
    int32 PredictionIndex = ActivePredictions.IndexOfByPredicate(
        [PredictionKey](const FSuspenseCoreVisualEffectPrediction& Pred) { return Pred.PredictionKey == PredictionKey; }
    );

    if (PredictionIndex == INDEX_NONE)
    {
        return;
    }

    FSuspenseCoreVisualEffectPrediction Prediction = ActivePredictions[PredictionIndex];

    if (!bSuccess)
    {
        // Stop the predicted effect
        StopPredictedEffect(Prediction);

        // Update visual state
        FScopeLock Lock(&VisualStateCriticalSection);
        CurrentVisualState.ActiveEffects.RemoveTag(Prediction.EffectType);
        CurrentVisualState.StateVersion++;
    }

    ActivePredictions.RemoveAt(PredictionIndex);
}

void USuspenseCoreEquipmentMeshComponent::SetMaterialParameter(const FName& ParameterName, float Value)
{
    FScopeLock Lock(&VisualStateCriticalSection);

    CurrentVisualState.MaterialScalarParams.Add(ParameterName, Value);

    for (UMaterialInstanceDynamic* DynMat : DynamicMaterials)
    {
        if (DynMat)
        {
            DynMat->SetScalarParameterValue(ParameterName, Value);
        }
    }
}

void USuspenseCoreEquipmentMeshComponent::SetMaterialColorParameter(const FName& ParameterName, const FLinearColor& Color)
{
    FScopeLock Lock(&VisualStateCriticalSection);

    CurrentVisualState.MaterialVectorParams.Add(ParameterName, Color);

    for (UMaterialInstanceDynamic* DynMat : DynamicMaterials)
    {
        if (DynMat)
        {
            DynMat->SetVectorParameterValue(ParameterName, Color);
        }
    }
}

//================================================
// Attachment Points
//================================================

FName USuspenseCoreEquipmentMeshComponent::GetAttachmentSocket(const FGameplayTag& ModificationType) const
{
    if (!CachedItemData.bIsWeapon)
    {
        return NAME_None;
    }

    return GetWeaponSocketName(CachedItemData, ModificationType);
}

bool USuspenseCoreEquipmentMeshComponent::HasAttachmentSocket(const FGameplayTag& ModificationType) const
{
    FName SocketName = GetAttachmentSocket(ModificationType);
    return SocketName != NAME_None && DoesSocketExist(SocketName);
}

FName USuspenseCoreEquipmentMeshComponent::GetWeaponSocketName(const FSuspenseCoreUnifiedItemData& WeaponData, const FGameplayTag& ModificationType) const
{
    // Map modification types to socket names from weapon data
    if (ModificationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Weapon.Attachment.Sight"))))
    {
        return WeaponData.SightSocket.IsNone() ? DefaultScopeSocket : WeaponData.SightSocket;
    }
    else if (ModificationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Weapon.Attachment.Magazine"))))
    {
        return WeaponData.MagazineSocket.IsNone() ? DefaultMagazineSocket : WeaponData.MagazineSocket;
    }
    else if (ModificationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Weapon.Attachment.Grip"))))
    {
        return WeaponData.GripSocket;
    }
    else if (ModificationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Weapon.Attachment.Stock"))))
    {
        return WeaponData.StockSocket;
    }
    else if (ModificationType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Weapon.Attachment.Muzzle"))))
    {
        return WeaponData.MuzzleSocket.IsNone() ? DefaultMuzzleSocket : WeaponData.MuzzleSocket;
    }

    return NAME_None;
}

//================================================
// Effect Management
//================================================

UNiagaraComponent* USuspenseCoreEquipmentMeshComponent::PlayVisualEffectAtLocation(const FGameplayTag& EffectType, const FVector& Location, const FRotator& Rotation)
{
    // Try to get effect from pool
    UNiagaraComponent* EffectComp = nullptr;
    if (PooledEffectComponents.Num() > 0)
    {
        EffectComp = PooledEffectComponents.Pop();
    }
    else
    {
        // Create new component if pool is empty
        EffectComp = NewObject<UNiagaraComponent>(this, NAME_None, RF_Transient);
        EffectComp->SetAutoActivate(false);
        EffectComp->RegisterComponent();
    }

    if (!EffectComp)
    {
        return nullptr;
    }

    // Setup effect based on type
    // In a real implementation, you'd load appropriate Niagara system based on effect type
    // For now, we'll just position the component
    EffectComp->SetWorldLocationAndRotation(Location, Rotation);
    EffectComp->Activate(true);

    // Track active effect
    ActiveEffectComponents.Add(EffectComp);

    return EffectComp;
}

void USuspenseCoreEquipmentMeshComponent::ApplyPredictedEffect(const FSuspenseCoreVisualEffectPrediction& Prediction)
{
    // Play appropriate effect based on type
    if (Prediction.EffectType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Use"))))
    {
        // Play use sound
        if (AudioComponent && !CachedItemData.UseSound.IsNull())
        {
            if (USoundBase* Sound = CachedItemData.UseSound.LoadSynchronous())
            {
                AudioComponent->SetSound(Sound);
                AudioComponent->Play();
            }
        }
    }
    else if (Prediction.EffectType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Reload"))))
    {
        // Play reload sound
        if (AudioComponent && CachedItemData.bIsWeapon && !CachedItemData.ReloadSound.IsNull())
        {
            if (USoundBase* Sound = CachedItemData.ReloadSound.LoadSynchronous())
            {
                AudioComponent->SetSound(Sound);
                AudioComponent->Play();
            }
        }
    }
    else if (Prediction.EffectType.MatchesTag(FGameplayTag::RequestGameplayTag(TEXT("Effect.Weapon"))))
    {
        // Play weapon effect at appropriate location
        FVector EffectLocation = GetMuzzleLocation();
        FRotator EffectRotation = GetSocketRotationSafe(CachedItemData.MuzzleSocket);

        if (UNiagaraComponent* EffectComp = PlayVisualEffectAtLocation(Prediction.EffectType, EffectLocation, EffectRotation))
        {
            const_cast<FSuspenseCoreVisualEffectPrediction&>(Prediction).EffectComponent = EffectComp;
        }
    }
}

void USuspenseCoreEquipmentMeshComponent::StopPredictedEffect(const FSuspenseCoreVisualEffectPrediction& Prediction)
{
    // Правильное приведение типа с проверкой
    if (USceneComponent* SceneComp = Prediction.EffectComponent.Get())
    {
        // Безопасное приведение к UNiagaraComponent
        if (UNiagaraComponent* EffectComp = Cast<UNiagaraComponent>(SceneComp))
        {
            EffectComp->Deactivate();

            // Return to pool
            ActiveEffectComponents.Remove(EffectComp);
            if (PooledEffectComponents.Num() < MaxPooledEffects)
            {
                PooledEffectComponents.Add(EffectComp);
            }
            else
            {
                EffectComp->DestroyComponent();
            }
        }
        else
        {
            // Обработка случая, когда компонент не является Niagara
            // Это может быть ParticleSystemComponent или другой тип эффекта
            EQUIPMENT_LOG(Warning, TEXT("StopPredictedEffect: Component is not a NiagaraComponent"));

            // Просто деактивируем компонент
            SceneComp->Deactivate();
        }
    }

    // Stop any associated sounds
    if (AudioComponent && AudioComponent->IsPlaying())
    {
        AudioComponent->Stop();
    }
}

void USuspenseCoreEquipmentMeshComponent::CleanupExpiredPredictions()
{
    if (!GetWorld())
    {
        return;
    }

    const float CurrentTime = GetWorld()->GetTimeSeconds();

    // Remove expired predictions
    ActivePredictions.RemoveAll([this, CurrentTime](const FSuspenseCoreVisualEffectPrediction& Prediction)
    {
        bool bExpired = (CurrentTime - Prediction.StartTime) > Prediction.Duration;
        if (bExpired)
        {
            StopPredictedEffect(Prediction);

            // Update visual state
            FScopeLock Lock(&VisualStateCriticalSection);
            CurrentVisualState.ActiveEffects.RemoveTag(Prediction.EffectType);
        }
        return bExpired;
    });
}

//================================================
// Cache Management
//================================================

USuspenseCoreItemManager* USuspenseCoreEquipmentMeshComponent::GetItemManager() const
{
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    if (!CachedItemManager.IsValid() || (CurrentTime - LastCacheValidationTime) > CacheValidationInterval)
    {
        LastCacheValidationTime = CurrentTime;

        if (UWorld* World = GetWorld())
        {
            if (UGameInstance* GameInstance = World->GetGameInstance())
            {
                CachedItemManager = GameInstance->GetSubsystem<USuspenseCoreItemManager>();
            }
        }
    }

    return CachedItemManager.Get();
}

USuspenseCoreEventManager* USuspenseCoreEquipmentMeshComponent::GetDelegateManager() const
{
    const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    if (!CachedDelegateManager.IsValid() || (CurrentTime - LastCacheValidationTime) > CacheValidationInterval)
    {
        LastCacheValidationTime = CurrentTime;

        if (UWorld* World = GetWorld())
        {
            if (UGameInstance* GameInstance = World->GetGameInstance())
            {
                CachedDelegateManager = GameInstance->GetSubsystem<USuspenseCoreEventManager>();
            }
        }
    }

    return CachedDelegateManager.Get();
}
