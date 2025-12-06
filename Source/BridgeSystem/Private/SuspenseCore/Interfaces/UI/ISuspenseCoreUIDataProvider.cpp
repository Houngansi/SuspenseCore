// ISuspenseCoreUIDataProvider.cpp
// SuspenseCore - UI Data Provider Interface Implementation
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Interfaces/UI/ISuspenseCoreUIDataProvider.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

//==================================================================
// USuspenseCoreUIDataProviderLibrary Implementation
//==================================================================

TScriptInterface<ISuspenseCoreUIDataProvider> USuspenseCoreUIDataProviderLibrary::FindDataProviderOnActor(
	AActor* Actor,
	ESuspenseCoreContainerType ContainerType)
{
	if (!Actor)
	{
		return nullptr;
	}

	// Get all components that implement the interface
	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}

		// Check if component implements the interface
		if (Component->Implements<USuspenseCoreUIDataProvider>())
		{
			ISuspenseCoreUIDataProvider* Provider = Cast<ISuspenseCoreUIDataProvider>(Component);
			if (Provider && Provider->GetContainerType() == ContainerType)
			{
				TScriptInterface<ISuspenseCoreUIDataProvider> Result;
				Result.SetInterface(Provider);
				Result.SetObject(Component);
				return Result;
			}
		}
	}

	return nullptr;
}

TArray<TScriptInterface<ISuspenseCoreUIDataProvider>> USuspenseCoreUIDataProviderLibrary::FindAllDataProvidersOnActor(
	AActor* Actor)
{
	TArray<TScriptInterface<ISuspenseCoreUIDataProvider>> Result;

	if (!Actor)
	{
		return Result;
	}

	// Get all components
	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}

		// Check if component implements the interface
		if (Component->Implements<USuspenseCoreUIDataProvider>())
		{
			ISuspenseCoreUIDataProvider* Provider = Cast<ISuspenseCoreUIDataProvider>(Component);
			if (Provider)
			{
				TScriptInterface<ISuspenseCoreUIDataProvider> Interface;
				Interface.SetInterface(Provider);
				Interface.SetObject(Component);
				Result.Add(Interface);
			}
		}
	}

	return Result;
}

TScriptInterface<ISuspenseCoreUIDataProvider> USuspenseCoreUIDataProviderLibrary::GetProviderFromComponent(
	UActorComponent* Component)
{
	TScriptInterface<ISuspenseCoreUIDataProvider> Result;

	if (!Component)
	{
		return Result;
	}

	if (Component->Implements<USuspenseCoreUIDataProvider>())
	{
		ISuspenseCoreUIDataProvider* Provider = Cast<ISuspenseCoreUIDataProvider>(Component);
		if (Provider)
		{
			Result.SetInterface(Provider);
			Result.SetObject(Component);
		}
	}

	return Result;
}
