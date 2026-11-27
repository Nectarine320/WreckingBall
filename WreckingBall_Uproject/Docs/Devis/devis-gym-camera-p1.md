# Wrecking Ball — Devis « GM_Gym et caméra P1 »

Brouillon du 2026-11-27 (8 h 50). Réponses du designer intégrées à 9 h 05 et 9 h 08. **Prêt à remettre à Claude++**, après la fin de la phase F (NPC). Source : `claude/prep-debug-menu-gym.md` (décisions) et `claude/plan-gyms-debug-design.md` (chantier A).

## Objectif
- Des **gyms de prototypage** pour tester et observer les passes, le gardien et l'IA, sans destruction ni minuterie.
- **Une seule map de gym**. L'état du jeu (composition, contrôle, rôles) se choisit par **une DataAsset par gym**, lue par **un seul GameMode, `GM_Gym`**.
- Un deuxième mode de caméra, **Caméra P1**, pour les Art Showrooms et les zones de test sans ballon.
- Le menu debug (devis séparé, à venir) s'appuiera sur les fonctions publiques créées ici.

## Prérequis
- Étape 15 (match 3v3) : **terminée** (designer, 2026-11-27).
- Phase F (NPC, `claude/devis-npc.md`) : **commencée, à terminer avant ce devis**. Le lot 3 s'appuie sur le coordinateur d'équipe et sur `CurrentRole` / `SetRole`.

## Les 4 gyms
| Gym | Équipe A (3) | Équipe B | Usage |
| --- | --- | --- | --- |
| 1 · Passe 3v1 | 1 joueur + 2 bots | 1 NPC **gardien permanent** | Passes et gardien |
| 2 · 3v2 | 1 joueur + 2 bots | 1 NPC gardien + 1 NPC **agressif** | Jeu sous pression |
| 3 · Observation 3v1 | 3 bots | 1 NPC gardien permanent | Observer l'IA |
| 4 · Observation 3v2 | 3 bots | 1 NPC gardien + 1 NPC agressif | Observer l'IA |

Pour toutes les gyms : **destruction désactivée**, **minuterie désactivée**.

## Ordre des lots
| Lot | Contenu | Priorité |
| --- | --- | --- |
| 0 | État des lieux (lecture seule) | — |
| 1 | Map de gym : copie de Soccer Field et de ses sous-niveaux, dossier `Gym` | Haute |
| 2 | DataAsset de gym + `GM_Gym` (équipes, contrôle, rôles, destruction, minuterie) | Haute |
| 3 | Rôles forcés des NPC (gardien permanent, agressif) | Haute |
| 4 | Caméra P1 + mode par défaut + changement en cours de jeu | Moyenne |
| 5 | Les 4 DataAssets de gym, réglages, tests | — |

Pour chaque lot, Claude++ donne d'abord la ligne **« C++ ou Blueprint »** demandée par `CLAUDE.md`, avec la raison et le coût.

---

## Lot 0 — État des lieux
Lire sans modifier :
- La map Soccer Field : nom exact, liste des sous-niveaux, World Partition ou Level Streaming, GameMode utilisé.
- `BP_SoccerGamemode` et `WB_SoccerBotManagerComponent` : création des 6 personnages, attribution des manettes, `ToggleBots` / `StartBotsOnPlay`, places des `BP_TeamSpawnPoint`.
- **Où la destruction et la minuterie s'activent** aujourd'hui (budget de destruction, `BP_PhysicsBudgetManager`, minuterie du match), et s'il existe déjà un interrupteur.
- `BP_SoccerCamera` (ou le camera manager actuel) : cadrage ballon + porteur, **DataAsset d'offsets**, offset dans la direction de marche du porteur.
- IA (phase F terminée) : `WB_TeamCoordinatorComponent`, `CurrentRole` / `SetRole`, intentions disponibles, Gameplay Tags des rôles.

## Lot 1 — Map de gym
- Dupliquer la map Soccer Field **et tous ses sous-niveaux**. Les copies sont indépendantes : modifier la gym ne touche jamais Soccer Field.
- **Une seule map pour toutes les gyms.** Rangement : dossier **`Gym`** du Content Browser ; les sous-niveaux dans un sous-dossier `SubLVL` (pour que le filtre du futur menu debug les cache).
- Nom proposé : `Lv_900_Gym_SoccerField`.
- Les objets destructibles peuvent rester : la destruction se coupe par le GameMode (lot 2).
- GameMode Override de la map = `GM_Gym`.

