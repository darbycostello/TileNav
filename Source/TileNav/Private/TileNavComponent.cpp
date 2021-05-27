#include "TileNavComponent.h"
#include "TileNavStructs.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

UTileNavComponent::UTileNavComponent(const FObjectInitializer& ObjectInitializer) {
	PrimaryComponentTick.bCanEverTick = true;
	BodyInstance.SetCollisionProfileName("BlockAll");
	CollisionTraceNormalArrow = ObjectInitializer.CreateDefaultSubobject<UArrowComponent>(this, TEXT("CollisionTraceNormal"));
}

void UTileNavComponent::OnComponentCreated() {
	Super::OnComponentCreated();
	CollisionTraceNormalArrow->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	CollisionTraceNormalArrow->SetRelativeRotation(FRotator(FQuat(CollisionTraceNormal) * FQuat(FRotator(90.f,90.f,90.f))));
}

void UTileNavComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UTileNavComponent::BuildNavigationTiles(const float GridScale, const float Clearance) {
	if (GetStaticMesh() == nullptr) return;
	Tiles.Empty();
	float Interval = GridScale * GridSize / GetComponentScale().Z;
	FVector Min, Max;
	GetLocalBounds(Min, Max);
	const int32 Width = FMath::CeilToInt(((Max - Min) / Interval).X);
	const int32 Height = FMath::CeilToInt(((Max - Min) / Interval).Y);
	TArray<FTileTraceVertex> TraceVertices;
	TMap<FIntPoint, FVector> WalkableTiles;
	TArray<int32> EdgeTileIndices;
	float MinDepth = FMath::Min3(Min.X, Min.Y, Min.Z);
	float MaxDepth = FMath::Max3(Max.X, Max.Y, Max.Z);
	for (int32 Y = 0; Y < Height; Y++) {
		for (int32 X = 0; X < Width; X++) {
			FVector StartVector(
				(X * Interval) + Min.X + (0.5f * Interval),
				(Y * Interval) + Min.Y + (0.5f * Interval),
				(MaxDepth)
			);
			FVector EndVector(
				(X * Interval) + Min.X + (0.5f * Interval),
	            (Y * Interval) + Min.Y + (0.5f * Interval),
	            (MinDepth)
			);
			StartVector = UKismetMathLibrary::Quat_RotateVector(CollisionTraceNormal.Quaternion(), StartVector);
			EndVector = UKismetMathLibrary::Quat_RotateVector(CollisionTraceNormal.Quaternion(), EndVector);
			FCollisionQueryParams QueryParams;
			FHitResult OutHit;
			bool bSurfaceExists = LineTraceComponent(
                OutHit,
                UKismetMathLibrary::TransformLocation(GetComponentTransform(), StartVector),
                UKismetMathLibrary::TransformLocation(GetComponentTransform(), EndVector),
                QueryParams
            );
			FVector Location = bSurfaceExists ? OutHit.ImpactPoint : FVector();
			TraceVertices.Add(FTileTraceVertex(Location, bSurfaceExists));
			if (X > 0 && Y > 0) {
				if (TraceVertices[Y * Width + X - 1].bCollision &&
                    TraceVertices[(Y-1) * Width + X - 1].bCollision &&
                    TraceVertices[(Y-1) * Width + X].bCollision &&
                    TraceVertices[Y * Width + X].bCollision
                ) {
					
					TArray<FVector> VectorArray;
					VectorArray.Add(TraceVertices[Y * Width + X - 1].WorldLocation);
					VectorArray.Add(TraceVertices[(Y-1) * Width + X - 1].WorldLocation);
					VectorArray.Add(TraceVertices[(Y-1) * Width + X].WorldLocation);
					VectorArray.Add(TraceVertices[Y * Width + X].WorldLocation);
					FVector TileLocation = UKismetMathLibrary::GetVectorArrayAverage(VectorArray);

					// Perform a sphere overlap of the surface point based on the default agent radius
					TArray<UPrimitiveComponent*> OutComponents;				
					UKismetSystemLibrary::SphereOverlapComponents(
                        GetWorld(),
                        TileLocation,
                        FNavigationSystem::GetDefaultSupportedAgent().AgentRadius + Clearance,
                        {ObjectTypeQuery1, ObjectTypeQuery2},
                        UPrimitiveComponent::StaticClass(),
                        {},
                        OutComponents
                    );

					// Add a walkable tile if the overlapped objects ONLY contains TileNav and ignored components 
					if (!OutComponents.ContainsByPredicate([this](UPrimitiveComponent* Component) {
                        return !Cast<UTileNavComponent>(Component) && !IgnoreComponents.Contains(Component);
                    })) {
						WalkableTiles.Add(FIntPoint(X-1, Y-1), TileLocation);
						
						// If this tile is on the edge then store its index for the edge tiles list
						if (X == 1 || Y == 1 || X == Width-1 || Y == Height-1 ) {
							EdgeTileIndices.Add(WalkableTiles.Num() - 1);
						}
                    }
                }
			}
		}
	}

	// Create an adjacency list for each walkable tile, used by pathfinding for neighbor searches
	TArray<FIntPoint> WalkableTileKeys;
	WalkableTiles.GenerateKeyArray(WalkableTileKeys);
	TArray<FVector> WalkableTileValues;
	WalkableTiles.GenerateValueArray(WalkableTileValues);
	TArray<int32> WalkableNeighbors;
	for (int32 Index = 0; Index < WalkableTileKeys.Num(); Index++) {
		WalkableNeighbors.Empty();
		FIntPoint Key = WalkableTileKeys[Index];
		for (int32 Ny = -1; Ny <= 1; Ny++) {
			for (int32 Nx = -1; Nx <= 1; Nx++) {
				if (Nx == 0 && Ny == 0) continue;
				if (WalkableTiles.Contains(FIntPoint(Key.X + Nx, Key.Y + Ny))) {
					WalkableNeighbors.Add(WalkableTileKeys.Find(FIntPoint(Key.X + Nx, Key.Y + Ny)));
				}
			}
		}
	
		Tiles.Add(WalkableTileValues[Index], FTileNavTile(true, WalkableNeighbors));
		if (EdgeTileIndices.Contains(Index)) {
			EdgeTiles.Add(WalkableTileValues[Index], FEdgeNeighbors());
		}
	}
}

