# Wrecking Ball — Devis « Match 3 contre 3 » (phase E, étape 15)

Brouillon du 2026-11-19 (19 h 46). Réponses de l'utilisateur intégrées à 20 h 09 et **20 h 20**. Remis à Claude++ le 2026-11-19.

> **État au 2026-11-20 : fait et testé par l'utilisateur.** Lots regroupés (A = 1 + 2 + 3, B = 4 + 5). Écarts au texte ci-dessous, décisions du chantier et points ouverts : voir `bilan-match-3v3.md`. Principaux écarts : changement de personnage **à la réception** de la passe (2ᵉ appui = prise immédiate du receveur) ; **reprise automatique du COM allié qui prend le ballon** (`bTakeBotCarrier`) ; caméra avec **porteur dans le tiers opposé** à sa direction (Data Asset caméra) ; **indicateurs aussi pour les COM** (sans texte), taille selon la distance, contour blanc ; prise du ballon au contact de la capsule ; ballon du coup d'envoi posé au sol.

## Contexte
Le jeu devient un soccer plus traditionnel : **toujours 2 équipes de 3 personnages** (6 sur le terrain). Les humains (1 à 4) prennent un personnage de leur équipe ; les autres sont contrôlés par l'IA. La destruction reste au cœur du plaisir.

Ce devis prend la partie « match » de l'ancien devis NPC : 6 personnages, spawn, passe et changement de personnage. **Les 3 rôles coordonnés sont la phase F** (`claude/devis-npc.md`). En attendant, les personnages non contrôlés utilisent l'IA actuelle (`WB_SoccerAIController_Base`).

**Menus (design pending, étape 17)** : sélection du nombre de joueurs et des équipes à passer au 3 contre 3. Hors devis.

**Couleurs d'équipe** : pour ce devis, **bleu** et **rouge**. Plus tard, les couleurs seront **au choix** (étape 18) : ne rien coder en dur, lire la couleur dans la définition d'équipe (`WB_TeamDefinitionData`) partout (étiquette, anneau, indicateurs hors écran).

## Ordre des lots
| Lot | Contenu | Priorité |
| --- | --- | --- |
| 0 | État des lieux (lecture seule) | — |
| 1 | 6 personnages au lancement, attribution des manettes | **Haute** |
| 2 | `BP_TeamSpawnPoint` (un par équipe), point principal, repli autour de (0, 0, 0) | **Haute** |
| 3 | Passe et changement de personnage (un seul bouton) | Haute |
| 4 | Caméra centrée sur le ballon et son porteur | Moyenne |
| 5 | Indicateurs des joueurs hors écran (sphères P1…P4) | Moyenne |
| V | Tests de l'utilisateur, rapport final | — |

---

## Lot 0 — État des lieux
Lire sans modifier :
- `BP_SoccerGamemode` + `WB_SoccerBotManagerComponent` : création des joueurs locaux, des bots, placement sur Player Start libre, `ToggleBots` / `SetBotsActive`.
- Règle actuelle d'équipe (ID pair / impair, `GetTeamForPlayer`, `GI_Core`, `WB_SoccerPlayerState`, `WB_TeamLibrary`), **numérotation des ID (P1 = 0 ou 1 ?)**, et d'où vient la couleur d'équipe aujourd'hui.
- `BP_SoccerCamera` : liste des cibles cadrées, calcul du zoom.
- `WB_BallPossessionSubsystem` : porteur, événements disponibles.
- `BP_SpawnManager.SpawnMatchBall` : où le ballon apparaît aujourd'hui.
- Ce qui dépend du contrôleur dans `BP_GenericCharacter` / `BP_SoccerPlayer` (charge du kick, sprint, Défense, widget de tête) et doit être relâché au `UnPossess`.

## Lot 1 — 6 personnages au lancement
- Au début du match, le GameMode s'assure qu'il y a **exactement 6 personnages** : 3 par équipe.
- **Manettes déjà actives** → chacune reçoit un personnage. **ID impair = équipe rouge, ID pair = équipe bleue** (validé le 2026-11-19, 20 h 20). La numérotation des ID (P1 = 0 ou 1) reste celle du code actuel, relevée au lot 0.
- **L'humain prend toujours la première place** de son équipe (place 1). Deux humains dans la même équipe : places 1 et 2, dans l'ordre des ID.
- Places restantes → personnages contrôlés par l'IA (`BP_SoccerAIController`).
- Équipe lue dans le `PlayerState` (une seule source) ; étiquette P1…P4 / COM inchangée.
- **`ToggleBots` est conservé** : l'IA **ne démarre pas** au Play (debug). Les 6 personnages sont créés, ceux de l'IA restent immobiles jusqu'à `ToggleBots`. Option : `StartBotsOnPlay` (faux par défaut) dans un réglage du GameMode.

