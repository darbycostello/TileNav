#include "TileNavigationData.h"
#include "TileNavComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

ATileNavigationData::ATileNavigationData(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer) {
    FindPathImplementation = FindPath;
}

void ATileNavigationData::PostInitProperties() {

    UWorld* MyWorld = GetWorld();
    if (MyWorld != nullptr 
        && HasAnyFlags(RF_NeedLoad)
        && FNavigationSystem::ShouldDiscardSubLevelNavData(*this)
    ) {
        if (!GEngine->IsSettingUpPlayWorld() && MyWorld->GetOutermost() != GetOutermost() && !IsRunningCommandlet()) {
#if WITH_EDITOR
        	if (bDisplayVerboseLogs) {
        		UE_LOG(LogNavigation, Display, TEXT("Discarding %s due to it not being part of PersistentLevel"), *GetNameSafe(this));
        	}
#endif
            CleanUpAndMarkPendingKill();
        }
    }
    Super::PostInitProperties();

}

void ATileNavigationData::BeginPlay() {
	Super::BeginPlay();

	// Rebuild the navigation on startup
	RebuildAll();
	
#if WITH_EDITOR
	DebugDrawTileNav();
#endif
}

void ATileNavigationData::RebuildAll() {
#if WITH_EDITOR
	if (bDisplayVerboseLogs) {
		UE_LOG(LogNavigation, Display, TEXT("Tile Navigation rebuild started."));
	}
#endif
    TileNavSections.Empty();
	TileNavComponents.Empty();

	if (!GetWorld()) {
#if WITH_EDITOR
		if (bDisplayVerboseLogs) {
			UE_LOG(LogNavigation, Error, TEXT("World invalid. Cannot build Tile navigation."));
		}
#endif
		return;
	}
	
	// First, gather all TileNavComponents in the world
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), Actors);
	for (auto& Actor: Actors) {
		TArray<UActorComponent*> TileNavActorComponents;
		Actor->GetComponents(TileNavActorComponents);
		for (auto& TileNavActorComponent: TileNavActorComponents) {
			UTileNavComponent* TileNav = Cast<UTileNavComponent>(TileNavActorComponent);
			if (TileNav) {
				TileNav->SectionIndex = TileNavComponents.Num();
				TileNav->BuildNavigationTiles(GridScale, Clearance);
				TileNavComponents.Add(TileNav);
			}
		}
	}

	TileCount = 0;
	int32 LinkedEdgeTileCount = 0;
	for (auto& TileNav: TileNavComponents) {
		TileCount += TileNav->Tiles.Num();
		LinkedEdgeTileCount += TileNav->EdgeTiles.Num();
		FTileNavSection NavSection(TileNav->SectionIndex);

		// Perform a proximity check between each TileNav component and all others 
		for (auto& OtherNav: TileNavComponents) {
			if (OtherNav == TileNav) {
				continue;
			}
			TArray<UPrimitiveComponent*> OutComponents;				
			UKismetSystemLibrary::SphereOverlapComponents(
				GetWorld(),
				(TileNav->GetComponentLocation() + OtherNav->GetComponentLocation()) * 0.5f,
				ComponentProximity,
				{ObjectTypeQuery1, ObjectTypeQuery2},
				UTileNavComponent::StaticClass(),
				{},
				OutComponents
			);

			if (OutComponents.ContainsByPredicate([TileNav] (UPrimitiveComponent*& Component) {
				return Component == TileNav;
			}) && OutComponents.ContainsByPredicate([OtherNav] (UPrimitiveComponent*& Component) {
                return Component == OtherNav;
            })) {
				// Find and link tiles
				TArray<FVector> TileEdgeKeys;
				TileNav->EdgeTiles.GetKeys(TileEdgeKeys);
				for (auto& Tile: TileEdgeKeys) {
					for (auto& Other: OtherNav->EdgeTiles) {
						if (FVector::Dist(Tile, Other.Key) <= TileProximity) {
							TArray<int32> NeighborSections;
							if (TileNav->EdgeTiles.Contains(Tile)) {
								NeighborSections = TileNav->EdgeTiles[Tile].NavSections;
							}
							NeighborSections.AddUnique(OtherNav->SectionIndex);
							TileNav->EdgeTiles.Add(Tile, FEdgeNeighbors(NeighborSections));

							// Nav sections are proximal so add the other TileNav component as a neighbor
							NavSection.Neighbors.AddUnique(OtherNav->SectionIndex);
						}
					}
				}
			}
		}
		TileNavSections.Add(NavSection);
    }
