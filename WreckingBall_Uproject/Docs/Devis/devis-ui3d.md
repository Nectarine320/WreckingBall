# Wrecking Ball — Devis : UI 3D en partie (étiquette de tête, anneau au sol, marqueur du ballon)

Créé le 2026-11-17 (21 h 50). **Révisé le 2026-11-17 (22 h 15)** après une première lecture dans l'éditeur (lot 0 partiel). À remettre à Claude++.
Sources : `claude/analyse-ui3d-2026-11-17.md`, lecture de l'éditeur du 2026-11-17 (22 h 05).
**Avant tout lot qui touche un Blueprint** : lire `claude/guide-ergonomie-blueprint.md` et l'appliquer. Chaque lot de ce type se termine par le rapport de la section 8 du guide.

---

## 0. En une phrase
Remplacer les deux widgets 3D de `BP_SoccerPlayer` par :
- **au sol**, un anneau en decal qui contient aussi la jauge du kick ;
- **au-dessus de la tête**, un seul widget avec l'étiquette `P1`-`P4` / `COM` et la barre d'énergie ;
- **sous le ballon actif**, un marqueur au sol.

Il faut aussi rendre l'équipe lisible partout (joueurs **et** IA) par le PlayerState, et supprimer les timers de 0,01 s.

## 1. Mode d'emploi pour Claude++
1. Les lots se font **dans l'ordre**, un lot à la fois. À la fin de chaque lot : compiler, sauvegarder, faire le rapport, puis **attendre l'accord de l'utilisateur** avant le lot suivant.
2. Avant chaque lot, demander à l'utilisateur de faire un commit Git.
3. **Arrête-toi et demande** si :
   - ce que tu lis dans l'éditeur contredit ce devis ;
   - une étape demande du nouveau C++ (accord obligatoire de l'utilisateur) ;
   - un comportement visible du jeu changerait alors que le lot dit « comportement inchangé ».
4. **Outils** : passe par `ProgrammaticToolset.execute_tool_script` pour grouper les lectures. Garde chaque script court (un ou deux Blueprints à la fois) : l'appareil coupe après 80 s. Une lecture qui expire peut être relancée ; **une écriture qui expire, non** (vérifie d'abord son effet).
5. Langue : rapport en français ; Descriptions des variables et fonctions en anglais (guide, section 7).

## 2. Ce qui est déjà connu (lu dans l'éditeur)

### 2.1 Widgets actuels (tous sur `BP_SoccerPlayer`, aucun ailleurs)
| Composant | Widget | Rôle | Problème |
|---|---|---|---|
| `WBP_PlayerBar` | `/Game/Blueprint/UI/WBP_PlayerBar` (UserWidget simple) | Jauge radiale du kick, à plat aux pieds. World Space, 150 × 150, Z -85, pitch 90. Matériau `MI_KickEnergy_ProgressBar` (parent `M_RadialProgressBar`). | Passe UMG + render target à chaque image, `CastShadow` vrai. Le plane est rigide : il traverse les marches. Le matériau dynamique est créé deux fois (PreConstruct + Construct). |
| `WBP_Sprint` | `/Game/Blueprint/UI/WBP_Sprintbar` (base C++ `WB_EnergyBarWidget_Base`) | Barre d'énergie. Screen Space, 35 × 35, Z +100. Matériau `MI_SprintProgressBar`. Visible pendant le sprint seulement. | Bonne technique, à garder et à étendre. |

- Mise à jour : `Set Timer by Event` à **0,01 s en boucle** (`UpdateKickPlayerBarUI`, `UpdateSprintUI`), soit 100 Hz par joueur.
- Fonctions UI de `BP_SoccerPlayer` : `UI Sprint Initialization`, `UI KickEnergy Initialization`, `GetEnergyFlashPercent`.
- Qui référence les widgets :
  - `WBP_PlayerBar` → `BP_SoccerPlayer`, `BPC_GrabObject` ;
  - `WBP_Sprintbar` → `BP_SoccerPlayer`.

