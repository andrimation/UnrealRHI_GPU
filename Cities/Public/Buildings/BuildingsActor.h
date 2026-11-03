// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UnrealRHI/Public/TrianglePrimitiveComponent.h"
#include "../Buildings/BuildingsGenerator.h"

#include "BuildingsActor.generated.h"

// Do holdowania Actorów w œwiecie lepiej u¿ywaæ WorldSubsystem. World Subsystem ¿yje wraz z UWorld - natomiast modu³ jest jakby jedn¹ instancj¹ na ca³y 
// projekt.

UCLASS()
class CITIES_API ABuildingsActor : public AActor
{
    GENERATED_BODY()

public:

	ABuildingsActor();
 
    // W³aœciwie to component tworzymy dopiero po pobraniu danych z OSM i wygenerowaniu geometrii - bo w momencie jego tworzenia powstaje scene proxy
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI")
	UStaticMeshComponent* SceneComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI")
	UTrianglePrimitiveComponent* BuildingsRenderComponent;

	// Ah ! No tak - Uproperty z TObjectPtr chroni componenty przed GC. Jak znika uproperty które holduje wskaŸniki, to componenty zostan¹ usuniête
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI")
	TArray<TObjectPtr<UTrianglePrimitiveComponent>> RenderComponents;

	void InitializePrimitiveComponent(FGeneratedMesh& InGeneratedMesh);

};