#if WITH_EDITOR
	UE_LOG(LogNavigation, Warning, TEXT("Nav sections: %d"), TileNavSections.Num());
	UE_LOG(LogNavigation, Warning, TEXT("Walkable tiles: %d"), TileCount);
	UE_LOG(LogNavigation, Warning, TEXT("Linked edge tiles: %d"), LinkedEdgeTileCount);
	DebugDrawTileNav();
#endif
}

int32 ATileNavigationData::GetNearestSection(const ATileNavigationData* NavGraph, const FVector Location, const float Radius) {
	TArray<UPrimitiveComponent*> OutComponents;				
	UKismetSystemLibrary::SphereOverlapComponents(
        NavGraph->GetWorld(),
        Location,
        Radius,
        {ObjectTypeQuery1, ObjectTypeQuery2},
        UTileNavComponent::StaticClass(),
        {},
        OutComponents
    );

	if (OutComponents.Num() > 0) {
		UTileNavComponent* TileNav = Cast<UTileNavComponent>(OutComponents[0]);
		if (TileNav) {
			return TileNav->SectionIndex;
		}
	}
	return -1;
}

FVector ATileNavigationData::GetNearestNavTile(UTileNavComponent* TileNavComponent, const FVector Location) const {
	if (TileNavComponent->Tiles.Num() == 0) {
		return FVector::ZeroVector;
	}
	TArray<float> Distances;			
	for (auto& Tile: TileNavComponent->Tiles) {
		if (Tile.Value.bWalkable) {
			Distances.Add(FVector::DistSquared(Location, Tile.Key));	
		}
	}
	int32 MinIndex;
	FMath::Min(Distances, &MinIndex);
	TArray<FVector> TileKeys;
	TileNavComponent->Tiles.GetKeys(TileKeys);
	return TileKeys[MinIndex];
}

bool ATileNavigationData::GetNearestEdgeTile(
	const FVector Location,
	UTileNavComponent* TileNav,
	UTileNavComponent* OtherTileNav,
	FVector &OutLocation) {

	TArray<FVector> CandidateKeys;
	for (auto& Tile: TileNav->EdgeTiles) {
		if (Tile.Value.NavSections.Contains(OtherTileNav->SectionIndex)) {
			CandidateKeys.Add(Tile.Key);
		}
	}
	if (CandidateKeys.Num() > 0) {
		TArray<float> Distances;			
		for (auto& Candidate: CandidateKeys) {
			Distances.Add(FVector::DistSquared(Location, Candidate));
		}
		int32 MinIndex;
		FMath::Min(Distances, &MinIndex);
		OutLocation = CandidateKeys[MinIndex];
		return true;
	}
	
	return false;
}

FPathFindingResult ATileNavigationData::FindPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query) {
    const ANavigationData* Self = Query.NavData.Get();
    check(Cast<const ATileNavigationData>(Self));
	const ATileNavigationData* NavGraph { Cast<const ATileNavigationData>(Self) };
    if (!NavGraph)  {
#if WITH_EDITOR
    	UE_LOG(LogNavigation, Warning, TEXT("Navigation graph is invalid"));
#endif
        return ENavigationQueryResult::Error;
    }
	if (NavGraph->TileCount == 0) {
#if WITH_EDITOR
		UE_LOG(LogNavigation, Warning, TEXT("Navigation graph has no tiles"));
#endif
		return ENavigationQueryResult::Error;
	}
	if (NavGraph->TileNavComponents.Num() == 0) {
#if WITH_EDITOR
		UE_LOG(LogNavigation, Warning, TEXT("Navigation graph has no TileNav components"));
#endif
		return ENavigationQueryResult::Error;
	}
    FPathFindingResult Result(ENavigationQueryResult::Error);
    Result.Path = Query.PathInstanceToFill.IsValid() ? Query.PathInstanceToFill : Self->CreatePathInstance<FNavigationPath>(Query);
    FNavigationPath* NavPath = Result.Path.Get();

    if (NavPath != nullptr) {
        if ((Query.StartLocation - Query.EndLocation).IsNearlyZero()) {
            Result.Path->GetPathPoints().Reset();
            Result.Path->GetPathPoints().Add(FNavPathPoint(Query.EndLocation));
            Result.Result = ENavigationQueryResult::Success;
        } else if(Query.QueryFilter.IsValid()) {
        	TArray<FVector> PathTiles;
        	bool bFound = false;
        	FindPathInternal(NavGraph, Query, bFound, PathTiles);

			if (PathTiles.Num() > 0) {
				if (NavGraph->bPathPruning) {
					NavGraph->ApplyPathPruning(PathTiles, AgentProperties);
				}
				for (auto& PathTile: PathTiles) {
					NavPath->GetPathPoints().Add(FNavPathPoint(PathTile));
				}
				NavPath->SetIsPartial(!bFound);
				NavPath->MarkReady();
				Result.Result = ENavigationQueryResult::Success;
			} else {
				Result.Result = ENavigationQueryResult::Fail;
			}
        }
    }
    
    return Result;
}

