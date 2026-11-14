# Devis destruction — lot 9 : gestionnaire de débris par morceau (C++) et budget physique

Étape 8 du devis maître. Complète `claude/devis-destruction.md`, `claude/devis-destruction-lot7.md` et `claude/devis-maitre.md`. **Version 3, 2026-11-13, 22 h 50** : prête à remettre à Claude Code. Commit Git à faire par l'utilisateur avant le début du lot.

## 0. Pour Claude Code : comment lire ce devis
- Le projet est décrit dans `CLAUDE.md` (conventions C++, compilation, Git). Ce devis dit **quoi** construire. `CLAUDE.md` dit **comment**.
- Les sections 3 à 6 sont à toi : le code C++, la compilation, et un rapport. Les sections 7 et 8 se font dans l'éditeur, par l'utilisateur ou par la conversation claude.ai. Ne les fais pas, mais tiens-en compte dans l'API.
- **Organisation et descriptions, comme en Blueprint** : chaque nouvelle `UPROPERTY` et chaque nouvelle `UFUNCTION` a une `Category`, avec des sous-catégories `A|B` au besoin, et un commentaire `/** */` en anglais. Ce commentaire est l'infobulle et la Description dans l'éditeur, et c'est toi qui l'écris. Les noms de catégories du tableau 3.2 sont un point de départ. Tu peux en créer d'autres si c'est plus clair, et tu les listes dans le rapport.
- Toute modification de Blueprint suit `Docs/guide-ergonomie-blueprint.md`.
- Pas de commit sans demande de l'utilisateur.
- En cas de doute sur une API Chaos d'UE 5.8, lis les en-têtes du moteur avant d'écrire du code. Ensuite, note ce que tu as trouvé dans le rapport (section 6). Ne devine pas.

## 1. Le but
Remplacer la règle provisoire du lot 7 : 6 murs effondrés gardent leurs débris, et au 7e, tous les débris du plus ancien disparaissent d'un coup. On veut une gestion **morceau par morceau** :
- Au fil du temps, les **petits** morceaux disparaissent, et on garde le plus longtemps possible les **gros**.
- On retire **un peu partout à la fois**. On ne vide jamais d'un coup tous les morceaux d'une même Geometry Collection (GC).
- Un morceau retiré **rapetisse jusqu'à disparaître**. Il ne disparaît pas d'un coup.
- Le budget tient compte des **props physiques** de la carte (vases, fleurs, livres, divans, armoires, bibliothèques…). Les GC ne peuvent pas prendre tout le budget.

Rappel : Blueprint n'a aucun accès aux morceaux individuels d'une GC. Et changer les réglages de retrait au sommeil en cours de partie n'a aucun effet (essai v2 du lot 7). C'est pour ça que ce gestionnaire est écrit en C++.

## 2. Décisions de l'utilisateur (2026-11-13, 22 h 37)
1. **Budget** : pas de chiffre au départ. Valeur initiale = nombre de morceaux de 6 murs effondrés (12 GC, soit 2 faces par mur). On ajuste ensuite au test de stress (section 8).
2. **Ordre de retrait** : seuls les morceaux **endormis** sont candidats. Parmi eux, on retire d'abord les plus **petits**, puis les plus **anciens**, et en dernier les plus **loin** des joueurs.
3. **Sort d'un morceau retiré** : il rapetisse jusqu'à disparaître. Méthode technique laissée à Claude (voir 3.4).
4. **Architecture** : classe C++ de base, et `BP_PhysicsBudgetManager` en hérite (convention `_Base` de `CLAUDE.md`).

Précisions de l'utilisateur (22 h 43) :
5. **Plan de contingence** : si le nombre de morceaux dépasse de beaucoup le budget, on retire massivement et plus vite (mode urgence, 3.5).
6. **Images par seconde** : cible de 60, seuil de tolérance de 30 dans le pire des cas.
7. **Usure sous le budget** : oui, mais à une cadence plus lente que le retrait pour budget.
8. **Distance** : elle se mesure aux **joueurs**, pas à la caméra. Comme il y a 2 à 4 joueurs, on prend la distance au **joueur le plus proche** (proposition de Claude : un morceau proche d'un seul joueur reste visible et utile, alors qu'une moyenne des positions le retirerait). La moyenne des positions est gardée en option, au cas où.

## 3. Ce qui est à construire (C++)

