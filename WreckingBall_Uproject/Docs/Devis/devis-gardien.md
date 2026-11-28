# Wrecking Ball — Devis Gardien (IA) — rôle à part entière

Créé le 2026-11-27 (17 h 10), révisé à 17 h 16 (points ouverts tranchés). **Prêt à remettre à Claude++.** Remplace les règles du Gardien de `claude/devis-npc.md` (zones, « devient défenseur », barricade) et le « gardien permanent » du lot 3 de `claude/devis-gym-camera-p1.md`. Les lots qui touchent un Blueprint suivent `claude/guide-ergonomie-blueprint.md` (rapport de la section 8) ; le C++ suit le `CLAUDE.md` du projet. Aucune valeur cachée dans le code : tous les réglages dans `WB_SoccerAIProfile` (section Goalkeeper), avec Category et Description, pour pouvoir les équilibrer.

## 1. Identité
- Le Gardien est un **rôle à part entière** : nouvel état `Player.State.Goalkeeping`, distinct de `Player.State.Defending` (mode Défense des humains, inchangé).
- Le coordinateur d'équipe :
  - **ne le transforme jamais en Attaquant**, même s'il est le plus proche du ballon (le Gardien est prioritaire s'il est le plus proche de son but) ;
  - **ne l'utilise jamais** pour la règle « un NPC presse toujours l'attaquant adverse ».
- Le Gardien est **passif** : il ne dash jamais sur un joueur ou un bot.

## 2. Condition « Menace » (réévaluée à chaque tick)
Vraie si, dans la zone défensive de l'équipe :
- un ennemi a le ballon ; **ou**
- un ennemi est présent et le ballon est dans la zone (libre ou non) ; **ou**
- un allié a le ballon et un ennemi est à **3 m ou moins** de lui (`ThreatNearAllyCarrierDistance` = 300).

Une menace **interrompt** immédiatement toute la branche « Pas de menace » (patrouille, transport d'objet, supporteur, soutien). S'il porte un objet, il le lâche.

## 3. Branche Menace (priorités dans l'ordre)
**P1 — Rejoindre la zone.** À plus de 5 m de son but (`GoalkeeperRunDistance` = 500) : il court. À 5 m ou moins : il marche. Ignore tout le reste.

**P2 — Se placer et regarder.**
- Position : sur la ligne ballon → centre du but, devant la boîte de gardien (distance réglable, 300 par défaut, reprise du devis NPC).
- Regard : **en continu** sur le porteur. Ballon libre : il le regarde s'il est à 5 m ou moins du filet.
- Passe détectée : délai avant de pivoter vers le nouveau porteur = 0,1 s (`PassLookDelayBase`), **+ 0,1 s** (`PassLookDelayStep`) par passe enchaînée dans les 0,3 s (`PassChainWindow`), **plafonné à 0,5 s** (`PassLookDelayMax`). Le délai revient à la base quand aucune passe n'a eu lieu pendant `PassChainWindow`.
- Réglages de difficulté : `ReactionTime`, `PositioningAccuracy`.

**P3 — Arrêter le tir** (interrompt P2 à la détection d'un kick).
- Déjà dans la trajectoire : il glisse pour rester dessus.
- Sinon, s'il est à 3 m ou moins de sa zone (`DiveMaxDistance` = 300) : plongeon à environ 25° (`DiveAngle`), avec `DiveReactionTime` et `DiveAccuracy`.
- Interception = le Gardien **garde le ballon**, puis le dégage vers l'avant.

## 4. Branche Pas de menace

### 4.1 Bouclier certifié ?
- Bouclier = un objet `Object.Size.Large` ou `XLarge` devant le filet, **ou deux objets `Medium`**.
- **Mesure (grille de rayons)** : une grille de 5 × 3 points sur l'ouverture du but ; un line trace de chaque point vers le terrain (3 à 4 m). On ne compte que les objets du bouclier. Couverture = % de rayons bloqués ; certifié si ≥ `ShieldCoverageThreshold` = **40 %**. Mesure après chaque dépôt, puis chaque fois que le Gardien revoit son bouclier (coût négligeable).
- Oui → **joueur de soutien** des attaquants (le Gardien quitte son but).

### 4.2 Patrouille
- Recherche par **ligne de vue** pendant 7 s au plus (`PatrolDuration` = 7).
- Objet `Large` / `XLarge` vu → il va le chercher tout de suite.
- À la fin des 7 s sans Gros+ : il prend le `Medium` vu **le plus proche** ; il en faut **2** pour faire un bouclier.
- Rien de `Medium` ou plus → **gardien supporteur**.
- Objets exclus : tag `NoGrab`.

### 4.3 Placement (porter, puis déposer)
1. Point cible : ligne ballon → centre du but, environ 1 m devant la boîte de gardien (`ShieldDropOffset`).
2. Prévision de chute : sweep vers le bas (forme de l'objet) depuis la position de prise ; il lâche quand le point d'impact prévu est dans la tolérance (`ShieldDropTolerance`).
3. Cible inaccessible : lancer à force minimale, trajectoire avec `PredictProjectilePath`.
4. Vérification de la couverture ; si insuffisante, il repousse l'objet ou recommence (`ShieldMaxAttempts`).
5. Dans la limite du budget de destruction et du budget physique.

### 4.4 Gardien supporteur
- Sait que son but n'est pas protégé. Reste **entre 8 et 12 m** de son but (`SupporterMinDistance` = 800, `SupporterMaxDistance` = 1200), toujours entre le ballon et le but.
- Toutes les **15 s** (`SupporterRepatrolInterval`) : il vérifie si le but est vide ; si oui, il relance une patrouille.

### 4.5 Bouclier déplacé
- Le Gardien mémorise ses objets de bouclier et leur position. Il ne constate un déplacement **que par la ligne de vue** (objet revu ailleurs, ou absent de là où il devrait être).
- Constat fait : sans menace → retour au but et patrouille ; avec menace → branche Menace.

## 5. Ligne de vue (tous les bots)
- `AIPerception` (sens Sight) sur tous les bots, quel que soit le rôle ; rayon et angle réglables **par rôle**.
- Murs et objets bloquent la vue. D'autres usages des murs comme occludants viendront plus tard.

## 6. Tailles d'objets
- Nouveaux Gameplay Tags sur les physics actors : `Object.Size.Tiny`, `Small`, `Medium`, `Large`, `XLarge`.
- Lot 0 : inventaire des physics actors existants et attribution d'une taille à chacun.

## 7. Découpage proposé (lots)
0. Inventaire : état actuel du gardien (`WB_SoccerAIController_Base`, intentions, coordinateur), `BPC_GrabObject` (porter / lâcher par l'IA), `NoGrab`, détection de passe et de kick.
1. Tags `Object.Size.*` + attribution aux objets.
2. `Player.State.Goalkeeping` + exclusions dans le coordinateur.
3. `AIPerception` Sight pour tous les bots, réglages par rôle, mémoire d'objets.
4. Branche Menace : P1, P2 (regard et délai de passe), P3 (glisse, plongeon, capture, dégagement).
5. Patrouille, placement, grille de rayons, certification.
6. Supporteur, relance à 15 s, détection de bouclier déplacé, soutien.
7. Debug à l'écran (rayons, couverture, état du gardien), tests dans un gym gardien, `stat unit`, rapport.

## 8. Points ouverts
Aucun (2026-11-27, 17 h 16). Toutes les valeurs sont des points de départ, à équilibrer en gym.