### 2.2 Équipe : ce qui existe déjà (**corrige la version précédente du devis**)
L'ancien devis disait « équipe déduite de la parité de l'ID ». C'est **faux en partie** :
- `BP_SoccerGamemode.GetTeamForPlayer(PlayerIndex)` lit d'abord l'équipe choisie dans **`GI_Core`** :
  - `FindSlotbyControllerId(GetLocalPlayerControllerID(GetPlayerController(PlayerIndex)))`, puis `GetSlotTeam(slot)` ;
  - la parité `PlayerIndex % 2` ne sert **qu'en repli** (slot introuvable ou cast vers `GI_Core` raté).
- L'équipe est un **entier `TeamIndex`** (1 = bleu, sinon rouge, selon `RegisterMatchPlayer`).
- `RegisterMatchPlayer(PlayerIndex, TeamIndex)` :
  - applique `MI_TeamBlue` / `MI_TeamRed` au mesh (slot 0) ;
  - appelle `SetTeamIndex(TeamIndex)` sur le **`WBPlayerHitComponent`** (C++) du joueur ;
  - ajoute le joueur au tableau `Players`.
- `SetupMatchPlayers` boucle sur `GetNumLocalPlayerControllers` seulement. **L'IA ne passe donc pas par ce chemin** : son équipe est attribuée ailleurs, ou pas du tout (à trouver, lot 0).
- Autres utilisateurs des matériaux d'équipe : `BP_GoalPost_01`, deux sous-niveaux, un External Actor du gym.
- Variables du GameMode : `ScoreRed`, `ScoreBlue`, `Players`, `PlayerTwo`, `SpawnManager`, `MatchPhase`, etc. Aucune variable d'équipe ni de ballon actif.

### 2.3 Chemins utiles
| Asset | Chemin |
|---|---|
| GameMode | `/Game/Blueprint/Character/BP_SoccerGamemode` |
| Personnage (parent) | `/Game/Blueprint/Character/BP_GenericCharacter` |
| Personnage joueur | `/Game/Blueprint/Character/BP_SoccerPlayer` |
| Contrôleur IA | `/Game/Blueprint/AI/BP_SoccerAIController` |
| Profil IA | `/Game/Blueprint/AI/Data/DA_SoccerAIProfile_Default` |
| Grab | `/Game/Blueprint/Character/BPC_GrabObject` |
| Game Instance | `/Game/Blueprint/Core/GI_Core` |
| Ballon maître | `/Game/Blueprint/Ball/BP_PO_Master_Ball_01` |
| But | `/Game/Blueprint/BP_GoalPost_01` |
| Matériaux d'équipe | `/Game/Art/Material/Instance_Materials/Color/MI_TeamBlue`, `MI_TeamRed` |
| Data Asset existant | `/Game/Blueprint/Character/Data/DA_PlayerState_Default` (à ouvrir : il ne s'agit peut-être pas d'un PlayerState) |

## 3. Règles validées par l'utilisateur
1. **Étiquette au-dessus de la tête** :
   - `P1` à `P4` = index de la manette (ControllerId du LocalPlayer + 1 ; P1 = manette 1 / clavier) ;
   - personnage contrôlé par `BP_SoccerAIController` → `COM` ;
   - texte à la couleur de l'équipe (bleu / rouge), **contour blanc** ;
   - fait toujours face à la caméra et garde sa taille.
2. **Anneau au sol** sous chaque joueur : cercle procédural (pas lié à la lumière) qui suit le sol.
3. **Jauge du kick dans l'anneau** :
   - au repos, cercle fin à la couleur d'équipe ;
   - pendant la charge, un arc se remplit autour du joueur (0 → 360°), avec le dégradé bleu → orange actuel ;
   - au relâchement, retour au cercle.
4. **Équipe dans le PlayerState**, attribuée par le GameMode. **L'IA connaît son équipe** par le même chemin.
5. **Marqueur du ballon actif** :
   - ombre procédurale : point noir au centre, dégradé du noir vers le transparent, **contour blanc par-dessus** ;
   - **toujours au sol**, même quand le ballon est en l'air, pour situer le ballon en XY.
