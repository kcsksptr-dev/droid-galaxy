"""Unreal editor Python: build the alien planet level /Game/Maps/Planet.

Run with the editor open:  python3 Tools/ue_remote.py Tools/build_alien.py
Inputs: SourceAssets/prepared/<planet, alien_*, ship*, part_*, gas_*> (Tools/make_alien.py) and the Poly Haven
ground maps rocky_terrain_02 + cracked_red_ground (Tools/fetch_polyhaven.sh).
Re-runnable: meshes already imported into /Game/Alien are reused (delete a folder to re-import), materials are
re-created only when missing (delete to rebuild), the level is rebuilt from scratch with a fixed seed.

C++ side: AAlienPlanet (gravity, sun, part spawning), ASpaceship, AShipPart, AMissionHUD, ARollerDroid.
"""
import importlib
import math
import os
import random
import sys

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
sys.path.insert(0, os.path.join(PROJECT, "Tools"))
import planet  # noqa: E402
import fg_collision  # noqa: E402
importlib.reload(planet)
importlib.reload(fg_collision)

PREP = os.path.join(PROJECT, "SourceAssets", "prepared")
PH = os.path.join(PROJECT, "SourceAssets", "polyhaven")
ALIEN = "/Game/Alien"
MATS = f"{ALIEN}/Materials"
MAP = "/Game/Maps/Planet"
SEED = 2321
R = planet.RADIUS_CM

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
P = unreal.MaterialProperty
S = unreal.MaterialSamplerType


def log(msg):
    unreal.log(f"[Alien] {msg}")


def setp(obj, names, value):
    for n in ([names] if isinstance(names, str) else names):
        try:
            obj.set_editor_property(n, value)
            return True
        except Exception:
            continue
    unreal.log_warning(f"[Alien] {obj.get_class().get_name()}: none of {names} exist, skipped")
    return False


def lc(rgb, a=1.0):
    return unreal.LinearColor(r=rgb[0], g=rgb[1], b=rgb[2], a=a)


# --------------------------------------------------------------------------- import

def import_file(path, dest):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = dest
    task.automated = True
    task.replace_existing = True
    task.save = False
    asset_tools.import_asset_tasks([task])


def mesh_of(name):
    """Import SourceAssets/prepared/<name> into /Game/Alien/<name> once; return its StaticMesh."""
    dest = f"{ALIEN}/{name}"
    path = f"{dest}/{name}"
    if not eal.does_asset_exist(path):
        log(f"importing {name} ...")
        import_file(os.path.join(PREP, name, f"{name}.gltf"), dest)
    m = eal.load_asset(path)
    if not isinstance(m, unreal.StaticMesh):
        found = [eal.load_asset(p) for p in eal.list_assets(dest, include_folder=False)]
        found = [a for a in found if isinstance(a, unreal.StaticMesh)]
        if not found:
            raise RuntimeError(f"no StaticMesh imported for {name}")
        m = found[0]
    ns = m.get_editor_property("nanite_settings")
    if ns.enabled:   # small meshes; Nanite also misrenders these glTF materials in 5.2 (see CLAUDE.md)
        ns.enabled = False
        m.set_editor_property("nanite_settings", ns)
    eal.save_directory(dest, only_if_is_dirty=False, recursive=True)
    return m


def import_texture(layer, map_name, dest, srgb, comp):
    stem = f"{layer}_{map_name}_2k"
    path = f"{dest}/{stem}"
    if not eal.does_asset_exist(path):
        import_file(os.path.join(PH, layer, f"{stem}.jpg"), dest)
    t = eal.load_asset(path)
    t.set_editor_property("srgb", srgb)
    t.set_editor_property("compression_settings", comp)
    return t


# --------------------------------------------------------------------------- materials

class Graph:
    """Tiny helper for building material graphs."""

    def __init__(self, mat):
        self.m = mat
        self.y = 0

    def e(self, cls, x=-600, **props):
        self.y += 140
        n = mel.create_material_expression(self.m, cls, x, self.y)
        for k, v in props.items():
            setp(n, k, v)
        return n

    def link(self, a, b, inp="", out=""):
        mel.connect_material_expressions(a, out, b, inp)
        return b

    def op(self, cls, a, b, a_out="", b_out="", x=-400):
        n = self.e(cls, x)
        self.link(a, n, "A", a_out)
        self.link(b, n, "B", b_out)
        return n

    def const(self, v, x=-900):
        return self.e(unreal.MaterialExpressionConstant, x, r=float(v))

    def vec(self, name, rgb, x=-900):
        return self.e(unreal.MaterialExpressionVectorParameter, x, parameter_name=name, default_value=lc(rgb))

    def lerp(self, a, b, alpha, a_out="", b_out="", alpha_out=""):
        n = self.e(unreal.MaterialExpressionLinearInterpolate, -300)
        self.link(a, n, "A", a_out)
        self.link(b, n, "B", b_out)
        self.link(alpha, n, "Alpha", alpha_out)
        return n

    def out(self, node, prop, node_out=""):
        mel.connect_material_property(node, node_out, prop)


def new_material(name, folder=MATS):
    path = f"{folder}/{name}"
    if eal.does_asset_exist(path):
        return None, eal.load_asset(path)
    m = asset_tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    return Graph(m), m


