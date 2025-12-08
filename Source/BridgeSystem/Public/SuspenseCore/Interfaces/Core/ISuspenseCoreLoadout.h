// ISuspenseCoreLoadout.h
// SuspenseCore - EventBus Architecture
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "SuspenseCore/Types/Loadout/SuspenseCoreLoadoutSettings.h"
#include "ISuspenseCoreLoadout.generated.h"

class USuspenseCoreLoadoutManager;

/**
 * Result of loadout application operation
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FSuspenseCoreLoadoutResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bSuccess = false;
    UPROPERTY(BlueprintReadOnly) FGameplayTagContainer AppliedComponents;
    UPROPERTY(BlueprintReadOnly) FGameplayTagContainer FailedComponents;
    UPROPERTY(BlueprintReadOnly) TArray<FString> ErrorMessages;
    UPROPERTY(BlueprintReadOnly) TArray<FString> Warnings;
    UPROPERTY(BlueprintReadOnly) FName AppliedLoadoutID = NAME_None;
    UPROPERTY(BlueprintReadOnly) FDateTime ApplicationTime;

    static FSuspenseCoreLoadoutResult CreateSuccess(const FName& LoadoutID, const FGameplayTagContainer& Components)
    {
        FSuspenseCoreLoadoutResult Result;
        Result.bSuccess = true;
        Result.AppliedLoadoutID = LoadoutID;
        Result.AppliedComponents = Components;
        Result.ApplicationTime = FDateTime::Now();
        return Result;
    }

    static FSuspenseCoreLoadoutResult CreateFailure(const FName& LoadoutID, const FString& ErrorMessage)
    {
        FSuspenseCoreLoadoutResult Result;
        Result.bSuccess = false;
        Result.AppliedLoadoutID = LoadoutID;
        Result.ErrorMessages.Add(ErrorMessage);
        Result.ApplicationTime = FDateTime::Now();
        return Result;
    }

    void MergeComponentResult(const FGameplayTag& ComponentTag, bool bComponentSuccess, const FString& Message = TEXT(""))
    {
        if (bComponentSuccess)
        {
            AppliedComponents.AddTag(ComponentTag);
        }
        else
        {
            FailedComponents.AddTag(ComponentTag);
            if (!Message.IsEmpty())
            {
                ErrorMessages.Add(FString::Printf(TEXT("[%s] %s"), *ComponentTag.ToString(), *Message));
            }
            bSuccess = false;
        }
    }

    FString GetSummary() const
    {
        if (bSuccess)
        {
            return FString::Printf(TEXT("Successfully applied loadout '%s' to %d components"),
                *AppliedLoadoutID.ToString(), AppliedComponents.Num());
        }
        return FString::Printf(TEXT("Failed to apply loadout '%s': %d errors, %d warnings"),
            *AppliedLoadoutID.ToString(), ErrorMessages.Num(), Warnings.Num());
    }
};

UINTERFACE(MinimalAPI, Blueprintable)
class USuspenseCoreLoadout : public UInterface
{
    GENERATED_BODY()
};

/**
 * ISuspenseCoreLoadout
 *
 * Interface for components that can be configured by loadout system.
 */
class BRIDGESYSTEM_API ISuspenseCoreLoadout
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Loadout")
    FSuspenseCoreLoadoutResult ApplyLoadoutConfiguration(const FName& LoadoutID, USuspenseCoreLoadoutManager* LoadoutManager, bool bForceApply = false);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Loadout")
    FName GetCurrentLoadoutID() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Loadout")
    bool CanAcceptLoadout(const FName& LoadoutID, const USuspenseCoreLoadoutManager* LoadoutManager, FString& OutReason) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Loadout")
    FGameplayTag GetLoadoutComponentType() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Loadout")
    void ResetForLoadout(bool bPreserveRuntimeData = false);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Loadout")
    FString SerializeLoadoutState() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Loadout")
    bool RestoreLoadoutState(const FString& SerializedState);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Loadout")
    void OnLoadoutPreChange(const FName& CurrentLoadoutID, const FName& NewLoadoutID);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Loadout")
    void OnLoadoutPostChange(const FName& PreviousLoadoutID, const FName& NewLoadoutID);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Loadout")
    FGameplayTagContainer GetRequiredLoadoutFeatures() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SuspenseCore|Loadout")
    bool ValidateAgainstLoadout(TArray<FString>& OutViolations) const;

    static FSuspenseCoreLoadoutResult ApplyLoadoutToComponents(
        const TArray<TScriptInterface<ISuspenseCoreLoadout>>& Components,
        const FName& LoadoutID,
        USuspenseCoreLoadoutManager* LoadoutManager,
        bool bStopOnFirstError = false);

    static void FindLoadoutComponents(
        AActor* Actor,
        TArray<TScriptInterface<ISuspenseCoreLoadout>>& OutComponents,
        const FGameplayTag& ComponentTypeFilter = FGameplayTag());

    static bool IsLoadoutChangeSafe(
        const TArray<TScriptInterface<ISuspenseCoreLoadout>>& Components,
        const FName& NewLoadoutID,
        const USuspenseCoreLoadoutManager* LoadoutManager,
        TArray<FString>& OutReasons);
};

