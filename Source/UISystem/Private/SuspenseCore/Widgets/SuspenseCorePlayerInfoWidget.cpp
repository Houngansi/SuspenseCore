// SuspenseCorePlayerInfoWidget.cpp
// SuspenseCore - Clean Architecture UI Layer
// Copyright (c) 2025. All Rights Reserved.

#include "SuspenseCore/Widgets/SuspenseCorePlayerInfoWidget.h"
#include "SuspenseCore/Events/SuspenseCoreEventBus.h"
#include "SuspenseCore/Events/SuspenseCoreEventManager.h"
#include "SuspenseCore/Services/SuspenseCoreServiceLocator.h"
#include "SuspenseCore/Repository/SuspenseCoreFilePlayerRepository.h"
#include "SuspenseCore/SuspenseCoreInterfaces.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Button.h"
#include "TimerManager.h"

USuspenseCorePlayerInfoWidget::USuspenseCorePlayerInfoWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void USuspenseCorePlayerInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetupButtonBindings();

	if (bSubscribeToEvents)
	{
		SetupEventSubscriptions();
	}

	if (AutoRefreshInterval > 0.0f)
	{
		StartAutoRefresh();
	}

	// Clear display initially
	ClearDisplay();
}

void USuspenseCorePlayerInfoWidget::NativeDestruct()
{
	TeardownEventSubscriptions();
	StopAutoRefresh();

	Super::NativeDestruct();
}

void USuspenseCorePlayerInfoWidget::SetupButtonBindings()
{
	if (RefreshButton)
	{
		RefreshButton->OnClicked.AddDynamic(this, &USuspenseCorePlayerInfoWidget::OnRefreshButtonClicked);
	}
}

void USuspenseCorePlayerInfoWidget::SetupEventSubscriptions()
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (!Manager)
	{
		return;
	}

	CachedEventBus = Manager->GetEventBus();
	if (!CachedEventBus.IsValid())
	{
		return;
	}

	// Subscribe to progression events
	ProgressionEventHandle = CachedEventBus->SubscribeNative(
		FGameplayTag::RequestGameplayTag(FName("Event.Progression")),
		const_cast<USuspenseCorePlayerInfoWidget*>(this),
		FSuspenseCoreNativeEventCallback::CreateUObject(this, &USuspenseCorePlayerInfoWidget::OnProgressionEvent),
		ESuspenseCoreEventPriority::Normal
	);
}

void USuspenseCorePlayerInfoWidget::TeardownEventSubscriptions()
{
	if (CachedEventBus.IsValid() && ProgressionEventHandle.IsValid())
	{
		CachedEventBus->Unsubscribe(ProgressionEventHandle);
	}
}

void USuspenseCorePlayerInfoWidget::LoadPlayerData(const FString& PlayerId)
{
	ISuspenseCorePlayerRepository* Repository = GetRepository();
	if (!Repository)
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCorePlayerInfoWidget: Failed to get repository"));
		return;
	}

	FSuspenseCorePlayerData LoadedData;
	if (Repository->LoadPlayer(PlayerId, LoadedData))
	{
		CurrentPlayerId = PlayerId;
		CachedPlayerData = LoadedData;
		UpdateUIFromData();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SuspenseCorePlayerInfoWidget: Failed to load player %s"), *PlayerId);
		ClearDisplay();
	}
}

void USuspenseCorePlayerInfoWidget::DisplayPlayerData(const FSuspenseCorePlayerData& PlayerData)
{
	CurrentPlayerId = PlayerData.PlayerId;
	CachedPlayerData = PlayerData;
	UpdateUIFromData();
}

void USuspenseCorePlayerInfoWidget::RefreshData()
{
	if (!CurrentPlayerId.IsEmpty())
	{
		LoadPlayerData(CurrentPlayerId);
	}
}

void USuspenseCorePlayerInfoWidget::ClearDisplay()
{
	CurrentPlayerId.Empty();
	CachedPlayerData = FSuspenseCorePlayerData();

	if (DisplayNameText) DisplayNameText->SetText(FText::FromString(TEXT("---")));
	if (PlayerIdText) PlayerIdText->SetText(FText::FromString(TEXT("---")));
	if (LevelText) LevelText->SetText(FText::FromString(TEXT("Lv. 0")));
	if (XPProgressBar) XPProgressBar->SetPercent(0.0f);
	if (XPText) XPText->SetText(FText::FromString(TEXT("0 / 0")));
	if (SoftCurrencyText) SoftCurrencyText->SetText(FText::FromString(TEXT("0")));
	if (HardCurrencyText) HardCurrencyText->SetText(FText::FromString(TEXT("0")));
	if (KillsText) KillsText->SetText(FText::FromString(TEXT("0")));
	if (DeathsText) DeathsText->SetText(FText::FromString(TEXT("0")));
	if (KDRatioText) KDRatioText->SetText(FText::FromString(TEXT("0.00")));
	if (WinsText) WinsText->SetText(FText::FromString(TEXT("0")));
	if (MatchesText) MatchesText->SetText(FText::FromString(TEXT("0")));
	if (PlaytimeText) PlaytimeText->SetText(FText::FromString(TEXT("0h 0m")));
}

