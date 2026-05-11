# Iris activation — DefaultEngine.ini, Build.cs, Target.cs

Load when you are bringing Iris up in SuspenseCore **for the first time**, or auditing whether
Iris is actually active vs. silently falling back to the legacy net driver.

---

## 1. DefaultEngine.ini — flip the runtime switches

Iris is **opt-in** in UE5.7. Without the lines below, the classic NetDriver keeps the legacy
polling path. Push-Model also defaults to off — without it, Iris degrades to a slower polling
loop and you lose most of the win.

```ini
; --- Iris core activation ---
[/Script/Engine.Engine]
net.Iris.UseIrisReplication=1
net.SubObjects.DefaultUseSubObjectReplicationList=1
net.IsPushModelEnabled=1

; --- Replace the legacy net driver with Iris-aware definition ---
[SystemSettings]
!IrisNetDriverConfigs=ClearArray
+IrisNetDriverConfigs=(NetDriverDefinition="GameNetDriver",bEnableIris=true)

; --- Diagnostics during migration (revert to Log after stabilization) ---
[Core.Log]
LogIris=Verbose
LogIrisReplication=Verbose
LogIrisFilter=Log
```

**Why each line matters:**
- `net.Iris.UseIrisReplication=1` — master switch. Without it nothing else activates.
- `net.SubObjects.DefaultUseSubObjectReplicationList=1` — Iris routes sub-objects (e.g. components
  on `USuspenseCoreInventoryComponent`) through a flat list; the legacy actor channel path is broken
  in Iris mode without this.
- `net.IsPushModelEnabled=1` — without this, `DOREPLIFETIME_WITH_PARAMS_FAST` with `bIsPushBased=true`
  silently degrades to polling. Catch this early — `LogIrisReplication Verbose` will show
  `Push model disabled` on startup if it's off.
- The `+IrisNetDriverConfigs` line is what actually replaces the driver instance for `GameNetDriver`.

---

## 2. Build.cs — add IrisCore + register macro support

Per-module dependency. Required in every SuspenseCore module that contains replicated actors
or components.

```csharp
using UnrealBuildTool;

public class SuspenseCore : ModuleRules
{
    public SuspenseCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "NetCore",
            "IrisCore"           // Iris runtime
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "GameplayAbilities", // GAS integration
            "GameplayTags",
            "GameplayTasks"
        });

        // Critical: enables UHT generation of Iris state descriptors,
        // UE_NET_DECLARE_IRIS_STATE expansion, and SetupIrisSupport macros.
        SetupIrisSupport(Target);
    }
}
```

**Forgetting `SetupIrisSupport(Target)`** is the most common silent failure. Code compiles, but
`UE_NET_DECLARE_IRIS_STATE` produces empty descriptors and your custom state never serializes.
If a custom struct "is replicated but never arrives" — check this line first.

---

## 3. Target.cs — global Iris flag

```csharp
using UnrealBuildTool;
using System.Collections.Generic;

public class SuspenseCoreTarget : TargetRules
{
    public SuspenseCoreTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        ExtraModuleNames.AddRange(new string[] { "SuspenseCore" });

        // Compile Iris paths into the build. When false, #if UE_WITH_IRIS
        // gates strip Iris calls and the legacy NetDriver is used.
        bUseIris = true;
    }
}
```

Mirror this in `SuspenseCoreServerTarget.cs` and `SuspenseCoreEditorTarget.cs`. Asymmetric values
(e.g. server has `bUseIris = true`, client does not) cause protocol-mismatch disconnects.

---

## 4. Verification — did Iris actually come online?

Run the dedicated server with the verbose categories from §1 and check the log on first client
join. Expect lines like:

```
LogIris: ReplicationSystem 0 initialized
LogIris: PushModel enabled, polling disabled for push-based properties
LogIrisReplication: Bound protocol 'ASuspenseCorePickupItem' (NetRefHandle 0x...)
LogIrisFilter: NetObject filter 'UNetObjectGridWorldLocFilter' registered, CellSizeX=10000
```

**Red flags that mean Iris did NOT come up:**
- `LogNet: Server: Spawn, Actor=ASuspenseCorePickupItem` without any `LogIrisReplication` line for
  the same actor → Iris driver not selected, legacy path taken.
- `LogIrisReplication: Falling back to polling for property X` → `net.IsPushModelEnabled` is off.
- `LogIris: SetupIrisSupport missing for module SuspenseCore` → `Build.cs` did not call the macro.
- Editor PIE: Iris is sometimes disabled in PIE depending on engine version — verify on a
  **dedicated server build** before declaring failure.

---

## 5. Pre-migration checklist

Before touching any actor:
- [ ] `DefaultEngine.ini` lines from §1 present and committed.
- [ ] All replicated-content modules have `IrisCore` + `SetupIrisSupport(Target)` in Build.cs.
- [ ] All `Target.cs` variants have `bUseIris = true`.
- [ ] Dedicated server log shows the green-flag lines from §4.
- [ ] `LogIris` set to `Verbose` for the migration sprint; flip back to `Log` before release.

Only after this is green do you start per-actor migration (see `push-model-migration.md`).