void ATileNavigationData::FindPathInternal(
	const ATileNavigationData* NavGraph,
	const FPathFindingQuery Query,
	bool& bFound,
	TArray<FVector>& PathTiles
) {

	const int32 StartSection = GetNearestSection(NavGraph, Query.StartLocation, Query.NavAgentProperties.AgentHeight);
	

	// Return early if start nav section was not found. This should only happen if the agent's feet are not on a floor mesh.
	if (StartSection == -1) {
#if WITH_EDITOR
		if (NavGraph->bDisplayVerboseLogs) {
			UE_LOG(LogNavigation, Warning, TEXT("Start section index invalid"));
		}
#endif
		return;
	}
	
	const int32 EndSection = GetNearestSection(NavGraph, Query.EndLocation, Query.NavAgentProperties.AgentHeight);
	if (EndSection == -1) {
#if WITH_EDITOR
		if (NavGraph->bDisplayVerboseLogs) {
			UE_LOG(LogNavigation, Warning, TEXT("End section index invalid"));
		}
#endif
	}

	FVector Start = NavGraph->GetNearestNavTile(GetTileNav(NavGraph, StartSection), Query.StartLocation);
	FVector End = NavGraph->GetNearestNavTile(GetTileNav(NavGraph, EndSection), Query.EndLocation);
	
	// Same nav section so find path within a single section
	if (StartSection == EndSection) {
#if WITH_EDITOR
		if (NavGraph->bDisplayVerboseLogs) {
			UE_LOG(LogNavigation, Warning, TEXT("Start and end in same nav section. Finding path within single section"));
		}
#endif
		GetTileNav(NavGraph, StartSection)->FindPath(Start, End, bFound, PathTiles);
	} else {
#if WITH_EDITOR
		if (NavGraph->bDisplayVerboseLogs) {
			UE_LOG(LogNavigation, Warning, TEXT("Multiple sections in path. Finding high level path across sections"));
		}
#endif
		// Multiple sections so find high level path across sections
		TArray<int32> SectionPath;
		if (!FindSectionPathInternal(NavGraph, StartSection, EndSection, SectionPath)) {
#if WITH_EDITOR
			if (NavGraph->bDisplayVerboseLogs) {
				UE_LOG(LogNavigation, Warning,TEXT("No section path found."));
			}
#endif
			return;	
		}

#if WITH_EDITOR
		if (NavGraph->bDisplayVerboseLogs) {
			FString SectionPathString = FString::FromInt(SectionPath[0]);
			for (int32 Index = 1; Index < SectionPath.Num(); Index++) {
				SectionPathString += "-->" + FString::FromInt(SectionPath[Index]);
			}
			UE_LOG(LogNavigation, Warning,TEXT("Section path found: %s"), *SectionPathString);
		}
#endif
		
		// Find initial path to the first link tile
		FVector LinkTile;
		if (!GetNearestEdgeTile( Start, GetTileNav(NavGraph, SectionPath[0]), GetTileNav(NavGraph, SectionPath[1]), LinkTile)) {
#if WITH_EDITOR
			if (NavGraph->bDisplayVerboseLogs) {
				UE_LOG(LogNavigation, Warning,TEXT("No nearest link found for start section."));
			}
#endif
			return;	
		}

		// Find a path to the exit link tile
		GetTileNav(NavGraph, SectionPath[0])->FindPath(Start, LinkTile, bFound, PathTiles);
		if (!bFound) {
#if WITH_EDITOR
			if (NavGraph->bDisplayVerboseLogs) {
				UE_LOG(LogNavigation, Warning,TEXT("No path found for start section."));
			}
#endif
			return;
		}
		
		// Find paths between link tiles of intermediary sections
		for (int32 Index = 1; Index < SectionPath.Num() - 1; Index++) {

			// Get the nearest start tile on the next section
			FVector SectionStart;
			if (!GetNearestEdgeTile(LinkTile, GetTileNav(NavGraph, SectionPath[Index]), GetTileNav(NavGraph, SectionPath[Index-1]), SectionStart)) {
				UE_LOG(LogNavigation, Warning, TEXT("No nearest link found for intermediary section."));
				return;	
			}
			if (!GetNearestEdgeTile(SectionStart, GetTileNav(NavGraph, SectionPath[Index]), GetTileNav(NavGraph, SectionPath[Index+1]), LinkTile)) {
#if WITH_EDITOR
				if (NavGraph->bDisplayVerboseLogs) {
					UE_LOG(LogNavigation, Warning, TEXT("No nearest link found for intermediary section."));
				}
#endif
				return;	
			}
			GetTileNav(NavGraph, SectionPath[Index])->FindPath(SectionStart, LinkTile, bFound, PathTiles);
			if (!bFound) {
#if WITH_EDITOR
				if (NavGraph->bDisplayVerboseLogs) {
					UE_LOG(LogNavigation, Warning, TEXT("No path found for intermediary section."));
				}
#endif
				return;
			}
		}
		
		// Get the nearest start tile on the final section
		FVector SectionStart;
		if (!GetNearestEdgeTile(
			LinkTile,
			GetTileNav(NavGraph, SectionPath[SectionPath.Num()-1]),
				GetTileNav(NavGraph, SectionPath[SectionPath.Num()-2]),
				SectionStart)
		) {
#if WITH_EDITOR
			if (NavGraph->bDisplayVerboseLogs) {
				UE_LOG(LogNavigation, Warning,TEXT("No nearest link found for final section."));
			}
#endif
			return;	
		}
		
		// Find the final path section
		GetTileNav(NavGraph, SectionPath[SectionPath.Num()-1])->FindPath(SectionStart, End, bFound, PathTiles);
		if (!bFound) {
#if WITH_EDITOR
			if (NavGraph->bDisplayVerboseLogs) {
				UE_LOG(LogNavigation, Warning,TEXT("No path found for final section."));	
			}
#endif
			return;
		}
		
		bFound = true;
		
#if WITH_EDITOR
		if (NavGraph->bDisplayPaths) {
			float Index = 1;
			for (auto& Tile: PathTiles) {
				NavGraph->DebugDrawSphere(Tile, 20.0f, FColor::White);
				Index++;
			}	
		}
#endif
	}
}

