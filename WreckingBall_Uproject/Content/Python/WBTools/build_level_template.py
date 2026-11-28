"""Build the level template of WB Tools > Level > Create Level (/Game/Level/Templates/Lv_Template) and tidy the lights of Bungalow.

Run IN THE EDITOR (moving actors between levels needs it): MCP WB_ToolsToolset.RunWBToolsScript("build_level_template.py"),
or the console: py "<this file>". The template levels must exist (empty): they are filled, then saved alone.

1. Bungalow: the lights of its persistent level move to SubLv_100_Bungalow_Light (saved).
2. SubLv_Template_Light: copy of the content of SubLv_100_Bungalow_Light.
3. SubLv_Template_Layout: one stretched cube covering the Soccer Field pitch, its white lines and circles, the coloured centre circle.
4. SubLv_Template_Gameplay: goals, team spawn points, spawn manager, play areas, player walls, nav mesh bounds, the 2 ball respawn
   points closest to each goal, and a kickoff point on the floor at the centre.
5. Lv_Template: persistent level streaming the three templates.
The source maps are never saved by steps 2 to 4 (actors are moved into the new levels, then only the new levels are saved).
Every line printed starts with WBTPL.
"""

import unreal

TPL = "/Game/Level/Templates"
BUNGALOW = "/Game/Level/Game/Lv_100_Bungalow/Lv_100_Bungalow"
SOCCER = "/Game/Level/Game/LV_000_Soccerfield/Lv_000_Soccerfield"
LIGHT_CLASSES = ("DirectionalLight", "SkyLight", "SkyAtmosphere", "ExponentialHeightFog", "VolumetricCloud")
GAMEPLAY_CLASSES = ("BP_GoalPost_01_C", "BP_TeamSpawnPoint_C", "BP_SpawnManager_C", "BP_PlayAreaVolume_C", "BP_PlayerBoundaryWall_C", "NavMeshBoundsVolume")

actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(text):
    unreal.log_warning("WBTPL " + text)


def level_of(actor):
    return actor.get_level().get_outermost().get_name().rsplit("/", 1)[-1]


def all_actors():
    return actors_sub.get_all_level_actors()


def streaming_level(map_path, suffix):
    name = map_path.rsplit("/", 1)[-1]
    for index in range(0, 32):
        ls = unreal.find_object(None, "%s.%s:LevelStreamingAlwaysLoaded_%d" % (map_path, name, index))
        if ls and str(ls.get_editor_property("world_asset")).split(".")[0].endswith(suffix):
            return ls
    return None


def open_map(path):
    """Opens a map without any save prompt: a blank map first drops the unsaved changes of the source maps (never saved here)."""
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    unreal.EditorLoadingAndSavingUtils.load_map(path)


def new_level_from(actors, path):
    """Streams the (empty) template level in the open map, moves the actors into it and saves it alone."""
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    ls = unreal.EditorLevelUtils.add_level_to_world(world, path, unreal.LevelStreamingAlwaysLoaded)
    level = ls.get_loaded_level() if ls else None
    if not level:
        log("ERROR %s not streamed" % path)
        return
    moved = unreal.EditorLevelUtils.move_actors_to_level(actors, ls, False, False)
    log("%s: %d actor(s) moved" % (path, moved))
    count = len([a for a in all_actors() if a.get_level() == level and a.get_class().get_name() not in ("WorldSettings", "Brush")])
    unreal.EditorLoadingAndSavingUtils.save_packages([level.get_outermost()], False)
    log("%s saved with %d actor(s) (%d selected)" % (path, count, len(actors)))


def bounds(actor):
    origin, extent = actor.get_actor_bounds(False)
    return origin - extent, origin + extent


# ---------------------------------------------------------------- 1. Bungalow lights -> Bungalow Light sub-level
def tidy_bungalow():
    open_map(BUNGALOW)
    found = {}
    for a in all_actors():
        if a.get_class().get_name() in LIGHT_CLASSES + ("PostProcessVolume",):
            key = "%s@%s" % (a.get_class().get_name(), level_of(a))
            found[key] = found.get(key, 0) + 1
    log("Bungalow light actors: %s" % found)
    lights = [a for a in all_actors() if a.get_class().get_name() in LIGHT_CLASSES and level_of(a) == "Lv_100_Bungalow"]
    ls = streaming_level(BUNGALOW, "_Light")
    if not ls:
        log("ERROR Bungalow Light sub-level not found")
        return
    moved = unreal.EditorLevelUtils.move_actors_to_level(lights, ls, False, False) if lights else 0
    light_level = ls.get_loaded_level()
    for actor in all_actors():
        if actor.get_level() != light_level:
            continue
        name = actor.get_class().get_name()
        if name in LIGHT_CLASSES:
            actor.set_folder_path("02_Light/01_Lighting")
        elif name == "PostProcessVolume":
            actor.set_folder_path("02_Light/02_PostProcess")
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
    log("Bungalow: %d light actor(s) moved to SubLv_100_Bungalow_Light, saved" % moved)


