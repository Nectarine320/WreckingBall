# Plan — Bot IA (debug, futur game feature)

Date : 2026-11-17. Niveau de test : `Content/Level/Art/Lvl_Destruction_Gym.umap`.

## Objectif
- Pavé numérique `+` : ajoute un **joueur local** (PlayerController + pawn), jusqu'à 4 joueurs au total.
- Pavé numérique `-` : retire le dernier joueur ajouté, sans descendre sous 2 (P1 et P2 restent).
- Pavé numérique `*` : interrupteur. À l'activation, chaque joueur local **sans manette assignée** est possédé par un `AIController`. P1 (clavier) reste toujours humain. À la désactivation, chaque pawn retourne à son PlayerController.
- Spawn : sur un PlayerStart libre ; sinon à côté de P1.
- Équipe : équilibrée à l'ajout (index 0 = rouge, 1 = bleu).
- Les touches sont actives seulement hors build distribué ; le code compile dans tous les builds.

## Comportement du bot (v1)
1. **Inactif** quand l'état du joueur bloque les actions (Disabled, Down, Celebrating…).
2. **Chercher le ballon** : quand le ballon est libre, il y va sur le NavMesh puis fait `Grab` à portée.
3. **Poursuivre le porteur** : quand un adversaire a le ballon, il le poursuit ; à 3-6 m, il fait `Dash` vers lui avec un écart aléatoire de ±15° (un dash peut rater).
4. **Attaquer** : quand il a le ballon, il va à 5-15 m du but adverse, s'oriente vers le but, charge selon la distance, puis tire.
- But adverse : celui dont l'équipe qui défend ≠ l'équipe du bot (`Is Red Team` = but défendu par les rouges).
- Hors v1 : sprint, plongeon, objets physiques.

## Constats techniques
- Commandes existantes dans BP_GenericCharacter : `RequestMove(X,Y)` (axes de stick relatifs à la caméra partagée), `RequestDash`, `RequestGrab`, `RequestKickOrThrow`, `ReleaseKickOrThrow`.
- Le tir part dans l'axe avant de la capsule (`BPC_GrabObject.ThrowPhysicActor`).
- Possession du ballon : `WB_PlayerStateComponent.GetCurrentHolding` = `Player.Holding.Ball`.
- Équipe du pawn : `WB_PlayerHitComponent.TeamIndex`.
- `RegisterMatchPlayer` fonctionne par index de joueur local ; à adapter pour recevoir un pawn.

## Lot A — Fondation (C++) — une compilation, éditeur fermé
Dossiers `Public/AI/` et `Private/AI/` (+ `Public/Player/Command/`, `Public/Match/`).
- `UWB_PlayerCommandComponent` : `Move(WorldDirection)`, `Dash()`, `Grab()`, `KickPress()`, `KickRelease()`, `FaceDirection()`. Convertit la direction monde en axes de la caméra partagée ; diffuse des delegates que le Blueprint branche sur les `Request*`.
- `UWB_TeamGoalComponent` : `DefendingTeamIndex`, `GetAimLocation()`.
- `UWB_SoccerAIProfile` (Data Asset) : distances de tir, portée du dash, écart du dash, cooldown, temps de charge selon la distance, rayon de ramassage, rayon d'acceptation.
- `AWB_SoccerAIController_Base` (AAIController + `IGenericTeamAgentInterface`) : machine à états en Tick, chemins avec `FindPathToLocationSynchronously`, décisions en `BlueprintNativeEvent`.
- `UWB_SoccerBotManagerComponent` (sur le GameMode) : `AddPlayer`, `RemoveLastPlayer`, `ToggleBots`, détection de la manette par `IPlatformInputDeviceMapper`.
- `Build.cs` : `AIModule`, `NavigationSystem`, `ApplicationCore` si nécessaire pour les périphériques.

## Lot B — Branchements (Blueprint, MCP)
- BP_GenericCharacter : `PlayerCommandComponent` + delegates → `Request*`.
- BP_GoalPost_01 : `TeamGoalComponent`, rempli depuis `Is Red Team` dans le Construction Script.
- BP_SoccerGamemode : `BotManagerComponent` ; `RegisterMatchPlayer` reçoit un pawn.
- BP_TP_SoccerPlayerController : touches `Add` / `Subtract` / `Multiply` du pavé numérique.
- `DA_SoccerAIProfile_Default`.

## Lot C — Niveau et config
- Lvl_Destruction_Gym : NavMeshBoundsVolume.
- `DefaultEngine.ini` : NavMesh Runtime Generation = Dynamic (murs détruits, débris).

## État (2026-11-17)
- Lots A, B et C faits. `RuntimeGeneration=Dynamic` était déjà dans `DefaultEngine.ini`.
- Ajout imprévu : BP_SoccerCamera parcourt `Players` du GameMode au lieu de `GetPlayerPawn(i)`, qui renvoie None pour un joueur piloté par un bot.
- Le volume de NavMesh couvre seulement la salle principale (41 × 41 m).

## Dash favorable (2026-11-17)
Le bot ne dash sur le porteur que si toutes ces conditions sont réunies (`EvaluateDash`, raison affichée avec `bDrawDebug`) :
- **Soi** : cooldown fini, `CanEnterState(Dashing)`, pas épuisé (`WalkType` ≠ Exhausted), énergie ≥ coût du Dash + `DashEnergyReserve`.
- **Cible touchable** : `UWB_PlayerHitComponent::CanBeDashedBy` (mêmes règles que `ReceiveDashHit` : grâce, cooldown, friendly fire, table d'états).
- **Anticipation** : vise `position + vitesse × temps de trajet du Dash × DashLeadFactor` ; cette position doit être dans `(DashScanDistance + rayons des capsules) × DashReachRatio` et au-delà de `DashMinRunUpDistancePlayer + rayons + DashRunUpMargin`.
- **Ligne dégagée** : sweep de la capsule ; bloque sur un mur ou un objet non simulé, ou sur un coéquipier qui serait mis Down. Ballon et objets physiques ignorés.
- Sinon : poursuit vers la position anticipée ; trop près : recule.
- Le Dash réel fait 3,5 m (`DashScanDistance`) : l'ancienne fenêtre 3-6 m (`DashMinDistance` / `DashMaxDistance`) est retirée.
- Les valeurs du pawn (énergie, Dash) sont lues par nom de variable Blueprint (`UWB_PlayerCommandComponent`, catégorie Readout). À remplacer quand l'énergie passera en C++.

## Tests (utilisateur)
- `+` ×2 → 4 joueurs, équipes équilibrées ; `-` → retour à 2.
- `*` → P2 sans manette devient bot ; `*` à nouveau → il redevient joueur.
- Le bot récupère le ballon, dash sur P1 porteur (certains dashs ratent), tire de 5-15 m dans le bon but.
