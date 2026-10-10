// Fill out your copyright notice in the Description page of Project Settings.


#include "RHIActorScene.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

// Sets default values
ARHIActorScene::ARHIActorScene()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	SceneComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RootComponent"));
	TriangleRenderComponent = CreateDefaultSubobject<UTrianglePrimitiveComponent>(TEXT("TriangleRenderComponent"));
	SetRootComponent(SceneComponent);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		SceneComponent->SetStaticMesh(CubeMesh.Object);
	}
}

void ARHIActorScene::RefreshMesh()
{	
	TriangleRenderComponent->UnregisterComponent();  // w momencie wyrejestrowania componentu, wywo³ywane jest DestroyRenderThreadResources() w FTrianglePrimitiveSceneProxy, który niszczy zasoby renderera (VertexBuffer, IndexBuffer, VertexFactory)
	TriangleRenderComponent->RegisterComponent();    // Rejestrujemy jeszcze raz component, co wywo³uje znów CreateSceneProxy() a tym samym odtworzenie zasobów do renderowania
}

// Called when the game starts or when spawned
void ARHIActorScene::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ARHIActorScene::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// FVertexPositionBuffer
/*
void FVertexPositionBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	check(IsInRenderingThread());

	int32 BufferSize = Vertices.Num() * sizeof(FVertex);
	FRHIBufferCreateDesc BufferDesc = FRHIBufferCreateDesc::CreateVertex(TEXT("MyVertexBuffer"), BufferSize);
	BufferDesc.SetInitialState(ERHIAccess::VertexOrIndexBuffer); // <- nie wiem czy w tym przypadku to jest potrzebne.
	VertexBufferRHI = RHICmdList.CreateBuffer(BufferDesc);       // Ok - czyli VertexBufferRHI jest polem klasy FVertexBuffer -  które inicjalizujemy

	// Robimy LockBuffer() - czyli chcemy dostaæ wskaŸnik do pamiêci bufora i poinformowaæ ¿e bêdziemy pisaæ do tej pamiêci z CPU
	void* BufferToWrite = RHICmdList.LockBuffer(VertexBufferRHI, 0, BufferSize, EResourceLockMode::RLM_WriteOnly);

	// Kopiujemy dane wierzcho³ków do bufora.    UWAGA -> Vertices.GetData() to nie to samo co &Vertices ! 
	memcpy(BufferToWrite, Vertices.GetData(), BufferSize);

	// UnlockBuffer - po skopiowaniu danych
	RHICmdList.UnlockBuffer(VertexBufferRHI);
}

void FMeshIndexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	int32 BufferSize = Indices.Num() * sizeof(int32);
	FRHIBufferCreateDesc BufferDesc = FRHIBufferCreateDesc::CreateIndex(TEXT("MyIndexBuffer"), BufferSize, sizeof(int32));
	BufferDesc.SetInitialState(ERHIAccess::VertexOrIndexBuffer); 

	IndexBufferRHI = RHICmdList.CreateBuffer(BufferDesc);

	void* BufferToWrite = RHICmdList.LockBuffer(IndexBufferRHI, 0, BufferSize, EResourceLockMode::RLM_WriteOnly);

	memcpy(BufferToWrite, Indices.GetData(), BufferSize);

	RHICmdList.UnlockBuffer(IndexBufferRHI);
}
*/
