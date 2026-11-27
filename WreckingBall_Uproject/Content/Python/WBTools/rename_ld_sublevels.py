"""Rename the old _LD sub-levels to _Layout (CLAUDE.md, "Maps : Outliner et sous-niveaux"). Devis wb-tools, point 4.

Run with the editor CLOSED:
    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>" -unattended -nosplash -nullrhi

For each map: renames the sub-level asset (Unreal fixes the persistent map's streaming reference), saves, then checks that the
persistent map streams the new sub-level. If it still points to the old name, the streaming level is removed and added again
(LevelStreamingAlwaysLoaded), the method used for the copy of Soccer Field. Every line printed starts with WBRENAME.
"""

import unreal

# (old sub-level, new sub-level, persistent map)
RENAMES = [
    (
        "/Game/Level/Game/LV_000_Soccerfield/SubLevels/SubLv_000_Soccerfield_LD",
        "/Game/Level/Game/LV_000_Soccerfield/SubLevels/SubLv_000_Soccerfield_Layout",
        "/Game/Level/Game/LV_000_Soccerfield/Lv_000_Soccerfield",
    ),
    (
        "/Game/Level/GYM/LV_AAA_Gym_SoccerField/SubLVL/SubLv_AAA_Gym_SoccerField_LD",
        "/Game/Level/GYM/LV_AAA_Gym_SoccerField/SubLVL/SubLv_AAA_Gym_SoccerField_Layout",
        "/Game/Level/GYM/LV_AAA_Gym_SoccerField/LV_AAA_Gym_SoccerField",
    ),
]


def log(text):
    unreal.log_warning("WBRENAME " + text)


def short(path):
    return path.rsplit("/", 1)[-1]


def streaming_levels(map_path):
    """Streaming level objects of the persistent map, found by name (World.streaming_levels is not exposed)."""
    name = short(map_path)
    found = []
    for cls in ("LevelStreamingAlwaysLoaded", "LevelStreamingDynamic"):
        for index in range(0, 32):
            obj = unreal.find_object(None, "%s.%s:%s_%d" % (map_path, name, cls, index))
            if obj:
                found.append(obj)
    return found


def streamed_package(level_streaming):
    world_asset = level_streaming.get_editor_property("world_asset")
    return str(world_asset).split(".")[0] if world_asset else ""


def check_and_fix(old_path, new_path, map_path):
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if not world:
        log("ERROR cannot load %s" % map_path)
        return
    packages = [(ls, streamed_package(ls)) for ls in streaming_levels(map_path)]
    for ls, package in packages:
        log("%s streams %s (%s)" % (short(map_path), package, ls.get_class().get_name()))

    stale = [(ls, package) for ls, package in packages if package == old_path]
    if not stale:
        # Save anyway: a reference read through the redirector is written back with the new path.
        unreal.EditorLoadingAndSavingUtils.save_map(world, map_path)
        log("OK %s no longer streams %s (map saved)" % (short(map_path), short(old_path)))
        return

    for ls, package in stale:
        level_class = ls.get_class()
        loaded = ls.get_loaded_level()
        if not loaded:
            log("ERROR %s: %s is not loaded, cannot repoint it" % (short(map_path), short(old_path)))
            continue
        unreal.EditorLevelUtils.remove_level_from_world(loaded)
        unreal.EditorLevelUtils.add_level_to_world(world, new_path, level_class)
        log("REPOINTED %s: %s -> %s" % (short(map_path), short(old_path), short(new_path)))
    unreal.EditorLoadingAndSavingUtils.save_map(world, map_path)


def main():
    for old_path, new_path, map_path in RENAMES:
        if unreal.EditorAssetLibrary.does_asset_exist(new_path):
            log("SKIP rename, %s already exists" % short(new_path))
        elif not unreal.EditorAssetLibrary.does_asset_exist(old_path):
            log("ERROR %s does not exist" % old_path)
            continue
        else:
            # A blank map makes sure the sub-level is not loaded while it is renamed (a loaded map cannot be renamed).
            unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
            renamed = unreal.EditorAssetLibrary.rename_asset(old_path, new_path)
            log("RENAME %s -> %s: %s" % (short(old_path), short(new_path), renamed))
            unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        check_and_fix(old_path, new_path, map_path)
        log("redirector left at old path: %s" % unreal.EditorAssetLibrary.does_asset_exist(old_path))
    log("DONE")


main()