### 3.1 Classes
- **`AWB_PhysicsBudgetManager_Base`** (Actor), dans `Public/Impact/Budget` et `Private/Impact/Budget`. `BP_PhysicsBudgetManager` sera reparenté dessus (section 7). C'est un acteur et non un Subsystem, parce que `BP_SoccerGamemode.InitializePhysicsBudget` le spawne déjà et que ses réglages restent en Blueprint.
- **`UWB_PhysicsBudgetData_Base`** (UPrimaryDataAsset), mêmes dossiers. `PDA_PhysicsBudget` sera reparenté dessus, et `DA_PhysicsBudget` garde ses valeurs. `MaxCollapsedWalls` disparaît (section 7).

### 3.2 Réglages (`UWB_PhysicsBudgetData_Base`)
Tous en `EditDefaultsOnly`, `BlueprintReadOnly`, chacun avec sa `Category` et son commentaire `/** */` en anglais.

| Nom | Type | Category | Défaut | Rôle |
| --- | --- | --- | --- | --- |
| `bUsePerPieceDebris` | bool | `Debris` | false | Active le gestionnaire par morceau. Faux = rien n'est retiré, sauf par le timer de secours du mur. |
| `MaxDebrisPieces` | int | `Debris\|Budget` | à mesurer (section 8) | Nombre maximal de morceaux de GC actifs sur la carte. |
| `ThinInterval` | float (s) | `Debris\|Thinning` | 0,5 | Délai entre deux passes de retrait. |
| `MaxRemovalsPerPass` | int | `Debris\|Thinning` | 4 | Nombre de morceaux retirés au maximum par passe. |
| `MaxRemovalsPerCollectionPerPass` | int | `Debris\|Thinning` | 1 | Nombre de morceaux d'une même GC retirés au maximum par passe : c'est ce qui donne le retrait « un peu partout ». |
| `SmallPieceVolume` | float (cm³) | `Debris\|Size` | à mesurer | En dessous de ce volume, un morceau est « petit ». |
| `SmallPieceLifetime` | float (s) | `Debris\|Wear` | 20 | Un petit morceau endormi depuis ce temps devient candidat à l'usure, **même sous le budget**. |
| `WearInterval` | float (s) | `Debris\|Wear` | 2,0 | Délai entre deux passes d'usure, plus lent que `ThinInterval`. |
| `MaxWearRemovalsPerPass` | int | `Debris\|Wear` | 1 | Nombre de morceaux retirés au maximum par passe d'usure. |
| `EmergencyThresholdRatio` | float | `Debris\|Emergency` | 1,5 | Le mode urgence commence au-dessus de `MaxDebrisPieces` × ce ratio. |
| `EmergencyInterval` | float (s) | `Debris\|Emergency` | 0,1 | Délai entre deux passes en mode urgence. |
| `EmergencyRemovalsPerPass` | int | `Debris\|Emergency` | 32 | Nombre de morceaux retirés au maximum par passe en urgence. |
| `EmergencyRemovalsPerCollectionPerPass` | int | `Debris\|Emergency` | 4 | Limite par GC en urgence. On retire toujours un peu partout, mais plus vite. |
| `EmergencyShrinkDuration` | float (s) | `Debris\|Emergency` | 0,3 | Rapetissement plus court en urgence. |
| `DistanceMode` | enum `EWB_DebrisDistanceMode` | `Debris\|Distance` | `NearestPlayer` | `NearestPlayer` (distance au joueur le plus proche) ou `PlayersAverage` (distance à la moyenne des positions des joueurs). |
| `ProtectedPieceVolume` | float (cm³) | `Debris\|Size` | à mesurer | Au-dessus de ce volume, un morceau est « gros » et n'est retiré que si le budget est dépassé **et** qu'il ne reste plus aucun autre candidat. |
| `ShrinkDuration` | float (s) | `Debris\|Removal` | 1,0 | Durée du rapetissement avant la disparition. |
| `PhysicsPropsReserve` | int | `Budget\|Props` | à mesurer | Nombre de corps simulés réservés aux props. Seulement mesuré et affiché : le gestionnaire ne retire jamais un prop. |
| `bShowDebugStats` | bool | `Debug` | false | Affiche les compteurs à l'écran (section 3.7). |