UTileNavComponent* ATileNavigationData::GetTileNav(const ATileNavigationData* NavGraph, const int32 SectionIndex) {
	return NavGraph->TileNavComponents[NavGraph->TileNavSections[SectionIndex].SectionIndex];
}

bool ATileNavigationData::FindSectionPathInternal(const ATileNavigationData* NavGraph, const int32 Start, const int32 Goal, TArray<int32> &Path) {
	Path.Empty();
	TArray<int32> Queue;
	TMap<int32, int32> Visited;

	Queue.Add(Start);
	Visited.Add(Start, Start);

	while (Queue.Num() > 0) {
		int32 Current = Queue[0];
		Queue.RemoveAt(0);
		if (Current == Goal) {
			int32 Breadcrumb = Current;
			while (Breadcrumb != Start) {
				Path.Insert(Breadcrumb, 0);
				Breadcrumb = Visited[Breadcrumb];
			}
			Path.Insert(Start, 0);
			return true;
		}
		for (auto& Neighbor: NavGraph->TileNavSections[Current].Neighbors) {
			if (!Visited.Contains(Neighbor)) {
				Queue.Push(Neighbor);
				Visited.Add(Neighbor, Current);
			}
		}
	}
	return false;
}

void ATileNavigationData::ApplyPathPruning(TArray<FVector>& Path, const FNavAgentProperties NavAgent) const {
	if (!GetWorld() || Path.Num() < 3) return;
	TArray<FVector> PrunedPath;
	PrunedPath.Add(Path[0]);
	int32 CurrentPoint = 0;
	while (CurrentPoint < Path.Num()) {

		for (int32 Index = CurrentPoint; Index < Path.Num(); Index++) {
			if (Index >= Path.Num() - 2) {
				PrunedPath.Add(Path[Path.Num() - 1]);
				CurrentPoint = Path.Num();
				break;
			}
			FCollisionQueryParams CollisionQueryParams;
			CollisionQueryParams.bTraceComplex = false;
			CollisionQueryParams.TraceTag = "TileNavPathPrune";
			for (auto& TileNavComponent: TileNavComponents) {
				CollisionQueryParams.AddIgnoredComponent(TileNavComponent);
				CollisionQueryParams.AddIgnoredComponents(TileNavComponent->IgnoreComponents);
			}
			
			FHitResult HitResult;
			FVector Start = Path[CurrentPoint];
			FVector End = Path[Index+2];

			// Use a sphere with nav agent radius to check clearance during line-of-sight checks
			GetWorld()->SweepSingleByChannel(
                HitResult,
                Start,
                End,
                FQuat::Identity,
                ECollisionChannel::ECC_WorldStatic,
                FCollisionShape::MakeSphere(NavAgent.AgentRadius),
                CollisionQueryParams
            );
			
			if (HitResult.bBlockingHit) {
				PrunedPath.Add(Path[Index + 1]);
				CurrentPoint = Index + 1;
				break;
			}
		}
	}
	Path = PrunedPath;
}