## Lot 2 — DataAsset de gym et `GM_Gym`

### DataAsset `DA_Gym_*`
Classe de DataAsset de gym (C++ `UPrimaryDataAsset` si les structs posent problème aux outils MCP, sinon Blueprint : à trancher au lot 0). Rangée dans le dossier `Gym`. Tous les champs ont une Category et une Description en anglais.
- `GymName` (texte affiché dans le menu debug).
- **Composition** : une liste de places par équipe. Pour chaque place :
  - `Team` (lu dans `WB_TeamDefinitionData`, pas de couleur en dur) ;
  - `ControlType` : Gameplay Tag `Gym.Control.Player` ou `Gym.Control.Bot` ;
  - `ForcedRole` : Gameplay Tag de rôle forcé (gardien permanent, agressif) ou vide (= IA normale de la phase F) ;
  - `SpawnPlace` : place 1 à 3 du `BP_TeamSpawnPoint`.
- Une équipe peut avoir **moins de 3 personnages** (1 ou 2 en équipe B).
- `bDestructionEnabled` (faux), `bTimerEnabled` (faux).
- `bStartBotsOnPlay` : vrai pour les gyms d'observation, faux sinon (`ToggleBots` reste disponible).
- `DefaultCameraMode` : `Camera.Mode.Ball` ou `Camera.Mode.Player1` (lot 4).

### `GM_Gym`
- Enfant de `BP_SoccerGamemode` : il garde le but, le score, le spawn du ballon et la remise en jeu.
- Réglage `GymDefinition` (DataAsset active), éditable dans la map. C'est lui qui choisit la gym au lancement.
- Au lancement, il remplace la règle « toujours 3 contre 3 » par la composition de la DataAsset :
  - places `Player` → manettes actives, dans l'ordre des ID ; plus de places `Player` que de manettes : la place passe en `Bot`, avec un avertissement dans le log ;
  - places `Bot` → `BP_SoccerAIController` ;
  - manette en trop (aucune place `Player` libre) : pas de personnage, avertissement dans le log.
- Applique `bDestructionEnabled` et `bTimerEnabled` aux systèmes trouvés au lot 0.
- **Fonctions publiques** pour le menu debug (sans UI dans ce devis) : `LoadGymDefinition(DataAsset)` (recharge la composition et replace tout le monde, sans relancer la map), `SetSlotControl(Slot, ControlTag)`, `SetSlotRole(Slot, RoleTag)`, `GetActiveGymDefinition`.

## Lot 3 — Rôles forcés des NPC
Le rôle forcé **passe avant** l'attribution du coordinateur d'équipe (phase F) : le coordinateur ne réattribue jamais un NPC qui a un rôle forcé.

- **Gardien permanent** (version de départ, le comportement sera retravaillé après la création des gyms) :
  - ne quitte jamais son rôle, même ballon loin ;
  - se place entre le ballon et son but (3 m devant le périmètre du filet, devis NPC) ; mode Défense et plongeon existants ;
  - **peut bloquer et dégager le ballon** (tir de dégagement vers l'avant quand il l'a).
- **Agressif** :
  - va au ballon s'il est libre, presse le porteur adverse sinon ; dash pour faire lâcher le ballon ;
  - **n'essaie pas de marquer** pour le moment : il ne tire pas au but quand il récupère le ballon ;
  - **avec le ballon, il fuit** : il garde le ballon et se maintient à **environ 3 m** de l'adversaire le plus proche (humain ou COM), en s'éloignant quand celui-ci s'approche. Distance réglable dans le profil d'IA (`AggressiveKeepAwayDistance`, 300 cm par défaut).
- Réglages dans le profil d'IA existant (`WB_SoccerAIProfile`), pas de valeur cachée dans le code.

## Lot 4 — Caméra P1

