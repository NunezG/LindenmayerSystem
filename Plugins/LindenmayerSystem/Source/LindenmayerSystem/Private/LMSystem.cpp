﻿// Robert Chubb a.k.a SaxonRah
#include "LindenmayerSystemPrivatePCH.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetArrayLibrary.h"
#include "LMSystem.h"

ALMSystem::ALMSystem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Generations 
	Generations = 3;
	// Create and Set our root as the RootComponent
	RootComp = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("Root Component"));
	RootComp->SetMobility(EComponentMobility::Static);
	RootComponent = RootComp;

	// Create LSystem Component
	LSystemComp = ObjectInitializer.CreateDefaultSubobject<ULSystemComponent>(this, TEXT("LSystem Component"));

	// Create Turtle
	TurtleComp = ObjectInitializer.CreateDefaultSubobject<UTurtleComponent>(this, TEXT("Turtle Component"));
	TurtleComp->SetMobility(EComponentMobility::Movable);

	TurtleComp->SetWorldTransform(this->GetActorTransform());
	TurtleComp->TurtleInfo.Transform = TurtleComp->GetComponentTransform();
};

void ALMSystem::ClearSplineSystem()
{
	// Clean up Spline Components 
	for (int32 sc = 0; sc < SplineComponents.Num(); ++sc)
	{
		SplineComponents[sc]->DestroyComponent(false);
	}
	SplineComponents.Empty();

	// Clean up Spline Mesh Components 
	for (int32 smc = 0; smc < SplineMeshComponents.Num(); ++smc)
	{
		SplineMeshComponents[smc]->DestroyComponent(false);
	}
	SplineMeshComponents.Empty();

	// Clean up Materials 
	for (int32 m = 0; m < Materials.Num(); ++m)
	{
		Materials[m]->MarkPendingKill();
	}
	Materials.Empty();
}

void ALMSystem::ClearDebugRender()
{
	if (GEngine)
	{
		FlushPersistentDebugLines(GEngine->GetWorld());
	}
}

UPrimitiveComponent* ALMSystem::CreateProceduralComponent(UClass* Type, const FTransform& Transform)
{
	UPrimitiveComponent* CreatedComponent = NewObject<UPrimitiveComponent>(this, Type);
	if (!CreatedComponent)
	{
		return (UPrimitiveComponent*)nullptr;
	}

	CreatedComponent->RegisterComponent(); //You must NewObject<>() with a valid Outer that has world, this == Outer
	CreatedComponent->SetWorldLocation(Transform.GetLocation());
	CreatedComponent->SetWorldRotation(Transform.GetRotation());
	CreatedComponent->AttachToComponent(RootComp, FAttachmentTransformRules::SnapToTargetIncludingScale, NAME_None);
	return CreatedComponent;
}
// SplineComponents is a TArray of USplineComponents
// We need a new spline component for every position from a save to restore
// We first add one spline which goes from the origin of the system until a restore takes place,
// Restore will move the turtle and add a new spline component at the new location.
// We Then target this new index in the TArray of USplineComponents to add points on.

