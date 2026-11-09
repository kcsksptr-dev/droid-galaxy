# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Personal Unreal Engine **5.2.0** (prebuilt Linux binary) game. You are **KDS-3-21** (model "Pip"), an original
rolling ball droid (C++ `ARollerDroid`), crash-landed on a small spherical alien planet (`/Game/Maps/Planet`, the
only map): collect 5 ship parts guarded by hazards, install them, launch to the next planet. The project was called
`ForestGlade` until 2026-11-09 (it started as a forest map, since removed); `[CoreRedirects]` in
`DefaultEngine.ini` map the old `/Script/ForestGlade` class paths.
Engine: `~/unreal/engine/UE_5.2.0` (`~/dev/unreal` is a symlink to `~/unreal`).

## The level is generated, not hand-built

Everything in `Content/Alien` and the map come from a re-runnable pipeline. Change the scripts and re-run them
rather than hand-editing the map — the next build overwrites it.

1. `Tools/fetch_polyhaven.sh SourceAssets/polyhaven` — CC0 Poly Haven downloads: rock sets + boulder (recoloured
   alien rocks) and the ground maps `rocky_terrain_02` + `cracked_red_ground`.
2. `blender -b --factory-startup --python Tools/prep_assets.py -- .` — splits the rock sets into one mesh per rock,
   re-pivots the boulder (via `Tools/gltf_fix.py`).
3. `blender -b --factory-startup --python Tools/make_alien.py -- . [name ...]` — procedural, original models: the
   planet (icosphere, 327k tris, box-projected UVs), crystals, glow mushrooms, tube plants, 3 alien trees, spike
   grass, ship + flame, the 5 parts, gas giant + ring, cacti, thorn cluster, lava pool, basalt wall, vent, the 3
   power-ups. Also writes `SourceAssets/prepared/ship_sockets.json` (where installed parts sit on the ship); those
   numbers are hard-coded in `Source/DroidGalaxy/Public/MissionTypes.h`.
4. `python3 Tools/ue_remote.py Tools/build_alien.py` (editor open, ~3 min) — imports into `/Game/Alien`, builds
   materials (`M_AlienGround` with per-planet `TintA/TintB/CrackGlow`, `M_AlienRock`, `M_SpaceSky` unlit *Is Sky*,
   `M_GasGiant`, `M_GasRing`, `M_Beacon`, `M_Lava`, `MI_Alien_*` instances of `/Game/Characters/Pip/M_PipPaint`),
   sets collision, rebuilds the level (lighting, sky, planet actor + its mesh/material references, ship,
   PlayerStart, seeded scatter on the sphere). Existing meshes/materials are reused; delete one to rebuild it.
   To redo one material without rebuilding the level: set `unreal._alien_lib_only = True`, `exec` the file in the
   editor, delete the asset, call its function (e.g. `lava_material()`) and re-assign it.

`SourceAssets/` is git-ignored (re-created by steps 1–3). `Tools/planet.py` is the single shape function
(`radius_cm(dir)`, `surface(dir)`) used by Blender and by placement. Unreal Y = −Blender Y; `make_alien.py` applies
that flip (verified by line traces: heights match exactly).

## Interchange (5.2 glTF/texture import) quirks

- `AssetImportTask.destination_name` is ignored — assets are named after the source file stem.
- A texture shared by several glTF materials is imported once per material; `build_alien.alien_rocks` keeps one
  diffuse + normal per rock set in `/Game/Alien/Rocks/Textures` and deletes the rest.
- **Do not use the headless `-run=pythonscript` commandlet**: Interchange opens a results window after a glTF import
  and the commandlet crashes (no Slate). Build scripts run in the GUI editor.

## Driving the open editor

`bRemoteExecution=True` is set in `DefaultEngine.ini`; `python3 Tools/ue_remote.py file.py` or `-c "stmt"` runs
Python in the running editor (multi-line code must go in a file).
- **Wait for the asset registry after starting the editor** (`unreal.AssetRegistryHelpers.get_asset_registry()
  .is_loading_assets()` must be False) — otherwise `does_asset_exist` is False for existing assets and the scripts
  try to recreate them.
- After a C++ change that adds classes/properties, restart the editor before running `build_alien.py`.
- Screenshots: `Tools/ue_shot.py` (moves the viewport, `HighResShot`) → `Saved/Screenshots/LinuxEditor/`.
  5.2 Python cannot start a real PIE session.