ISuspenseCorePlayerRepository* USuspenseCorePlayerInfoWidget::GetRepository()
{
	USuspenseCoreEventManager* Manager = USuspenseCoreEventManager::Get(GetWorld());
	if (Manager)
	{
		USuspenseCoreServiceLocator* ServiceLocator = Manager->GetServiceLocator();
		if (ServiceLocator && ServiceLocator->HasService(TEXT("PlayerRepository")))
		{
			UObject* RepoObj = ServiceLocator->GetServiceByName(TEXT("PlayerRepository"));
			if (RepoObj && RepoObj->Implements<USuspenseCorePlayerRepository>())
			{
				return Cast<ISuspenseCorePlayerRepository>(RepoObj);
			}
		}
	}

	// Create default file repository
	USuspenseCoreFilePlayerRepository* FileRepo = NewObject<USuspenseCoreFilePlayerRepository>(this);

	if (Manager)
	{
		USuspenseCoreServiceLocator* ServiceLocator = Manager->GetServiceLocator();
		if (ServiceLocator)
		{
			ServiceLocator->RegisterServiceByName(TEXT("PlayerRepository"), FileRepo);
		}
	}

	return FileRepo;
}

void USuspenseCorePlayerInfoWidget::UpdateUIFromData()
{
	const FSuspenseCorePlayerData& Data = CachedPlayerData;

	// Basic info
	if (DisplayNameText)
	{
		DisplayNameText->SetText(FText::FromString(Data.DisplayName));
	}

	if (PlayerIdText)
	{
		// Show first 8 chars of ID
		FString ShortId = Data.PlayerId.Left(8) + TEXT("...");
		PlayerIdText->SetText(FText::FromString(ShortId));
	}

	// Level & XP
	if (LevelText)
	{
		LevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv. %d"), Data.Level)));
	}

	if (XPProgressBar)
	{
		// Calculate XP progress (simple exponential curve)
		int64 XPForCurrentLevel = 100 * FMath::Pow(1.15f, Data.Level - 1);
		int64 XPForNextLevel = 100 * FMath::Pow(1.15f, Data.Level);
		int64 XPInLevel = Data.ExperiencePoints - XPForCurrentLevel;
		int64 XPNeeded = XPForNextLevel - XPForCurrentLevel;

		float Progress = (XPNeeded > 0) ? static_cast<float>(XPInLevel) / XPNeeded : 1.0f;
		XPProgressBar->SetPercent(FMath::Clamp(Progress, 0.0f, 1.0f));
	}

	if (XPText)
	{
		XPText->SetText(FText::FromString(
			FString::Printf(TEXT("%s XP"), *FormatLargeNumber(Data.ExperiencePoints))));
	}

	// Currency
	if (SoftCurrencyText)
	{
		SoftCurrencyText->SetText(FText::FromString(FormatLargeNumber(Data.SoftCurrency)));
	}

	if (HardCurrencyText)
	{
		HardCurrencyText->SetText(FText::FromString(FormatLargeNumber(Data.HardCurrency)));
	}

	// Stats
	if (KillsText)
	{
		KillsText->SetText(FText::FromString(FormatLargeNumber(Data.Stats.Kills)));
	}

	if (DeathsText)
	{
		DeathsText->SetText(FText::FromString(FormatLargeNumber(Data.Stats.Deaths)));
	}

	if (KDRatioText)
	{
		float KDRatio = Data.Stats.GetKDRatio();
		KDRatioText->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), KDRatio)));
	}

	if (WinsText)
	{
		WinsText->SetText(FText::FromString(FormatLargeNumber(Data.Stats.Wins)));
	}

	if (MatchesText)
	{
		MatchesText->SetText(FText::FromString(FormatLargeNumber(Data.Stats.MatchesPlayed)));
	}

	if (PlaytimeText)
	{
		PlaytimeText->SetText(FText::FromString(FormatPlaytime(Data.Stats.PlayTimeSeconds)));
	}
}

FString USuspenseCorePlayerInfoWidget::FormatLargeNumber(int64 Value) const
{
	if (Value >= 1000000000)
	{
		return FString::Printf(TEXT("%.1fB"), Value / 1000000000.0);
	}
	else if (Value >= 1000000)
	{
		return FString::Printf(TEXT("%.1fM"), Value / 1000000.0);
	}
	else if (Value >= 1000)
	{
		return FString::Printf(TEXT("%.1fK"), Value / 1000.0);
	}

	return FString::Printf(TEXT("%lld"), Value);
}

FString USuspenseCorePlayerInfoWidget::FormatPlaytime(int64 Seconds) const
{
	int64 Hours = Seconds / 3600;
	int64 Minutes = (Seconds % 3600) / 60;

	if (Hours > 0)
	{
		return FString::Printf(TEXT("%lldh %lldm"), Hours, Minutes);
	}
	else
	{
		return FString::Printf(TEXT("%lldm"), Minutes);
	}
}

void USuspenseCorePlayerInfoWidget::OnRefreshButtonClicked()
{
	RefreshData();
}

void USuspenseCorePlayerInfoWidget::OnProgressionEvent(FGameplayTag EventTag, const FSuspenseCoreEventData& EventData)
{
	// Refresh if the event is for our player
	if (!CurrentPlayerId.IsEmpty())
	{
		RefreshData();
	}
}

void USuspenseCorePlayerInfoWidget::DisplayTestPlayerData(const FString& DisplayName)
{
	// Create test player with sample data for UI debugging
	FSuspenseCorePlayerData TestData = FSuspenseCorePlayerData::CreateTestPlayer(DisplayName);
	DisplayPlayerData(TestData);

	UE_LOG(LogTemp, Log, TEXT("SuspenseCorePlayerInfoWidget: Displaying test data for '%s'"), *DisplayName);
}

void USuspenseCorePlayerInfoWidget::StartAutoRefresh()
{
	if (AutoRefreshInterval > 0.0f && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			AutoRefreshTimerHandle,
			this,
			&USuspenseCorePlayerInfoWidget::RefreshData,
			AutoRefreshInterval,
			true // Loop
		);
	}
}

void USuspenseCorePlayerInfoWidget::StopAutoRefresh()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoRefreshTimerHandle);
	}
}
