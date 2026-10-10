// Fill out your copyright notice in the Description page of Project Settings.


#include "RHIActor.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "RenderingThread.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "PipelineStateCache.h"
#include "DeclareShader.h"
#include "RHIStaticStates.h"

//
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "RHITransition.h"

// Sets default values
ARHIActor::ARHIActor()
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


void ARHIActor::TestRHI()
{
	ENQUEUE_RENDER_COMMAND(TestRHICommand)(
		[this](FRHICommandListImmediate& RHICmdList)   // FRHICommandListImmediate jest najbardziej rozszerzon¹ klas¹ FRHICommandList, która pozwala na wykonywanie poleceñ RHI w w¹tku renderowania
		{										   // np mamy równie¿ FRHIComputeCommandList która pozwala na wykonywanie poleceñ RHI w w¹tku obliczeniowym ( czyli robienie compute shaderów )
			check(IsInRenderingThread());

			UE_LOG(LogTemp, Warning, TEXT("Render Thread: In Render Thread - Creating Vertex Buffer!"));

			// Generalnie Vertex Buffer alokujemy z Render Thread, wydaj¹c polecenie RHI alokowania bufora
			// Taki bufor jest traktowany jako zasób GPU, ale nie jest to zapewnione ¿e zosta³ on fizycznie zaalokowany na GPU - to ju¿ zale¿y od DX/Vulkan i sterownika i rodzaju karty.
			int32 BufferSize = sizeof(Triangle);
			FRHIBufferCreateDesc BufferDesc = FRHIBufferCreateDesc::CreateVertex(TEXT("MyVertexBuffer"), BufferSize);
			BufferDesc.SetInitialState(ERHIAccess::VertexOrIndexBuffer);// Musimy dodaæ SetInitialState() aby RHI wiedzia³o w jakim stanie jest bufor po utworzeniu. W tym przypadku bêdzie to ERHIAccess::VertexOrIndexBuffer
			// - stan zasobu opisuje sposób dostêpu a nie jego faktyczn¹ zawartoœæ ( dotyczy to równie¿ DX/Vulkan )

			VertexBuffer = RHICmdList.CreateBuffer(BufferDesc);   // Bufor podlega RAII - wiêc jeœli np nie jest w polu klasy, to po wyjœciu z zakresu zostanie zwolniony. Jednak nie oznacza to ¿e stanie siê to natychmiast
			// - RHI dostaje informacjê ¿e bufor nie jest ju¿ potrzebny, ale jego zwolnienie zale¿y jeszcze od GPU - np jeœli gpu z niego korzysta, moze zostaæ zwolniony póŸniej.

			UE_LOG(LogTemp, Warning, TEXT("Vertex buffer created: %p"), VertexBuffer.GetReference());

			// I teraz jest tak - FBufferRHIRef VertexBuffer; jest tylko handlerem do utworzonego bufora. Sam nie daje nam jeszcze pamiêci do któej mo¿emy zapisywaæ. GPU i CPU nie s¹ zsynchronizowane. Dlatego aby dostaæ
			// pamiêæ bufora do której CPU mo¿e coœ zapisaæ, musimy wywo³aæ LockBuffer() - to zwraca nam wskaŸnik na pamiêæ bufora i informuje ¿e bêdziemy do tej pamieci coœ pisaæ z CPU.
			// Otrzymujemy totalnie surowy wskaŸnik na pamiêæ void* 
			void* MemToWrite = RHICmdList.LockBuffer(VertexBuffer, 0, BufferSize, EResourceLockMode::RLM_WriteOnly);
			// Powy¿ej 0 oznacza offset w buforze - czyli jeœli bufor ma 100 bajtów, to mozemy daæ offset na 10 i bêdziemy pisaæ tylko do kolejnych 90 bajtów, a pierwsze 10 pozostanie niezmienione
			// Uwaga - LockBuffer() nie oznacza "daj mi dostêp do pamiêci GPU" - oznacza "daj mi mechanizm zapisu danych do RHI, które póŸniej zostan¹ przekazane GPU"

			// PóŸniej u¿ywamy memcpy() aby skopiowaæ wierzcho³ki do bufora.
			memcpy(MemToWrite, Triangle, BufferSize);

			// Po skopiowaniu odblokowujemy bufor, w ten sposób informuj¹c RHI (i GPU) ¿e ta pamiêæ nie jest ju¿ u¿ywana przez CPU i mo¿e byæ u¿yta przez GPU.
			RHICmdList.UnlockBuffer(VertexBuffer);
			// ^ puki co skopiowaliœmy surowe dane do bufora, ale GPU nie wie co one oznaczaj¹. Dla niego to tylko surowe bajty.


			// Vertex Declaration   - opisuje jak GPU ma interptretowaæ dane w buforze
			FVertexDeclarationElementList VertexDeclarationElements;

			VertexDeclarationElements.Add(
				FVertexElement(
					0, // StreamIndex -> bêdzie ustawiany w RHICmdList.SetStreamSource()
					0, // Offset,
					VET_Float3,  // Type
					0, // AttributeIndex  -> ³¹czy siê z ATTRIBUTE0
					sizeof(FVertex) // Stride
				)
			);

			// Dodajemy vertex color
			VertexDeclarationElements.Add(
				FVertexElement(
					0, // StreamIndex -> bêdzie ustawiany w RHICmdList.SetStreamSource()
					sizeof(FVector3f), // Offset,
					VET_Float3,  // Type
					1, // AttributeIndex  -> ³¹czy siê z ATTRIBUTE0
					sizeof(FVertex) // Stride
				)
			);

			// A Gdybym chcia³ ¿eby nasza tablica Triangle by³a tablic¹ struktur które maj¹ np kolor i pozycjê, to dodajê kolejny element do deklaracji wierzchio³ków, który wskazuje na kolor
			// - dla shadera bêdzie to po prostu ATTRIBUTE1
			/*
			VertexDeclarationElements.Add(
				FVertexElement(
					0, // StreamIndex
					sizeof(FVector3f), // Offset,  - ale w tym przypadku musimy zmieniæ offset - offset mówi gdzie w strukturze zaczyna siê kolejny element w ramach jednego Vertexa.
					VET_Float3,  // Type
					1, // AttributeIndex  -> ³¹czy siê z ATTRIBUTE1
					sizeof(FVertexPosAndColor) // Stride -> stride mówi ile bajtów trzeba siê przesun¹æ aby zacz¹³ siê kolejny Vertex.
				)
			);
			*/

			FRHIVertexDeclaration* VertexDeclaration = PipelineStateCache::GetOrCreateVertexDeclaration(
				VertexDeclarationElements
			);

			// Pobieranie utworzonych Shaderów
			// UWAGA !! zeby to zadzia³a³o, to modu³ w którym jest ten kod ( tzn ³adowane sa œcie¿ki shaderów - FUnrealRHIModule::StartupModule() ) musi byæ w .uproject mieæ:  "LoadingPhase": "PostConfigInit" !! )
			auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

			// Poni¿ej pobieramy skompilowane instancje shaderów.
			TShaderMapRef<FTriangleShaderVS> VertexShader(ShaderMap);
			TShaderMapRef<FTriangleShaderPS> PixelShader(ShaderMap);
		
			
			// PSO - tworzymy Pipeline State Object - ³¹czy on nasze elementy i opisuje pipeline
			FGraphicsPipelineStateInitializer GraphicsPSOInit;

			// Podstawowe elementy PSO
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<FM_Solid, CM_None>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			// Pod³¹czanie shaderów do PSO
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = VertexDeclaration;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();


			// Ustawiamy Render Target  - puki co bêdziemy renderowaæ do czystego render targetu, nie do backbuffera viewportu
			// Tworzymy texturê render targetu
			constexpr int32 RenderTargetWidth = 1024;
			constexpr int32 RenderTargetHeight = 1024;

			FRHITextureCreateDesc RenderTargetDesc = FRHITextureCreateDesc::Create2D(
				TEXT("OutputRenderTarger"),
				RenderTargetWidth,
				RenderTargetHeight,
				PF_B8G8R8A8
			)
				.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource) // oznacza ¿e bêdziemy mogli do niej renderowaæ, ora¿ ¿e shader bêdzie móg³ j¹ czytaæ/wyœwietliæ
				.SetClearValue(FClearValueBinding(FLinearColor::Black))
				.SetInitialState(ERHIAccess::RTV); // stan RTV - Render Target View - oznacza ¿e mo¿emy do niej renderowaæ

			RenderTargetTexture = RHICmdList.CreateTexture(RenderTargetDesc);

			// Mamy texturê, i teraz musimy wskazaæ ¿e to do niej bêdziemy renderowaæ.
			// Ustawiamy ¿e na pocz¹tku render passu targer ma byæ wyczyszczony (clear) a na koñcu ma byæ zachowany (store) 
			FRHIRenderPassInfo RenderPassInfo(
				RenderTargetTexture.GetReference(),
				ERenderTargetActions::Clear_Store
			);


			// RENDER PASS //
			// Od momentu BeginRenderPass() a¿ do EndRenderPass() - wszystkie draw calle bêd¹ renderowane do RenderTargetTexture
			RHICmdList.BeginRenderPass(
				RenderPassInfo,
				TEXT("TriangleRenderPass")
			);

			// Ustawiamy Viewport - czyli obszar renderowania w render target ( tak jak w vulkanie mo¿na by³o przyci¹æ obszar rendeowania )
			// (To jest GPU vierwport a nie vierport unreala ) 
			RHICmdList.SetViewport(
				0.0f,
				0.0f,
				0.0f,
				RenderTargetWidth,
				RenderTargetHeight,
				1.0f
			);

			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);



			// Stream Source + Draw Primitive:
			
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit,0);

			RHICmdList.SetStreamSource(
				0,             // StreamIndex
				VertexBuffer,  // Buffer
				0              // Offset
			);

			RHICmdList.DrawPrimitive(
				0, // BaseVertexIndex - offest w buorze od którego zaczynamy pobieraæ wierzcho³ki - zaczynamy od 0
				1, // NumPrimitiver   - ile trójk¹tów chcemy narysowaæ ( u nas jeden trójk¹t )
				1  // NumInstances    - ile instancji chcemy narysowaæ  ( np gdybyœmy chcieli póŸniej instancjonowaæ ten sam trójk¹t ( np instanced mesh ) to tu mo¿emy podaæ ile instancji )  
			);

			//

			// END RENDER PASS //
			RHICmdList.EndRenderPass();



			// Zapis wyrenderowanej textury do pliku PNG

			// Odczyt render targetu do CPU - 
			RHICmdList.Transition(
				FRHITransitionInfo(
				RenderTargetTexture.GetReference(),
				ERHIAccess::RTV,  // z jakiego stanu przechodzimy
				ERHIAccess::CopySrc // do jakiego stanu przechodzimy - CopySrc oznacza ¿e bêdziemy kopiowaæ dane z gpu do cpu
			)
			);

			TArray<FColor> Pixels;

			// Odczyt danych z RenderTarget do tablicy Pixels
			RHICmdList.ReadSurfaceData(
				RenderTargetTexture.GetReference(),
				// FIntRect - obszar który chcemy odczytaæ z render targetu
				FIntRect(   
					0,
					0,
					RenderTargetWidth,
					RenderTargetHeight
				),
				Pixels,   // Pixels - tablica do której kopiujemy dane z render targetu
				FReadSurfaceDataFlags()
			);


			// Zapis do PNG

			TArray<uint8> CompressedPNG;

			FImageUtils::CompressImageArray(
				RenderTargetWidth,
				RenderTargetHeight,
				Pixels,
				CompressedPNG
			);

			const FString OutPath =  FPaths::ProjectSavedDir() + TEXT("Saved/RenderOutput.png");

			IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutPath), true);

			bool bSaved = FFileHelper::SaveArrayToFile(CompressedPNG, *OutPath);

			
		}
	);

}

// Called when the game starts or when spawned
void ARHIActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ARHIActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

