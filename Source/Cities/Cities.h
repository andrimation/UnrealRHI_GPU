// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FCitiesModule : public FDefaultGameModuleImpl
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};