// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "RHIActor.generated.h"

struct FVertex
{
	FVector3f Position;
	FVector3f Color;
};


UCLASS()
class UNREALRHI_API ARHIActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ARHIActor();

	UFUNCTION(CallInEditor, Category="RHI")
	void TestRHI();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI")
	UStaticMeshComponent* SceneComponent;

	// Wierzcho³ki trójk¹ta do skopiowania do bufora
	FVertex Triangle[3] 
	{	
		FVertex(
			FVector3f(-0.5f, -0.5f, 0.0f),
			FVector3f(1.0f, 0.0f, 0.0f)
		),

		FVertex(
			FVector3f(0.0f,  0.5f, 0.0f),
			FVector3f(0.0f, 1.0f, 0.0f)
		),

		FVertex(
			FVector3f(0.5f, -0.5f, 0.0f),
			FVector3f(0.0f, 0.0f, 1.0f)
		),
	
		/*
		FVector3f(-100.0f, -100.f, 0.0f),
		FVector3f(0.0f,  100.f, 0.0f),
		FVector3f(100.0f, -100.0f, 0.0f)
		*/
		
	};


	// Generalnie Vertex Buffer nie powinien byæ trzymany w tym miejscu - jako zasób actora. Teraz jest tu tylko dla testu. Normalnie powinien mieæ swój osobny obiekt
	FBufferRHIRef VertexBuffer;

	FTextureRHIRef RenderTargetTexture;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
