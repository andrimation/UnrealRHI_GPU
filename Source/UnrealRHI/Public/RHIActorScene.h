// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TrianglePrimitiveComponent.h"
#include "RHIActorScene.generated.h"


UCLASS()
class UNREALRHI_API ARHIActorScene : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ARHIActorScene();

	UFUNCTION(CallInEditor, Category = "RHI")
	void RefreshMesh();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI")
	UStaticMeshComponent* SceneComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RHI")
	UTrianglePrimitiveComponent* TriangleRenderComponent;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