void UTileNavComponent::GetCollisionBounds(FBox &CollisionBounds) const {
	if (GetStaticMesh()) {
		CollisionBounds = Bounds.ExpandBy(1.0f).GetBox();
	}	
}

bool UTileNavComponent::UpdateTiles(TArray<FVector> InTiles, const bool bWalkable) {
	for (auto& Tile : InTiles) {
		if (Tiles.Contains(Tile)) {
			const TArray<int32> Neighbours = Tiles[Tile].Neighbors;
			Tiles.Add(Tile, FTileNavTile(bWalkable, Neighbours));
		} else {
			return false;
		}
	}
	return true;
}

bool UTileNavComponent::UpdateTile(const FVector Tile, const bool bWalkable) {
	return UpdateTiles({Tile}, bWalkable);
}

void UTileNavComponent::FindPath(const FVector Start, const FVector End, bool &bFound, TArray<FVector> &PathTiles) {
	if (!Tiles.Contains(Start) || !Tiles.Contains(End) || Start == End) return;
	
	// Hard limit for the number of tiles to search. Prevents an infinite loop.
	int32 Limit = 5000; 
	TArray<FVector> NavKeys;
	Tiles.GenerateKeyArray(NavKeys);
	TArray<FPathTile> OpenList;
	TArray<FPathTile> ClosedList;
	const int32 PathIndex = PathTiles.Num();
	OpenList.Add(FPathTile(NavKeys.IndexOfByKey(Start)));

	do {
		Limit--;	
		FPathTile CurrentPathTile = OpenList[0];
		int32 CurrentPathTileIndex = 0;
		for (int Index = 0; Index < OpenList.Num(); Index++) {
			if (OpenList[Index].F < CurrentPathTile.F) {
				CurrentPathTileIndex = Index;
				CurrentPathTile = OpenList[Index];
			}
		}
		
		OpenList.RemoveAtSwap(CurrentPathTileIndex);
		ClosedList.Add(CurrentPathTile);
		if (NavKeys[CurrentPathTile.Index] == End) {
			bFound = true;
			do {
				const FPathTile NextTile = *ClosedList.FindByPredicate([CurrentPathTile](const FPathTile Tile) {
                    return Tile.Index == CurrentPathTile.Parent;
                });
				PathTiles.Insert(NavKeys[NextTile.Index], PathIndex);
				CurrentPathTile = NextTile;
			} while (CurrentPathTile.Parent != -1);
			PathTiles.Add(End);
			return;
		}

		for (auto& ChildIndex: Tiles[NavKeys[CurrentPathTile.Index]].Neighbors) {
			if (ClosedList.ContainsByPredicate([ChildIndex](const FPathTile Tile) {
                return Tile.Index == ChildIndex;
            }) || !Tiles[NavKeys[ChildIndex]].bWalkable) {
				continue;
            }

			FPathTile ChildTile(ChildIndex);
			ChildTile.G = CurrentPathTile.G + FVector::Distance(NavKeys[ChildIndex], NavKeys[CurrentPathTile.Index]);
			ChildTile.H = FVector::Distance(NavKeys[ChildIndex], End);
			ChildTile.F = ChildTile.G + ChildTile.H;
			ChildTile.Parent = CurrentPathTile.Index;

			if (OpenList.ContainsByPredicate([ChildTile](const FPathTile Tile) {
                return Tile.Index == ChildTile.Index;
            })) {
				if (ChildTile.G > CurrentPathTile.G) {
					continue;
				}
            }

			OpenList.Add(ChildTile);
		}
	} while (OpenList.Num() > 0 && Limit > 0);
}