6. **Le ballon actif peut changer** pendant une partie : aucun ballon codé en dur.

## 4. Décisions à faire confirmer par l'utilisateur (au début du lot 1)
Proposition de Claude, d'après la lecture de l'éditeur :
- **Source de l'équipe = `GI_Core`**. `AssignTeam` réutilise la logique de `GetTeamForPlayer` (slot de `GI_Core`, sinon parité de l'index en repli). On ne crée **pas** une nouvelle règle « slot pair → A ».
- **Type = entier `TeamIndex`**, compatible avec `GetSlotTeam` et `WBPlayerHitComponent.SetTeamIndex`. Un Gameplay Tag obligerait à toucher le C++. Option : enum `E_Team` seulement si sa conversion vers et depuis l'entier reste triviale.
- **Une seule source** : le PlayerState. `RegisterMatchPlayer` continue d'appeler `SetTeamIndex` sur le `WBPlayerHitComponent`, mais avec la valeur lue dans le PlayerState.

Si l'utilisateur refuse, revenir à la version tag / enum et lister le C++ à changer.

## 5. Architecture cible
Principe repris de la migration des menus : une base commune, un affichage **piloté par événements**, aucune mise à jour par image quand rien ne change.

| Élément | Technique | Remplace |
|---|---|---|
| Anneau au sol (joueurs) et marqueur du ballon | `DecalComponent` + `M_GroundRing` (Deferred Decal, unlit), dans un composant réutilisable `BPC_GroundRing`. Deux Material Instances : `MI_GroundRing_Player`, `MI_GroundRing_Ball` | `WBP_PlayerBar` (WidgetComponent World) |
| Étiquette + énergie au-dessus de la tête | **Un seul** `WidgetComponent` Screen Space, `WBP_PlayerOverhead` (enfant de `WB_EnergyBarWidget_Base`) | `WBP_Sprint` + `WBP_Sprintbar` |
| Équipe (joueurs et IA) | `TeamIndex` dans le **PlayerState**, écrit par le **GameMode**, couleurs dans `DA_TeamDefinitions` | lectures directes de `GI_Core` / parité |
| Ballon actif | `ActiveBall` dans le GameMode + dispatcher `OnActiveBallChanged(OldBall, NewBall)` | — |

**Pourquoi le PlayerState aussi pour l'IA** :
- un AIController reçoit un PlayerState si `bWantsPlayerState` = vrai ;
- tout le monde lit alors l'équipe de la même façon : `Controller → PlayerState → TeamIndex` (HUD, anneau, étiquette, GameMode, IA) ;
- l'équipe reste juste si une IA est remplacée par un joueur, ou l'inverse ;
- le PlayerState servira aussi au score individuel plus tard.

`IGenericTeamAgentInterface` (C++) : **hors devis**. Seulement si la perception d'IA d'Unreal est utilisée un jour.

## 6. Lots

### Lot 0 · Fin des vérifications et mesure de référence (lecture seule)
Déjà fait : section 2. **Reste à faire** :
1. Lire l'EventGraph de `BP_SoccerAIController` et `DA_SoccerAIProfile_Default`. Comment l'IA connaît-elle son équipe, son but à attaquer / défendre, ses coéquipiers ? Lister chaque lecture.
2. Lire `BP_GoalPost_01` : comment le but connaît-il son équipe ?
3. Lire `BP_GenericCharacter` : chercher toute lecture d'équipe (`TeamIndex`, `GetPlayerHitComponent`, `MI_Team…`).
4. `BP_SoccerAIController` : valeur de `bWantsPlayerState`. GameMode : `PlayerStateClass` (un PlayerState Blueprint du projet existe-t-il ?). Ouvrir `DA_PlayerState_Default` pour savoir ce que c'est.
5. Où l'IA est créée (`SpawnManager` ?) et à quel moment elle reçoit son pawn.
6. Qui appelle `KickProgressBarUpdate`, `UpdateKickPlayerBarUI`, `UpdateSprintUI` (y compris `BPC_GrabObject`).
7. `GI_Core` : signature de `FindSlotbyControllerId` et `GetSlotTeam`. Valeurs possibles de l'équipe (0/1 ? -1 = aucune ?).
8. **Mesure** : PIE 4 joueurs (ou 2 + 2 IA), relever `stat unit`, `stat slate`, `stat scenerendering` (draw calls, temps Slate). Si tu ne peux pas lancer PIE, demande à l'utilisateur de relever les chiffres.