void ALMSystem::RenderSplineLSystem(FLSystem System, FRLSRenderInfo RenderInfo)
{
	ClearSplineSystem();

	// Setup Index variables
	int32 CurrentSplineIndex = 0, PointIndex = 0;

	// Create, Setup and Add Component to Array
	USplineComponent* RootSplineTemp;
	FTransform Temp = TurtleComp->GetComponentTransform();
	RootSplineTemp = (USplineComponent*)CreateProceduralComponent(USplineComponent::StaticClass(), Temp);
	RootSplineTemp->SetClosedLoop(false);
	RootSplineTemp->ClearSplinePoints(false);
	RootSplineTemp->AddSplineLocalPoint(Temp.GetLocation());

	CurrentSplineIndex = SplineComponents.Add(RootSplineTemp);

	// Loop through entire string state
	FString TempString = System.Info.States.Last();
	for (int32 i = 0; i < TempString.Len(); ++i)
	{
		// loop through rules
		for (int32 r = 0; r < RenderInfo.Rules.Num(); ++r)
		{
			// check if character == variable
			if (TempString[i] == RenderInfo.Rules[r].Variable[0])
			{
				// loop through rules for each variable
				for (int32 rt = 0; rt < RenderInfo.Rules[r].RuleType.Num(); ++rt)
				{
					// Switch on each rule from RuleType Array
					switch (RenderInfo.Rules[r].RuleType[rt])
					{
					default:
					{
						break;
					}
					case ERLSRenderRuleType::LSRR_DoNothing:
					{
						break;
					}
					case ERLSRenderRuleType::LSRR_Move:
					{
						TurtleComp->Move(RenderInfo.Rules[r].Length);
						break;
					}
					case ERLSRenderRuleType::LSRR_Draw:
					{
						if (SplineDrawMesh->IsValidLowLevel() && SplineDrawMaterial->IsValidLowLevel())
						{
							// Draw then Add a new point to the current spline
							TurtleComp->Draw(RenderInfo.Rules[r].Length);

							SplineComponents[CurrentSplineIndex]->AddSplineLocalPoint(TurtleComp->GetComponentTransform().GetLocation());
							PointIndex = PointIndex + 1;

							//Set the color!
							UMaterialInstanceDynamic* dynamicMat = UMaterialInstanceDynamic::Create(SplineDrawMaterial, this);
							dynamicMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.5f, 0.3f, 0.1f, 1.0f));

							USplineMeshComponent* SplineMesh;
							Temp = TurtleComp->GetComponentTransform();
							SplineMesh = (USplineMeshComponent*)CreateProceduralComponent(USplineMeshComponent::StaticClass(), Temp);
							SplineMesh->SetMobility(EComponentMobility::Movable);

							//Width of the mesh 
							/*
							float StartSize = 0.2f * (SplineComponents[sc]->GetNumberOfSplinePoints() - sp);
							float EndSize = 0.1f * (SplineComponents[sc]->GetNumberOfSplinePoints() - sp);
							SplineMesh->SetStartScale(FVector2D(StartSize, StartSize), true);
							SplineMesh->SetEndScale(FVector2D(EndSize, EndSize), true);
							*/

							SplineMesh->SetStartScale(FVector2D(1.0f, 1.0f), true);
							SplineMesh->SetEndScale(FVector2D(1.0f, 1.0f), true);

							FVector pointLocationStart, pointTangentStart, pointLocationEnd, pointTangentEnd;
							if (PointIndex == 0 && SplineComponents[CurrentSplineIndex]->GetNumberOfSplinePoints() >= 2)
							{
								SplineComponents[CurrentSplineIndex]->GetLocalLocationAndTangentAtSplinePoint(PointIndex, pointLocationStart, pointTangentStart);
								SplineComponents[CurrentSplineIndex]->GetLocalLocationAndTangentAtSplinePoint(PointIndex + 1, pointLocationEnd, pointTangentEnd);
							}
							else if (PointIndex >= 1 && SplineComponents[CurrentSplineIndex]->GetNumberOfSplinePoints() >= 2)
							{
								SplineComponents[CurrentSplineIndex]->GetLocalLocationAndTangentAtSplinePoint(PointIndex - 1, pointLocationStart, pointTangentStart);
								SplineComponents[CurrentSplineIndex]->GetLocalLocationAndTangentAtSplinePoint(PointIndex, pointLocationEnd, pointTangentEnd);
							}

							SplineMesh->SetStartAndEnd(pointLocationStart, pointTangentStart, pointLocationEnd, pointTangentEnd, false);

							SplineMesh->bCastDynamicShadow = false;
							SplineMesh->SetStaticMesh(SplineDrawMesh);
							SplineMesh->SetMaterial(0, dynamicMat);
							Materials.Add(dynamicMat);
							SplineMeshComponents.Add(SplineMesh);
							break;
						}
						else
						{
							// Draw then Add a new point to the current spline
							TurtleComp->Draw(RenderInfo.Rules[r].Length);

							SplineComponents[CurrentSplineIndex]->AddSplineLocalPoint(TurtleComp->GetComponentTransform().GetLocation());
							PointIndex = PointIndex + 1;
							break;
						}
					}
					case ERLSRenderRuleType::LSRR_DrawLeaf:
					{
						if (SplineDrawLeafMesh->IsValidLowLevel() && SplineDrawLeafMaterial->IsValidLowLevel())
						{
							// Draw Leaf then Add a new point to the current spline
							TurtleComp->DrawLeaf(RenderInfo.Rules[r].Angle, RenderInfo.Rules[r].Length);

							SplineComponents[CurrentSplineIndex]->AddSplineLocalPoint(TurtleComp->GetComponentTransform().GetLocation());
							PointIndex = PointIndex + 1;

							//Set the color!
							UMaterialInstanceDynamic* dynamicMat = UMaterialInstanceDynamic::Create(SplineDrawLeafMaterial, this);
							dynamicMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.0f, 0.5f, 0.0f, 1.0f));

							USplineMeshComponent* SplineMesh;
							Temp = TurtleComp->GetComponentTransform();
							SplineMesh = (USplineMeshComponent*)CreateProceduralComponent(USplineMeshComponent::StaticClass(), Temp);
							SplineMesh->SetMobility(EComponentMobility::Movable);

							//Width of the mesh 
							/*
							float StartSize = 0.2f * (SplineComponents[sc]->GetNumberOfSplinePoints() - sp);
							float EndSize = 0.1f * (SplineComponents[sc]->GetNumberOfSplinePoints() - sp);
							SplineMesh->SetStartScale(FVector2D(StartSize, StartSize), true);
							SplineMesh->SetEndScale(FVector2D(EndSize, EndSize), true);
							*/

							SplineMesh->SetStartScale(FVector2D(1.0f, 1.0f), true);
							SplineMesh->SetEndScale(FVector2D(1.0f, 1.0f), true);

							FVector pointLocationStart, pointTangentStart, pointLocationEnd, pointTangentEnd;
							if (PointIndex == 0 && SplineComponents[CurrentSplineIndex]->GetNumberOfSplinePoints() >= 2)
							{
								SplineComponents[CurrentSplineIndex]->GetLocalLocationAndTangentAtSplinePoint(PointIndex, pointLocationStart, pointTangentStart);
								SplineComponents[CurrentSplineIndex]->GetLocalLocationAndTangentAtSplinePoint(PointIndex + 1, pointLocationEnd, pointTangentEnd);
							}
							else if (PointIndex >= 1 && SplineComponents[CurrentSplineIndex]->GetNumberOfSplinePoints() >= 2)
							{
								SplineComponents[CurrentSplineIndex]->GetLocalLocationAndTangentAtSplinePoint(PointIndex - 1, pointLocationStart, pointTangentStart);
								SplineComponents[CurrentSplineIndex]->GetLocalLocationAndTangentAtSplinePoint(PointIndex, pointLocationEnd, pointTangentEnd);
							}

							SplineMesh->SetStartAndEnd(pointLocationStart, pointTangentStart, pointLocationEnd, pointTangentEnd, false);

							SplineMesh->bCastDynamicShadow = false;
							SplineMesh->SetStaticMesh(SplineDrawLeafMesh);
							SplineMesh->SetMaterial(0, dynamicMat);
							Materials.Add(dynamicMat);
							SplineMeshComponents.Add(SplineMesh);
							break;
						}
						else
						{ // Draw Leaf then Add a new point to the current spline
							TurtleComp->DrawLeaf(RenderInfo.Rules[r].Angle, RenderInfo.Rules[r].Length);

							SplineComponents[CurrentSplineIndex]->AddSplineLocalPoint(TurtleComp->GetComponentTransform().GetLocation());
							PointIndex = PointIndex + 1;
							break;
						}
					}
					case ERLSRenderRuleType::LSRR_TurnRight:
					{
						TurtleComp->TurnRight(RenderInfo.Rules[r].Angle);
						break;
					}
					case ERLSRenderRuleType::LSRR_TurnLeft:
					{
						TurtleComp->TurnLeft(RenderInfo.Rules[r].Angle);
						break;
					}
					case ERLSRenderRuleType::LSRR_Turn180:
					{
						TurtleComp->Turn180();
						break;
					}
					case ERLSRenderRuleType::LSRR_PitchDown:
					{
						TurtleComp->PitchDown(RenderInfo.Rules[r].Angle);
						break;
					}
					case ERLSRenderRuleType::LSRR_PitchUp:
					{
						TurtleComp->PitchUp(RenderInfo.Rules[r].Angle);
						break;
					}
					case ERLSRenderRuleType::LSRR_RollRight:
					{
						TurtleComp->RollRight(RenderInfo.Rules[r].Angle);
						break;
					}
					case ERLSRenderRuleType::LSRR_RollLeft:
					{
						TurtleComp->RollLeft(RenderInfo.Rules[r].Angle);
						break;
					}
					case ERLSRenderRuleType::LSRR_Save:
					{
						TurtleComp->Save();
						break;
					}
					case ERLSRenderRuleType::LSRR_Restore:
					{
						TurtleComp->Restore();

						// Restore turtle position then start a new spline
						USplineComponent* SplineTemp;
						Temp = TurtleComp->GetComponentTransform();
						SplineTemp = (USplineComponent*)CreateProceduralComponent(USplineComponent::StaticClass(), Temp);
						SplineTemp->SetClosedLoop(false);
						SplineTemp->ClearSplinePoints(false);

						// Add point at restores spline index
						SplineTemp->AddSplineLocalPoint(Temp.GetLocation());
						PointIndex = 0;
						CurrentSplineIndex = SplineComponents.Add(SplineTemp);
						break;
					}
					case ERLSRenderRuleType::LSRR_COUNT:
					{
						break;
					}
					}
				}
			}
		}
	}

	TurtleComp->SetWorldTransform(this->GetActorTransform());
	TurtleComp->TurtleInfo.Transform = TurtleComp->GetComponentTransform();

	RegisterAllComponents();
}


