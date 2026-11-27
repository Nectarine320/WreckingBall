# Bilan — devis « GM_Gym et caméra P1 »

Rédigé le 2026-11-27 par Claude++ à la fin du chantier. Complète `devis-gym-camera-p1.md`. Tests de l'utilisateur : gyms, touches de debug et caméra fonctionnels (2026-11-27). Les lots 2, 3 et 4 ont été regroupés en **une seule compilation C++**, à la demande de l'utilisateur.

---

## 1. État des lots

| Lot | État | Notes |
|---|---|---|
| 0 État des lieux | Fait | Soccer Field en Level Streaming (pas de World Partition), 3 sous-niveaux AlwaysLoaded (LD, Light, Gameplay). |
| 1 Map de gym | Fait, avec écarts | Nom, dossier et choix de la gym : voir §3. |
| 2 DataAsset de gym + `GM_Gym` | Fait | Classe C++ `UWB_GymDefinition`. |
| 3 Rôles forcés des NPC | Fait (version de départ) | Gardien permanent et agressif. Le comportement sera retravaillé plus tard. |
| 4 Caméra P1 | Fait | Mode par défaut, changement en jeu (commande console + touche Moins). |
| 5 DataAssets et tests | Fait, avec ajouts | 6 gyms au lieu de 4 (voir §3). Tests : voir §1 bis. |

### 1 bis. Tests du lot 5