**Livrable** : un tableau « qui lit l'équipe, comment, à remplacer par quoi », plus les mesures. Rien n'est modifié.

### Lot 1 · Équipe dans le PlayerState (joueurs et IA)
Pré-requis : section 4 confirmée.
- **PlayerState du projet** (créer `PS_SoccerPlayer` si aucun n'existe, et le régler dans le GameMode) :
  - variable `TeamIndex` (int, Category `Team`, Description), avec une valeur « aucune équipe » documentée ;
  - dispatcher `OnTeamChanged(NewTeamIndex)` ;
  - `SetTeam(NewTeamIndex)` : diffuse seulement si la valeur change ;
  - `GetTeam` (Pure).
- **IA** : `bWantsPlayerState` = vrai sur `BP_SoccerAIController`. L'IA lit son équipe dans son propre PlayerState, jamais dans une copie.
- **Bibliothèque de fonctions** (`BFL_Team`) :
  - `GetTeamOf(Actor)` : Pawn → PlayerState → TeamIndex, renvoie « aucune » si invalide ;
  - `AreTeammates(A, B)`.
- **GameMode** :
  - `AssignTeam(Controller)` est appelée pour **chaque joueur et chaque IA** à son arrivée : `PostLogin` / `SetupMatchPlayers` pour les joueurs, création de l'IA pour l'IA ;
  - la valeur vient de la logique actuelle de `GetTeamForPlayer` (GI_Core, puis repli) ;
  - **ne pas utiliser `PlayerState.PlayerId`** : il ne vaut pas 0-3 et change d'une partie à l'autre ;
  - `RegisterMatchPlayer` lit l'équipe dans le PlayerState (matériau + `SetTeamIndex` du `WBPlayerHitComponent`).
- **`DA_TeamDefinitions`** (PDA `PDA_TeamDefinition`), une entrée par équipe :
  - `DisplayName`, `LabelColor`, `RingColor`, `TeamMaterial` (`MI_TeamBlue` / `MI_TeamRed`) ;
  - le but de l'équipe, seulement si le lot 0 montre que l'IA en a besoin ;
  - lecture par `GetTeamDefinition(TeamIndex)`.
- Remplacer les lectures d'équipe trouvées au lot 0 (joueurs **et** IA) par le PlayerState.
- **Comportement visible inchangé** : mêmes équipes, mêmes matériaux, l'IA attaque le même but.

### Lot 2 · Anneau au sol (`M_GroundRing` + `BPC_GroundRing`)
- **`M_GroundRing`** : Material Domain = Deferred Decal, Blend = Translucent, unlit / émissif. Tout est procédural (distance au centre + angle, aucune texture). Couches, de bas en haut :
  1. **Ombre diffuse** (optionnelle) : noir au centre, dégradé vers le transparent. Paramètres `ShadowStrength` (0 = pas d'ombre), `ShadowFalloff`.
  2. **Arc de jauge** : `KickPercent` (0-1), `KickColorStart`, `KickColorEnd`.
  3. **Contour**, par-dessus : `RingColor`, `RingThickness`.
  - Plus `Opacity` et `Pulse` (0 par défaut).
- **Material Instances** :
  - `MI_GroundRing_Player` : sans ombre, contour à la couleur d'équipe, jauge active ;
  - `MI_GroundRing_Ball` : ombre noire → transparent, contour blanc, sans jauge.
- **`BPC_GroundRing`** (SceneComponent Blueprint ; C++ seulement avec l'accord de l'utilisateur) :
  - crée le `DecalComponent` et son MID **une seule fois** ;
  - **rotation absolue** : projette toujours vers le bas, même si le parent tourne ou roule ;
  - taille et profondeur de projection réglables (Category `GroundRing`, Descriptions avec unités) ;
  - fonctions `SetRingColor`, `SetKickPercent`, `SetRingActive(bool)` ;
  - option `SnapToGround` (lot 6), **faux par défaut** (règle des composants partagés).
- **Sur `BP_SoccerPlayer`** : ajouter `BPC_GroundRing` avec `MI_GroundRing_Player`. La couleur vient de `DA_TeamDefinitions` et se met à jour sur `OnTeamChanged`.
- `bReceivesDecals` = faux sur le mesh du personnage et les meshes des ballons.
- **Jauge du kick** : `KickProgressBarUpdate` appelle `SetKickPercent` de l'anneau au lieu du widget.
- **Retirer le composant `WBP_PlayerBar`** et mettre à jour `BPC_GrabObject`. Supprimer l'asset `WBP_PlayerBar` seulement quand `get_referencers` ne renvoie plus rien.
- Les valeurs de réglage (rayon, épaisseur, couleurs, ombre) vont dans les Material Instances ou un Data Asset, jamais dans le graphe.

### Lot 3 · Widget de tête unique (`WBP_PlayerOverhead`)
- Nouveau WBP enfant de `WB_EnergyBarWidget_Base` :
  - `PlayerLabel` (TextBlock, police du projet, `Outline Settings` : taille ≥ 2, blanc) ;
  - placé au-dessus de la barre d'énergie existante (même matériau `MI_SprintProgressBar`).
- Un seul `WidgetComponent` Screen Space sur `BP_SoccerPlayer` : `Draw at Desired Size`, pivot bas-centre, au-dessus de la tête.
- Retirer le composant `WBP_Sprint`. La visibilité « pendant le sprint seulement » s'applique **à la barre seulement** ; l'étiquette reste toujours visible.
- Supprimer `WBP_Sprintbar` quand plus rien ne le référence.
- Aucun binding de propriété, aucun Tick dans le WBP.

### Lot 4 · Logique P1-P4 / COM et couleur
- `WBP_PlayerOverhead` : `SetLabel(Text)` et `SetLabelColor(LinearColor)`.
- Sur le personnage, `RefreshOverheadLabel` est appelée à `PossessedBy` et au changement de controller (et une fois au BeginPlay si le personnage est déjà possédé) :
  - PlayerController local → `P` + (ControllerId du LocalPlayer + 1) ;
  - AIController → `COM` ;
  - sans controller → étiquette cachée ;
  - texte en FText (pour une String Table plus tard).
- Couleur : `OnTeamChanged` du PlayerState → `SetLabelColor` + `SetRingColor`. Bind à la possession, unbind à la dépossession. Même chemin pour les joueurs et l'IA.
- Cas à couvrir : une IA remplacée par un joueur (ou l'inverse) en cours de partie. L'étiquette et la couleur suivent sans redémarrer.

### Lot 5 · Fin des timers de 0,01 s
- Supprimer les deux `Set Timer by Event` à 0,01 s en boucle.
- Kick et sprint : mise à jour **une fois par image, seulement pendant la charge ou le sprint**. Le Tick du composant s'active au début et se désactive à la fin, avec le delta réel.
- Option préférée si elle est propre : envoyer `StartTime` et `Duration` au matériau une seule fois, puis laisser le shader calculer le remplissage avec `Time`. Claude++ choisit et justifie son choix dans le rapport.
- `GetEnergyFlashPercent` et l'effet de flash doivent rester **identiques à l'œil**.
- Vérifier qu'aucun autre widget ne crée son matériau dynamique deux fois.

### Lot 6 · Marqueur du ballon actif
- **GameMode** : `ActiveBall` (Category `Match|Ball`) et `SetActiveBall(NewBall)`, qui diffuse `OnActiveBallChanged(OldBall, NewBall)`. Réglé quand le ballon du match apparaît (lien avec le ballon pondéré du devis de sélection du ballon).
- **`BP_PO_Master_Ball_01`** : `BPC_GroundRing` avec `MI_GroundRing_Ball`, **éteint par défaut**. À `OnActiveBallChanged`, l'ancien ballon éteint son marqueur et le nouveau l'allume.
- **`SnapToGround` = vrai** : le decal n'est **pas** attaché sous le ballon. Une projection profonde se poserait sur les tables, et elle décrocherait quand le ballon monte haut. Tant que le marqueur est actif, à chaque image :
  - un line trace part du ballon, vers le bas ;
  - il ignore le ballon, les personnages, les débris et les objets physiques en mouvement (Claude++ propose le canal / profil) ;
  - le decal est placé au point touché, aux X et Y du ballon.
  - Le Tick est actif seulement quand le marqueur l'est : une trace par image, coût négligeable.
- Taille **constante** quelle que soit la hauteur. Option `HeightScaleFactor`, désactivée par défaut : l'ombre rétrécit ou s'estompe en hauteur, le contour blanc reste.
- Aucun sol sous le ballon : marqueur caché jusqu'au prochain sol.
- Ajouter une **commande de debug** pour changer de ballon actif (test 9).

### Lot 7 · Mesure finale et tests
- Mêmes mesures qu'au lot 0, même scène. Tableau avant / après dans le rapport.
- Tests de la section 7. PIE sans erreur ni « Accessed None ».

## 7. Protocole de test
1. 2, 3 et 4 joueurs humains : P1 à P4 correspondent aux manettes (P1 = manette 1 / clavier).
2. Avec des IA : `COM` au-dessus des IA. Chaque IA joue pour la bonne équipe (attaque le bon but, ne vise pas ses coéquipiers).
3. Équipes choisies dans la sélection de personnage (`GI_Core`) : bien reprises en match. Sans sélection : repli identique à aujourd'hui.
4. Couleurs : texte et anneau bleus pour une équipe, rouges pour l'autre (joueurs et IA). Contour blanc lisible sur tous les fonds de la map.
5. L'étiquette fait toujours face à la caméra et garde sa taille.
6. Anneau sous chaque joueur : suit les marches, tapis et meubles bas. Il ne se projette ni sur le personnage ni sur le ballon.
7. Charge du kick : l'arc se remplit en dégradé bleu → orange et revient au cercle au relâchement. Ressenti identique à aujourd'hui.
8. Sprint : barre d'énergie et flash identiques à aujourd'hui.
9. Ballon actif :
   - ombre noire + contour blanc sous lui, pas sous les autres ballons ;
   - ballon kické haut : le marqueur reste au sol, exactement sous le ballon en XY ;
   - ballon au-dessus d'une table : marqueur sur le sol *(ou sur la table si l'utilisateur préfère, à valider au test)*.
10. Changement de ballon actif (commande de debug) : le marqueur passe au nouveau ballon.
11. Performances : `stat slate` en baisse, aucune régression de `stat unit`.

## 8. Points ouverts
- **Section 4** : source et type de l'équipe, à confirmer au début du lot 1.
- Coût du decal sur PC modeste. Repli si trop cher : plane au sol avec matériau translucide unlit (sans widget) + trace vers le bas. Décision après la mesure du lot 7.
- Ballon au-dessus d'un meuble : marqueur sur le sol ou sur le meuble (test 9).
- Couleur du contour du ballon : blanc (décidé) ; option future « couleur du dernier joueur qui l'a touché », hors devis.
- Nouveau C++ (`BPC_GroundRing` en C++, `IGenericTeamAgentInterface`) : seulement avec l'accord de l'utilisateur. Réglages exposés dans des Data Assets.
