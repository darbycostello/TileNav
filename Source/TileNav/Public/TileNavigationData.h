#pragma once
#include "CoreMinimal.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "TileNavStructs.h"
#include "TileNavigationData.generated.h"

/**
 * Custom tile-based navigation class distributed across interconnected collision meshes 
 */
UCLASS( ClassGroup=(TileNav) )
class TILENAV_API ATileNavigationData final : public ANavigationData
{
	GENERATED_BODY()
public:
	// Maximum distance between two TileNav components to be considered for linkage
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileNav|Settings")
	float ComponentProximity = 200.0f;

	// Maximum distance between two tiles of proximal components to be considered for linkage
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileNav|Settings")
	float TileProximity = 250.0f;

	// Global grid size multiplier applied to all TileNav components
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileNav|Settings")
	float GridScale = 1.0f;

	// Additional clearance for tile collisions. Increase this if paths clip through obstacles 
	UPROPERTY(BlueprintReadWrite, BlueprintReadWrite, Category = "TileNav|Settings")
	float Clearance = 35.0f;
	
	// Prune paths with line-of-sight smoothing. Best used within walled areas
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileNav|Settings")
	bool bPathPruning = true;

#if WITH_EDITORONLY_DATA
	// Whether to debug draw
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileNav|Debug")
	bool bDebugDrawEnabled;

	// Whether to display section indices
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileNav|Debug")
	bool bDisplaySections;

	// Whether to display edge linkage indices
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileNav|Debug")
	bool bDisplayLinkage;
	
	// Whether to debug draw the navigation tiles 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileNav|Debug")
	bool bDisplayTiles;

	// Whether to debug draw paths 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileNav|Debug")
	bool bDisplayPaths;

	// Whether to display all console logs during operation 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileNav|Debug")
	bool bDisplayVerboseLogs;

	// Scale multiplier for debug drawing of numeric labels  
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TileNav|Debug")
	float DebugScaleFactor = 5.0f;
#endif
	
private:
	UPROPERTY()
	TArray<FTileNavSection> TileNavSections;

	UPROPERTY()
	TArray<UTileNavComponent*> TileNavComponents;

	UPROPERTY()
	int32 TileCount;

	static int32 GetNearestSection(const ATileNavigationData* NavGraph, const FVector Location, const float Radius);
	static bool GetNearestEdgeTile(const FVector Location, UTileNavComponent* TileNav, UTileNavComponent* OtherTileNav, FVector& OutLocation);
	static bool FindSectionPathInternal(const ATileNavigationData* NavGraph, const int32 Start, const int32 Goal, TArray<int32> &Path);
	static void FindPathInternal(const ATileNavigationData* NavGraph, const FPathFindingQuery Query, bool& bFound, TArray<FVector>& PathTiles);	
	static UTileNavComponent* GetTileNav(const ATileNavigationData* NavGraph, const int32 SectionIndex);
	void ApplyPathPruning(TArray<FVector>& Path, const FNavAgentProperties NavAgent) const; 

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	void DebugDrawTileNav();
	void DebugDrawSphere(FVector Location, float Radius, FColor Colour) const;
	void DebugDrawBox(FVector Location, FVector Extent, FColor Colour) const;
	void DebugDrawNumeric(FVector Location, FString String, FColor Colour) const;
	void FlushDebugDraw() const;
#endif

protected:
	static FPathFindingResult FindPath(const FNavAgentProperties& AgentProperties, const FPathFindingQuery& Query);
	ATileNavigationData(const FObjectInitializer& ObjectInitializer);
	virtual void PostInitProperties() override;
	virtual void BeginPlay() override;
	virtual void RebuildAll() override;
	FVector GetNearestNavTile(UTileNavComponent* TileNavComponent, FVector Location) const;
};
