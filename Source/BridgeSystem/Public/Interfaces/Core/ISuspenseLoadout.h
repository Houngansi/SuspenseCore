// Copyright Suspense Team. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Types/Loadout/SuspenseLoadoutSettings.h"
#include "ISuspenseLoadout.generated.h"

// Forward declarations
class USuspenseLoadoutManager;

/**
 * Result of loadout application operation
 * Provides detailed information about what was applied and any issues
 */
USTRUCT(BlueprintType)
struct BRIDGESYSTEM_API FLoadoutApplicationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bSuccess=false;
    UPROPERTY(BlueprintReadOnly) FGameplayTagContainer AppliedComponents;
    UPROPERTY(BlueprintReadOnly) FGameplayTagContainer FailedComponents;
    UPROPERTY(BlueprintReadOnly) TArray<FString> ErrorMessages;
    UPROPERTY(BlueprintReadOnly) TArray<FString> Warnings;
    UPROPERTY(BlueprintReadOnly) FName AppliedLoadoutID=NAME_None;
    UPROPERTY(BlueprintReadOnly) FDateTime ApplicationTime;

    static FLoadoutApplicationResult CreateSuccess(const FName& LoadoutID,const FGameplayTagContainer& Components)
    {
        FLoadoutApplicationResult Result;
        Result.bSuccess=true;
        Result.AppliedLoadoutID=LoadoutID;
        Result.AppliedComponents=Components;
        Result.ApplicationTime=FDateTime::Now();
        return Result;
    }

    static FLoadoutApplicationResult CreateFailure(const FName& LoadoutID,const FString& ErrorMessage)
    {
        FLoadoutApplicationResult Result;
        Result.bSuccess=false;
        Result.AppliedLoadoutID=LoadoutID;
        Result.ErrorMessages.Add(ErrorMessage);
        Result.ApplicationTime=FDateTime::Now();
        return Result;
    }

    void MergeComponentResult(const FGameplayTag& ComponentTag,bool bComponentSuccess,const FString& Message=TEXT(""))
    {
        if(bComponentSuccess){AppliedComponents.AddTag(ComponentTag);}
        else
        {
            FailedComponents.AddTag(ComponentTag);
            if(!Message.IsEmpty())
            {
                ErrorMessages.Add(FString::Printf(TEXT("[%s] %s"),*ComponentTag.ToString(),*Message));
            }
            bSuccess=false;
        }
    }

    FString GetSummary() const
    {
        if(bSuccess)
        {
            return FString::Printf(TEXT("Successfully applied loadout '%s' to %d components"),
                *AppliedLoadoutID.ToString(),AppliedComponents.Num());
        }
        return FString::Printf(TEXT("Failed to apply loadout '%s': %d errors, %d warnings"),
            *AppliedLoadoutID.ToString(),ErrorMessages.Num(),Warnings.Num());
    }
};

/**
 * Interface for components that can be configured by loadout system
 */
UINTERFACE(MinimalAPI,Blueprintable)
class USuspenseLoadout:public UInterface
{
    GENERATED_BODY()
};

class BRIDGESYSTEM_API ISuspenseLoadout
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Loadout")
    FLoadoutApplicationResult ApplyLoadoutConfiguration(const FName& LoadoutID,USuspenseLoadoutManager* LoadoutManager,bool bForceApply=false);

    UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Loadout")
    FName GetCurrentLoadoutID() const;

    UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Loadout")
    bool CanAcceptLoadout(const FName& LoadoutID,const USuspenseLoadoutManager* LoadoutManager,FString& OutReason) const;

    UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Loadout")
    FGameplayTag GetLoadoutComponentType() const;

    UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Loadout")
    void ResetForLoadout(bool bPreserveRuntimeData=false);

    UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Loadout")
    FString SerializeLoadoutState() const;

    UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Loadout")
    bool RestoreLoadoutState(const FString& SerializedState);

    UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Loadout")
    void OnLoadoutPreChange(const FName& CurrentLoadoutID,const FName& NewLoadoutID);

    UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Loadout")
    void OnLoadoutPostChange(const FName& PreviousLoadoutID,const FName& NewLoadoutID);

    UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Loadout")
    FGameplayTagContainer GetRequiredLoadoutFeatures() const;

    UFUNCTION(BlueprintNativeEvent,BlueprintCallable,Category="Loadout")
    bool ValidateAgainstLoadout(TArray<FString>& OutViolations) const;

    // Static helpers (definitions в .cpp вашего проекта — без изменений интерфейса)
    static FLoadoutApplicationResult ApplyLoadoutToComponents(const TArray<TScriptInterface<ISuspenseLoadout>>& Components,const FName& LoadoutID,USuspenseLoadoutManager* LoadoutManager,bool bStopOnFirstError=false);
    static void FindLoadoutComponents(AActor* Actor,TArray<TScriptInterface<ISuspenseLoadout>>& OutComponents,const FGameplayTag& ComponentTypeFilter=FGameplayTag());
    static bool IsLoadoutChangeSafe(const TArray<TScriptInterface<ISuspenseLoadout>>& Components,const FName& NewLoadoutID,const USuspenseLoadoutManager* LoadoutManager,TArray<FString>& OutReasons);
};