### Comportement
- **Caméra ballon** (`Camera.Mode.Ball`) : comportement actuel, inchangé, par défaut.
- **Caméra P1** (`Camera.Mode.Player1`) : suit **uniquement** le personnage contrôlé par le Player 1, même s'il y a d'autres humains. Elle garde **le même offset dans la direction de marche** que celui appliqué aujourd'hui au porteur du ballon.
- Elle lit **la même DataAsset d'offsets** que la caméra ballon. **Pas de copie.** Si un réglage propre à P1 devient nécessaire, il s'ajoute à cette DataAsset, dans une Category à part.
- Fonctionne **sans ballon dans la map** (Art Showrooms) : aucun « Accessed None » si le ballon ou le porteur n'existe pas.
- Changement de personnage de P1 (passe ou switch) : la caméra suit le nouveau personnage, avec le lissage habituel.
- Passage d'un mode à l'autre : transition lissée, pas de saut.

### Mode par défaut au lancement de la map
Ordre de priorité :
1. `DefaultCameraMode` de la DataAsset de gym, si `GM_Gym` est actif.
2. Sinon, réglage **`DefaultCameraMode` sur le GameMode de base** (`BP_SoccerGamemode`, hérité par tous). Une map sans gym (Art Showroom) le règle par un GameMode Override ou un enfant léger du GameMode. *(Choix du designer, 2026-11-27 : GameMode plutôt que World Settings.)*
3. Sinon, `Camera.Mode.Ball`.

### Changement en cours de jeu
- Fonctions publiques `SetCameraMode(ModeTag)` et `GetCameraMode()` sur le camera manager. Le menu debug (catégorie Player) les appellera.
- En attendant le menu : une commande console ou un raccourci de debug temporaire, à retirer quand le menu existe.

## Lot 5 — DataAssets et tests
Créer dans le dossier `Gym` : `DA_Gym_01_Pass3v1`, `DA_Gym_02_3v2`, `DA_Gym_03_Observe3v1`, `DA_Gym_04_Observe3v2`.

### Tests
1. Chaque DataAsset : bonne composition, bonnes places, bons contrôleurs (joueur ou bot), étiquettes P1 / COM correctes.
2. Gyms 3 et 4 : aucun personnage humain, l'IA démarre au Play.
3. Plus de manettes que de places `Player`, et l'inverse : avertissement dans le log, rien ne plante.
4. Gardien permanent : ne sort jamais de son rôle ; bloque et dégage. Agressif : va au ballon, presse le porteur, ne tire jamais au but ; avec le ballon, il fuit et reste à environ 3 m de l'adversaire le plus proche. Le coordinateur ne réattribue pas ces NPC.
5. Aucune destruction, aucune minuterie dans la gym. Soccer Field reste inchangée (destruction et minuterie actives).
6. `LoadGymDefinition` en cours de jeu : passage de la Gym 1 à la Gym 2 sans relancer la map.
7. Caméra : ballon par défaut ; Caméra P1 dans une map sans ballon ; mode par défaut lu dans la DataAsset, puis dans le GameMode ; `SetCameraMode` en jeu, transition lissée ; switch de personnage suivi ; avec 2 humains, seul P1 est suivi.
8. Une seule DataAsset d'offsets dans le projet.
9. PIE sans « Accessed None », compilation avec avertissements comme erreurs, guide d'ergonomie appliqué, rapport de la section 8.

Les lots qui touchent un Blueprint suivent `claude/guide-ergonomie-blueprint.md` et se terminent par le rapport de sa section 8. Le C++ suit le `CLAUDE.md` du projet.

## Décisions (designer, 2026-11-27)
- Une seule map de gym ; l'état du GameMode se choisit par DataAsset.
- « Onglet Gym » = dossier `Gym` du Content Browser.
- Mode de caméra par défaut hors gym : réglage sur le GameMode.
- NPC agressif : prend le ballon, n'essaie pas de marquer pour le moment ; avec le ballon, il fuit en restant à environ 3 m de l'adversaire (9 h 08).
- Gardien : bloque et dégage ; son comportement sera retravaillé après la création des gyms.
- Étape 15 terminée ; phase F commencée, à terminer avant ce devis.
- Caméra P1 : suit uniquement P1.

## Points ouverts
Aucun (2026-11-27, 9 h 08).
