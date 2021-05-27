# TileNav for Unreal Engine

![TileNav banner image](https://user-images.githubusercontent.com/891532/119829126-d338a500-bef2-11eb-8d45-b4a05a9343a2.png)

Example Project (UE 4.26): [Download](https://drive.google.com/drive/folders/1tL3tclTDTVTMkBJIxiOCiS8wIammDJh8?usp=sharing)

TileNav is a replacement navigation system for UE4 which performs pathfinding across custom static mesh surfaces in any orientation. TileNav meshes are automatically linked together as sections when placed near one another, allowing the system to build a high-level connectivity graph for faster pathfinding across multiple meshes.

## Features

 - *ATileNavigationData* actor replaces the standard UE4 navigation system (and therefore does not use Recast). Easily set up as the preferred agent within the project settings.
 - Supports *SimpleMoveToLocation* and other built-in pathfinding queries.
 - *ATileNavComponent* comprises a single static mesh which is used to create a grid of collision checks in order to build a tiled navigation section.
 - Adjustable collision normals for each TileNav component, allowing you to perform tile collision detections from different angles.
 - Variable grid size per TileNav component allows for different tile densities per instance, with a global multiplier built into the main navigation data actor.
 - Supports partial paths when pathfinding across multiple sections.
 - Path pruning using line-of-sight checks to produce most direct path (best used in walled or enclosed spaces).
 - A variety of options available for debug drawing and logging.

## Basic Usage
- After installing the plugin, open the Project Settings window and go to Engine --> Navigation System.
 - In the Agents section, add an item to the **Supported Agents** list then set the **Nav Data Class** to *TileNavigationData*. You should ensure that this class is also set in the **Preferred Nav Data** item and that at least the **Can Walk** option is checked.
- Add a **NavMeshBoundsVolume** actor to the world and this will automatically create an instance of the **TileNavigationData** actor.

Add a **TileNav** component to any actor and set its static mesh to the collision surface you wish to use. Typically this should be a planar mesh like a quad, but irregular shapes are fine (e.g. corridor floor shapes or bumpy terrain), as is a small amount of curvature. Note that your static mesh must have some form of collision geometry, so either ensure this on import or set it to use *Complex collision as Simple* in the settings window for the imported static mesh.

The red arrow gizmo on the component illustrates the normal of the collision surface. By default this will match the component's *Up* vector, therefore collision traces will be performed in the inverse direction to it, (e.g. down relative to the component's transform). You can modify this by adjusting the **Collision Trace Normal** if required.

Multiple *TileNav* components can be added to a single actor, or added to separate actors in the world and they can all interlink, so long as the **Component Proximity** and **Tile Proximity** parameters are set in the *TileNavigationData* actor. Use the **Debug** tools in this actor to analyse how your various *TileNav* components fit together. Selecting **Build Paths** from the *Build* toolbar menu will trigger a full navigation rebuild, as will modifying any *TileNavigationData* actor *TileNav* settings.

## Limitations and Future Work

 - Currently there is no solid support for dynamic navigation. It should be straightforward to add and should carry minimal performance penalty as only affected *TileNav* components can be rebuilt instead of a full rebuild. Coming soon.
 - Collision traces are only performed with planar mapping, so they won't follow a curved surface, like a sphere. It would still be possible to build a sphere as separate interlinked components, each comprising a reasonably flat segment of the sphere mesh, then rotated appropriately to create the whole tiled surface. Obviously this would be easier with a quadrilateralized or 'cube sphere'. In future I may look at building in some additional projection methods.
 - There is no serialization of navigation data, so it must be rebuilt in editor or on begin play (performed automatically currently). I'll be getting to this when time allows.