## C++ (`Source/DroidGalaxy`)

Build (editor closed, or restart it afterwards):
`~/unreal/engine/UE_5.2.0/Engine/Build/BatchFiles/Linux/Build.sh DroidGalaxyEditor Linux Development -Project="$PWD/DroidGalaxy.uproject" -WaitMutex`

- `ARollerDroid` — physics sphere rolled by camera-relative torque; head placed every tick (`TG_PostPhysics`), faces
  travel, leans, looks at the camera when idle; glow MIDs pulse/blink. On a planet it disables engine gravity and
  applies its own toward the centre; the camera keeps its own tangent-frame heading (no pole flips). Legacy input
  via `UPlayerInput::AddEngineDefined*Mapping` (no Enhanced Input assets). Droid model:
  `Tools/make_droid.py` + `Tools/build_droid.py` → `/Game/Characters/Pip`.
- `AAlienPlanet` — planet mesh (complex collision), `UpAt()`, sun that follows the player (always afternoon),
  ground palette per planet, and at BeginPlay spawns (seeded by planet number, from the travel option
  `?Planet=N`): 5 `AShipPart`s, a challenge around each, wild hazards, power-ups. `TravelToNextPlanet()` reopens
  the map with N+1.
- `AHazard` — **Cactus** (solid, −15 + knockback), **Spikes** (roll-through thorns, −8 only above 230 cm/s, plus
  drag; Ctrl = careful mode 190 cm/s), **Lava** (−12 every 0.8 s + bounce), **Vent** (warning glow, then eruption:
  −18 and launched up), **Wall** (knee-high basalt, jump it). Challenges per part
  (`(partIndex + planet − 1) % 5`): lava moat, cactus thicket, thorn field, wall ring with vents, vent field.
  Hazards set `BlockAll` (WorldStatic) explicitly — runtime-spawned mesh components default to WorldDynamic.
- `APowerUp` — repair kit (+40 HP), shield (20 s bubble, blocks damage), blaster (30 s, LMB fires `ABlasterBolt`,
  shatters cacti/thorns/walls). A blaster and a shield always lie ~9 m from the ship.
- Health 100; 0.7 s invulnerability after a hit; at 0 HP "systems offline" → reboot at the ship, parts kept.
- `ASpaceship` — install mask, part meshes at the sockets, launch (flame, light, accelerate), travel after 8 s.
- `AMissionHUD` (canvas, no UMG) — objective, HP bar + power-up chips, whole-planet minimap (azimuthal equidistant
  around the droid, rim = far side; parts, ship, power-ups), inventory, prompts, damage flash, crosshair, banners.

**Self-test** (editor **closed** — both together exhaust the 8 GB GPU and crash):
`unreal "$PWD/DroidGalaxy.uproject" -game -windowed -ResX=1280 -ResY=720 -PipTest -log` — checks cactus, lava,
repair kit, careful/fast thorns, vent, shield, blaster, knockout + reboot (logs `[PipTest] after ...: hp=`), drives
~70° round the planet, collects all parts, installs, launches, arrives on planet 2, takes HighResShots
(`Saved/Screenshots/LinuxEditor/`), quits (~90 s). Console: `PipDrive <secs> <fwd> <right>`, `PipFire`.

## Running

- Editor: `unreal "$PWD/DroidGalaxy.uproject"` (wrapper in `~/.local/bin`, forces the NVIDIA GPU via `prime-run`).
- Play: Play button / Alt+P. WASD roll, mouse look, Shift boost, Ctrl careful, Space jump, E interact,
  LMB fire (with blaster), wheel zoom.

## Rendering setup (Config/DefaultEngine.ini)

Lumen GI + reflections, virtual shadow maps, no static lighting, TSR, Vulkan SM5. All alien meshes are non-Nanite
(small; and opaque glTF materials rendered black through Nanite in 5.2 once usage flags were persisted —
`Tools/fg_collision.py` persists `used_with_*` flags, needed for `-game`).

## Known issues

- Planet ground shows a faint straight shading seam in places (box-projected UVs / icosphere edges); cosmetic.
- Lava reads very bright/yellow-white under auto-exposure + bloom.

## Git

Unreal binaries (`*.uasset`, `*.umap`, images, glTF) go through Git LFS (`.gitattributes`). Needs `git-lfs`
installed (`sudo dnf install git-lfs && git lfs install`) before committing `Content/`.