#if WITH_EDITOR
void ATileNavigationData::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) {
	Super::PostEditChangeProperty(PropertyChangedEvent);
	FProperty* Property = PropertyChangedEvent.Property;
	const FString PropertyName = Property != nullptr ? Property->GetFName().ToString() : "";

	const TSet<FString> DebugProperties = {
		"ComponentProximity",
		"TileProximity",
		"GridScale",
		"bDebugDrawEnabled",
		"DebugScaleFactor",
		"bDisplaySections",
		"bDisplayLinkage",
		"bDisplayTiles",
        "bDisplayPaths",
	};
	if (DebugProperties.Contains(PropertyName)) {
		RebuildAll();
		DebugDrawTileNav();
	}
}

void ATileNavigationData::DebugDrawTileNav() {
	FlushDebugDraw();
	if (!bDebugDrawEnabled) {
		return;
	}
	for (auto& NavSection: TileNavSections) {
		if (bDisplaySections) {
			DebugDrawNumeric(
				TileNavComponents[NavSection.SectionIndex]->GetComponentLocation() + TileNavComponents[NavSection.SectionIndex]->GetUpVector()*100.f,
				FString::FromInt(TileNavComponents[NavSection.SectionIndex]->SectionIndex),
				FColor::Red);
		}
		for (auto& Tile: TileNavComponents[NavSection.SectionIndex]->Tiles) {
			const bool bEdgeTile = TileNavComponents[NavSection.SectionIndex]->EdgeTiles.Contains(Tile.Key);
			if (bDisplayTiles) {
				DebugDrawBox(Tile.Key, FVector(TileNavComponents[NavSection.SectionIndex]->GridSize * GridScale * 0.5f), bEdgeTile ? FColor::Orange : FColor::Cyan);
			}
			if (bEdgeTile) {
				if (TileNavComponents[NavSection.SectionIndex]->EdgeTiles[Tile.Key].NavSections.Num() > 0) {
					if (bDisplayLinkage) {
						FString Numeric= "";
						for (int32 Index = 0; Index < TileNavComponents[NavSection.SectionIndex]->EdgeTiles[Tile.Key].NavSections.Num(); Index++ ) {
							DebugDrawNumeric(
								Tile.Key + FVector(0,0,100.f + 100.0f * Index ),
								FString::FromInt(TileNavComponents[NavSection.SectionIndex]->EdgeTiles[Tile.Key].NavSections[Index]),
								FColor::Yellow
							);
						}
					}
				}
			}
			
		}
	}
}