| Test | État |
|---|---|
| 1 Composition des DataAssets | Validé par l'utilisateur (gyms 1 à 6). |
| 2 Gyms d'observation sans humain, IA au Play | Validé. |
| 3 Manettes en trop / places `Player` sans manette | Validé : avertissement dans le log, pas de personnage, aucun plantage. Les erreurs d'input qui apparaissaient ensuite sont corrigées (voir §3). |
| 4 Gardien et agressif | Validé (version de départ). |
| 5 Destruction et minuterie | **Changé** : la destruction reste **active** dans les gyms (voir §3). Minuterie coupée : validé. Soccer Field inchangée. |
| 6 `LoadGymDefinition` en cours de jeu | **En suspens** (décision de l'utilisateur) : rien ne l'appelle avant le menu debug. |
| 7 Caméra | Ballon par défaut, `SetCameraMode` en jeu, transition lissée et suivi au switch : validés. **Non testés** : le mode de départ lu dans la DataAsset ou le GameMode, une map sans ballon (à voir avec les Art Showrooms) et le cas « 2 humains, seul P1 suivi », qui est un cas limite possible, car en pratique un seul game dev joue à la manette. |
| 8 Une seule DataAsset d'offsets | Validé : les deux modes lisent `DA_SoccerCamera_Default`. |
| 9 PIE et compilation | Aucun « Accessed None » après les corrections. C++ compilé sans avertissement (pas de passe « avertissements comme erreurs » forcée). Blueprints compilés sans erreur. |

---

## 2. Ce qui a été construit

### C++ (fondation, tout exposé à Blueprint)
| Classe / fonction | Rôle |
|---|---|
| `UWB_GymDefinition` (`UPrimaryDataAsset`) | `GymName`, `Slots`, `bStartBotsOnPlay`, `bDestructionEnabled` (vrai par défaut), `bTimerEnabled`, `DefaultCameraMode`. |
| `FWB_RosterSlot` | Une place : `Team`, `ControlType` (`Gym.Control.*`), `ForcedRole` (`AI.ForcedRole.*`), `SpawnPlace` (1 à 3). |
| `AWB_GymSettings` (`BP_GymSettings`) | Acteur posé dans la map. Son réglage `GymDefinition` choisit la gym au lancement. |
| `UWB_GymDirectorComponent` (sur `GM_Gym`) | `LoadGymDefinition`, `SetSlotControl`, `SetSlotRole`, `GetActiveGymDefinition`, `GetSlots`, `IsTimerEnabled`, `GetDefaultCameraMode`, événement `OnGymLoaded`. |
| `UWB_TeamRosterComponent` | Composition par places (`SetSlotComposition`, `RebuildRoster`, `SetSlotForcedRole`, `GetForcedRoleOf`, `GetCharacterAtSlot`). Option coupée par défaut : le match ne change pas. |
| `UWB_DestructionSwitchSubsystem` + `UWB_DestructibleLibrary::IsDestructionEnabled / SetDestructionEnabled` | Interrupteur de destruction par monde (actif par défaut). |
| `UWB_SoccerAIController_Base` | Rôles forcés : intentions `ClearBall` (gardien) et `KeepAway` (agressif) ; le coordinateur ignore les NPC qui ont un rôle forcé. |
| `UWB_SoccerAIProfile` | `AggressiveKeepAwayDistance` (300), `GoalkeeperClearChargeTime` (0,4), Category `SoccerAI|Forced Roles`. |
| `UWB_CameraModeComponent` (sur `CAMM_SoccerPlayer`) | `SetCameraMode`, `GetCameraMode`, `ApplyStartMode`, `ComputeCameraTarget`, `OnCameraModeChanged` ; commande console temporaire `WB.CameraMode Ball | Player1`. |
| `UWB_SoccerCameraProfile::ComputePlayerCameraTarget` | Même offset de marche que le porteur (code partagé). |
| Gameplay Tags natifs | `Gym.Control.Player/Bot`, `AI.ForcedRole.Goalkeeper/Aggressive`, `Camera.Mode.Ball/Player1`. |

### Blueprints et assets
- `GM_Gym` (enfant de `BP_SoccerGamemode`, composant `GymDirector`) et `BP_GymSettings`, dans `/Game/Blueprint/Gym/`.
- `BP_SoccerGamemode` :
  - `ApplyDebugTimerFreeze` gèle la minuterie quand la gym la coupe ;
  - nouvelle variable `DefaultCameraMode`.
- `BP_SoccerCamera` : `UpdateCameraLocation` passe par `ComputeCameraTarget`. Ça corrige un « Accessed None » quand le GameMode n'est pas un `BP_SoccerGamemode`. Ajouts : nouvelle fonction `ApplyCameraTarget` et `ApplyStartMode` au BeginPlay.
- `BP_Destructible_Base.ApplyImpact` : refuse l'impact quand l'interrupteur est coupé.
- `BP_TP_SoccerPlayerController` :
  - touche **Moins** → `DebugToggleCameraMode` (Ball ↔ Player1, coupée en build de distribution) ;
  - `IsValid(ControlledCharacter)` devant les 9 appels d'input.
- `BPC_HeldObjectActions.ForceDrop` : `IsValid(AsBPGenericCharacter)` à l'entrée.
- Map `/Game/Level/GYM/LV_AAA_Gym_SoccerField` (GameMode Override = `GM_Gym`) et ses sous-niveaux dans `SubLVL/`.
- DataAssets dans `/Game/Level/GYM/Data/` :

| DataAsset | Équipe A | Équipe B | Bots au Play |
|---|---|---|---|
| `DA_Gym_01_Pass3v1` | joueur + 2 bots | gardien | non |
| `DA_Gym_02_3v2` | joueur + 2 bots | gardien + agressif | non |
| `DA_Gym_03_Observe3v1` | 3 bots | gardien | oui |
| `DA_Gym_04_Observe3v2` | 3 bots | gardien + agressif | oui |
| `DA_Gym_05_3v3` | joueur + 2 bots | 3 bots | non |
| `DA_Gym_06_Observe3v3` | 3 bots | 3 bots | oui |

Pour toutes : **destruction active**, minuterie coupée, caméra Ball.

---

## 3. Écarts au devis et décisions du chantier (2026-11-27)
- **Phase F mise en suspens** (prérequis du devis non rempli) : le gardien n'est pas terminé, ce n'est pas bloquant. Les comportements des NPC seront peaufinés avec les gyms.
- **Nom de la map** : `LV_AAA_Gym_SoccerField` au lieu de `Lv_900_Gym_SoccerField`. Les prochaines gyms seront `LV_BBB_…`, `LV_CCC_…`, sans numéros. Dossier `/Game/Level/GYM`.
- **Choix de la gym** : le réglage `GymDefinition` est sur l'acteur `BP_GymSettings`, posé dans le niveau persistant, et pas sur `GM_Gym`. Un GameMode n'est pas posé dans la map, donc il ne peut pas avoir de réglage d'instance.
- **Destruction active dans les gyms** : le devis disait « destruction désactivée ». L'utilisateur veut que les objets destructibles cassent dans les gyms. Les 6 DataAssets ont `bDestructionEnabled` = vrai, et c'est aussi la valeur par défaut en C++. L'interrupteur reste disponible pour une gym qui voudrait la couper.
- **Gyms 5 et 6 (3v3)** ajoutées. C'est le même jeu que le match, mais la minuterie n'est jamais active.
- **Changement de caméra en jeu** : en plus de `WB.CameraMode`, la **touche Moins** (à côté du 0) bascule Ball / Player1, parce que la touche de la console posait problème au clavier de l'utilisateur. Les deux sont temporaires, en attendant le menu debug.
- **Corrections trouvées pendant les tests** :
  - manette sans personnage : les events d'input lisaient `ControlledCharacter` vide (« Accessed None » sur RequestMove, KickOrThrow…) ;
  - gyms sans place `Player` : le pawn de départ de P1 était détruit avant son BeginPlay, donc `ForceDrop` lisait `AsBPGenericCharacter` vide.

---

## 4. Points ouverts
- **Test 6 en suspens** : `LoadGymDefinition` en cours de jeu, à tester avec le menu debug.
- **Test 7 partiel** : mode de départ lu dans la DataAsset ou le GameMode, Caméra P1 dans une map sans ballon (Art Showrooms). **Cas limite possible** : avec 2 humains, seul P1 doit être suivi (jamais testé, peu probable en pratique).
- **Limites connues de la version 1** :
  - après `LoadGymDefinition`, le ballon n'est pas remis au centre ;
  - l'agressif peut se coincer dans un coin quand il fuit ;
  - le widget de minuterie reste affiché, même gelé ;
  - `DebugToggleMatchTimer` (F7) peut relancer la minuterie dans une gym.
- **Ancien système** : `BP_MasterDestructible` n'est pas couvert par l'interrupteur de destruction.
- **Log** : « no BP_KickoffPoint in the map », hérité de Soccer Field.
- **Descriptions à coller** (pas d'outil MCP pour les écrire) :
  - `BP_SoccerGamemode.DefaultCameraMode` ;
  - `BP_SoccerCamera.ApplyCameraTarget` ;
  - `BP_TP_SoccerPlayerController.DebugToggleCameraMode`.
- **Sous-niveaux `_LD` à renommer `_Layout`** (nouvelle règle de `CLAUDE.md`) : `Lv_000_Soccerfield` et `LV_AAA_Gym_SoccerField`.
- **Commit et push** : à faire par l'utilisateur, avec la DLL compilée le 2026-11-27 à 14 h 09.

---

## 5. Constats techniques à retenir
- **Ordre du BeginPlay** : les composants d'un acteur reçoivent BeginPlay avant le ReceiveBeginPlay de son Blueprint. Le `GymDirector` pose donc la composition avant que `BP_SoccerGamemode` construise l'effectif.
- **Copie d'une map en Level Streaming** : la copie pointe encore vers les sous-niveaux d'origine, et MCP ne peut pas changer `WorldAsset` (lecture seule). La solution est un script Python lancé éditeur fermé (`remove_level_from_world` puis `add_level_to_world`).
- **MCP, tableaux** : `set_properties` refuse de changer la taille d'un tableau et ses éléments dans le même appel. Il faut d'abord ajouter les éléments, puis les modifier.
- **Pawn détruit avant son BeginPlay** : ses variables posées au BeginPlay sont vides dans `EndPlay` / `Destroyed`. Il faut un `IsValid` dans tout ce qui peut s'exécuter à ce moment-là.

---

## 6. À reporter dans le projet claude.ai
- **Devis `devis-gym-camera-p1.md`** :
  - marquer le devis **fait** (2026-11-27) ;
  - reporter les écarts du §3 : destruction active, nom de la map, `BP_GymSettings`, gyms 5 et 6, touche Moins ;
  - test 6 en suspens.
- **Devis du menu debug (à venir)** :
  - appeler `LoadGymDefinition`, `SetSlotControl`, `SetSlotRole`, `GetActiveGymDefinition`, `SetCameraMode` et `GetCameraMode` ;
  - retirer `WB.CameraMode` et la touche Moins ;
  - inclure le test 6.
- **Backlog** :
  - lot « WB Tools » (outils d'éditeur), à faire après ce devis, en une seule compilation :
    - un menu **WB Tools** dans la barre principale de l'éditeur ;
    - l'outil **Sort Level**, qui range les acteurs selon la règle Outliner et sous-niveaux de `CLAUDE.md`, avec des règles dans un Data Asset et un aperçu avant d'appliquer ;
    - une entrée pour l'**outil de retarget** (`WB_RetargetToolset`) ;
  - renommer les sous-niveaux `_LD` en `_Layout`.
- **Devis maître** : phase F (NPC) en pause depuis le 2026-11-27, à reprendre avec les gyms.
