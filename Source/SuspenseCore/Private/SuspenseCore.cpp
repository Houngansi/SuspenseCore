// SuspenseCore.cpp
// Main entry point for SuspenseCore plugin
// Copyright Suspense Team. All Rights Reserved.

#include "SuspenseCore.h"

DEFINE_LOG_CATEGORY_STATIC(LogSuspenseCoreMain, Log, All);

#define LOCTEXT_NAMESPACE "FSuspenseCoreModule"

void FSuspenseCoreModule::StartupModule()
{
	UE_LOG(LogSuspenseCoreMain, Log, TEXT("SuspenseCore: Main module loaded"));
	UE_LOG(LogSuspenseCoreMain, Log, TEXT("SuspenseCore: All subsystems available via ServiceProvider"));
}

void FSuspenseCoreModule::ShutdownModule()
{
	UE_LOG(LogSuspenseCoreMain, Log, TEXT("SuspenseCore: Main module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FSuspenseCoreModule, SuspenseCore)