### 3.3 API exposée à Blueprint (`AWB_PhysicsBudgetManager_Base`)
- `RegisterGeometryCollection(UGeometryCollectionComponent* Collection)` (BlueprintCallable) : appelée par le mur après l'effondrement, à la place de `RegisterCollapsedWall`. Elle mémorise l'heure d'inscription et le volume de chaque morceau (calculé une fois, à partir des bornes ou du volume stocké dans la collection : à déterminer par Claude Code).
- `UnregisterGeometryCollection(UGeometryCollectionComponent* Collection)` (BlueprintCallable) : appelée quand un acteur est détruit. Le nettoyage automatique (pointeurs faibles) reste obligatoire de toute façon.
- `RegisterPhysicsProp(UPrimitiveComponent* Prop)` et `UnregisterPhysicsProp(...)` (BlueprintCallable) : comptage seulement, pour le budget et le test de stress.
- `GetDebrisStats()` (BlueprintPure) : renvoie un `USTRUCT` `FWB_PhysicsBudgetStats` avec les morceaux actifs, endormis, en rapetissement et retirés au total, les props inscrits et simulés, et le nombre de GC inscrites.
- Événement `OnPieceRemoved(Collection, PieceIndex)` (BlueprintAssignable), pour les futurs VFX du lot 10.

### 3.4 Retrait d'un morceau : rapetissement, puis désactivation
Recommandation de Claude : **désactiver** le morceau plutôt que détruire quoi que ce soit. Une fois rapetissé, le morceau sort de la simulation, il n'a plus de collision et il n'est plus rendu. Le composant GC est détruit seulement quand tous ses morceaux sont retirés. Détruire morceau par morceau n'est pas possible sans reconstruire la collection, et désactiver coûte presque rien.

Étape 0, obligatoire : **sonder le moteur.** UE a déjà un mécanisme de retrait avec rapetissement (`Remove On Sleep` / `Remove On Break` et les facades « removal / decay » de la collection dynamique). Claude Code doit vérifier dans les en-têtes d'UE 5.8 s'il peut :
- (a) **déclencher ce mécanisme sur un morceau précis**, depuis le C++ et en cours de partie. C'est la solution préférée, parce que le rendu et la désactivation sont alors gérés par le moteur ;
- (b) sinon, faire le rapetissement lui-même : réduire l'échelle du transform du morceau dans la collection dynamique sur `ShrinkDuration`, puis désactiver la particule côté physique.

Le choix (a) ou (b) et la raison vont dans le rapport. Si aucun des deux ne marche, **arrête-toi et fais un rapport** avant d'inventer autre chose.

### 3.5 Les trois modes de retrait
Un seul timer. Son délai change selon le mode, de la façon suivante.

| Mode | Quand | Cadence | Limites |
| --- | --- | --- | --- |
| **Usure** | Le nombre de morceaux actifs est ≤ `MaxDebrisPieces`. | `WearInterval` | `MaxWearRemovalsPerPass`, petits morceaux seulement, plus vieux que `SmallPieceLifetime`. |
| **Budget** | Au-dessus de `MaxDebrisPieces`. | `ThinInterval` | `MaxRemovalsPerPass`, `MaxRemovalsPerCollectionPerPass`. |
| **Urgence** | Au-dessus de `MaxDebrisPieces` × `EmergencyThresholdRatio`. | `EmergencyInterval` | `EmergencyRemovalsPerPass`, `EmergencyRemovalsPerCollectionPerPass`, `EmergencyShrinkDuration`. |

Le gestionnaire sort du mode urgence dès qu'il repasse sous `MaxDebrisPieces`, sans attendre de redescendre sous le seuil d'urgence. Ça évite les allers-retours entre les deux modes.

