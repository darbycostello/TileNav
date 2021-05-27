#pragma once
#include "CoreMinimal.h"
#include "TileNavStructs.h"
#include "Components/ArrowComponent.h"

#include "TileNavComponent.generated.h"

UCLASS( ClassGroup=(TileNav), meta=(BlueprintSpawnableComponent) )
class TILENAV_API UTileNavComponent final : public UStaticMeshComponent {

	GENERATED_BODY()

public:
	UTileNavComponent(const FObjectInitializer& ObjectInitializer);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnComponentCreated() override;
	
	UPROPERTY()
	UArrowComponent* CollisionTraceNormalArrow;
	
	UPROPERTY()
	int32 SectionIndex;
	
	UPROPERTY()
	TMap<FVector, FTileNavTile> Tiles;

	UPROPERTY()
	TMap<FVector, FEdgeNeighbors> EdgeTiles;

	UPROPERTY()
	TArray<UPrimitiveComponent*> IgnoreComponents;

	// The distance between each tile
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "TileNav")
    float GridSize = 100.0f;

	// Inverse of the direction used to perform the line traces on this component
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "TileNav")
	FRotator CollisionTraceNormal = FRotator::ZeroRotator;
	
    void BuildNavigationTiles(float GridScale, float Clearance);
    void GetCollisionBounds(FBox& CollisionBounds) const;

	UFUNCTION(BlueprintCallable, Category = "TileNav")
	bool UpdateTile(FVector Tile, bool bWalkable);

	UFUNCTION(BlueprintCallable, Category = "TileNav")
	bool UpdateTiles(TArray<FVector> InTiles, bool bWalkable);

	UFUNCTION(BlueprintCallable, Category = "TileNav")
	void FindPath(const FVector Start, const FVector End, bool &bFound, TArray<FVector> &PathTiles);
};
