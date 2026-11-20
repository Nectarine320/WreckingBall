# Wrecking Ball — Devis NPC (3 rôles coordonnés) — phase F

Brouillon du 2026-11-18. Révisé à 9 h 24, 17 h 48, 17 h 59, 18 h 03 et 18 h 30 ; le 2026-11-19 à 19 h 57 (règles des rôles ; phase F du devis maître ; match à 6, spawn et passe déplacés au devis `claude/devis-match-3v3.md`) ; à 20 h 09 (zones, barricade, lancers, difficulté) ; à 20 h 20 (attaquant, recherche d'objets à 5 m, cibles des lancers) ; **à 20 h 26 (un NPC presse l'attaquant adverse)**.

## Objectif
- **Prochaine mouture de l'IA** : chaque équipe de 3 a un **Attaquant**, un **Défenseur** et un **Gardien**.
- Les 3 personnages d'une équipe sont dirigés par un **manager d'équipe** (communication omnisciente) qui attribue les rôles au bon moment.
- **Un seul niveau de difficulté** pour commencer ; on observera les comportements avant d'équilibrer.
- **Prérequis (phase E, étape 15)** : 6 personnages, `BP_TeamSpawnPoint`, passe et changement de personnage, `ToggleBots` (l'IA ne démarre pas au Play).

## Zones
- **Zone de gardien** : une **boîte de collision** sur le but (composant du but). **Tout NPC contrôlé par un AIController dont le rôle est Gardien passe automatiquement en mode Défense quand il est dans cette boîte.**
- **Périmètre du filet** = cette boîte.
- **Zone défensive** = la moitié de terrain de l'équipe. Règle simple pour le Défenseur : un **rayon mesuré depuis le périmètre du filet de son équipe**, égal à **la distance entre les deux filets ÷ 2**.
- Plus tard (option) : un outil en **spline** pour dessiner la zone défensive.

## Règles des rôles

### Personne de l'équipe n'a le ballon (ballon libre ou chez l'adversaire)
- **Attaquant** : **toujours le joueur le plus proche du ballon** (pour le moment), y compris quand l'adversaire a le ballon. Va chercher le ballon / presse le porteur adverse (Dash pour faire lâcher).
- **Défenseur** : reste dans sa zone défensive (rayon ci-dessus, mesuré depuis son filet), entre le porteur et son filet.
- **Gardien** : **3 m devant le périmètre de son filet**, sur la ligne ballon → centre du but. En mode Défense dans la boîte ; plongeon existant.

### L'équipe a le ballon
- **Porteur** (Attaquant) : avance vers le but adverse, tire ; passe si bloqué.
- **Le Défenseur devient attaquant** : se place dans un **angle mort** à une certaine distance du porteur. **Occasionnellement, il se place devant le ballon** si ça peut aider à marquer.
- **Le Gardien devient défenseur** : quitte sa zone et monte, **en restant toujours le joueur de l'équipe le plus proche de son but**.

### Objets du décor
- **Défenseur — lancer** :
  - Il **scanne un rayon de 5 m** autour de lui, à la recherche d'objets **sans le tag `NoGrab`**.
  - S'il en trouve un : il va le prendre et le lance. Sinon : **il priorise son positionnement**.
  - Délai entre deux lancers : **4 s**. Force et intensité **variables** (plage réglable).
  - Cibles, **par ordre de priorité** : 1) l'attaquant adverse (porteur ou le plus proche du ballon) ; 2) un adversaire proche d'un de ses défenseurs. **Humains et COM adverses sont traités pareil.**
- **Gardien — barricade** : si le ballon est **assez loin de son but**, il va chercher des objets, idéalement **de grande taille** (mêmes règles : sans `NoGrab`). Il les **lance à la force minimale**, devant le filet ou dans la moitié de sa zone défensive. **3 objets au maximum.**
- **L'IA adverse peut retirer les objets** d'une barricade.
- Dans la limite du budget de destruction et du budget physique.

### Humains
- Les humains ne reçoivent pas d'ordre : le manager **déduit leur rôle** de leur position. S'ils ne tiennent pas leur rôle, le manager peut réorganiser les NPC autour d'eux.
- **Toujours 1 NPC qui presse l'attaquant adverse** (2026-11-19, 20 h 26) : un seul NPC de l'équipe agresse en permanence l'attaquant adverse (humain ou COM). Les autres adversaires (ex. un défenseur humain) ne sont pas forcément pressés.