def finish(m):
    setp(m, "used_with_instanced_static_meshes", True)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def ground_material():
    g, m = new_material("M_AlienGround")
    if g is None:
        return m
    dest = f"{ALIEN}/Ground"
    C = unreal.TextureCompressionSettings
    layers = []
    for layer in ("rocky_terrain_02", "cracked_red_ground"):
        layers.append({
            "d": import_texture(layer, "Diffuse", dest, True, C.TC_DEFAULT),
            "n": import_texture(layer, "nor_gl", dest, False, C.TC_NORMALMAP),
            "a": import_texture(layer, "arm", dest, False, C.TC_MASKS)})
    uv = g.e(unreal.MaterialExpressionTextureCoordinate, -1600)
    uv_b = g.e(unreal.MaterialExpressionTextureCoordinate, -1600, u_tiling=0.7, v_tiling=0.7)
    smp = []
    for i, tex in enumerate(layers):
        coords = uv if i == 0 else uv_b
        row = {}
        for key, kind in (("d", S.SAMPLERTYPE_COLOR), ("n", S.SAMPLERTYPE_NORMAL), ("a", S.SAMPLERTYPE_MASKS)):
            s = g.e(unreal.MaterialExpressionTextureSample, -1300, texture=tex[key], sampler_type=kind)
            g.link(coords, s, "UVs")
            row[key] = s
        smp.append(row)

    def tinted(sample, param, rgb, gain):
        de = g.e(unreal.MaterialExpressionDesaturation, -1000)
        g.link(sample, de, "", "RGB")
        g.link(g.const(0.85), de, "Fraction")
        t = g.op(unreal.MaterialExpressionMultiply, de, g.vec(param, rgb))
        return g.op(unreal.MaterialExpressionMultiply, t, g.const(gain))

    col_a = tinted(smp[0]["d"], "TintA", (0.55, 0.45, 0.78), 1.9)
    col_b = tinted(smp[1]["d"], "TintB", (0.95, 0.38, 0.22), 1.7)

    # lowlands (crater floors) get layer B, broken up by world-space noise
    wp = g.e(unreal.MaterialExpressionWorldPosition, -1600)
    dist = g.op(unreal.MaterialExpressionDistance, wp, g.e(unreal.MaterialExpressionConstant3Vector, -1300))
    low = g.op(unreal.MaterialExpressionSubtract, g.const(R - 25), dist)
    low = g.op(unreal.MaterialExpressionMultiply, low, g.const(1 / 60.0))
    noise = g.e(unreal.MaterialExpressionNoise, -1000, scale=0.0025, levels=3, output_min=-0.9, output_max=0.7)
    g.link(wp, noise, "Position")
    mask = g.e(unreal.MaterialExpressionSaturate, -500)
    g.link(g.op(unreal.MaterialExpressionAdd, low, noise), mask)

    # large-scale brightness variation
    noise2 = g.e(unreal.MaterialExpressionNoise, -1000, scale=0.0007, levels=2, output_min=0.75, output_max=1.2)
    g.link(wp, noise2, "Position")
    base = g.op(unreal.MaterialExpressionMultiply, g.lerp(col_a, col_b, mask), noise2)
    g.out(base, P.MP_BASE_COLOR)
    g.out(g.lerp(smp[0]["n"], smp[1]["n"], mask, "RGB", "RGB"), P.MP_NORMAL)
    g.out(g.lerp(smp[0]["a"], smp[1]["a"], mask, "G", "G"), P.MP_ROUGHNESS)
    g.out(g.lerp(smp[0]["a"], smp[1]["a"], mask, "R", "R"), P.MP_AMBIENT_OCCLUSION)

    # glowing cracks in layer B: low ambient occlusion = deep crack
    crack = g.op(unreal.MaterialExpressionSubtract, g.const(0.6), smp[1]["a"], b_out="R")
    crack = g.op(unreal.MaterialExpressionMultiply, crack, g.const(3.0))
    crack_s = g.e(unreal.MaterialExpressionSaturate, -500)
    g.link(crack, crack_s)
    crack2 = g.op(unreal.MaterialExpressionMultiply, crack_s, crack_s)
    glow = g.op(unreal.MaterialExpressionMultiply, crack2, mask)
    glow = g.op(unreal.MaterialExpressionMultiply, glow, g.vec("CrackGlow", (0.1, 1.0, 0.8)))
    g.out(g.op(unreal.MaterialExpressionMultiply, glow, g.const(6.0)), P.MP_EMISSIVE_COLOR)
    return finish(m)


def rock_material():
    """Alien recolour of the Poly Haven rocks: desaturated diffuse x tint, their normal map, matte."""
    g, m = new_material("M_AlienRock")
    if g is None:
        return m
    d = g.e(unreal.MaterialExpressionTextureSampleParameter2D, -1200, parameter_name="Diffuse",
            sampler_type=S.SAMPLERTYPE_COLOR)
    n = g.e(unreal.MaterialExpressionTextureSampleParameter2D, -1200, parameter_name="Normal",
            sampler_type=S.SAMPLERTYPE_NORMAL)
    de = g.e(unreal.MaterialExpressionDesaturation, -900)
    g.link(d, de, "", "RGB")
    g.link(g.const(0.9), de, "Fraction")
    t = g.op(unreal.MaterialExpressionMultiply, de, g.vec("Tint", (0.5, 0.42, 0.62)))
    g.out(g.op(unreal.MaterialExpressionMultiply, t, g.const(1.8)), P.MP_BASE_COLOR)
    g.out(n, P.MP_NORMAL, "RGB")
    g.out(g.const(0.78), P.MP_ROUGHNESS)
    return finish(m)