# ---------------------------------------------------------------- 2. Light template
def build_light():
    open_map(BUNGALOW)
    picked = [a for a in all_actors() if level_of(a) == "SubLv_100_Bungalow_Light" and a.get_class().get_name() in LIGHT_CLASSES + ("PostProcessVolume",)]
    new_level_from(picked, TPL + "/SubLv_Template_Light")


# ---------------------------------------------------------------- 3 and 4. Layout and Gameplay templates
def material_name(actor):
    comp = actor.static_mesh_component
    material = comp.get_material(0) if comp else None
    return material.get_name() if material else ""


def mesh_name(actor):
    comp = actor.static_mesh_component
    return comp.static_mesh.get_name() if comp and comp.static_mesh else ""


def build_soccer_templates():
    open_map(SOCCER)
    layout = [a for a in all_actors() if level_of(a) == "SubLv_000_Soccerfield_Layout" and a.get_class().get_name() == "StaticMeshActor"]

    # Pitch extents: the green cubes of the striped floor.
    green_cubes = [a for a in layout if mesh_name(a) == "SM_Cube_01" and "Green" in material_name(a)]
    lo, hi = None, None
    for a in green_cubes:
        a_lo, a_hi = bounds(a)
        lo = a_lo if lo is None else unreal.Vector(min(lo.x, a_lo.x), min(lo.y, a_lo.y), min(lo.z, a_lo.z))
        hi = a_hi if hi is None else unreal.Vector(max(hi.x, a_hi.x), max(hi.y, a_hi.y), max(hi.z, a_hi.z))
    log("pitch from %d green cubes: %s -> %s" % (len(green_cubes), lo, hi))

    # One stretched cube as the floor, same mesh and green material.
    floor = actors_sub.spawn_actor_from_class(unreal.StaticMeshActor, (lo + hi) * 0.5)
    floor.static_mesh_component.set_static_mesh(green_cubes[0].static_mesh_component.static_mesh)
    floor.static_mesh_component.set_material(0, green_cubes[0].static_mesh_component.get_material(0))
    floor.set_actor_label("SM_Floor")
    f_lo, f_hi = bounds(floor)
    size, mesh_size = hi - lo, f_hi - f_lo
    floor.set_actor_scale3d(unreal.Vector(size.x / mesh_size.x, size.y / mesh_size.y, size.z / mesh_size.z))
    f_lo, f_hi = bounds(floor)
    floor.set_actor_location(floor.get_actor_location() + (lo - f_lo), False, False)
    log("floor: %s -> %s" % bounds(floor))

    # White lines and circles, and the coloured centre circle.
    painted = [a for a in layout if "White" in material_name(a) or mesh_name(a) == "SM_Cylinder"]
    new_level_from(painted + [floor], TPL + "/SubLv_Template_Layout")

    # Gameplay: what a match needs.
    actors = all_actors()
    picked = [a for a in actors if a.get_class().get_name() in GAMEPLAY_CLASSES and level_of(a).startswith(("Lv_000", "SubLv_000"))]
    goals = [a for a in picked if a.get_class().get_name() == "BP_GoalPost_01_C"]
    respawns = [a for a in actors if a.get_class().get_name() == "BP_RespawnPoint_C"]
    chosen = []
    for goal in goals:
        g = goal.get_actor_location()
        for r in sorted([r for r in respawns if r not in chosen], key=lambda r: (r.get_actor_location() - g).length())[:2]:
            chosen.append(r)
    kickoff_class = unreal.EditorAssetLibrary.load_blueprint_class("/Game/Blueprint/Match/BP_KickoffPoint")
    centre = (lo + hi) * 0.5
    kickoff = actors_sub.spawn_actor_from_class(kickoff_class, unreal.Vector(centre.x, centre.y, hi.z))
    kickoff.set_actor_label("BP_KickoffPoint")
    log("gameplay: %s + %d respawn point(s) + kickoff at %s" % (sorted(a.get_class().get_name() for a in picked), len(chosen), kickoff.get_actor_location()))
    new_level_from(picked + chosen + [kickoff], TPL + "/SubLv_Template_Gameplay")


# ---------------------------------------------------------------- 5. Persistent template
def build_template_map():
    if unreal.EditorAssetLibrary.does_asset_exist(TPL + "/Lv_Template"):
        open_map(TPL + "/Lv_Template")
        log("Lv_Template opened")
        return
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    for suffix in ("Layout", "Gameplay", "Light"):
        unreal.EditorLevelUtils.add_level_to_world(world, TPL + "/SubLv_Template_" + suffix, unreal.LevelStreamingAlwaysLoaded)
    unreal.EditorLoadingAndSavingUtils.save_map(world, TPL + "/Lv_Template")
    log("Lv_Template saved")


tidy_bungalow()
build_light()
build_soccer_templates()
build_template_map()
log("DONE")