void ATileNavigationData::DebugDrawSphere(const FVector Location, const float Radius, const FColor Colour) const {
	DrawDebugSphere(GetWorld(), Location, Radius, 12, Colour, true, -1.f, 0, 1.0f);		
}

void ATileNavigationData::DebugDrawBox(const FVector Location, const FVector Extent, const FColor Colour) const {
	DrawDebugBox(GetWorld(), Location, Extent, FQuat::Identity, Colour, true, -1.f, 0, 1.0f);		
}

void ATileNavigationData::DebugDrawNumeric(const FVector Location, const FString String, const FColor Colour) const {

	FVector Start, End;
	const FVector Scale = FVector(1.f, 3.0f, 6.0f) * DebugScaleFactor;
	const float Tracking = 6.f * DebugScaleFactor;

	// Draw the background box
	const FVector Extents = FVector(0, (String.Len() * Scale.Y + (String.Len() - 1) * Tracking) * 0.5f + DebugScaleFactor * 4, Scale.Z + DebugScaleFactor * 2);
	const FColor BoxColour = FColor(Colour.R * 0.25f, Colour.G * 0.25f, Colour.B * 0.25f, Colour.A); 
	const TArray<FVector> Vertices = {
		{Location.X + 1.f, Location.Y - Extents.Y * 1.5f, Location.Z + Extents.Z},
		{Location.X + 1.f, Location.Y + Extents.Y, Location.Z + Extents.Z},
		{Location.X + 1.f, Location.Y + Extents.Y, Location.Z - Extents.Z},
		{Location.X + 1.f, Location.Y - Extents.Y * 1.5f, Location.Z - Extents.Z}};
	const TArray<int32> Indices = {0, 1, 2, 0, 2, 3};
	DrawDebugMesh(GetWorld(), Vertices, Indices, BoxColour, true, -1.0, 0);

	// Draw the numeric string with drawn lines, like a calculator interface
	for (int32 I = 0; I < String.Len(); I++) {
		TArray<bool> Layout;
		switch (String[I]) {
			case 0x30: Layout = {true, true, true, false, true, true, true }; break;
			case 0x31: Layout = {false, false, true, false, false, true, false }; break;
			case 0x32: Layout = {true, false, true, true, true, false, true }; break;
			case 0x33: Layout = {true, false, true, true, false, true, true }; break;
			case 0x34: Layout = {false, true, true, true, false, true, false }; break;
			case 0x35: Layout = {true, true, false, true, false, true, true }; break;
			case 0x36: Layout = {true, true, false, true, true, true, true }; break;
			case 0x37: Layout = {true, false, true, false, false, true, false }; break;
			case 0x38: Layout = {true, true, true, true, true, true, true }; break;
			case 0x39: Layout = {true, true, true, true, false, true, true }; break;
			default: Layout = {false, false, false, true, false, false, false }; break;
		}
		for (int32 J = 0; J < 7; J++) {
			if (!Layout[J]) continue;
            switch (J) {
                case 0: Start = FVector(0, -1, 1); End = FVector(0, 1, 1); break;
                case 1: Start = FVector(0, -1, 1); End = FVector(0, -1, 0); break;
                case 2: Start = FVector(0, 1, 1); End = FVector(0, 1, 0); break;
                case 3: Start = FVector(0, -1, 0); End = FVector(0, 1, 0); break;
                case 4: Start = FVector(0, -1, 0); End = FVector(0, -1, -1); break;
                case 5: Start = FVector(0, 1, 0); End = FVector(0, 1, -1); break;
                case 6: Start = FVector(0, -1, -1); End = FVector(0, 1, -1); break;
                default: break;
            }
            const float YOffset = (String.Len() * Scale.Y + (String.Len()-1) * Tracking) * -0.5f + (Scale.Y + Tracking) * I;
            Start = Start * Scale + FVector(0, YOffset, 0) + Location;
            End = End * Scale + FVector(0, YOffset, 0) + Location;
			DrawDebugLine(GetWorld(), Start, End, Colour, true, -1.0f, 0, DebugScaleFactor);	
		}
	}
}

void ATileNavigationData::FlushDebugDraw() const {
	if (!GetWorld()) return;
	FlushPersistentDebugLines(GetWorld());
	FlushDebugStrings(GetWorld());
}
#endif