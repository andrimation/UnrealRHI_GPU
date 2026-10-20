#include "CitiesSubsystem/CitiesSubsystem.h"
#include "Buildings/BuildingsGenerator.h"
#include "Modules/ModuleManager.h"
#include "Buildings/BuildingsActor.h"

class FLoadOSMDataTask : public FNonAbandonableTask
{
	FLoadOSMDataTask(const FString& InFilePath, UCitiesWorldSubsystem::FOnOSMDataLoaded InCallback)
		: FilePath(InFilePath)
		, Callback(InCallback)
	{
	}

private:
	void DoWork()
	{
		FOSMData OSMData;
		if (FOSMImporter::LoadFromFile(FilePath, OSMData))
		{
			// Tu wracamy do GT
			AsyncTask(ENamedThreads::GameThread, [OutOSMData = MoveTemp(OSMData), Cb = Callback]() mutable
			{
				Cb.ExecuteIfBound(MoveTemp(OutOSMData));
			});

		}
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FLoadOSMDataTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	// Private members
	FString FilePath;
	UCitiesWorldSubsystem::FOnOSMDataLoaded Callback;
	friend class FAutoDeleteAsyncTask<FLoadOSMDataTask>;
};

class FGenerateMeshTask : public FNonAbandonableTask
{
	FGenerateMeshTask(TArray<FOSMFeature>&& InBuildingsData, UCitiesWorldSubsystem::FOnMeshGenerated InCallback)
		: BuildingsData(MoveTemp(InBuildingsData))
		, Callback(MoveTemp(InCallback))
	{
	}

private:

	void DoWork()
	{
		FGeneratedBuildingsMesh GeneratedBuildingsMesh;
		GeneratedBuildingsMesh.WallMesh = FBuildingsGenerator::GenerateBuildingsWallMesh(BuildingsData, 300.0f);
		GeneratedBuildingsMesh.RoofMesh = FBuildingsGenerator::GenerateBuildingsRoofMesh(BuildingsData, 300.0f);
		// Tu wracamy do GT
		AsyncTask(ENamedThreads::GameThread, [OutGeneratedMesh = MoveTemp(GeneratedBuildingsMesh), Cb = MoveTemp(Callback)]() mutable
		{
			Cb.ExecuteIfBound(
				MoveTemp(OutGeneratedMesh)
			);
			});
		
	}
	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FGenerateMeshTask, STATGROUP_ThreadPoolAsyncTasks);
	}
	TArray<FOSMFeature> BuildingsData;
	UCitiesWorldSubsystem::FOnMeshGenerated Callback;
	friend class FAutoDeleteAsyncTask<FGenerateMeshTask>;
};

void UCitiesWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();

	if (!World )
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	BuildingsActor = World->SpawnActor<ABuildingsActor>(
		ABuildingsActor::StaticClass(),
		FTransform::Identity,
		SpawnParams
	);

	LoadOSMData("Source/Cities/Public/OSMImporter/OSMData/map.osm");
}

void UCitiesWorldSubsystem::LoadOSMData(const FString& InFilePath)
{
	FString OSMFilePath = FPaths::Combine(FPaths::ProjectDir(), InFilePath);
	
	(new FAutoDeleteAsyncTask<FLoadOSMDataTask>(OSMFilePath, FOnOSMDataLoaded::CreateUObject(this,&UCitiesWorldSubsystem::UtilizeLoadedOSMData)))->StartBackgroundTask();
}

void UCitiesWorldSubsystem::UtilizeLoadedOSMData(FOSMData&& InOSMData)
{
	GenerateBuildings(MoveTemp(InOSMData.Buildings));
}

void UCitiesWorldSubsystem::GenerateBuildings(TArray<FOSMFeature>&& InBuildingsData)
{
	(new FAutoDeleteAsyncTask<FGenerateMeshTask>(MoveTemp(InBuildingsData), FOnMeshGenerated::CreateUObject(this, &UCitiesWorldSubsystem::UtilizeGeneratedBuildingsMeshes)))->StartBackgroundTask();
}

void UCitiesWorldSubsystem::UtilizeGeneratedBuildingsMeshes(FGeneratedBuildingsMesh&& InGeneratedBuilding)
{
	// Trzymamy obiekt GeneratedMesh tu, a funkcje building actora albo road actora po prostu konsumuj¹ jego pola
	FGeneratedBuildingsMesh GeneratedBuilding = MoveTemp(InGeneratedBuilding);

	BuildingsActor->InitializePrimitiveComponent(GeneratedBuilding.WallMesh);
	BuildingsActor->InitializePrimitiveComponent(GeneratedBuilding.RoofMesh);
}
