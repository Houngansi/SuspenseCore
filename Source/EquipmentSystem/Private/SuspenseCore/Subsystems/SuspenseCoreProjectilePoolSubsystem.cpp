// SuspenseCoreProjectilePoolSubsystem.cpp
// Object pooling implementation for grenade projectiles
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/Subsystems/SuspenseCoreProjectilePoolSubsystem.h"
#include "SuspenseCore/Actors/SuspenseCoreGrenadeProjectile.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectilePool, Log, All);

//==================================================================
// Subsystem Lifecycle
//==================================================================

void USuspenseCoreProjectilePoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Only active on server/standalone (clients receive replicated projectiles)
	ENetMode NetMode = World->GetNetMode();
	bPoolActive = (NetMode == NM_DedicatedServer || NetMode == NM_ListenServer || NetMode == NM_Standalone);

	if (bPoolActive)
	{
		// Start periodic cleanup timer
		World->GetTimerManager().SetTimer(
			CleanupTimerHandle,
			this,
			&USuspenseCoreProjectilePoolSubsystem::CleanupExcessPooled,
			CleanupDelay,
			true  // Looping
		);

		UE_LOG(LogProjectilePool, Log, TEXT("Projectile pool initialized (DefaultSize=%d, MaxSize=%d)"),
			DefaultPoolSize, MaxPoolSize);
	}
	else
	{
		UE_LOG(LogProjectilePool, Log, TEXT("Projectile pool disabled (client mode)"));
	}
}

void USuspenseCoreProjectilePoolSubsystem::Deinitialize()
{
	// Clear timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CleanupTimerHandle);
	}

	// Destroy all pooled actors
	ClearPool();

	Super::Deinitialize();
}

bool USuspenseCoreProjectilePoolSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Create for all worlds
	return true;
}

//==================================================================
// Static Access
//==================================================================

USuspenseCoreProjectilePoolSubsystem* USuspenseCoreProjectilePoolSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		return nullptr;
	}

	return World->GetSubsystem<USuspenseCoreProjectilePoolSubsystem>();
}

//==================================================================
// Pool API
//==================================================================

ASuspenseCoreGrenadeProjectile* USuspenseCoreProjectilePoolSubsystem::AcquireProjectile(
	TSubclassOf<ASuspenseCoreGrenadeProjectile> ProjectileClass,
	const FTransform& SpawnTransform)
{
	if (!ProjectileClass || !bPoolActive)
	{
		// Fallback to direct spawn
		return SpawnPooledProjectile(ProjectileClass, SpawnTransform);
	}

	FScopeLock Lock(&PoolLock);

	// Find available projectile in pool
	TArray<FSuspenseCorePooledProjectile>* PoolArray = ProjectilePool.Find(ProjectileClass);
	if (PoolArray)
	{
		for (FSuspenseCorePooledProjectile& Entry : *PoolArray)
		{
			if (!Entry.bInUse && Entry.Projectile && IsValid(Entry.Projectile))
			{
				Entry.bInUse = true;
				ActivateProjectile(Entry.Projectile, SpawnTransform);

				UE_LOG(LogProjectilePool, Verbose, TEXT("Acquired pooled projectile: %s"),
					*Entry.Projectile->GetName());

				return Entry.Projectile;
			}
		}
	}

	// Pool exhausted - spawn new
	ASuspenseCoreGrenadeProjectile* NewProjectile = SpawnPooledProjectile(ProjectileClass, SpawnTransform);
	if (NewProjectile)
	{
		// Add to pool tracking
		if (!PoolArray)
		{
			PoolArray = &ProjectilePool.Add(ProjectileClass);
		}

		// Check max pool size
		if (PoolArray->Num() < MaxPoolSize)
		{
			FSuspenseCorePooledProjectile NewEntry;
			NewEntry.Projectile = NewProjectile;
			NewEntry.bInUse = true;
			NewEntry.ProjectileClass = ProjectileClass;
			PoolArray->Add(NewEntry);

			UE_LOG(LogProjectilePool, Log, TEXT("Pool expanded: %s (size=%d)"),
				*ProjectileClass->GetName(), PoolArray->Num());
		}
	}

	return NewProjectile;
}

void USuspenseCoreProjectilePoolSubsystem::ReleaseProjectile(ASuspenseCoreGrenadeProjectile* Projectile)
{
	if (!Projectile || !bPoolActive)
	{
		// Direct destroy fallback
		if (Projectile)
		{
			Projectile->Destroy();
		}
		return;
	}

	FScopeLock Lock(&PoolLock);

	// Find in pool
	for (auto& Pair : ProjectilePool)
	{
		for (FSuspenseCorePooledProjectile& Entry : Pair.Value)
		{
			if (Entry.Projectile == Projectile)
			{
				Entry.bInUse = false;
				Entry.ReturnTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

				ResetProjectile(Projectile);
				DeactivateProjectile(Projectile);

				UE_LOG(LogProjectilePool, Verbose, TEXT("Released projectile to pool: %s"),
					*Projectile->GetName());

				return;
			}
		}
	}

	// Not found in pool - just destroy
	UE_LOG(LogProjectilePool, Warning, TEXT("Released projectile not in pool, destroying: %s"),
		*Projectile->GetName());
	Projectile->Destroy();
}

