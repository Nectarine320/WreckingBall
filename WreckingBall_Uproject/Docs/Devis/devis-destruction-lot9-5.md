# Devis destruction — lot 9.5 : variantes de murs, fenêtre, colonne et porte à pentures

> **État au 2026-11-19 : lot fait**, sauf placement et calibrage (passe à part). Écarts et décisions : `devis-destruction-lot9-5-ouvertures.md` (addenda) et **`bilan-lot9-5.md`** (bilan, base commune, restes). Le « défonçage » du 9.5-6 est remplacé : un verrou cassé décroche la porte.

Rédigé le 2026-11-14 (conversation de planification avec l'utilisateur). Étape 8 du devis maître (phase B), **entre le lot 9 (fait) et le lot 10**. Le benchmark de l'étape 8 du lot 9 reste à faire, mais il ne bloque pas ce lot. La phase C ne commence qu'à la fin de la phase B (lot 9.5, puis lot 10).

> **Claude++** = **Claude Code**, l'outil qui travaille directement dans l'éditeur Unreal (MCP) et sur les fichiers C++. Ce surnom vient de l'utilisateur (plus facile à prononcer). Ce devis est écrit pour Claude++.

## Règles du chantier
- Avant de toucher un Blueprint : lire `claude/guide-ergonomie-blueprint.md` et l'appliquer en entier. Chaque sous-lot se termine par le rapport de la section 8 du guide (Descriptions par Blueprint en tableau, format de `claude/descriptions-a-coller.md`).
- Commit Git avant et après chaque sous-lot (fait par l'utilisateur). Test de l'utilisateur à la fin de chaque sous-lot.
- Au début du chantier, Claude++ liste les créations manuelles (assets que l'outil ne sait pas créer : interfaces, variables de type enum, etc.).
- Pas de nouvel enum : utiliser des Gameplay Tags ou des booléens.
- Tout réglage visuel (meshes, matériaux, assets du profil) passe par le Construction Script : visible dans le viewport sans lancer le jeu.
- Nouveaux Blueprints de base : suffixe `_Base`.
- Tout est en **placeholder** (meshes et GC de test) en attendant les assets des artistes.

## Principes de design validés (2026-11-14)
1. **Couches** : un mur a trois couches, côté A, milieu, côté B. **La couche du milieu ne se détruit jamais.** Chaque côté peut être destructible ou non (option du profil). Exemple : mur extérieur = gypse destructible du côté intérieur, brique indestructible du côté extérieur. Pour l'instant, les murs extérieurs ne se détruisent jamais, sauf leurs fenêtres.
2. **Dépendants** : une vitre, une porte ou un objet accroché dépendent de leur mur. **Si le mur est détruit, le dépendant l'est aussi. L'inverse est faux.** Le mur diffuse ce qui lui arrive, et chaque dépendant choisit sa propre réaction (le cadre fait son test de secousse, la vitre ignore la secousse mais éclate quand le mur meurt, la porte s'arrache quand le mur meurt).
3. **Oscillation interrompue** : un nouveau coup remplace l'oscillation en cours, **en partant de la pose actuelle** (pas de saut visuel). La rotation en cours s'estompe vite pendant que la nouvelle commence. **L'oscillation revient toujours exactement à la rotation de départ**, même après un changement d'axe en cours de route. Même règle pour le mur et la colonne.
4. **Colonne à quatre faces** : la normale de l'impact est lue **en repère local**, puis ramenée à la face dominante (+X, −X, +Y ou −Y). La colonne penche dans le sens opposé à l'impact, pivot à la base. Pas d'angle libre (il n'y aura pas de colonnes rondes).
5. **Porte à pentures**, en deux stades : sur ses pentures (Physics Constraint, ressort de porte de saloon), puis arrachée (contrainte cassée, chute d'un bloc comme un objet physique). **Le stade 3 (éclatement en GC) est en attente.** Une porte verrouillée ne se déverrouille pas : elle se **défonce** par la force (Dash, ballon, projectile).

---

## Sous-lot 9.5-1 — Renommage générique du mur (seul, en premier)
Le mur ne sera pas toujours en plâtre (céramique, brique, papier peint…).

| Avant | Après |
| --- | --- |
| `GC_PlasterSideA` / `GC_PlasterSideB` | `GC_SideA` / `GC_SideB` |
| `Middle` | `MiddleMesh` |
| `PlasterThickness` | `SideThickness` |
| `CollapsePlaster` | `CollapseSides` |
| `BreakPlasterAtImpact` | `BreakSideAtImpact` |

- Chercher aussi les autres usages de « Plaster » dans les noms de `BP_Destructible_Wall_Base` et du PDA (variables, Descriptions, Comments). `DA_DestructibleWall_Plaster_Default` **garde son nom** (c'est bien un profil de plâtre).
- ⚠️ **Avertissement accepté par l'utilisateur** : renommer un composant peut faire perdre les surcharges faites sur les acteurs placés. C'est le cas sur les **14 murs de `Lvl_Destruction_Gym`** (`bOverrideCustomRenderer` = vrai, `CustomRendererType` = None sur les GC, et positions éventuelles). Après le renommage, vérifier ces 14 murs et réappliquer les surcharges perdues. Le risque est acceptable : peu de cartes contiennent déjà ces murs.
- Test : les murs du gym s'affichent et se cassent comme avant.

## Sous-lot 9.5-2 — Remontée dans `BP_Destructible_Base` et dépendants
**Objectif** : à la fin du lot 9.5, la base contient tout ce qui est commun. Claude++ fait l'inventaire du code du mur au début de ce sous-lot, et à la fin de chaque sous-lot suivant, propose ce qu'il reste à remonter.

À remonter (au minimum) :
- **Oscillation** (`StartWobbleFromImpact`, `ComputeWobbleSettings`, `PlayWobbleAnim`, `WobbleAnimStep`, `GetWobbleAngle`) avec deux modes, choisis par un tag ou un booléen (pas d'enum) : **un axe** (le mur) et **quatre faces** (la colonne). Ajouter la règle d'interruption (principe 3). Le pivot à animer est fourni par une fonction surchargeable (par exemple `GetWobblePivot(ImpactData)`).
- `GetAttachSurfaceForSide` (déjà dans la base, à vérifier).
- **À examiner** : le profil de la base est `PDA_DestructibleWallProfile`, un nom propre au mur. Proposer, si utile, un `PDA_DestructibleProfile_Base` (PV, seuils, surface, debounce, oscillation) avec des enfants pour le mur, la vitre, la colonne et la porte. Décision de l'utilisateur avant de le faire.
- Vérifier que le debounce (`TryRegisterHit`, cooldown 0,2 s) ne bloque pas un **deuxième ballon** qui frappe pendant ce délai (le debounce doit viser la même source, pas toutes les sources).

**`BPI_DestructibleDependent`** (nouvelle interface, création manuelle probable) :
- `OnParentDamaged(ImpactData: STR_ImpactData, Damage: float)`
- `OnParentDestroyed()`
- La base appelle ces fonctions sur ses dépendants (ses acteurs attachés qui implémentent l'interface), ou les dépendants s'abonnent aux dispatchers : Claude++ choisit et justifie. Le mur n'a pas besoin de connaître le type de ses dépendants.
- **Adapter les objets accrochés** de `BP_M_PhysicObject` (lot 8) à cette interface : la logique de `HandleParentDamaged` (rayon, secousse, vie ≤ 0) devient leur implémentation. Le comportement doit rester identique.
- Test : le mur actuel oscille, se fissure, s'effondre comme avant ; les cadres et l'horloge du lot 8 tombent comme avant.

## Sous-lot 9.5-3 — Couches destructibles et matériaux multiples
**Profil** (`PDA_DestructibleWallProfile`) :
- `SideADestructible`, `SideBDestructible` (booléens, vrai par défaut). Le milieu n'a pas d'option : il ne se détruit jamais.
- Impact sur un côté protégé : aucun dégât, aucune fissure, Accepted = faux, feedback léger seulement (pour garder le son et la poussière). Pas d'oscillation, sauf avis contraire de l'utilisateur au test.
- Effondrement : seuls les côtés destructibles tombent. Un objet accroché du côté protégé ne tombe que par un coup direct (ou si le mur entier est détruit, voir ci-dessous).
- Si les deux côtés sont protégés, le mur est indestructible ; il peut quand même porter une fenêtre ou une porte.
- **À trancher au test** : quand le seul côté destructible est effondré, est-ce que le mur est « détruit » pour ses dépendants (la vitre éclate) ? Proposition : oui, l'état Destroyed est atteint quand tous les côtés **destructibles** sont tombés.
- Nouveau profil : `DA_DestructibleWall_Exterior` (A destructible, B indestructible, placeholders).

**Matériaux** : remplacer `SideMaterialSlotIndex` par trois tableaux dans le profil : `SideAMaterials`, `SideBMaterials`, `MiddleMaterials`. L'entrée *i* s'applique au slot *i* de la GC (ou du mesh du milieu) ; une entrée vide garde le matériau d'origine. Le Construction Script applique le tout (visible dans le viewport). Reporter la valeur actuelle (slot 1 = face de plâtre) dans `DA_DestructibleWall_Plaster_Default`.

Test : mur extérieur placé dans le gym ; frapper les deux côtés ; changer une GC et vérifier les matériaux dans le viewport.

## Sous-lot 9.5-4 — Fenêtre
- **`BP_Destructible_Wall_Window`**, enfant de `BP_Destructible_Wall_Base` : mur avec une ouverture. Le `Blocker` est remplacé par des boîtes autour de l'ouverture (gauche, droite, haut, bas), ajustées par le Construction Script selon la taille et la position de l'ouverture (dans le profil ou par instance, Claude++ propose).
- **`BP_Destructible_Glass`**, enfant de `BP_Destructible_Base`, **acteur attaché au mur** (pas un composant) : GC de vitre placeholder, `PM_Glass`, très peu de PV (casse presque au premier coup), son profil `DA_DestructibleGlass_Default`. Implémente `BPI_DestructibleDependent` : ignore `OnParentDamaged`, éclate sur `OnParentDestroyed`. La vitre se casse sans affecter le mur.
- La vitre doit pouvoir casser même dans un mur extérieur indestructible.
- Test : casser la vitre seule (le mur reste) ; casser le mur (la vitre éclate aussi) ; passer le ballon par la fenêtre cassée.

## Sous-lot 9.5-5 — Colonne
- **`BP_Destructible_Column`**, enfant de `BP_Destructible_Base` (pas du mur : pas de côtés A et B).
- Composants : `BeamMesh` (la poutre, indestructible, comme le milieu du mur) ; `GC_Cover` (l'enrobage, une seule GC) ; un pivot à la base.
- Oscillation en mode **quatre faces** (principe 4) avec interruption (principe 3).
- Effondrement : l'enrobage tombe, la poutre reste. Les dépendants (affiches, etc.) fonctionnent grâce à l'interface.
- Test : frapper chaque face ; deux ballons de deux angles en même temps (l'oscillation change d'axe sans saut et revient à sa rotation de départ).

## Sous-lot 9.5-6 — Porte à pentures
- **`BP_Destructible_Wall_Door`**, enfant de `BP_Destructible_Wall_Base` : mur avec une ouverture de porte (boîtes de blocage gauche, droite, linteau). Mur destructible à part entière.
- **`BP_Destructible_HingedDoor_Base`**, enfant de `BP_Destructible_Base` (nom choisi pour laisser la place à un futur `BP_Destructible_SlidingDoor_Base`), acteur attaché au mur :
  - `DoorMesh` (static mesh qui simule) + **Physics Constraint** vers le cadre : rotation libre sur un axe, limite d'environ ±100°, ouverture des deux côtés.
  - **Ressort de saloon** : Angular Drive (cible 0°) qui ramène la porte fermée.
  - **Stade 2, arrachée** : au-delà d'un seuil de dégâts, la contrainte casse, la porte tombe d'un bloc comme un objet physique.
  - **`IsLocked`** (booléen, éditable par instance dans le niveau, Category à suivre selon le guide) : verrouillée, la rotation est bloquée et la porte agit comme un mur. Quand ses PV de verrou tombent à 0 (Dash, ballon, projectile), elle est **défoncée** : le verrou saute, elle s'ouvre violemment (impulsion) et devient une porte normale.
  - Implémente `BPI_DestructibleDependent` : arrachée sur `OnParentDestroyed`.
  - Réglages dans un profil (par exemple `PDA_HingedDoorProfile`) : limite d'angle, force et amortissement du ressort, seuil d'arrachage, PV du verrou, impulsion de défonçage, masse.
- **Stabilité** (point délicat, l'utilisateur ne maîtrise pas ce calibrage ; Claude++ documente les valeurs retenues) : collision coupée entre la porte et le cadre (option de la contrainte), limites et amortissement solides, masse raisonnable, mise en sommeil à l'arrêt, pas de conflit permanent avec les objets autour (vérifier la projection de la contrainte et les itérations du solveur au besoin).
- **Stade 3 (éclatement en GC) : en attente.**
- Test de stress : 4 joueurs et le ballon qui poussent la porte en même temps ; porte verrouillée défoncée au Dash et au ballon ; mur détruit avec la porte (elle s'arrache).

## Sous-lot 9.5-7 — Placement, calibrage et ménage de la base
- Une instance de chaque variante dans `Lvl_Destruction_Gym` (dossier `Test/Destruction`), sans chevaucher d'objet physique ni de `PlayerStart`.
- Calibrage avec l'utilisateur (PV, oscillation, porte).
- Bilan final de ce qui a été remonté dans `BP_Destructible_Base` et de ce qui reste propre à chaque enfant.
- Mise à jour du devis maître et du devis vivant.

## Hors devis (backlog, nice to have)
- **Variables qui s'affichent seulement si une case est cochée** (métadonnée `EditCondition` + `EditConditionHides`). Voie sûre : déclarer la classe de base du profil (PDA) en C++ avec `UPROPERTY(meta = (EditCondition = "bUseX", EditConditionHides))`. Voie à tester (5 min) : poser la métadonnée sur une variable Blueprint par l'outil MCP. Travail pour Claude++, à planifier dans un devis à part (voir `claude/setup-cpp.md`).
- Stade 3 de la porte (éclatement en GC).
- Portes coulissantes (`BP_Destructible_SlidingDoor_Base`).
