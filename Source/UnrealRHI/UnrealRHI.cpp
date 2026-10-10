// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealRHI.h"
#include "Modules/ModuleManager.h"


void FUnrealRHIModule::StartupModule()
{
	FString ShaderPath = FPaths::Combine
	(
		FPaths::ProjectDir(),
		TEXT("Source/UnrealRHI/Shaders")
	);

	// Mapujemy katalog Ÿród³owy shaderów do wirtualnej œcie¿ki "/UnrealRHI"
	// I póŸniej przekazujemy "/UnrealRHI jako folder do implementacji shaderów
	AddShaderSourceDirectoryMapping(
		TEXT("/UnrealRHI"),
		ShaderPath
	);
}

void FUnrealRHIModule::ShutdownModule()
{
}

IMPLEMENT_PRIMARY_GAME_MODULE( FUnrealRHIModule, UnrealRHI, "UnrealRHI" );
