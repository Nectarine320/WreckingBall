"""Replace the old destructible walls of LV_999_IntroMenu (Content/Blueprint/Destructible, removed) by the new Concrete_A walls.

Run with the editor CLOSED:
    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>" -unattended -nosplash -nullrhi

Same pivot on both sides (wall start, checked 2026-11-27): the new wall takes the location, rotation, scale, Outliner folder and
sub-level of the old one. Sizes: full wall 528 -> Wall_Exterior_4m_01, half wall 328 -> Wall_Exterior_2m_01, windows -> Windows_Exterior_A_01.
Also removes duplicate rules of DA_SortLevelRules. Every line printed starts with WBWALLS.
"""

import unreal

MAP = "/Game/Level/Game/LV_999_IntroMenu/LV_999_IntroMenu"
NEW = "/Game/Blueprint/Impact/Destructible/BP_Concrete_A/BP_Destructible_Concrete_A_"
REPLACE = {
    "BP_Destructible_Wall_Concrete_01_C": NEW + "Wall_Exterior_4m_01",
    "BP_Destructible_Wall_Concrete_Half_01_C": NEW + "Wall_Exterior_2m_01",
    "BP_Destructible_Wall_Concrete_Windows_A_C": NEW + "Windows_Exterior_A_01",
}
RULES = "/Game/Blueprint/Tools/DA_SortLevelRules"


def log(text):
    unreal.log_warning("WBWALLS " + text)


def replace_walls():
    unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    classes = {name: unreal.EditorAssetLibrary.load_blueprint_class(path) for name, path in REPLACE.items()}
    for name, cls in classes.items():
        if not cls:
            log("ERROR new class not found for %s" % name)
            return

    replaced = 0
    for actor in actors_sub.get_all_level_actors():
        old_class = actor.get_class().get_name()
        if old_class not in classes:
            if "MasterDestructible" in old_class or "Destructible_Wall_Concrete" in old_class:
                log("WARNING unknown old wall %s (%s), left as is" % (actor.get_actor_label(), old_class))
            continue
        level = actor.get_level()
        level_name = level.get_outermost().get_name().rsplit("/", 1)[-1]
        if not unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).set_current_level_by_name(level_name):
            log("ERROR cannot make %s the current level, %s skipped" % (level_name, actor.get_actor_label()))
            continue
        new_actor = actors_sub.spawn_actor_from_class(classes[old_class], actor.get_actor_location(), actor.get_actor_rotation())
        if not new_actor:
            log("ERROR spawn failed for %s" % actor.get_actor_label())
            continue
        new_actor.set_actor_scale3d(actor.get_actor_scale3d())
        new_actor.set_folder_path(actor.get_folder_path())
        same_level = new_actor.get_level() == level
        log("%s -> %s (%s)%s" % (actor.get_actor_label(), new_actor.get_actor_label(), level.get_outermost().get_name().rsplit("/", 1)[-1],
                                 "" if same_level else " WRONG LEVEL"))
        actors_sub.destroy_actor(actor)
        replaced += 1

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, False)
    log("replaced %d wall(s), map saved" % replaced)


def dedupe_rules():
    rules_asset = unreal.load_asset(RULES)
    rules = list(rules_asset.get_editor_property("class_rules"))
    seen, kept = set(), []
    for rule in rules:
        key = (str(rule.get_editor_property("actor_class")), str(rule.get_editor_property("folder")))
        if key in seen:
            continue
        seen.add(key)
        kept.append(rule)
    if len(kept) != len(rules):
        rules_asset.set_editor_property("class_rules", kept)
        unreal.EditorAssetLibrary.save_loaded_asset(rules_asset)
    log("sort rules %d -> %d" % (len(rules), len(kept)))


replace_walls()
dedupe_rules()
log("DONE")
