#include "Buildings/BuildingsActor.h"

ABuildingsActor::ABuildingsActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SceneComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RootComponent"));
	SetRootComponent(SceneComponent);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		SceneComponent->SetStaticMesh(CubeMesh.Object);
	}
}

void ABuildingsActor::InitializePrimitiveComponent(FGeneratedMesh& InGeneratedMesh)
{
	const FName UniqueName = MakeUniqueObjectName(this,UTrianglePrimitiveComponent::StaticClass(),FName(TEXT("BuildingsPrimitiveComponent")));

	UTrianglePrimitiveComponent* RenderComponent = NewObject<UTrianglePrimitiveComponent>(this,UTrianglePrimitiveComponent::StaticClass(),UniqueName);
	RenderComponent->SetVerticesAndIndices(MoveTemp(InGeneratedMesh.DynamicVertices), MoveTemp(InGeneratedMesh.Indices));
	RenderComponent->RegisterComponent();		
	RenderComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	RenderComponent->MarkRenderStateDirty();
	
	RenderComponents.Add(RenderComponent);
}
