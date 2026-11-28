# Wrecking Ball — consignes pour Claude Code

Les faits du projet partagés avec l'équipe (ouvrir le projet, maps, nommage, arborescence) sont dans le README : @../README.md

## 1. Langue et communication
- Réponds en **français**, dans **tous** les messages : courtes lignes pendant le travail, questions et rapports de fin d'action.
- Le jargon technique (Unreal, programmation, game dev) peut rester en anglais : Blueprint, Event, Cast, Branch, Tick, etc. Les noms d'assets et de variables restent en anglais.
- **Dans le projet** :
  - en **anglais** : le code, les commentaires, les Descriptions (variables, fonctions) et les messages de commit ;
  - en **français** : les docs (`Docs/`, devis, bilans, README).
- Relis chaque rapport avant de l'envoyer. La dérive vers l'anglais arrive surtout après beaucoup de sorties d'outils en anglais.
- Le format des messages et des rapports est dans `CLAUDE.local.md` (préférences du lead), quand ce fichier est présent.

## 2. Le jeu et le projet
- **Le jeu** : soccer en multijoueur local, de 2 à 4 joueurs, dans des lieux improvisés (maison, école, musée…). Le plaisir vient de la compétition et de la destruction du décor : murs, pots de fleurs, aquariums…
- **Unreal Engine 5.8**, projet `WreckingBall.uproject`. La racine du dépôt Git est le dossier parent de `WreckingBall_Uproject`.
- Le projet a commencé en Blueprint. Le C++ porte maintenant la fondation réutilisable (voir section 4) et les outils d'éditeur (WB Tools, section 7).
- **Docs de référence** : elles vivent dans le projet claude.ai « Wrecking Ball - Alagaesia » (devis maître, devis par chantier, backlog).
  - Des copies locales des devis et des bilans de lot sont dans `Docs/Devis/`. Lis-les quand une tâche en dépend.
  - **Lecture seule.** Si un devis devrait changer, propose-le au lead. Si le devis utile n'y est pas, demande-lui de l'ajouter ou de coller la section.
- **Guide d'ergonomie Blueprint** : copie locale dans `Docs/guide-ergonomie-blueprint.md`. **Lecture seule** : l'original est dans le projet claude.ai et se modifie uniquement dans la conversation dédiée au guide. Si une règle devrait changer, propose-le au lead au lieu de modifier la copie.

## 3. Façon de travailler
- Une conversation par chantier ou par lot. Le lead teste à la fin de chaque lot.
- **Analyse d'un devis** : avant de commencer, donne pour chaque étape ou sous-lot une ligne « **C++ ou Blueprint** » avec la raison (critères de la section 4) et ce que ça coûte (compilation éditeur fermé, création manuelle évitée…). Signale aussi les étapes qu'on peut fusionner ou redécouper. Le lead tranche avant que tu commences.
- **Autonomie dans l'éditeur** (décision du lead, 2026-11-14) : quand la connexion MCP à l'éditeur (`unreal-mcp`) est disponible, fais toi-même les branchements Blueprint et les réglages d'assets d'un lot, y compris ceux qu'un devis attribue à l'éditeur ou à la conversation claude.ai. Reste le plus près possible du guide d'ergonomie.
- **Fermer et rouvrir l'éditeur** : tu peux le faire toi-même (compilation, pull). Avant de fermer, sauvegarde par MCP seulement les assets que tu as modifiés. Après la compilation, rouvre l'éditeur sur la map de test.
- Ce qui reste au lead : les tests en jeu, le calibrage, et l'accord pour commiter et pousser (section 6).

## 4. C++ ou Blueprint

### Répartition (décision du lead, 2026-11-15)
- Le C++ a la **priorité** dès qu'il est plus efficace que Blueprint, c'est-à-dire quand au moins un de ces critères s'applique :
  - logique réutilisée par plusieurs Blueprints (composant, bibliothèque de fonctions, interface) ;
  - calcul lourd ou exécuté à chaque frame (math, Tick, animation par code) ;
  - API absente ou incomplète en Blueprint (morceaux d'une Geometry Collection, timers avec lambda, itération sur les acteurs…) ;
  - limite des outils MCP qui forcerait une création manuelle (interface, paramètre de type struct Blueprint ou objet, node `Return`, enum).
- **Blueprint reste le bon choix** pour l'assemblage (composants d'un acteur, branchements entre systèmes), le réglage (Data Assets, valeurs par défaut), le visuel (Construction Script) et ce qu'on veut itérer vite en jeu.
- **Convertir un asset Blueprint existant en C++** (reparenter un Blueprint, remplacer une struct Blueprint) : seulement si le gain est clair et que le lead l'approuve. Préfère ajouter du C++ à côté (composant, bibliothèque).
- Compte le coût : un `.h` modifié oblige à fermer l'éditeur et à recompiler. Regroupe les changements de `.h` d'un même sous-lot en une seule compilation.

