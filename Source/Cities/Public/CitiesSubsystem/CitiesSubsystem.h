// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "../OSMImporter/OSMImporter.h"
#include "../Buildings/BuildingsGenerator.h"

#include "CitiesSubsystem.generated.h"

class ABuildingsActor;

struct FGeneratedBuildingsMesh
{
	FGeneratedMesh WallMesh;
	FGeneratedMesh RoofMesh;
};

// Do holdowania Actorów w œwiecie lepiej u¿ywaæ WorldSubsystem. World Subsystem ¿yje wraz z UWorld - natomiast modu³ jest jakby jedn¹ instancj¹ na ca³y 
// projekt.
UCLASS()
class CITIES_API UCitiesWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    DECLARE_DELEGATE_OneParam(FOnOSMDataLoaded, FOSMData&&);
    DECLARE_DELEGATE_OneParam(FOnMeshGenerated, FGeneratedBuildingsMesh&&);
    
    UPROPERTY()
    TObjectPtr<ABuildingsActor> BuildingsActor;

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	void LoadOSMData(const FString& FilePath);
	void UtilizeLoadedOSMData(FOSMData&& InOSMData);
    void GenerateBuildings(TArray<FOSMFeature>&& InBuildingsData);

	void UtilizeGeneratedBuildingsMeshes(FGeneratedBuildingsMesh&& InGeneratedBuilding);
    //virtual void Deinitialize() override;

    // Delegates
	FOnOSMDataLoaded OnOSMDataLoaded;

private:

	FBuildingsGenerator BuildingsGenerator;

};