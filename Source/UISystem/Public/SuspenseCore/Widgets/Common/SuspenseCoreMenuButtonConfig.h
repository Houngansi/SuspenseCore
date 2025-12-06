// SuspenseCoreMenuButtonConfig.h
// SuspenseCore - Menu Button Configuration Types
// Copyright Suspense Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SuspenseCoreButtonWidget.h"
#include "SuspenseCoreMenuButtonConfig.generated.h"

/**
 * Configuration for a single menu button
 * Used to procedurally create buttons in menus
 */
USTRUCT(BlueprintType)
struct UISYSTEM_API FSuspenseCoreMenuButtonConfig
{
	GENERATED_BODY()

	/** Unique action tag for this button (e.g., SuspenseCore.UIAction.Play) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FGameplayTag ActionTag;

	/** Button text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FText ButtonText;

	/** Button style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	ESuspenseCoreButtonStyle Style = ESuspenseCoreButtonStyle::Secondary;

	/** Is button enabled by default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	bool bEnabled = true;

	/** Optional icon texture */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	TObjectPtr<UTexture2D> Icon;

	/** Optional tooltip */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	FText Tooltip;

	/** Sort order (lower = higher in list) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	int32 SortOrder = 0;

	FSuspenseCoreMenuButtonConfig()
		: Style(ESuspenseCoreButtonStyle::Secondary)
		, bEnabled(true)
		, SortOrder(0)
	{
	}

	FSuspenseCoreMenuButtonConfig(FGameplayTag InTag, const FText& InText, ESuspenseCoreButtonStyle InStyle = ESuspenseCoreButtonStyle::Secondary)
		: ActionTag(InTag)
		, ButtonText(InText)
		, Style(InStyle)
		, bEnabled(true)
		, SortOrder(0)
	{
	}
};

/**
 * Delegate for button click from dynamic menu
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMenuButtonAction, FGameplayTag, ActionTag, USuspenseCoreButtonWidget*, Button);