void USuspenseCoreProjectilePoolSubsystem::PreWarmPool(
	TSubclassOf<ASuspenseCoreGrenadeProjectile> ProjectileClass,
	int32 Count)
{
	if (!ProjectileClass || !bPoolActive || Count <= 0)
	{
		return;
	}

	FScopeLock Lock(&PoolLock);

	TArray<FSuspenseCorePooledProjectile>& PoolArray = ProjectilePool.FindOrAdd(ProjectileClass);

	int32 ToSpawn = FMath::Min(Count, MaxPoolSize - PoolArray.Num());
	if (ToSpawn <= 0)
	{
		return;
	}

	UE_LOG(LogProjectilePool, Log, TEXT("Pre-warming pool: %s (count=%d)"),
		*ProjectileClass->GetName(), ToSpawn);

	for (int32 i = 0; i < ToSpawn; ++i)
	{
		ASuspenseCoreGrenadeProjectile* Projectile = SpawnPooledProjectile(
			ProjectileClass, FTransform::Identity);

		if (Projectile)
		{
			FSuspenseCorePooledProjectile Entry;
			Entry.Projectile = Projectile;
			Entry.bInUse = false;
			Entry.ReturnTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			Entry.ProjectileClass = ProjectileClass;
			PoolArray.Add(Entry);

			DeactivateProjectile(Projectile);
		}
	}
}

void USuspenseCoreProjectilePoolSubsystem::GetPoolStats(
	int32& OutTotalPooled,
	int32& OutInUse,
	int32& OutAvailable) const
{
	FScopeLock Lock(&PoolLock);

	OutTotalPooled = 0;
	OutInUse = 0;
	OutAvailable = 0;

	for (const auto& Pair : ProjectilePool)
	{
		for (const FSuspenseCorePooledProjectile& Entry : Pair.Value)
		{
			OutTotalPooled++;
			if (Entry.bInUse)
			{
				OutInUse++;
			}
			else
			{
				OutAvailable++;
			}
		}
	}
}

void USuspenseCoreProjectilePoolSubsystem::ClearPool()
{
	FScopeLock Lock(&PoolLock);

	for (auto& Pair : ProjectilePool)
	{
		for (FSuspenseCorePooledProjectile& Entry : Pair.Value)
		{
			if (Entry.Projectile && IsValid(Entry.Projectile))
			{
				Entry.Projectile->Destroy();
			}
		}
	}

	ProjectilePool.Empty();

	UE_LOG(LogProjectilePool, Log, TEXT("Pool cleared"));
}

//==================================================================
// Internal
//==================================================================

ASuspenseCoreGrenadeProjectile* USuspenseCoreProjectilePoolSubsystem::SpawnPooledProjectile(
	TSubclassOf<ASuspenseCoreGrenadeProjectile> ProjectileClass,
	const FTransform& SpawnTransform)
{
	UWorld* World = GetWorld();
	if (!World || !ProjectileClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	return World->SpawnActor<ASuspenseCoreGrenadeProjectile>(
		ProjectileClass,
		SpawnTransform.GetLocation(),
		SpawnTransform.GetRotation().Rotator(),
		SpawnParams
	);
}

void USuspenseCoreProjectilePoolSubsystem::ResetProjectile(ASuspenseCoreGrenadeProjectile* Projectile)
{
	if (!Projectile)
	{
		return;
	}

	// Reset to default state
	// NOTE: GrenadeProjectile should implement a Reset() method for full state reset
	// For now, we rely on InitializeGrenade() being called after acquire

	// Clear any active timers/effects
	// Projectile->Defuse();  // If not already exploded
}

void USuspenseCoreProjectilePoolSubsystem::DeactivateProjectile(ASuspenseCoreGrenadeProjectile* Projectile)
{
	if (!Projectile)
	{
		return;
	}

	// Hide and disable
	Projectile->SetActorHiddenInGame(true);
	Projectile->SetActorEnableCollision(false);
	Projectile->SetActorTickEnabled(false);

	// Move to origin (out of way)
	Projectile->SetActorLocation(FVector(0, 0, -10000.0f));
}

void USuspenseCoreProjectilePoolSubsystem::ActivateProjectile(
	ASuspenseCoreGrenadeProjectile* Projectile,
	const FTransform& Transform)
{
	if (!Projectile)
	{
		return;
	}

	// Position and show
	Projectile->SetActorTransform(Transform);
	Projectile->SetActorHiddenInGame(false);
	Projectile->SetActorEnableCollision(true);
	Projectile->SetActorTickEnabled(true);
}

void USuspenseCoreProjectilePoolSubsystem::CleanupExcessPooled()
{
	if (!bPoolActive)
	{
		return;
	}

	FScopeLock Lock(&PoolLock);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	float CurrentTime = World->GetTimeSeconds();
	int32 CleanedUp = 0;

	for (auto& Pair : ProjectilePool)
	{
		TArray<FSuspenseCorePooledProjectile>& PoolArray = Pair.Value;

		// Count available
		int32 Available = 0;
		for (const FSuspenseCorePooledProjectile& Entry : PoolArray)
		{
			if (!Entry.bInUse)
			{
				Available++;
			}
		}

		// If more than default available and some are old, cleanup
		if (Available > DefaultPoolSize)
		{
			for (int32 i = PoolArray.Num() - 1; i >= 0; --i)
			{
				FSuspenseCorePooledProjectile& Entry = PoolArray[i];

				if (!Entry.bInUse &&
					(CurrentTime - Entry.ReturnTime) > CleanupDelay &&
					Available > DefaultPoolSize)
				{
					if (Entry.Projectile && IsValid(Entry.Projectile))
					{
						Entry.Projectile->Destroy();
					}
					PoolArray.RemoveAt(i);
					Available--;
					CleanedUp++;
				}
			}
		}
	}

	if (CleanedUp > 0)
	{
		UE_LOG(LogProjectilePool, Log, TEXT("Cleaned up %d excess pooled projectiles"), CleanedUp);
	}
}