## Déjà en place (inspection du 2026-11-18, 18 h 30)
- **IA en C++** : `WB_SoccerAIController_Base` (parent de `BP_SoccerAIController`). Pas de Behavior Tree : une **intention** par image (`EWB_SoccerAIIntent` : Idle, SeekBall, ChaseCarrier, Support, Attack), NavMesh, `EvaluateDash`, tir, soutien du porteur. `GetTeamIndex`, `IsOpponent`, `GetGoalToAttack`, `GetBall`, `FindBallCarrier`, `OnIntentChanged`.
- **Réglages** : `WB_SoccerAIProfile` (`DA_SoccerAIProfile_Default`).
- **Équipes** : `WB_SoccerPlayerState`, `WB_TeamLibrary`, `WB_TeamDefinitionData`. **But** : `WB_TeamGoalComponent`. **Possession** : `WB_BallPossessionSubsystem`.
- **Commandes publiques** sur `BP_GenericCharacter` ; `RequestPass` arrive à l'étape 15. Mode Défense (`Player.State.Defending`).
- **Grab / lancer** : `BPC_GrabObject` (GrabMode Centered / AtPoint). Tag `NoGrab` à vérifier au lot 0 (existe-t-il ? Gameplay Tag ou tag d'acteur ?).

## Architecture proposée

### Manager d'équipe
- **`WB_TeamCoordinatorComponent`** (C++, un par équipe). Seul à décider des rôles.
- Connaît ses 3 membres, les 3 adversaires, le ballon et son porteur, les deux buts et leurs boîtes de gardien.
- **Situation d'équipe** : `Defending` / `Attacking`.
- **Réévaluation** : timer (0,25-0,5 s) + événements (changement de porteur, but, passe, changement de personnage, Stunned / Down).
- **Attribution** : Attaquant = le plus proche du ballon (ou le porteur) ; Gardien = le plus proche de son but parmi les deux autres ; Défenseur = le dernier. Règle « 1 NPC presse toujours l'attaquant adverse » appliquée par-dessus. **Hystérésis** contre le clignotement des rôles.
- Diffuse `OnRolesChanged` ; chaque `WB_SoccerAIController_Base` reçoit son rôle (`SetRole`).
- **Mémoire de barricade** : 3 objets au plus ; un objet poussé hors de la zone sort de la liste.

### Côté NPC
- `WB_SoccerAIController_Base` étendu : `CurrentRole`, intention selon le rôle et la situation.
- Intentions à ajouter : `Defend`, `GuardGoal`, `SupportOpen`, `LastMan`, `ThrowAtTarget` (Défenseur : scan 5 m, `NoGrab` exclu, priorité des cibles, 4 s), `Barricade`, `ClearBarricade`, `ReceivePass`.
- **Reprise d'un personnage en cours d'action** : attendre l'état Free ; s'il tient un objet, s'en servir ou le lâcher selon le rôle.
- **Réglages** (profil, section par rôle) : 3 m du gardien, multiplicateur du rayon défensif, distance de l'angle mort, chance de se placer devant le ballon, rayon de recherche d'objets (5 m), distance « ballon loin », taille minimale des objets de barricade, force minimale, 3 objets max, délai de lancer (4 s), plage de force.

## Découpage proposé (lots)
0. Inventaire : code C++ de `WB_SoccerAIController_Base`, `BPC_GrabObject` (prise et lancer par l'IA, force minimale), tag `NoGrab`, boîte de gardien, état de l'étape 15.
1. Manager d'équipe : situation, attribution, règle « 1 NPC presse l'attaquant adverse », hystérésis, debug à l'écran.
2. Rôles dans l'IA : `CurrentRole` / `SetRole`, intentions de base.
3. Gardien : boîte, mode Défense automatique, 3 m, `LastMan`.
4. Défenseur : rayon, angle mort, placement devant le ballon.
5. Objets : scan 5 m, lancer par priorité, barricade, retrait.
6. Réglages, tests à 1, 2 et 4 humains, `stat unit`, rapport.

Les lots qui touchent un Blueprint suivent `claude/guide-ergonomie-blueprint.md` et se terminent par le rapport de sa section 8. Le C++ suit le `CLAUDE.md` du projet.

## Points ouverts
Aucun (2026-11-19, 20 h 26). Prêt à remettre à Claude++ après l'étape 15.

## Historique — passe et changement de personnage
Règles déplacées au lot 3 de `claude/devis-match-3v3.md` (étape 15).