### Les deux modules
- **`WreckingBall`** (runtime, macro `WRECKINGBALL_API`) : tout ce qui tourne en jeu (gameplay, destruction, caméra, gyms, debug en jeu).
- **`WreckingBallEditor`** (editor only, macro `WRECKINGBALLEDITOR_API`, jamais packagé) :
  - le menu WB Tools et ses panneaux Slate ;
  - les toolsets MCP ;
  - les profils et les réglages des outils ;
  - plus tard : la validation d'assets et les commandlets.
- Dépendance à sens unique : `WreckingBallEditor` → `WreckingBall`, jamais l'inverse.
- Critère simple : si le code a besoin de `UnrealEd`, `Slate` ou `ToolMenus`, ou s'il sert seulement à produire ou ranger du contenu, il va dans le module Editor.
- Le module Editor charge en `PostEngineInit` dans le `.uproject`. En `Default`, les toolsets MCP ne s'enregistrent pas.
- Nommage dans le module Editor : `SWB_<Outil>Panel` (panneau Slate), `UWB_<Outil>Library` (logique, appelable en Blueprint et depuis le toolset), `UWB_<Domaine>Toolset` (outils MCP).

### Règles C++
- Les fichiers sont dans `Source/<Module>/` : `<Module>.Build.cs`, `Public/` (`.h`) et `Private/` (`.cpp`). Classe-les dans des sous-dossiers par système, en miroir dans `Public/` et `Private/` (ex. `Public/Impact/Budget/`, `Private/Impact/Budget/`, ou `Tools/<Outil>/` dans le module Editor).
- **Un `.h` placé dans un sous-dossier de `Public` s'inclut avec ce sous-dossier** : `#include "Impact/Budget/WB_PhysicsBudgetManager_Base.h"`. L'oubli a déjà causé une erreur C1083.
- Le C++ sert de **fondation exposée à Blueprint** : `UCLASS(Blueprintable)`, `UFUNCTION(BlueprintCallable / BlueprintPure)`, `UPROPERTY(EditAnywhere, BlueprintReadWrite)`, avec toujours une `Category`.
- **Chaque `UPROPERTY` et `UFUNCTION` exposée reçoit un commentaire `/** … */` en anglais** : Unreal en fait l'infobulle dans l'éditeur, comme la Description d'une variable Blueprint.
- Nommage des classes : préfixe Unreal habituel (`U`, `A`, `F`, `E`, `S`) + nom en PascalCase. Une classe qui sert de base à plusieurs enfants prend le suffixe `_Base` (même règle en Blueprint).
- **Aucune valeur de réglage cachée dans le C++** (décision du lead, 2026-11-17) : l'équipe de design doit pouvoir calibrer le jeu en tout temps, sans dépendre d'un programmeur.
  - Toute valeur qui peut affecter le gameplay ou le feel (vitesses, durées, distances, coûts, seuils, angles, courbes, couleurs d'affichage) vit dans un Data Asset (ou un profil), avec une Description en anglais.
  - Les réglages d'un outil d'éditeur vivent dans `UWB_ToolsDeveloperSettings` (Project Settings > Editor > WB Tools), dans un Data Asset ou dans un profil de l'outil. Les préférences personnelles vont dans `UWB_ToolsUserSettings` (Editor Preferences).
  - Pas de repli silencieux : si un Data Asset manque, avertissement dans le log et action coupée, pas de `GetDefault<…>()` qui cache le manque.
  - Un système = un seul endroit pour ses réglages.
  - Peuvent rester dans le code : les valeurs purement techniques sans effet sur le gameplay (gardes de division, clamps 0..1, constantes mathématiques, tailles de buffers, cadences internes de back-end…).
- Nouvelle fonctionnalité dans une classe de base partagée : passe par une option booléenne **désactivée par défaut**.
- `Build.cs` : ajoute seulement les dépendances qui servent vraiment, avec un commentaire qui dit pour quoi (ex. `GeometryCollectionEngine`, `Chaos`, `ChaosSolverEngine`, `PhysicsCore` pour la destruction).
- Un `UENUM(BlueprintType)` est permis pour un choix fixe et interne à une classe C++ (ex. un mode de calcul) : chaque valeur reçoit un commentaire `/** */` en anglais et, au besoin, un `UMETA(DisplayName = "…")` lisible dans l'éditeur. Pour une catégorie de gameplay extensible (types d'objets, d'impacts…), garde les Gameplay Tags.

### Règles Blueprint
- Avant de créer ou de modifier un Blueprint, lis `Docs/guide-ergonomie-blueprint.md` et applique-le en entier. Termine par le rapport final de sa section 8.
- Les Descriptions (variables, fonctions) sont en anglais. Une variable est toujours rangée dans une Category.
- Les réglages visuels d'un acteur (meshes, matériaux, assets du profil) passent par le Construction Script, pas par le BeginPlay.
- Plus de nouveaux enums **Blueprint** : utilise des Gameplay Tags (les toolsets MCP ne permettent pas de créer des enums dans l'éditeur). Les enums existants restent.

## 5. Compiler
- Toolchain : Visual Studio 2026 avec MSVC 14.50. Évite MSVC 14.51, qui retire `hash_map` et casse UE 5.8.
- **Modification dans un `.cpp` seulement** : Live Coding (Ctrl+Alt+F11), éditeur ouvert. Un patch Live Coding ne met pas la DLL à jour : avant un commit, fais une compilation complète.
- **Modification dans un `.h`** (`UPROPERTY`, `UFUNCTION`, nouvelle classe) **ou du `Build.cs`** : l'éditeur doit être fermé. Ferme-le toi-même (section 3), puis lance :
  `"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" WreckingBallEditor Win64 Development -Project="<chemin absolu>/WreckingBall.uproject" -WaitMutex`
- Quand la compilation échoue, l'erreur exacte est dans la sortie de Build.bat ou dans `Saved/Logs/WreckingBall.log` : cherche les lignes qui contiennent `error`.

## 6. Git
- **Le lead autorise ou refuse chaque commit et chaque push.** Il peut les faire lui-même ou te demander de les faire. Ne commite et ne pousse que s'il le demande, et n'enchaîne jamais un pull ou un push à un commit sans demande explicite.
- Un commit avant et un commit après chaque lot, à proposer au lead.
- Ferme l'éditeur avant un pull : les maps et les assets binaires ne doivent pas être ouverts pendant la mise à jour.
- **À commiter** : `Source/`, `Config/`, `Content/`, `WreckingBall.uproject`, `Docs/`, `CLAUDE.md`, le `README.md` de la racine.
- **Exception `Binaries/`** (décision du lead, 2026-11-17) : `Binaries/Win64/UnrealEditor-WreckingBall.dll`, `Binaries/Win64/UnrealEditor-WreckingBallEditor.dll`, `Binaries/Win64/UnrealEditor.modules` et `Binaries/Win64/WreckingBallEditor.target` sont commités, pour que les collègues sans Visual Studio puissent ouvrir le projet. Après tout changement C++, fais une compilation complète avec `Build.bat` et commite la DLL avec le code source correspondant.
- **Ne commite jamais** : `Binaries/` (sauf l'exception ci-dessus), `Intermediate/`, `DerivedDataCache/`, `Saved/`, `.vs/`, `.vscode/`, `CLAUDE.local.md`, ni les fichiers `*.sln`, `*.slnx`, `*.code-workspace`.

## 7. Conventions de contenu

### Outils WB Tools
Le menu **WB Tools** de l'éditeur regroupe les outils de l'équipe. Les mêmes outils sont exposés au MCP dans `WreckingBallEditor.WB_ToolsToolset` (retarget : `WreckingBallEditor.WB_RetargetToolset`). Ils travaillent sur la map ouverte dans l'éditeur. Leurs réglages sont dans Project Settings > Editor > WB Tools ; les préférences personnelles, dans Editor Preferences > WB Tools.

| Outil (menu) | Ce qu'il fait | Outils MCP | Quand t'en servir |
|---|---|---|---|
| Level > **Create Level** | Crée une map à partir de `Lv_Template` : code de 3 chiffres (officielle, `/Game/Level/Game`) ou de 3 lettres (gym, `/Game/Level/GYM`, gym mode activé), et un nom. Duplique les sous-niveaux du template. N'écrase jamais rien. | `CheckNewLevel`, `CreateLevel` | Toute nouvelle map. Sauvegarde la map ouverte avant `CreateLevel`. |
| Level > **Sort Level** | Range chaque acteur de la map dans son dossier de l'Outliner et son sous-niveau, selon `DA_SortLevelRules`. Les acteurs groupés ou attachés ne bougent pas, ils sont listés. | `PreviewSortLevel`, `ApplySortLevel` | Après avoir placé des acteurs. Preview d'abord, puis sauvegarde la map. |
| Content > **Collections** | Construit les collections locales du Content Browser à partir des tags `Collection.*`. | `GetCollectionTags`, `SetCollectionTags`, `PreviewCollections`, `UpdateCollections` | Après avoir créé une classe de Data Asset ou un Blueprint d'acteur final. |
| Animation > **Retarget** | Copie des animations d'un personnage à un autre en 4 étapes : IK Rigs, retargeter, mapping des chaînes, batch retarget, puis les retouches du profil du personnage cible (`UWB_RetargetProfile`). | `WB_RetargetToolset` (IK Rig, retargeter, `RetargetAnimations`, `OffsetBoneRotation`, `ApplyRetargetProfile`…) | Nouveau personnage ou nouvelles anims. N'applique jamais un profil deux fois aux mêmes anims : les rotations s'additionnent. |
| Gym > **Gym Mode** | Transforme la map ouverte en gym (`GM_Gym` en GameMode Override, `BP_GymSettings` dans le niveau persistant) et choisit le gym du prochain Play. Liste aussi les maps du projet par groupe. Préférence perso : démarrer le Play avec la caméra du joueur 1. | `GetGymModeState`, `GetGymDefinitions`, `SetGymModeEnabled`, `SetPlayer1Camera` | Pour tester un comportement dans un gym. Ne commite pas une map hors du dossier des gyms avec le gym mode activé. |
| Debug > **Debug Keys & Commands** | Liste les touches de debug des Blueprints et les commandes console `WB.*`, avec leurs descriptions et les conflits. Toujours relue dans le projet, jamais tenue à la main. | `GetDebugKeys`, `SetDebugKeyDescription` | Avant de choisir une touche de debug ; documente chaque nouvelle touche. |
| Entrées en plus | Les Editor Utility Widgets et les scripts Python ajoutés dans `DA_WBToolsMenu`, sans recompiler. | `RunWBToolsScript` (scripts de `Content/Python/WBTools/`) | Les étapes qui ont besoin de l'éditeur ouvert (déplacer des acteurs entre niveaux…). |

### Collections du Content Browser (décision du lead, 2026-11-27)
- Chaque enfant direct de `Collection` est un master tag (`Collection.Design`, `Collection.Environment`, `Collection.Maps`, `Collection.Tools`), que chaque membre de l'équipe coche ou décoche pour lui.
- **Toute nouvelle classe de Data Asset reçoit un tag de collection** :
  - classe C++ : `UCLASS(BlueprintType, meta = (WBCollection = "Collection.Design.Player"))` ;
  - PDA Blueprint : bouton **Collections** de son éditeur, clic droit **Collection Tags...** dans le Content Browser, ou `SetCollectionTags`.
- Un asset peut avoir **plusieurs tags**. Pour chaque master tag, c'est le tag le plus proche qui compte : l'asset lui-même, puis sa classe, puis ses parents.
- Pour les Blueprints d'acteurs (`Collection.Environment`…), seuls les **derniers enfants** de chaque branche sont rangés : sans Blueprint enfant, nom qui ne finit pas par `_Base`.
- Les maps sont rangées d'après leur nom (Official, Gyms, Unsorted).
- Après un tag, sauvegarde l'asset, puis lance **Update Collections**.

### Maps : Outliner et sous-niveaux (décision du lead, 2026-11-27)
L'outil Sort Level applique ces règles à partir de `DA_SortLevelRules`. Si le Data Asset et cette section ne disent pas la même chose, signale-le au lead.
- **Dossiers de l'Outliner**, toujours les mêmes à la racine, numérotés :
  - `00_Layout` : tout objet d'environnement qui meuble l'espace.
    - `01_RoughBox` : la géométrie en static mesh (`_Cube` et pareils), avec des sous-dossiers par matériau au besoin ;
    - `02_PhysicsActor` : les Blueprints de physics actors ;
    - `03_Destructible` : les Blueprints destructibles (murs, pots, aquariums…).
  - `01_Gameplay` : les éléments interactifs du jeu, chacun dans un sous-dossier descriptif numéroté : `00_GameModeSettings` (`BP_GymSettings`…), `01_Goal`, puis `02_Camera`, `03_Spawn`, `04_PlayArea`, etc.
  - `02_Light` : `01_Lighting` (lumières, Sky Light, Sky Atmosphere, brouillard…) et `02_PostProcess` (le Post Process Volume).
  - `99_Unsorted` : ce que Sort Level ne sait pas ranger. Il reste dans son niveau, avec un avertissement : à ranger à la main.
- **Sous-niveaux** : un dossier racine de l'Outliner = un sous-niveau : `SubLv_<Map>_Layout` (`00_Layout`), `SubLv_<Map>_Gameplay` (`01_Gameplay`), `SubLv_<Map>_Light` (`02_Light`). D'autres pourront s'ajouter. Un ancien sous-niveau `_LD` se renomme `_Layout` : propose-le au lead quand tu en vois un.
- **Exception** : ce qui choisit le mode de jeu (`BP_GymSettings`) reste dans le **niveau persistant**, dans `01_Gameplay/00_GameModeSettings`.
- Quand tu places un acteur par MCP, range-le tout de suite avec Sort Level.