def sky_material():
    """Space: dark nebula gradient plus a star field, as an unlit 'Is Sky' material (lights the SkyLight)."""
    g, m = new_material("M_SpaceSky")
    if g is None:
        return m
    setp(m, "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    setp(m, "two_sided", True)
    setp(m, "is_sky", True)
    cam = g.e(unreal.MaterialExpressionCameraVectorWS, -1500)
    d = g.op(unreal.MaterialExpressionMultiply, cam, g.const(-1.0))   # view direction
    neb = g.e(unreal.MaterialExpressionNoise, -1100, scale=1.6, levels=4, output_min=0.0, output_max=1.0)
    g.link(d, neb, "Position")
    neb2 = g.e(unreal.MaterialExpressionNoise, -1100, scale=3.1, levels=3, output_min=0.0, output_max=1.0)
    g.link(g.op(unreal.MaterialExpressionAdd, d, g.e(unreal.MaterialExpressionConstant3Vector, -1300,
                                                     constant=lc((3.1, 1.7, 0.4)))), neb2, "Position")
    c = g.lerp(g.vec("SpaceColor", (0.001, 0.001, 0.004)), g.vec("NebulaA", (0.03, 0.006, 0.045)), neb)
    c = g.lerp(c, g.vec("NebulaB", (0.0, 0.018, 0.025)),
               g.op(unreal.MaterialExpressionMultiply, neb2, neb2))
    # stars: voronoi cells, bright only very near the cell point, brightness varied by a second noise
    vor = g.e(unreal.MaterialExpressionNoise, -1100, scale=350.0, levels=1, output_min=0.0, output_max=1.0,
              noise_function=unreal.NoiseFunction.NOISEFUNCTION_VORONOI_ALU, quality=1)
    g.link(d, vor, "Position")
    star = g.op(unreal.MaterialExpressionSubtract, g.const(0.035), vor)
    star = g.op(unreal.MaterialExpressionMultiply, star, g.const(28.0))
    star_s = g.e(unreal.MaterialExpressionSaturate, -500)
    g.link(star, star_s)
    star = g.op(unreal.MaterialExpressionMultiply, star_s, star_s)
    star = g.op(unreal.MaterialExpressionMultiply, star, star)
    bright = g.e(unreal.MaterialExpressionNoise, -1100, scale=60.0, levels=1, output_min=-1.5, output_max=1.5)
    g.link(d, bright, "Position")
    bright_s = g.e(unreal.MaterialExpressionSaturate, -500)
    g.link(bright, bright_s)
    star = g.op(unreal.MaterialExpressionMultiply, star, bright_s)
    star = g.op(unreal.MaterialExpressionMultiply, star, g.const(12.0))
    g.out(g.op(unreal.MaterialExpressionAdd, c, star), P.MP_EMISSIVE_COLOR)
    return finish(m)


def gas_giant_material():
    g, m = new_material("M_GasGiant")
    if g is None:
        return m
    uv = g.e(unreal.MaterialExpressionTextureCoordinate, -1500)
    v = g.e(unreal.MaterialExpressionComponentMask, -1300, r=False, g=True, b=False, a=False)
    g.link(uv, v)
    wp = g.e(unreal.MaterialExpressionWorldPosition, -1500)
    turb = g.e(unreal.MaterialExpressionNoise, -1100, scale=0.0006, levels=4, output_min=-0.6, output_max=0.6)
    g.link(wp, turb, "Position")
    bands = g.e(unreal.MaterialExpressionSine, -800, period=0.07)
    g.link(g.op(unreal.MaterialExpressionAdd, v, g.op(unreal.MaterialExpressionMultiply, turb, g.const(0.03))), bands)
    bands2 = g.e(unreal.MaterialExpressionSine, -800, period=0.23)
    g.link(g.op(unreal.MaterialExpressionAdd, v, g.op(unreal.MaterialExpressionMultiply, turb, g.const(0.05))), bands2)

    def unit(x):   # -1..1 -> 0..1
        return g.op(unreal.MaterialExpressionAdd, g.op(unreal.MaterialExpressionMultiply, x, g.const(0.5)), g.const(0.5))

    c = g.lerp(g.vec("BandA", (0.55, 0.25, 0.12)), g.vec("BandB", (0.9, 0.75, 0.55)), unit(bands))
    c = g.lerp(c, g.vec("BandC", (0.45, 0.2, 0.45)), unit(bands2))
    g.out(c, P.MP_BASE_COLOR)
    g.out(g.const(0.9), P.MP_ROUGHNESS)
    return finish(m)


def ring_material():
    g, m = new_material("M_GasRing")
    if g is None:
        return m
    setp(m, "blend_mode", unreal.BlendMode.BLEND_MASKED)
    setp(m, "two_sided", True)
    uv = g.e(unreal.MaterialExpressionTextureCoordinate, -1500)
    u = g.e(unreal.MaterialExpressionComponentMask, -1300, r=True, g=False, b=False, a=False)
    g.link(uv, u)
    s1 = g.e(unreal.MaterialExpressionSine, -900, period=0.11)
    g.link(u, s1)
    s2 = g.e(unreal.MaterialExpressionSine, -900, period=0.037)
    g.link(u, s2)
    g.out(g.lerp(g.vec("RingA", (0.6, 0.5, 0.45)), g.vec("RingB", (0.3, 0.25, 0.35)), s1), P.MP_BASE_COLOR)
    gaps = g.op(unreal.MaterialExpressionAdd, g.op(unreal.MaterialExpressionMultiply, s2, g.const(0.6)), g.const(0.75))
    g.out(gaps, P.MP_OPACITY_MASK)
    g.out(g.const(0.85), P.MP_ROUGHNESS)
    return finish(m)


def lava_material():
    """Molten rock: two drifting world-space noises -> dark crust through orange to yellow-white, emissive."""
    g, m = new_material("M_Lava")
    if g is None:
        return m
    wp = g.e(unreal.MaterialExpressionWorldPosition, -1600)
    t = g.e(unreal.MaterialExpressionTime, -1600)
    drift = g.op(unreal.MaterialExpressionMultiply, t, g.e(unreal.MaterialExpressionConstant3Vector, -1400,
                                                             constant=lc((14.0, 9.0, 6.0))))
    n1 = g.e(unreal.MaterialExpressionNoise, -1000, scale=0.012, levels=3, output_min=0.0, output_max=1.0)
    g.link(g.op(unreal.MaterialExpressionAdd, wp, drift), n1, "Position")
    n2 = g.e(unreal.MaterialExpressionNoise, -1000, scale=0.035, levels=2, output_min=0.0, output_max=1.0)
    g.link(g.op(unreal.MaterialExpressionSubtract, wp, drift), n2, "Position")
    heat = g.op(unreal.MaterialExpressionMultiply, n1, n2)
    heat = g.op(unreal.MaterialExpressionMultiply, heat, g.const(2.2))
    heat_s = g.e(unreal.MaterialExpressionSaturate, -500)
    g.link(heat, heat_s)
    col = g.lerp(g.vec("Crust", (0.25, 0.02, 0.0)), g.vec("Hot", (1.0, 0.42, 0.05)), heat_s)
    col = g.lerp(col, g.vec("White", (1.0, 0.85, 0.4)), g.op(unreal.MaterialExpressionMultiply, heat_s, heat_s))
    g.out(g.const(0.0), P.MP_BASE_COLOR)
    g.out(g.op(unreal.MaterialExpressionMultiply, col, g.const(3.5)), P.MP_EMISSIVE_COLOR)
    g.out(g.const(0.6), P.MP_ROUGHNESS)
    return finish(m)


def beacon_material():
    g, m = new_material("M_Beacon")
    if g is None:
        return m
    setp(m, "shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    setp(m, "blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    setp(m, "two_sided", True)
    g.out(g.op(unreal.MaterialExpressionMultiply, g.vec("Color", (0.2, 1, 0.9)), g.const(0.6)), P.MP_EMISSIVE_COLOR)
    return finish(m)


# slot -> (base colour, metallic, roughness, emissive colour, emissive strength); instances of M_PipPaint
PAINT = {
    "RockDark":   ((0.06, 0.05, 0.08), 0.0, 0.6, (0, 0, 0), 0.0),
    "GlowPink":   ((1.0, 0.3, 0.8), 0.0, 0.2, (1.0, 0.22, 0.7), 5.0),
    "GlowCyan":   ((0.2, 1.0, 0.95), 0.0, 0.2, (0.15, 1.0, 0.9), 5.0),
    "GlowLime":   ((0.6, 1.0, 0.2), 0.0, 0.3, (0.55, 1.0, 0.15), 6.0),
    "GlowPod":    ((1.0, 0.5, 0.15), 0.0, 0.3, (1.0, 0.45, 0.1), 7.0),
    "GlowOrange": ((1.0, 0.45, 0.1), 0.0, 0.3, (1.0, 0.4, 0.08), 8.0),
    "GlowViolet": ((0.7, 0.35, 1.0), 0.0, 0.1, (0.65, 0.3, 1.0), 10.0),
    "Stalk":      ((0.75, 0.7, 0.85), 0.0, 0.5, (0.3, 0.2, 0.5), 0.4),
    "Cap":        ((0.25, 0.08, 0.45), 0.0, 0.4, (0, 0, 0), 0.0),
    "TubeSkin":   ((0.1, 0.45, 0.35), 0.0, 0.45, (0, 0, 0), 0.0),
    "TubeInner":  ((0.02, 0.02, 0.03), 0.0, 0.8, (0, 0, 0), 0.0),
    "Bark":       ((0.12, 0.06, 0.18), 0.0, 0.7, (0, 0, 0), 0.0),
    "Leaf":       ((0.65, 0.15, 0.55), 0.0, 0.5, (0.4, 0.05, 0.3), 0.3),
    "Blade":      ((0.32, 0.25, 0.78), 0.0, 0.6, (0.15, 0.1, 0.5), 0.35),
    "ShipHull":   ((0.85, 0.86, 0.88), 0.3, 0.35, (0, 0, 0), 0.0),
    "ShipTrim":   ((0.95, 0.45, 0.1), 0.0, 0.4, (0, 0, 0), 0.0),
    "ShipDark":   ((0.05, 0.05, 0.06), 0.7, 0.35, (0, 0, 0), 0.0),
    "ShipGlass":  ((0.02, 0.05, 0.1), 0.0, 0.05, (0.1, 0.4, 0.8), 1.0),
    "Flame":      ((1.0, 0.6, 0.2), 0.0, 0.5, (1.0, 0.55, 0.2), 40.0),
    "CactusSkin": ((0.07, 0.3, 0.2), 0.0, 0.55, (0, 0, 0), 0.0),
    "Spine":      ((1.0, 0.85, 0.5), 0.0, 0.4, (1.0, 0.3, 0.1), 1.5),
    "SpikeDark":  ((0.04, 0.02, 0.05), 0.6, 0.25, (0, 0, 0), 0.0),
    "GlowRed":    ((1.0, 0.1, 0.05), 0.0, 0.3, (1.0, 0.08, 0.03), 8.0),
    "Crust":      ((0.03, 0.02, 0.02), 0.0, 0.8, (0.3, 0.05, 0.0), 0.3),
    "Basalt":     ((0.09, 0.08, 0.11), 0.1, 0.7, (0, 0, 0), 0.0),
    "VentRock":   ((0.12, 0.07, 0.06), 0.0, 0.85, (0, 0, 0), 0.0),
    "GlowGreen":  ((0.3, 1.0, 0.4), 0.0, 0.3, (0.25, 1.0, 0.35), 8.0),
    "GlowBlue":   ((0.3, 0.6, 1.0), 0.0, 0.2, (0.25, 0.55, 1.0), 8.0),
}


def paint_instance(slot, master):
    name = f"MI_Alien_{slot}"
    path = f"{MATS}/{name}"
    mi = eal.load_asset(path) if eal.does_asset_exist(path) else asset_tools.create_asset(
        name, MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    base, metal, rough, emis, strength = PAINT[slot]
    mel.set_material_instance_vector_parameter_value(mi, "BaseColor", lc(base))
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metal)
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    mel.set_material_instance_vector_parameter_value(mi, "EmissiveColor", lc(emis))
    mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", strength)
    eal.save_loaded_asset(mi)
    return mi


def assign_paint(mesh, master, special=None):
    for i, sm in enumerate(mesh.get_editor_property("static_materials")):
        slot = str(sm.get_editor_property("material_slot_name"))
        key = slot.replace("M_Alien_", "")
        key = next((k for k in list(PAINT) + list(special or {}) if key.startswith(k)), None)
        if key is None:
            unreal.log_warning(f"[Alien] {mesh.get_name()}: unknown slot {slot}")
            continue
        mesh.set_material(i, special[key] if special and key in special else paint_instance(key, master))
    eal.save_loaded_asset(mesh)


def set_collision(mesh, mode):
    bs = mesh.get_editor_property("body_setup")
    ag = bs.get_editor_property("agg_geom")
    for k in ("convex_elems", "box_elems", "sphere_elems", "sphyl_elems"):
        ag.set_editor_property(k, [])
    bs.set_editor_property("agg_geom", ag)
    bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE
                           if mode == "complex" else unreal.CollisionTraceFlag.CTF_USE_DEFAULT)
    mesh.set_editor_property("body_setup", bs)
    eal.save_loaded_asset(mesh)


def alien_rocks(rock_mat):
    """Poly Haven rocks (SourceAssets/prepared/rock_moss_set_*, boulder_01) recoloured with M_AlienRock.

    Meshes: /Game/Alien/Rocks/SM_Alien_<name> (complex collision). One diffuse + normal texture per rock set in
    /Game/Alien/Rocks/Textures; everything else Interchange imports (its own materials, duplicate textures) is
    deleted after the import.
    """
    dest, texdir = f"{ALIEN}/Rocks", f"{ALIEN}/Rocks/Textures"
    for folder in (dest, texdir):
        if not eal.does_directory_exist(folder):
            eal.make_directory(folder)
    out = []
    names = sorted(n for n in os.listdir(PREP) if n.startswith("rock_moss_set_") or n == "boulder_01")
    for name in names:
        set_ = "boulder_01" if name == "boulder_01" else name[:len("rock_moss_set_0X")]
        path = f"{dest}/SM_Alien_{name}"
        if not eal.does_asset_exist(path):
            tmp = f"{dest}/Import/{name}"
            import_file(os.path.join(PREP, name, f"{name}.gltf"), tmp)
            assets = [eal.load_asset(p) for p in eal.list_assets(tmp, recursive=True, include_folder=False)]
            mesh = next(a for a in assets if isinstance(a, unreal.StaticMesh))
            for t in assets:
                if isinstance(t, unreal.Texture2D):
                    for kind in ("diff", "nor_gl"):
                        stem = f"{set_}_{kind}_2k"
                        if t.get_name().startswith(stem) and not eal.does_asset_exist(f"{texdir}/{stem}"):
                            eal.rename_asset(t.get_path_name().split(".")[0], f"{texdir}/{stem}")
            eal.rename_asset(mesh.get_path_name().split(".")[0], path)
            m = eal.load_asset(path)
            ns = m.get_editor_property("nanite_settings")
            if ns.enabled:
                ns.enabled = False
                m.set_editor_property("nanite_settings", ns)
            set_collision(m, "complex")
        mesh = eal.load_asset(path)
        mi_path = f"{MATS}/MI_AlienRock_{name}"
        mi = eal.load_asset(mi_path) if eal.does_asset_exist(mi_path) else asset_tools.create_asset(
            f"MI_AlienRock_{name}", MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, rock_mat)
        mel.set_material_instance_texture_parameter_value(mi, "Diffuse", eal.load_asset(f"{texdir}/{set_}_diff_2k"))
        mel.set_material_instance_texture_parameter_value(mi, "Normal", eal.load_asset(f"{texdir}/{set_}_nor_gl_2k"))
        mel.update_material_instance(mi)
        eal.save_loaded_asset(mi)
        for i in range(len(mesh.get_editor_property("static_materials"))):
            mesh.set_material(i, mi)
        eal.save_loaded_asset(mesh)
        out.append(mesh)
    if eal.does_directory_exist(f"{dest}/Import"):
        eal.delete_directory(f"{dest}/Import")
    eal.save_directory(dest, only_if_is_dirty=False, recursive=True)
    log(f"alien rocks: {len(out)}")
    return out


# --------------------------------------------------------------------------- level

def fresh_level():
    if eal.does_asset_exist(MAP):
        levels.load_level(MAP)
        actors.destroy_actors(actors.get_all_level_actors())
    else:
        levels.new_level(MAP)


def spawn_class(cls, loc=(0, 0, 0), rot=None, folder="Lighting", label=None):
    a = actors.spawn_actor_from_class(cls, unreal.Vector(*loc), rot or unreal.Rotator())
    a.set_folder_path(folder)
    if label:
        a.set_actor_label(label)
    return a


def spawn_mesh(mesh, loc, rot, scale, folder, collide=True, cull=0.0, shadow=True):
    a = actors.spawn_actor_from_object(mesh, unreal.Vector(*loc), rot)
    a.set_actor_scale3d(unreal.Vector(scale, scale, scale) if isinstance(scale, (int, float)) else unreal.Vector(*scale))
    a.set_folder_path(folder)
    c = a.static_mesh_component
    if not collide:
        c.set_collision_profile_name("NoCollision")
    if cull:
        setp(c, "ld_max_draw_distance", cull)
    if not shadow:
        c.set_cast_shadow(False)
    return a


def rot_up(up, rng):
    """Rotator whose Z axis is `up`, with a random heading."""
    upv = unreal.Vector(*up)
    ref = unreal.Vector(rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1))
    x = ref - upv * ref.dot(upv)
    return unreal.MathLibrary.make_rot_from_zx(upv, x.normal())