// Debug Render
void ALMSystem::RenderDebugLSystem(FLSystem System, FRLSRenderInfo RenderInfo)
{
	ClearDebugRender();

	// Loop through entire string state
	FString TempString = System.Info.States.Last();
	for (int32 i = 0; i < TempString.Len(); ++i)
	{
		// loop through rules
		for (int32 r = 0; r < RenderInfo.Rules.Num(); ++r)
		{
			// check if character == variable
			if (TempString[i] == RenderInfo.Rules[r].Variable[0])
			{
				// loop through rules for each variable
				for (int32 rt = 0; rt < RenderInfo.Rules[r].RuleType.Num(); ++rt)
				{
					// Switch on each rule from RuleType Array
					switch (RenderInfo.Rules[r].RuleType[rt])
					{
					default:
					{
						break;
					}
					case ERLSRenderRuleType::LSRR_DoNothing:
					{
						break;
					}
					case ERLSRenderRuleType::LSRR_Move:
					{
						TurtleComp->DebugMove(RenderInfo.Rules[r].Length);
						break;
					}
					case ERLSRenderRuleType::LSRR_Draw:
					{
						TurtleComp->DebugDraw(RenderInfo.Rules[r].Length);
						break;
					}
					case ERLSRenderRuleType::LSRR_DrawLeaf:
					{
						TurtleComp->DebugDrawLeaf(RenderInfo.Rules[r].Angle, RenderInfo.Rules[r].Length);
						break;
					}
					case ERLSRenderRuleType::LSRR_TurnRight:
					{
						TurtleComp->DebugTurnRight(RenderInfo.Rules[r].Angle);
						break;
					}
					case ERLSRenderRuleType::LSRR_TurnLeft:
					{
						TurtleComp->DebugTurnLeft(RenderInfo.Rules[r].Angle);
						break;
					}
					case ERLSRenderRuleType::LSRR_Turn180:
					{
						TurtleComp->DebugTurn180();
						break;
					}
					case ERLSRenderRuleType::LSRR_PitchDown:
					{
						TurtleComp->DebugPitchDown(RenderInfo.Rules[r].Angle);
						break;
					}
					case ERLSRenderRuleType::LSRR_PitchUp:
					{
						TurtleComp->DebugPitchUp(RenderInfo.Rules[r].Angle);
						break;
					}
					case ERLSRenderRuleType::LSRR_RollRight:
					{
						TurtleComp->DebugRollRight(RenderInfo.Rules[r].Angle);
						break;
					}
					case ERLSRenderRuleType::LSRR_RollLeft:
					{
						TurtleComp->DebugRollLeft(RenderInfo.Rules[r].Angle);
						break;
					}
					case ERLSRenderRuleType::LSRR_Save:
					{
						TurtleComp->DebugSave();
						break;
					}
					case ERLSRenderRuleType::LSRR_Restore:
					{
						TurtleComp->DebugRestore();
						break;
					}
					case ERLSRenderRuleType::LSRR_COUNT:
					{
						break;
					}
					}
				}
			}
		}
	}
	TurtleComp->SetWorldTransform(this->GetActorTransform());
	TurtleComp->TurtleInfo.Transform = TurtleComp->GetComponentTransform();
}
