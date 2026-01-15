// SuspenseCoreCameraShakeDataAsset.cpp
// SuspenseCore - Camera Shake Configuration Data Asset
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore/CameraShake/SuspenseCoreCameraShakeDataAsset.h"
#include "SuspenseCore/Settings/SuspenseCoreSettings.h"

USuspenseCoreCameraShakeDataAsset* USuspenseCoreCameraShakeDataAsset::Get()
{
	const USuspenseCoreSettings* Settings = USuspenseCoreSettings::Get();
	if (Settings && Settings->CameraShakePresetsAsset.IsValid())
	{
		UDataAsset* LoadedAsset = Settings->CameraShakePresetsAsset.LoadSynchronous();
		return Cast<USuspenseCoreCameraShakeDataAsset>(LoadedAsset);
	}
	return nullptr;
}

USuspenseCoreCameraShakeDataAsset::USuspenseCoreCameraShakeDataAsset()
	: MasterScale(1.0f)
	, bUsePerlinNoiseByDefault(true)
	, DefaultOscillatorMode(ESuspenseCoreOscillatorMode::Combined)
	, bEnablePriorityBlending(true)
{
	// Initialize default weapon presets
	{
		FSuspenseCoreWeaponShakePreset Rifle;
		Rifle.PresetName = FName(TEXT("Rifle"));
		Rifle.ShakeParams = FSuspenseCoreWeaponShakeParams::GetRiflePreset();
		WeaponPresets.Add(Rifle);

		FSuspenseCoreWeaponShakePreset Pistol;
		Pistol.PresetName = FName(TEXT("Pistol"));
		Pistol.ShakeParams = FSuspenseCoreWeaponShakeParams::GetPistolPreset();
		WeaponPresets.Add(Pistol);

		FSuspenseCoreWeaponShakePreset SMG;
		SMG.PresetName = FName(TEXT("SMG"));
		SMG.ShakeParams = FSuspenseCoreWeaponShakeParams::GetSMGPreset();
		WeaponPresets.Add(SMG);

		FSuspenseCoreWeaponShakePreset Shotgun;
		Shotgun.PresetName = FName(TEXT("Shotgun"));
		Shotgun.ShakeParams = FSuspenseCoreWeaponShakeParams::GetShotgunPreset();
		WeaponPresets.Add(Shotgun);

		FSuspenseCoreWeaponShakePreset Sniper;
		Sniper.PresetName = FName(TEXT("Sniper"));
		Sniper.ShakeParams = FSuspenseCoreWeaponShakeParams::GetSniperPreset();
		WeaponPresets.Add(Sniper);
	}

	// Initialize default movement presets
	{
		FSuspenseCoreMovementShakePreset Landing;
		Landing.PresetName = FName(TEXT("Landing"));
		Landing.ShakeParams = FSuspenseCoreMovementShakeParams::GetLandingPreset();
		MovementPresets.Add(Landing);

		FSuspenseCoreMovementShakePreset HardLanding;
		HardLanding.PresetName = FName(TEXT("HardLanding"));
		HardLanding.ShakeParams = FSuspenseCoreMovementShakeParams::GetHardLandingPreset();
		MovementPresets.Add(HardLanding);

		FSuspenseCoreMovementShakePreset Sprint;
		Sprint.PresetName = FName(TEXT("Sprint"));
		Sprint.ShakeParams = FSuspenseCoreMovementShakeParams::GetSprintPreset();
		MovementPresets.Add(Sprint);
	}

	// Initialize default damage presets
	{
		FSuspenseCoreDamageShakePreset Light;
		Light.PresetName = FName(TEXT("Light"));
		Light.ShakeParams = FSuspenseCoreDamageShakeParams::GetLightPreset();
		DamagePresets.Add(Light);

		FSuspenseCoreDamageShakePreset Heavy;
		Heavy.PresetName = FName(TEXT("Heavy"));
		Heavy.ShakeParams = FSuspenseCoreDamageShakeParams::GetHeavyPreset();
		DamagePresets.Add(Heavy);

		FSuspenseCoreDamageShakePreset Critical;
		Critical.PresetName = FName(TEXT("Critical"));
		Critical.ShakeParams = FSuspenseCoreDamageShakeParams::GetCriticalPreset();
		DamagePresets.Add(Critical);
	}

	// Initialize default explosion presets
	{
		FSuspenseCoreExplosionShakePreset Grenade;
		Grenade.PresetName = FName(TEXT("Grenade"));
		Grenade.ShakeParams = FSuspenseCoreExplosionShakeParams::GetGrenadePreset();
		Grenade.MaxDistance = 1500.0f;
		ExplosionPresets.Add(Grenade);

		FSuspenseCoreExplosionShakePreset Artillery;
		Artillery.PresetName = FName(TEXT("Artillery"));
		Artillery.ShakeParams = FSuspenseCoreExplosionShakeParams::GetArtilleryPreset();
		Artillery.MaxDistance = 5000.0f;
		ExplosionPresets.Add(Artillery);

		FSuspenseCoreExplosionShakePreset Vehicle;
		Vehicle.PresetName = FName(TEXT("Vehicle"));
		Vehicle.ShakeParams = FSuspenseCoreExplosionShakeParams::GetVehiclePreset();
		Vehicle.MaxDistance = 3000.0f;
		ExplosionPresets.Add(Vehicle);
	}
}

bool USuspenseCoreCameraShakeDataAsset::FindWeaponPreset(FName PresetName, FSuspenseCoreWeaponShakePreset& OutPreset) const
{
	for (const FSuspenseCoreWeaponShakePreset& Preset : WeaponPresets)
	{
		if (Preset.PresetName == PresetName)
		{
			OutPreset = Preset;
			return true;
		}
	}
	return false;
}

bool USuspenseCoreCameraShakeDataAsset::FindMovementPreset(FName PresetName, FSuspenseCoreMovementShakePreset& OutPreset) const
{
	for (const FSuspenseCoreMovementShakePreset& Preset : MovementPresets)
	{
		if (Preset.PresetName == PresetName)
		{
			OutPreset = Preset;
			return true;
		}
	}
	return false;
}

bool USuspenseCoreCameraShakeDataAsset::FindDamagePreset(FName PresetName, FSuspenseCoreDamageShakePreset& OutPreset) const
{
	for (const FSuspenseCoreDamageShakePreset& Preset : DamagePresets)
	{
		if (Preset.PresetName == PresetName)
		{
			OutPreset = Preset;
			return true;
		}
	}
	return false;
}

bool USuspenseCoreCameraShakeDataAsset::FindExplosionPreset(FName PresetName, FSuspenseCoreExplosionShakePreset& OutPreset) const
{
	for (const FSuspenseCoreExplosionShakePreset& Preset : ExplosionPresets)
	{
		if (Preset.PresetName == PresetName)
		{
			OutPreset = Preset;
			return true;
		}
	}
	return false;
}

FPrimaryAssetId USuspenseCoreCameraShakeDataAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("SuspenseCoreCameraShakeConfig"), GetFName());
}

#if WITH_EDITOR
void USuspenseCoreCameraShakeDataAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Apply default oscillator mode to new presets if needed
	if (bUsePerlinNoiseByDefault)
	{
		for (FSuspenseCoreWeaponShakePreset& Preset : WeaponPresets)
		{
			Preset.ShakeParams.Pitch.Mode = DefaultOscillatorMode;
			Preset.ShakeParams.Yaw.Mode = DefaultOscillatorMode;
		}
	}
}
#endif
