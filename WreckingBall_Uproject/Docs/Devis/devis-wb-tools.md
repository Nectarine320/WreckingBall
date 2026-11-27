# Devis — Lot « WB Tools » (outils d'éditeur)

Reçu le 2026-11-27 (message de Claude++ relayé par l'utilisateur). **À faire après le mandat gym et caméra**, terminé le 2026-11-27 (`bilan-gym-camera-p1`). Un seul lot, **une seule compilation, éditeur fermé**. Mis à jour le 2026-11-27 avec ce bilan.

## 1. Menu WB Tools
- Menu dans la barre principale de l'éditeur, à côté de Help. C++, module `WreckingBallEditor` (via `UToolMenus`).
- Sections : Level, Animation, Gym, Debug (extensibles).
- Une entrée peut lancer : une fonction C++, un Editor Utility Widget, ou un script Python.
- Option : entrées lues dans un Data Asset (ex. `DA_WBToolsMenu`) → un nouvel EUW s'ajoute sans recompiler.

## 2. Sort Level
- Range les acteurs selon la règle Outliner et sous-niveaux de `CLAUDE.md` : dossiers `00_Layout`, `01_Gameplay` ; sous-niveaux `_Layout`, `_Gameplay`, `_Light`.
- Règles (classe → dossier → sous-niveau) dans un Data Asset modifiable par le design.
- Aperçu de ce qui va bouger avant d'appliquer.
- Acteur sans règle → `_Unsorted`, avec avertissement.
- Règle d'exception : `BP_GymSettings` reste dans le niveau persistant (voir point 5) ; Sort Level ne le déplace pas.
- Exposé à MCP (toolset) pour que Claude l'appelle après avoir placé des acteurs.

## 3. Retarget
- Entrée WB Tools → Animation qui ouvre `WB_RetargetToolset` : l'équipe lance elle-même le retarget en lot (IK Rig, Retargeter, batch).

## 4. Renommage lié
- Sous-niveaux `_LD` → `_Layout` dans `Lv_000_Soccerfield` et `/Game/Level/GYM/LV_AAA_Gym_SoccerField` (sous-niveaux dans `SubLVL/`).
- Constat du chantier gym : MCP ne peut pas changer `WorldAsset` d'un sous-niveau (lecture seule). Le renommage et la remise à jour des liens du niveau persistant passent par un **script Python lancé éditeur fermé** (`remove_level_from_world` puis `add_level_to_world`), comme pour la copie de Soccer Field. À faire dans la même session éditeur fermé que la compilation.

## 5. Sélecteur de gym (ajouté le 2026-11-27, demande de l'utilisateur)

### Intention
**Toute map du jeu peut devenir une gym** pour tester et débugger. La map dupliquée (`/Game/Level/GYM/…`) reste justifiée pour l'instant, mais elle ne passe pas à l'échelle de la production : à terme, on teste directement dans les vraies maps.

### Existant (chantier gym, 2026-11-27)
- `AWB_GymSettings` (C++) / `BP_GymSettings` (`/Game/Blueprint/Gym/`) : acteur posé **dans le niveau persistant**, réglage `GymDefinition`. `GM_Gym` le lit déjà au lancement (composant `UWB_GymDirectorComponent`). **Rien à changer côté jeu.**
- DataAssets de gym : classe C++ `UWB_GymDefinition`, rangées dans `/Game/Level/GYM/Data/` (`DA_Gym_01` à `06`).
- GameMode soccer par défaut : `BP_SoccerGamemode`.

### Panneau
Entrée WB Tools → Gym, qui ouvre un petit panneau (Editor Utility Widget) agissant sur **la map ouverte**.
- **Case « Mode gym activé »** :
  - **cochée** : la liste des DataAssets de classe `UWB_GymDefinition` (trouvées par l'Asset Registry, **par classe**, où qu'elles soient ; triées par `GymName`) s'affiche ; on en choisit une. Le GameMode Override des World Settings passe à `GM_Gym`.
  - **décochée** : la liste se cache, le GameMode Override revient à `BP_SoccerGamemode`.
- À l'ouverture, le panneau lit l'état réel de la map (override, `BP_GymSettings`, gym choisie) : il ne ment jamais.
- Changer de gym dans la liste s'applique tout de suite ; un PIE lancé ensuite utilise cette gym.

### `BP_GymSettings` dans la map
- **Quand on coche la case, le script :**
  1. cherche un `BP_GymSettings` dans la map (niveau persistant et sous-niveaux chargés) ;
  2. **s'il existe**, il l'utilise et écrit la DA choisie dans `GymDefinition` ;
  3. **sinon**, il le crée dans le **niveau persistant** (même endroit que dans `LV_AAA_Gym_SoccerField`), dans le dossier Outliner `01_Gameplay/Gym`, nom `GymSettings` ;
  4. s'il en trouve **plusieurs**, il utilise le premier et avertit.
- **Quand on décoche** : l'override revient à `BP_SoccerGamemode` ; l'acteur **reste** dans la map (inerte, car seul `GM_Gym` le lit), pour garder la dernière gym choisie quand on recoche.
- Avertissement si la map n'a aucun `BP_TeamSpawnPoint`.

### Limites à connaître
- Cocher ou décocher **modifie la map** (World Settings + niveau persistant) : attention à ne pas committer une map de jeu laissée en mode gym. Le panneau affiche donc un rappel tant que le mode gym est coché dans une map hors de `/Game/Level/GYM`.
- Le menu debug en jeu (catégorie GameMode : `LoadGymDefinition`, etc.) reste un autre outil : il agit en PIE, sans toucher la map.

## 6. Aide-mémoire des touches debug (ajouté le 2026-11-27, demande de l'utilisateur)
> **Étape 1, validée comme base le 2026-11-27.** L'utilisateur prévoit du fignolage ensuite : livrer cette base simple, sans chercher à anticiper ; les ajustements viendront après usage, dans un lot séparé.

Entrée WB Tools → Debug → « Touches debug », qui ouvre une fenêtre avec un tableau : **Touche | Description brève | Où (asset)**.

### Suivi automatique
La liste n'est **pas tenue à la main** : elle est reconstruite **à chaque ouverture** de la fenêtre (bouton « Rafraîchir » en plus), en scannant le projet. Une touche ajoutée, changée ou supprimée apparaît donc sans rien faire.
- **Blueprints** (`/Game`) : nodes d'événement clavier (`InputKey`, `InputDebugKey`), avec la touche et les modificateurs (Shift, Ctrl, Alt).
- **Enhanced Input** : les touches des Input Mapping Contexts de debug (préfixe ou dossier `Debug` à confirmer au lot 0), avec leur Input Action.
- **Commandes console** du projet (`WB.*`, ex. `WB.CameraMode`) : listées à part, avec leur texte d'aide.
- **Conflits** : une touche utilisée deux fois (ou déjà prise par le jeu) est surlignée.
- Cas de test connus : touche **Moins** → `DebugToggleCameraMode`, **F7** → `DebugToggleMatchTimer` (`BP_TP_SoccerPlayerController`).

### Description brève
Lue dans cet ordre, la première trouvée gagne :
1. le **commentaire du node** de l'événement clavier (bulle de commentaire), ou la `Description` de l'Input Action ;
2. sinon, le titre du Comment qui entoure le node (le guide d'ergonomie en impose déjà un par bloc d'event) ;
3. sinon, « (sans description) » en orange, pour repérer ce qu'il faut documenter.

Règle à ajouter pour l'équipe : **toute touche debug a un commentaire de node d'une ligne** (à proposer dans la conversation du guide d'ergonomie, pas modifié ici).

### Option
- Bouton « Copier en Markdown » : pour coller la liste dans un doc du projet, ou servir d'inventaire pour migrer les raccourcis vers le menu debug en jeu (étape 2 de `prep-debug-menu-gym.md`).
- Exposé à MCP, pour que Claude puisse lire la liste.

## Points à vérifier au lot 0
- Le module `WreckingBallEditor` existe-t-il déjà (`setup-cpp.md`) ? Sinon, le créer (dépend du module de jeu, qui contient `UWB_GymDefinition` et `AWB_GymSettings`).
- `WB_RetargetToolset` : existe déjà (asset/toolset) ou à créer ?
- Autres maps avec des sous-niveaux `_LD` (ex. `SubLv_999_IntroMenu_LD`) : à renommer aussi ou non ?
- Déplacer un acteur de sous-niveau change ses références (Level Blueprint, séquences, tags de binding) : l'aperçu de Sort Level doit le signaler.
- Touches debug : où vivent les raccourcis actuels (nodes clavier dans les BP, IMC de debug, ou les deux) ; temps du scan sur tout `/Game` (s'il est long, limiter aux BP de gameplay ou mettre en cache).