def rand_dir(rng):
    while True:
        v = (rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1))
        l = math.sqrt(sum(c * c for c in v))
        if l > 1e-6:
            return tuple(c / l for c in v)


def norm(v):
    l = math.sqrt(sum(c * c for c in v))
    return tuple(c / l for c in v)


CLEAR_ANGLE = 0.2   # keep the landing site (~9 m) free of big things


def sphere_scatter(rng, meshes, count, scale, spacing, folder, sink=0.0, align=1.0, collide=True, cull=0.0,
                   instanced=False, avoid=None, clear=CLEAR_ANGLE, shadow=True):
    """Seeded rejection sampling over the whole planet. align: 0 = stand radially (trees), 1 = on the surface normal."""
    cell = max(spacing, 1.0)
    grid = {}
    items, tries = [], 0
    while len(items) < count and tries < count * 40:
        tries += 1
        d = rand_dir(rng)
        if math.acos(max(-1, min(1, d[2]))) < clear:
            continue
        p, n = planet.surface(d)
        key = tuple(int(c // cell) for c in p)
        close = False
        for gx in (key[0] - 1, key[0], key[0] + 1):
            for gy in (key[1] - 1, key[1], key[1] + 1):
                for gz in (key[2] - 1, key[2], key[2] + 1):
                    for q in grid.get((gx, gy, gz), ()):
                        if sum((a - b) ** 2 for a, b in zip(p, q)) < spacing * spacing:
                            close = True
        if close or (avoid and any(sum((a - b) ** 2 for a, b in zip(p, q)) < r * r for q, r in avoid)):
            continue
        grid.setdefault(key, []).append(p)
        up = norm(tuple(dd * (1 - align) + nn * align for dd, nn in zip(d, n)))
        mesh = rng.choice(meshes)
        s = rng.uniform(*scale)
        height = mesh.get_bounds().box_extent.z * 2 * s
        loc = tuple(pc - uc * (sink * height + 3) for pc, uc in zip(p, up))
        items.append((mesh, loc, rot_up(up, rng), s, p))

    if instanced:
        holders = {}
        for mesh, loc, rot, s, _ in items:
            holders.setdefault(mesh.get_path_name(), (mesh, []))[1].append(
                unreal.Transform(location=unreal.Vector(*loc), rotation=rot, scale=unreal.Vector(s, s, s)))
        for mesh, xf in holders.values():
            comp = instanced_holder(mesh, folder, f"HISM_{mesh.get_name()}", collide, cull)
            comp.add_instances(xf, False, True)
    else:
        for mesh, loc, rot, s, _ in items:
            spawn_mesh(mesh, loc, rot, s, folder, collide=collide, cull=cull, shadow=shadow)
    log(f"placed {len(items)}/{count} in {folder}")
    return [it[4] for it in items]


def instanced_holder(mesh, folder, label, collide, cull):
    holder = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    holder.set_folder_path(folder)
    holder.set_actor_label(label)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    root = sds.k2_gather_subobject_data_for_instance(holder)[0]
    params = unreal.AddNewSubobjectParams(parent_handle=root,
                                          new_class=unreal.HierarchicalInstancedStaticMeshComponent)
    handle, fail_reason = sds.add_new_subobject(params)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    comp = lib.get_object(lib.get_data(handle))
    if comp is None:
        raise RuntimeError(f"could not add HISM component: {fail_reason}")
    comp.set_static_mesh(mesh)
    if not collide:
        comp.set_collision_profile_name("NoCollision")
    if cull:
        setp(comp, "instance_start_cull_distance", int(cull * 0.8))
        setp(comp, "instance_end_cull_distance", int(cull))
    comp.set_cast_shadow(False)
    return comp


def lighting(sky_mat):
    sun = spawn_class(unreal.DirectionalLight, (0, 0, R + 2000), unreal.Rotator(pitch=-50, yaw=30), label="Sun")
    lc_ = sun.get_component_by_class(unreal.DirectionalLightComponent)
    setp(sun.root_component, "mobility", unreal.ComponentMobility.MOVABLE)
    setp(lc_, "mobility", unreal.ComponentMobility.MOVABLE)
    setp(lc_, "intensity", 7.0)
    setp(lc_, "light_color", unreal.Color(r=255, g=238, b=225, a=255))

    sky = spawn_class(unreal.SkyLight, (0, 0, R + 1000), label="SkyLight")
    sc = sky.get_component_by_class(unreal.SkyLightComponent)
    setp(sc, "mobility", unreal.ComponentMobility.MOVABLE)
    setp(sc, "real_time_capture", True)
    setp(sc, ["lower_hemisphere_is_black", "lower_hemisphere_is_solid_color"], False)
    setp(sc, "intensity", 3.0)

    dome = spawn_mesh(eal.load_asset("/Engine/EngineSky/SM_SkySphere"), (0, 0, 0), unreal.Rotator(), 1.0,
                      "Lighting", collide=False, shadow=False)
    b = dome.static_mesh_component.static_mesh.get_bounds().box_extent
    k = 300000.0 / max(b.x, 1)   # ~3 km radius: encloses the gas giant
    dome.set_actor_scale3d(unreal.Vector(k, k, k))
    dome.static_mesh_component.set_material(0, sky_mat)
    dome.set_actor_label("SpaceSky")

    ppv = spawn_class(unreal.PostProcessVolume, label="PostProcess")
    setp(ppv, ["unbound", "b_unbound"], True)
    pp = ppv.get_editor_property("settings")
    for k_, v in (("override_auto_exposure_bias", True), ("auto_exposure_bias", 0.3),
                  ("override_auto_exposure_min_brightness", True), ("auto_exposure_min_brightness", 0.6),
                  ("override_auto_exposure_max_brightness", True), ("auto_exposure_max_brightness", 3.0),
                  ("override_bloom_intensity", True), ("bloom_intensity", 1.2),
                  ("override_vignette_intensity", True), ("vignette_intensity", 0.5)):
        try:
            pp.set_editor_property(k_, v)
        except Exception as e:
            unreal.log_warning(f"[Alien] post-process {k_}: {e}")
    ppv.set_editor_property("settings", pp)
    return sun


def main():
    rng = random.Random(SEED)
    for folder in (ALIEN, MATS, f"{ALIEN}/Rocks"):
        if not eal.does_directory_exist(folder):
            eal.make_directory(folder)

    paint_master = eal.load_asset("/Game/Characters/Pip/M_PipPaint")
    ground = ground_material()
    special = {"Ground": ground, "GasGiant": gas_giant_material(), "GasRing": ring_material(), "Lava": lava_material()}

    names = ["planet", "alien_crystal_pink", "alien_crystal_cyan", "alien_shrooms", "alien_tubes", "alien_tree_5",
             "alien_tree_6", "alien_tree_7", "alien_spikes", "ship", "ship_flame", "part_power_core",
             "part_nav_computer", "part_thruster", "part_fuel_cell", "part_hyper_crystal", "gas_giant", "gas_ring",
             "alien_cactus_11", "alien_cactus_12", "spike_cluster", "lava_pool", "basalt_wall", "vent",
             "pu_heal", "pu_shield", "pu_blaster"]
    m = {n: mesh_of(n) for n in names}
    for n, mesh in m.items():
        assign_paint(mesh, paint_master, special)
        if n in ("planet", "ship", "spike_cluster", "lava_pool", "basalt_wall") or n.startswith(
                ("alien_crystal", "alien_shrooms", "alien_tubes", "alien_tree", "alien_cactus")):
            set_collision(mesh, "complex")
        else:
            set_collision(mesh, "none")
    rocks = alien_rocks(rock_material())
    boulders = [r for r in rocks if "boulder" in r.get_name()]
    small_rocks = [r for r in rocks if "boulder" not in r.get_name()]
    fg_collision.fix_material_usage(ALIEN)
    fg_collision.fix_material_usage("/Game/Characters")

    fresh_level()
    sun = lighting(sky_material())

    pl = spawn_class(unreal.AlienPlanet, (0, 0, 0), folder="Planet", label="Planet")
    pl.get_editor_property("surface").set_static_mesh(m["planet"])
    pl.set_editor_property("radius", R)
    pl.set_editor_property("sun", sun)
    pl.set_editor_property("part_meshes", [m[n] for n in ("part_power_core", "part_nav_computer", "part_thruster",
                                                          "part_fuel_cell", "part_hyper_crystal")])
    pl.set_editor_property("beacon_mesh", eal.load_asset("/Engine/BasicShapes/Cylinder"))
    pl.set_editor_property("beacon_material", beacon_material())
    pl.set_editor_property("cactus_meshes", [m["alien_cactus_11"], m["alien_cactus_12"]])
    pl.set_editor_property("spike_mesh", m["spike_cluster"])
    pl.set_editor_property("lava_mesh", m["lava_pool"])
    pl.set_editor_property("wall_mesh", m["basalt_wall"])
    pl.set_editor_property("vent_mesh", m["vent"])
    pl.set_editor_property("power_up_meshes", [m["pu_heal"], m["pu_shield"], m["pu_blaster"]])
    pl.set_editor_property("effect_sphere_mesh", eal.load_asset("/Engine/BasicShapes/Sphere"))

    top, _ = planet.surface((0, 0, 1))
    ship = spawn_class(unreal.Spaceship, (0, 0, top[2] - 4), unreal.Rotator(yaw=200), folder="Planet", label="Ship")
    ship.get_editor_property("hull").set_static_mesh(m["ship"])
    ship.get_editor_property("flame").set_static_mesh(m["ship_flame"])
    pl.set_editor_property("ship", ship)

    # droid spawns 6 m from the ship, facing it
    d = norm((600, 0, R))
    p, n = planet.surface(d)
    spawn_class(unreal.PlayerStart, tuple(pc + nc * 80 for pc, nc in zip(p, n)),
                unreal.Rotator(pitch=0, yaw=180), folder="Planet", label="PlayerStart")

    # gas giant with a tilted ring, high in the sky behind the ship as seen from the start
    gd = norm((-0.7, 0.35, 0.62))
    gpos = tuple(c * 36000 for c in gd)
    spawn_mesh(m["gas_giant"], gpos, unreal.Rotator(roll=12, pitch=-18, yaw=30), 85.0, "Sky", collide=False)
    spawn_mesh(m["gas_ring"], gpos, unreal.Rotator(roll=12, pitch=-18, yaw=30), 85.0, "Sky", collide=False)
    moon = eal.load_asset("/Engine/BasicShapes/Sphere")
    for md, ms in (((0.5, -0.7, 0.5), 18.0), ((0.2, 0.9, -0.4), 10.0)):
        a = spawn_mesh(moon, tuple(c * 26000 for c in norm(md)), unreal.Rotator(), ms, "Sky", collide=False)
        a.static_mesh_component.set_material(0, eal.load_asset(f"{MATS}/MI_AlienRock_boulder_01"))

    big = []
    trees = [m["alien_tree_5"], m["alien_tree_6"], m["alien_tree_7"]]
    for p in sphere_scatter(rng, trees, 42, (0.8, 1.25), 900, "Flora/Trees", align=0.15):
        big.append((p, 300))
    for p in sphere_scatter(rng, boulders, 14, (1.0, 2.0), 1500, "Rocks", sink=0.2, align=1.0, avoid=big):
        big.append((p, 250))
    sphere_scatter(rng, [m["alien_crystal_pink"]], 40, (0.7, 1.6), 600, "Flora/Crystals", sink=0.05, align=0.7, avoid=big)
    sphere_scatter(rng, [m["alien_crystal_cyan"]], 40, (0.7, 1.6), 600, "Flora/Crystals", sink=0.05, align=0.7, avoid=big)
    sphere_scatter(rng, [m["alien_shrooms"]], 70, (0.7, 1.4), 400, "Flora/Shrooms", align=0.4, avoid=big,
                   clear=0.13)
    sphere_scatter(rng, [m["alien_tubes"]], 70, (0.8, 1.5), 400, "Flora/Tubes", align=0.5, avoid=big, clear=0.13)
    sphere_scatter(rng, small_rocks, 150, (0.5, 1.6), 260, "Rocks", sink=0.12, align=1.0, avoid=big, clear=0.12)
    sphere_scatter(rng, [m["alien_spikes"]], 7000, (0.8, 1.6), 50, "Flora/GroundCover", align=0.8, collide=False,
                   cull=4500, instanced=True, clear=0.06)

    levels.save_current_level()
    eal.save_directory("/Game", only_if_is_dirty=True, recursive=True)
    log("BUILD OK")


if not getattr(unreal, "_alien_lib_only", False):   # set by patch scripts that only want the helpers
    main()
