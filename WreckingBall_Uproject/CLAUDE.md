# Wrecking Ball — consignes pour Claude Code

Réponds en français. Les termes Unreal (Blueprint, Event, Cast, Branch…) restent en anglais.

## Le jeu
Jeu de soccer en multijoueur local, de 2 à 4 joueurs, dans des lieux improvisés (maison, école, musée…). Le plaisir vient de la compétition et de la destruction du décor : murs, pots de fleurs, aquariums…

## Le projet
- **Unreal Engine 5.8**, projet `WreckingBall.uproject`. La racine du dépôt Git est un dossier parent de `WreckingBall_Uproject`.
- Le projet est **surtout en Blueprint**. Un module C++ a été ajouté le 2026-11-13 pour le lot 9 du devis destruction (gestionnaire de débris morceau par morceau, parce que Blueprint n'a pas accès aux morceaux d'une Geometry Collection).
- **Les docs de référence vivent dans le projet claude.ai « Wrecking Ball - Alagaesia »** (devis maître, devis destruction et ses lots, backlog). Tu n'y as pas accès. Si une tâche dépend d'un devis, demande à l'utilisateur de coller la section utile.
- Copie locale du guide d'ergonomie Blueprint : `Docs/guide-ergonomie-blueprint.md`. **Lecture seule** : l'original est dans le projet claude.ai et se modifie uniquement dans la conversation dédiée au guide. Si tu trouves qu'une règle devrait changer, propose-le à l'utilisateur au lieu de modifier la copie.

## C++
- Module `WreckingBall`, macro d'export `WRECKINGBALL_API`. Les fichiers sont dans `Source/WreckingBall/` : `WreckingBall.Build.cs`, `Public/` (fichiers `.h`) et `Private/` (fichiers `.cpp`).
- Classe des fichiers dans des sous-dossiers par système, en miroir dans `Public/` et `Private/` (ex. `Public/Blueprint/`, `Private/Blueprint/`).
- **Un `.h` placé dans un sous-dossier de `Public` s'inclut avec ce sous-dossier** : `#include "Blueprint/WB_SetupTestLibrary.h"`. L'oubli a déjà causé une erreur C1083.
- Le C++ sert de **fondation exposée à Blueprint** : `UCLASS(Blueprintable)`, `UFUNCTION(BlueprintCallable / BlueprintPure)`, `UPROPERTY(EditAnywhere, BlueprintReadWrite)`, avec toujours une `Category`. Le gameplay et le réglage restent en Blueprint et en Data Assets, sauf si l'utilisateur décide autrement.
- **Chaque `UPROPERTY` et `UFUNCTION` exposée reçoit un commentaire `/** … */` en anglais** : Unreal en fait l'infobulle dans l'éditeur, comme la Description d'une variable Blueprint.
- Nommage des classes : préfixe Unreal habituel (`U`, `A`, `F`, `E`) + nom en PascalCase. Une classe qui sert de base à plusieurs enfants prend le suffixe `_Base` (règle du projet, même chose en Blueprint).
- Nouvelle fonctionnalité dans une classe de base partagée : passe par une option booléenne **désactivée par défaut**.
- Pour les modules Chaos et Geometry Collection (lot 9), ajoute au `Build.cs` les dépendances utiles (`GeometryCollectionEngine`, `Chaos`, `ChaosSolverEngine`, `PhysicsCore`), et seulement celles qui servent vraiment.
- Classe temporaire à supprimer quand le premier vrai code du lot 9 sera en place : `UWB_SetupTestLibrary` (`Public/Blueprint/` et `Private/Blueprint/`).

## Compiler
- Toolchain : Visual Studio 2026 avec MSVC 14.50. Évite MSVC 14.51, qui retire `hash_map` et casse UE 5.8.
- Une modification dans un `.cpp` seulement : l'utilisateur recompile avec Live Coding (Ctrl+Alt+F11), éditeur ouvert.
- Une modification dans un `.h` (`UPROPERTY`, `UFUNCTION`, nouvelle classe), ou une modification du `Build.cs` : **l'éditeur doit être fermé**. Demande à l'utilisateur de le fermer avant de compiler toi-même, puis lance :
  `"C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat" WreckingBallEditor Win64 Development -Project="<chemin absolu>/WreckingBall.uproject" -WaitMutex`
- Quand la compilation échoue, l'erreur exacte est dans la sortie de Build.bat ou dans `Saved/Logs/WreckingBall.log` : cherche les lignes qui contiennent `error`.

## Blueprints
- Avant de créer ou de modifier un Blueprint, lis `Docs/guide-ergonomie-blueprint.md` et applique-le en entier. Termine par le rapport final de sa section 8.
- Les Descriptions (variables, fonctions) sont en anglais. Une variable est toujours rangée dans une Category.
- Les réglages visuels d'un acteur (meshes, matériaux, assets du profil) passent par le Construction Script, pas par le BeginPlay.
- Plus de nouveaux enums : utilise des Gameplay Tags. Les enums existants restent.

## Façon de travailler
- Une conversation par chantier ou par lot. Commit Git avant et après chaque lot. L'utilisateur teste à la fin de chaque lot.
- **Ne commite pas et ne pousse pas sans que l'utilisateur le demande.**
- Ne commite jamais ces dossiers : `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`, `.vs/`, `.vscode/`, ni les fichiers `*.sln`, `*.slnx`, `*.code-workspace`. À commiter : `Source/`, `Config/`, `Content/`, `WreckingBall.uproject`, `Docs/`, `CLAUDE.md`.
- Réponses concises : dis ce qui a changé et pourquoi, en une ligne. Les explications détaillées seulement si l'utilisateur les demande.