## Lot 2 — `BP_TeamSpawnPoint`
- **Un Blueprint par équipe** (une instance posée par équipe dans la map), avec l'équipe en réglage. Il garantit la zone où les personnages de l'équipe apparaissent.
- Il contient **3 points de place** (composants déplaçables, place 1 à 3) et **un point principal**. Le ballon peut apparaître au point principal (voir plus bas).
- Chaque personnage apparaît **toujours** à sa place, au lancement et après chaque but.
- Aperçu dans l'éditeur (Construction Script) : couleur de l'équipe, numéro de place, marqueur du point principal.
- **Coup d'envoi (2026-11-19, 20 h 20)** : **pour le moment, le ballon apparaît en (0, 0, 0)**, au lancement et après un but. Le point principal existe déjà dans le Blueprint (réglage `UseMainPointForBall`, faux par défaut) pour passer plus tard au ballon chez l'équipe qui a encaissé.
- **Repli** : pas de `BP_TeamSpawnPoint` pour une équipe, ou places manquantes → personnages **autour de (0, 0, 0), côte à côte sur le NavMesh disponible** (pas superposés), par exemple des points sur un cercle projetés sur le NavMesh. Avertissement dans le log, pas d'erreur bloquante. Le ballon reste au centre ; les personnages ne doivent pas apparaître sur lui.
- À poser dans `Lv_998_GameplayGym`, `Lvl_Destruction_Gym` et plus tard `Lv_100_Bungalow` (étape 13).

## Lot 3 — Passe et changement de personnage
Règles validées le 2026-11-18 et le 2026-11-19 :
- **A** (manette) / **Q** et **TAB** (clavier). Sans le ballon = changer de personnage ; avec le ballon = **passe + prise du receveur, au même appui**. **Le receveur devient le nouveau détenteur du ballon** (`WB_BallPossessionSubsystem`).
- Ciblage directionnel par le stick ; stick au neutre = allié le plus proche ; un seul allié libre = pris d'office ; jamais un adversaire.
- Allié contrôlé par un autre humain : la passe part, pas de changement. **Pas de son de refus** (pour le moment).
- Personnage quitté : garde son état, passe à l'IA. Délai minimum entre deux changements : 0,2 s (Config).
- Passe : ballon au sol, ~40 % du kick, augmentée si nécessaire, plafonnée au kick ; visée version A (position anticipée), version B en test (`PassAimMode`). Interceptable, obstacles conservés.
- **Son de réception** : à **chaque prise de possession** du ballon.
- **Receveur IA** : démarquage devant le ballon = **phase F**. Ici, rien de spécial.
- Mise en œuvre : `FindDirectionalAlly(Direction)` (C++, `WB_TeamLibrary.AreTeammates`), `IA_SwitchOrPass` dans `IMC_SoccerDefault`, `RequestPass(Target)` dans `WB_PlayerCommandComponent`, `UnPossess` / `Possess` + AIController pour le personnage quitté (qui respecte `ToggleBots`).

## Lot 4 — Caméra : ballon et porteur
Remplace le comportement actuel (tous les joueurs + le ballon dans le cadre).
- **Ballon tenu** : cadre le **ballon et son porteur**.
- **Ballon libre** : cadre **seulement le ballon**.
- **Les autres humains n'ont aucun poids dans le cadrage** (pour le moment) → lot 5.
- Ballon hors terrain : caméra figée sur sa dernière position valide jusqu'au nouveau ballon (`BP_SpawnManager.SpawnMatchBall`). Absorbe la ligne caméra de `claude/backlog-ajustements.md`.
- Corrige le backlog `IsValid` sur `GetPlayerPawn` (« Accessed None » à chaque tick).
- Réglages dans un Data Asset (zoom min / max, lissage, avance vers la direction du ballon).

## Lot 5 — Indicateurs hors écran
- Pour chaque **humain** (P1…P4) dont le personnage sort du cadre : une **sphère à la couleur de l'équipe**, avec **« P1 », « P2 », « P3 » ou « P4 »** à l'intérieur, collée à la **bordure de l'écran** dans la direction du personnage.
- Disparaît quand le personnage revient à l'écran.
- **Pas d'indicateur pour les COM** (pour le moment).
- Activable / désactivable (réglage + fonction publique).
- Pool de 4 indicateurs, mis à jour seulement pour ceux qui sont hors écran ; couleur lue dans la définition d'équipe.

## Lot V — Tests
1. 1, 2, 3 et 4 manettes : toujours 6 personnages, 3 par équipe ; ID impair = rouge, pair = bleu ; humains en place 1 ; P1…P4 / COM corrects.
2. Au Play, l'IA ne bouge pas ; `ToggleBots` la démarre.
3. Deux `BP_TeamSpawnPoint` : chacun à sa place, ballon en (0, 0, 0). Sans spawn point : personnages côte à côte autour de (0, 0, 0) sur le NavMesh, pas sur le ballon, + avertissement.
4. Remise en jeu après un but : retour aux places, ballon en (0, 0, 0).
5. Passe : le receveur reçoit, devient détenteur et est pris par le passeur ; son de réception à chaque prise ; pas de son de refus.
6. Caméra : ballon + porteur ; ballon libre = ballon seul ; ballon hors terrain = caméra figée puis nouveau ballon.
7. Indicateurs : sphère P1…P4 à la couleur de l'équipe sur la bonne bordure ; rien pour les COM ; désactivation.
8. PIE sans « Accessed None », compile avec avertissements comme erreurs ; guide d'ergonomie appliqué, rapport de la section 8.
