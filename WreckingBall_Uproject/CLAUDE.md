# Wrecking Ball — consignes pour Claude Code

Réponds en français, dans **tous** les messages : courtes lignes pendant le travail, questions et longs rapports de fin d'action. Les termes Unreal (Blueprint, Event, Cast, Branch…), les noms d'assets et de variables, et les Descriptions restent en anglais. Relis chaque rapport avant de l'envoyer : la dérive vers l'anglais arrive surtout après beaucoup de sorties d'outils en anglais.

## Le jeu
Jeu de soccer en multijoueur local, de 2 à 4 joueurs, dans des lieux improvisés (maison, école, musée…). Le plaisir vient de la compétition et de la destruction du décor : murs, pots de fleurs, aquariums…

## Le projet
- **Unreal Engine 5.8**, projet `WreckingBall.uproject`. La racine du dépôt Git est un dossier parent de `WreckingBall_Uproject`.
- Le projet est **historiquement en Blueprint**. Un module C++ a été ajouté le 2026-11-13 (lot 9 : gestionnaire de débris morceau par morceau). Depuis le lot 9.5 (2026-11-15), le C++ porte la fondation réutilisable de la destruction (interface des dépendants, oscillation, effondrement d'une GC) ; voir la section C++ pour la répartition.
- **Les docs de référence vivent dans le projet claude.ai « Wrecking Ball - Alagaesia »** (devis maître, devis destruction et ses lots, backlog). Des copies locales des devis sont dans `Docs/Devis/` (ex. `Docs/Devis/devis-destruction-lot9.md`) : lis-les quand une tâche en dépend. **Lecture seule**, comme le guide : si un devis devrait changer, propose-le à l'utilisateur. Si le devis utile n'y est pas, demande-lui de l'ajouter ou de coller la section.
- Copie locale du guide d'ergonomie Blueprint : `Docs/guide-ergonomie-blueprint.md`. **Lecture seule** : l'original est dans le projet claude.ai et se modifie uniquement dans la conversation dédiée au guide. Si tu trouves qu'une règle devrait changer, propose-le à l'utilisateur au lieu de modifier la copie.

## C++
- Module `WreckingBall`, macro d'export `WRECKINGBALL_API`. Les fichiers sont dans `Source/WreckingBall/` : `WreckingBall.Build.cs`, `Public/` (fichiers `.h`) et `Private/` (fichiers `.cpp`).
- Classe des fichiers dans des sous-dossiers par système, en miroir dans `Public/` et `Private/` (ex. `Public/Impact/Budget/`, `Private/Impact/Budget/`).
- **Un `.h` placé dans un sous-dossier de `Public` s'inclut avec ce sous-dossier** : `#include "Impact/Budget/WB_PhysicsBudgetManager_Base.h"`. L'oubli a déjà causé une erreur C1083.
- Le C++ sert de **fondation exposée à Blueprint** : `UCLASS(Blueprintable)`, `UFUNCTION(BlueprintCallable / BlueprintPure)`, `UPROPERTY(EditAnywhere, BlueprintReadWrite)`, avec toujours une `Category`.
- **Répartition (décision de l'utilisateur, 2026-11-15, après l'essai du lot 9.5)** : le C++ a la **priorité** dès qu'il est plus efficace que Blueprint, c'est-à-dire quand au moins un de ces critères s'applique :
  - logique réutilisée par plusieurs Blueprints (composant, bibliothèque de fonctions, interface) ;
  - calcul lourd ou exécuté à chaque frame (math, Tick, animation par code) ;
  - API absente ou incomplète en Blueprint (morceaux d'une Geometry Collection, timers avec lambda, itération sur les acteurs…) ;
  - limite des outils MCP qui forcerait une création manuelle (interface, paramètre de type struct Blueprint ou objet, node `Return`, enum).
- **Blueprint reste le bon choix** pour l'assemblage (composants d'un acteur, branchements entre systèmes), le réglage (Data Assets, valeurs par défaut), le visuel (Construction Script) et ce qu'on veut itérer vite en jeu.
- **Conversion d'un asset Blueprint existant en C++** (reparenter un Blueprint, remplacer une struct Blueprint) : seulement si le gain est clair et que l'utilisateur l'approuve. Préfère ajouter du C++ à côté (composant, bibliothèque) plutôt que convertir.
- Compte aussi le coût : un `.h` modifié oblige à fermer l'éditeur et à recompiler. Regroupe les changements de `.h` d'un même sous-lot en une seule compilation.
- **Chaque `UPROPERTY` et `UFUNCTION` exposée reçoit un commentaire `/** … */` en anglais** : Unreal en fait l'infobulle dans l'éditeur, comme la Description d'une variable Blueprint.
- Nommage des classes : préfixe Unreal habituel (`U`, `A`, `F`, `E`) + nom en PascalCase. Une classe qui sert de base à plusieurs enfants prend le suffixe `_Base` (règle du projet, même chose en Blueprint).
- **Aucune valeur de réglage cachée dans le C++** (décision de l'utilisateur, 2026-11-17) : le but est que l'équipe de design puisse calibrer le jeu en tout temps, sans dépendre d'un programmeur.
  - Toute valeur qui peut affecter le gameplay ou le feel (vitesses, durées, distances, coûts, seuils, angles, courbes, couleurs d'affichage) vit dans un Data Asset (ou un profil), avec une Description en anglais.
  - Pas de repli silencieux : si un Data Asset manque, avertissement dans le log et action coupée, pas de `GetDefault<…>()` qui cache le manque.
  - Un système = un seul endroit pour ses réglages.
  - Peuvent rester dans le code : les valeurs purement techniques sans effet sur le gameplay (gardes de division, clamps 0..1, constantes mathématiques, tailles de buffers, cadences internes de back-end…).
- Nouvelle fonctionnalité dans une classe de base partagée : passe par une option booléenne **désactivée par défaut**.
- Pour les modules Chaos et Geometry Collection (lot 9), ajoute au `Build.cs` les dépendances utiles (`GeometryCollectionEngine`, `Chaos`, `ChaosSolverEngine`, `PhysicsCore`), et seulement celles qui servent vraiment.

## Compiler
- Toolchain : Visual Studio 2026 avec MSVC 14.50. Évite MSVC 14.51, qui retire `hash_map` et casse UE 5.8.
- Une modification dans un `.cpp` seulement : l'utilisateur recompile avec Live Coding (Ctrl+Alt+F11), éditeur ouvert.
- Une modification dans un `.h` (`UPROPERTY`, `UFUNCTION`, nouvelle classe), ou une modification du `Build.cs` : **l'éditeur doit être fermé**. Demande à l'utilisateur de le fermer avant de compiler toi-même, puis lance :
  `"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" WreckingBallEditor Win64 Development -Project="<chemin absolu>/WreckingBall.uproject" -WaitMutex`
- Quand la compilation échoue, l'erreur exacte est dans la sortie de Build.bat ou dans `Saved/Logs/WreckingBall.log` : cherche les lignes qui contiennent `error`.

## Blueprints
- Avant de créer ou de modifier un Blueprint, lis `Docs/guide-ergonomie-blueprint.md` et applique-le en entier. Termine par le rapport final de sa section 8.
- **Autonomie dans l'éditeur (décision de l'utilisateur, 2026-11-14)** : quand la connexion MCP à l'éditeur (`unreal-mcp`) est disponible, tu peux faire toi-même les branchements Blueprint et les réglages d'assets d'un lot, y compris ceux qu'un devis attribue à l'éditeur ou à la conversation claude.ai, sans attendre l'utilisateur. Reste le plus près possible du guide d'ergonomie. Ce qui reste à l'utilisateur : les tests en jeu, le calibrage et les commits.
- Les Descriptions (variables, fonctions) sont en anglais. Une variable est toujours rangée dans une Category.
- Les réglages visuels d'un acteur (meshes, matériaux, assets du profil) passent par le Construction Script, pas par le BeginPlay.
- Plus de nouveaux enums **Blueprint** : utilise des Gameplay Tags (les toolsets MCP ne permettent pas à l'IA de créer des enums dans l'éditeur). Les enums existants restent.
- En C++, un `UENUM(BlueprintType)` est permis pour un choix fixe et interne à une classe C++ (ex. un mode de calcul), avec une `Category`-équivalent clair et un commentaire `/** */` en anglais par valeur. Pour une catégorie de gameplay extensible (types d'objets, d'impacts…), garde les Gameplay Tags.

## Collections du Content Browser (décision de l'utilisateur, 2026-11-27)
- Les collections locales se construisent avec **WB Tools → Content → Collections**, à partir des Gameplay Tags `Collection.*`. Chaque enfant direct de `Collection` est un master tag (`Collection.Design`, `Collection.Environment`, `Collection.Maps`, `Collection.Tools`), que chaque utilisateur coche ou décoche.
- **Toute nouvelle classe de Data Asset reçoit un tag de collection** :
  - classe C++ : `UCLASS(BlueprintType, meta = (WBCollection = "Collection.Design.Player"))` ;
  - PDA Blueprint : bouton **Collections** de son éditeur, clic droit **Collection Tags...** dans le Content Browser, ou outil MCP `WB_ToolsToolset.SetCollectionTags`.
- Un asset peut avoir **plusieurs tags**. Pour chaque master tag, c'est le tag le plus proche qui compte : l'asset lui-même, puis sa classe, puis ses parents.
- Pour les Blueprints d'acteurs (`Collection.Environment`…), seuls les **derniers enfants** de chaque branche sont rangés : sans Blueprint enfant, nom qui ne finit pas par `_Base`.
- Après un tag, sauvegarde l'asset, puis lance **Update Collections** (ou `WB_ToolsToolset.UpdateCollections`).

## Maps : Outliner et sous-niveaux (décision de l'utilisateur, 2026-11-27)
- **Dossiers de l'Outliner**, toujours les mêmes à la racine, numérotés :
  - `00_Layout` : tout objet d'environnement qui meuble l'espace.
    - `01_RoughBox` : la géométrie en static mesh (`_Cube` et pareils), avec des sous-dossiers par matériau au besoin ;
    - `02_PhysicsActor` : les Blueprints de physics actors ;
    - `03_Destructible` : les Blueprints destructibles (murs, pots, aquariums…).
  - `01_Gameplay` : les éléments interactifs du jeu, chacun dans un sous-dossier descriptif numéroté.
    - `00_GameModeSettings` : réglages de partie (`BP_GymSettings`…) ;
    - `01_Goal` : les buts ;
    - puis d'autres sous-dossiers (`02_Camera`, `03_Spawn`, `04_PlayArea`…) pour les réglages de caméra, le spawn des équipes, le spawner manager, les volumes de play area, etc.
  - `02_Light` : l'éclairage et le post-process.
    - `01_Lighting` : lumières, Sky Light, Sky Atmosphere, brouillard… ;
    - `02_PostProcess` : le Post Process Volume.
  - `99_Unsorted` : ce que l'outil Sort Level (WB Tools) ne sait pas ranger. Il reste dans son niveau, avec un avertissement : à ranger à la main.
- **Sous-niveaux** : un dossier de l'Outliner correspond à un sous-niveau.
  - `SubLv_<Map>_Layout` contient tout ce qui est dans `00_Layout`. Un ancien sous-niveau `_LD` se renomme `_Layout` (propose-le à l'utilisateur quand tu en vois un).
  - `SubLv_<Map>_Gameplay` contient tout ce qui est dans `01_Gameplay`.
  - `SubLv_<Map>_Light` contient tout ce qui est dans `02_Light`.
  - D'autres sous-niveaux pourront s'ajouter.
- **Exception** : ce qui choisit le mode de jeu (`BP_GymSettings`) reste dans le **niveau persistant**, rangé dans le dossier `01_Gameplay/00_GameModeSettings`.
- Les acteurs groupés (Group Actor) ne sont pas déplacés par Sort Level : ils sont recensés dans le log.
- Quand tu places un acteur par MCP, range-le tout de suite dans le bon dossier et le bon sous-niveau (outil Sort Level du toolset WB Tools quand il est disponible).

## Façon de travailler
- Une conversation par chantier ou par lot. Commit Git avant et après chaque lot. L'utilisateur teste à la fin de chaque lot.
- **Analyse d'un devis** : avant de commencer, pour chaque étape ou sous-lot, donne une ligne « **C++ ou Blueprint** » avec la raison (critères de la section C++) et ce que ça coûte (compilation éditeur fermé, création manuelle évitée…). Signale aussi les étapes qu'on peut fusionner ou redécouper. L'utilisateur tranche avant que tu commences.
- **Ne commite pas et ne pousse pas sans que l'utilisateur le demande.**
- **Exception `Binaries/` (décision de l'utilisateur, 2026-11-17)** : `Binaries/Win64/UnrealEditor-WreckingBall.dll`, `Binaries/Win64/UnrealEditor-WreckingBallEditor.dll` (module Editor des outils MCP, ajouté le 2026-11-27), `Binaries/Win64/UnrealEditor.modules` et `Binaries/Win64/WreckingBallEditor.target` sont commités, pour que les collègues sans Visual Studio puissent ouvrir le projet. Après tout changement C++, fais une compilation complète avec `Build.bat` (un patch Live Coding ne met pas la DLL à jour) et commite la DLL avec le code source correspondant.
- Ne commite jamais ces dossiers : `Binaries/` (sauf l'exception ci-dessus), `Intermediate/`, `DerivedDataCache/`, `Saved/`, `.vs/`, `.vscode/`, ni les fichiers `*.sln`, `*.slnx`, `*.code-workspace`. À commiter : `Source/`, `Config/`, `Content/`, `WreckingBall.uproject`, `Docs/`, `CLAUDE.md`.
- Réponses concises : dis ce qui a changé et pourquoi, en une ligne. Les explications détaillées seulement si l'utilisateur les demande.
- **Pendant le travail** : avant ou juste après chaque changement, une courte ligne qui dit quel fichier, Blueprint ou asset change, et quoi. Le raisonnement interne complet n'est pas nécessaire, sauf pour un changement majeur.
- **Rapport de fin d'action** :
  - un titre par sujet, avec son type entre parenthèses à côté : **(C++)**, **(Blueprint)**, **(Data Asset)**, **(Config)** pour `Build.cs`, `Target.cs` et `.ini`, **(Git)**, **(Docs)** ;
  - sous chaque titre, des puces pour ce qui a changé ;
  - toujours une section **À vérifier / À faire de ton côté** quand quelque chose n'est pas testé ou que l'utilisateur a une action à faire ;
  - un résumé court de la démarche à la fin.
