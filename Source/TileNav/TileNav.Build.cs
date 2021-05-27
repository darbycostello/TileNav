using UnrealBuildTool;

public class TileNav : ModuleRules {
	public TileNav(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange( new [] { "Core", "CoreUObject", "Engine", "InputCore", "NavigationSystem" } );
	}
}