### 3.6 Algorithme d'une passe
1. Mettre à jour l'état de chaque morceau inscrit : actif ou non, endormi ou non, et depuis quand il dort. Choisir le mode (3.5).
2. **Candidats** = les morceaux actifs, endormis, pas déjà en rapetissement et non protégés. En usure, seulement les petits qui dorment depuis plus de `SmallPieceLifetime`.
3. **Tri des candidats** : le **volume** croissant, puis l'**heure d'inscription** croissante, puis la **distance** décroissante (selon `DistanceMode`, calculée à partir des pawns des joueurs locaux).
4. On retire dans l'ordre du tri, en respectant les limites du mode. Quand une GC a atteint sa limite, on passe à la GC suivante. Le reste attend la passe suivante.
5. Budget ou urgence avec une liste vide : les gros morceaux deviennent candidats, avec le même tri.
6. Un morceau éveillé (en mouvement) n'est **jamais** retiré, même en urgence. Si l'urgence ne redescend pas à cause de morceaux éveillés, on le signale dans le log (une fois par épisode d'urgence), et on décidera au test s'il faut une règle de plus.

Performance : pas de Tick. Un timer, aucune allocation par passe dans le cas normal (tableaux réservés), et le tri porte seulement sur les candidats.

### 3.7 Debug
Si `bShowDebugStats` est vrai, `GEngine->AddOnScreenDebugMessage` affiche une ligne à chaque passe, avec le mode en cours, les morceaux actifs sur le max, les endormis, les rapetissements en cours, les props simulés et la réserve. Une commande console `wb.Debris.Stats` donne la même chose dans le log.

## 4. Compilation et contrôles
- Compiler avec la commande de `CLAUDE.md`. Zéro avertissement.
- Ajouter au `Build.cs` les modules nécessaires (au minimum `GeometryCollectionEngine`, et peut-être `Chaos` et `ChaosSolverEngine`). Ajoute seulement ce qui est utilisé.
- Retirer `UWB_SetupTestLibrary` (classe temporaire du setup), comme prévu dans `setup-cpp.md`. Préviens l'utilisateur de retirer les nodes de test du Level Blueprint.

## 5. Hors champ pour Claude Code
- Modifier `BP_PhysicsBudgetManager`, `BP_Destructible_Wall_Base` ou `BP_SoccerGamemode` (section 7).
- Les prefabs, le calibrage et le coût de rendu (section 8).
- Retirer un prop physique : on le compte, on ne le retire jamais.

## 6. Rapport de fin de Claude Code
Ce rapport est à coller dans la conversation claude.ai. Il contient :
- les fichiers créés ou modifiés ;
- le choix (a) ou (b) de la section 3.4, et les API du moteur utilisées ;
- la façon dont le volume d'un morceau est calculé ;
- les limites connues ;
- les noms exacts des fonctions et des propriétés à brancher en Blueprint ;
- les commentaires anglais de chaque propriété (ils servent aussi de Descriptions).

## 7. Branchement Blueprint (éditeur, après le C++)
- Reparenter `BP_PhysicsBudgetManager` sur `AWB_PhysicsBudgetManager_Base`. Retirer `CollapsedWalls`, `RegisterCollapsedWall` et `ClearOldestWall`.
- Reparenter `PDA_PhysicsBudget` sur `UWB_PhysicsBudgetData_Base` et retirer `MaxCollapsedWalls`. Dans `DA_PhysicsBudget`, mettre `bUsePerPieceDebris` à vrai.
- `BP_Destructible_Wall_Base.CollapsePlaster` : appeler `RegisterGeometryCollection` pour `GC_PlasterSideA` et `GC_PlasterSideB`. Garder le timer de secours pour les cartes sans GameMode.
- `BP_M_PhysicObject` (BeginPlay) : appeler `RegisterPhysicsProp` ; `EndPlay` : `UnregisterPhysicsProp`.
- Rapport final selon la section 8 du guide d'ergonomie.

## 8. Test de stress, calibrage, prefabs, rendu (éditeur, par l'utilisateur)
1. **Mesure de départ** : effondrer 6 murs de test dans `Lvl_Destruction_Gym`, puis lire les morceaux actifs dans les stats. → valeur initiale de `MaxDebrisPieces`. Noter aussi le volume des morceaux (petit, moyen, gros) pour fixer `SmallPieceVolume` et `ProtectedPieceVolume`. Attention : la GC placeholder est un seul gros cluster par face, et les chiffres changeront avec les GC des artists.
2. **Scène de stress « maison »** : une pièce garnie de props représentatifs (vases, fleurs, livres, divans, armoires et bibliothèques **sans** contenu dans les tiroirs : le meuble est d'un seul tenant), 4 joueurs, puis effondrer des murs jusqu'au budget. Mesurer `stat unit`, `stat fps` et les stats Chaos (corps actifs, temps du solveur).
3. **Budget global** : cible de **60 images par seconde**, plancher de **30** dans le pire cas (4 joueurs, mode urgence en cours). On ajuste `MaxDebrisPieces` à la baisse jusqu'à la tenir, avec la scène de props au complet. La différence avec le total mesuré donne `PhysicsPropsReserve`.
4. **Ressenti** : vérifier que le retrait se voit peu et qu'il est réparti (aucune GC vidée d'un coup), et que les gros morceaux restent.
5. **Rendu des GC sans Root Proxy Renderer** : comparer le coût de rendu (`stat scenerendering`, `stat gpu`) avec 6, puis 12 murs effondrés.
6. **Prefabs** : un seul existe pour l'instant, et c'est lui qui sert au benchmark. Les autres prefabs se feront plus tard, hors lot 9 (leçon du lot 7 : aucun chevauchement avec un objet physique ou un `PlayerStart`).
7. **Machine de référence** : le PC de l'utilisateur (le jeu sort sur PC). Au début du benchmark, noter le CPU, le GPU, la RAM et la résolution de test dans le rapport. Mesurer en **Standalone Game**, pas en PIE dans l'éditeur, parce que l'éditeur fausse les images par seconde.
