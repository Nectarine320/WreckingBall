# Devis destruction — lot 9.5, addenda : collision des murs à ouverture (portes et fenêtres)

Rédigé le 2026-11-19. Remplace la partie « boîtes de blocage ajustées par le Construction Script » des sous-lots **9.5-4 (fenêtre)** et **9.5-6 (porte)** de `claude/devis-destruction-lot9-5.md`. Le reste de ces sous-lots (vitre, porte à pentures, profils, tests) ne change pas. Écrit pour Claude++.

## Contexte (relevé dans l'éditeur le 19 novembre)
- La collision du mur est maintenant **posée à la main** par l'artiste ou le designer (plus de `FitBlockerToWall`, plus de paramètres de taille de boîte).
- `BP_Destructible_Wall_Base` a **un seul** `Blocker` (Box). Il sert à deux choses : recevoir les coups (`OnComponentHit(Blocker)` → `BuildImpactDataFromHit` → `ApplyImpact`) et bloquer les joueurs. `OnEnterDestroyed` le coupe si `DisableBlockerOnDestroyed`.
- Hiérarchie actuelle : `BP_Destructible_Wall_Base` → `BP_Destructible_Wall_Interior` / `_Exterior` → prefabs `BP_Concrete_A/…` (`_Door_4m_01`, `_Windows_A/B/C_01`, `_Wall_*`). Les prefabs porte et fenêtre sont des placeholders, enfants de `_Interior`, sans logique propre.
- Composants du mur : `Blocker`, `SM_MiddleMesh`, `SideAPivot` → `GC_SideA`, `SideBPivot` → `GC_SideB`, `Wobble`. `CollapseSides` appelle `CollapseSide(1)` et `CollapseSide(-1)`.
- Aucun asset `BPI_DestructibleDependent`, mais l'interface existe en C++ (`UWB_DestructibleDependent`, voir §6). Aucun `BP_Destructible_Glass` ni `BP_Destructible_HingedDoor_Base`.

## Problème
Une fenêtre = un mur troué → 4 boîtes (gauche, droite, haut, bas). Une porte → 3 boîtes (gauche, droite, linteau). Un seul `Blocker` ne suffit plus, et on ne veut pas une classe par forme d'ouverture.

## Solution retenue : boîtes de blocage par tag
Le mur ne connaît plus **un** `Blocker`, mais **tous ses composants qui portent le tag `DestructibleBlocker`**. L'artiste ajoute autant de boîtes qu'il veut, les place à la main, leur met le tag. Le code est le même pour un mur plein, une fenêtre, une porte, une arche, un mur en L.

### 1. `BP_Destructible_Wall_Base`
- Variable `BlockerComponents` (tableau de Primitive Component, privée, Category `Wall`) : remplie à l'initialisation.
- Variable `BlockerTag` (Name, défaut `DestructibleBlocker`, non éditable par instance).
- Dans la surcharge **`InitializeDestructible`** : `GetComponentsByTag(PrimitiveComponent, BlockerTag)` → `BlockerComponents` ; pour chacun : `SetNotifyRigidBodyCollision(true)` (l'artiste n'a pas à cocher Hit Events lui-même) puis **Bind Event to OnComponentHit** → Custom Event **`HandleBlockerHit`** (nommé dès la création, selon le guide).
- `HandleBlockerHit` reprend exactement le corps actuel (`BuildImpactDataFromHit` → `ApplyImpact`), avec un filtre en plus : **ignorer un `OtherActor` attaché à ce mur** (ses dépendants : la porte qui frotte le cadre ne doit pas abîmer le mur ni spammer le feedback).
- Supprimer l'event lié `OnComponentHit(Blocker)`.
- `OnEnterDestroyed` : `SetCollisionEnabled` sur **chaque** élément de `BlockerComponents` (même condition `DisableBlockerOnDestroyed`).
- Si `BlockerComponents` est vide à l'initialisation : avertissement (Print String si `DebugPrint`, sinon log) avec le nom de l'acteur, pour attraper un prefab mal monté.
- Le `Blocker` existant **reçoit le tag** dans le template. Les murs déjà placés n'ont rien à faire (le tag vient du template) ; vérifier quand même les 14 murs de `Lvl_Destruction_Gym`.

Choix de `PrimitiveComponent` plutôt que `BoxComponent` : l'artiste peut utiliser une capsule ou un mesh simple de collision si une ouverture est cintrée.

### 2. Le `Blocker` hérité dans les prefabs à ouverture (validé)
Un composant hérité ne peut pas être supprimé dans un enfant. Règle : **le `Blocker` hérité devient la boîte n° 1** (par exemple le montant gauche). L'artiste le redimensionne, puis ajoute les autres boîtes (`Blocker_Right`, `Blocker_Top`, `Blocker_Bottom`) avec le tag. Rien à désactiver.

### 3. Preset de collision (écrit par Claude++ dans `Config/DefaultEngine.ini`, éditeur fermé)
Créer le preset **`DestructibleBlocker`** : WorldDynamic, QueryAndPhysics, Block partout sauf Camera et Debris (Ignore) — les réglages actuels du `Blocker`. L'artiste choisit le preset au lieu de régler chaque boîte. Appliquer le preset au `Blocker` du template.

### 4. Règles pour l'artiste (à mettre dans la Description du composant `Blocker` et dans le rapport)
- Chaque boîte de blocage : tag `DestructibleBlocker`, preset `DestructibleBlocker`.
- Les boîtes **englobent l'armature, le revêtement et la moulure**, et **s'arrêtent au bord de l'ouverture avec environ 1 à 2 cm de jeu** : la vitre et la porte ne doivent jamais toucher une boîte au repos.
- Le mesh du milieu et les GC des côtés sont modélisés avec le trou (assets des artistes).

### 5. Moulure (cadrage de porte ou de fenêtre) (validé)
Demande de l'utilisateur (19 novembre) : la moulure appartient **au mur**, pas à la porte. Si la porte casse, la moulure reste ; si le mur est détruit, la moulure tombe. Ce n'est ni le mesh du milieu ni une partie du prefab de porte.

- **Une seule GC** : composant `GC_Trim` dans `BP_Destructible_Wall_Base`, **enfant de la racine** (pas d'un pivot de côté), **vide par défaut** (aucune GC assignée = rien à faire, aucun coût ; le mur plein ne change pas).
- **N'oscille pas** : quand un côté tremble, la moulure reste immobile. Accepté par l'utilisateur (écart visuel jugé acceptable).
- **Tombe seulement quand le mur passe à `Destroyed`**, dans `OnEnterDestroyed` (via `CollapseSides` ou une étape juste après) : ancre retirée, crumble, impulsion, inscription au budget, comme les GC des côtés. Sur un mur extérieur, elle tombe quand l'état `Destroyed` est atteint (règle du 9.5-3 : tous les côtés destructibles tombés).
- L'asset de GC est posé **directement sur le composant dans le prefab** (la moulure dépend de la forme de l'ouverture, pas du profil partagé). Visible dans le viewport sans Construction Script.
- Mêmes réglages que `GC_SideA/B` : Object Type Debris, PhysicsOnly, Pawn / Visibility / Camera en Ignore, Simulate Physics vrai tenu par l'ancrage, Enable Damage From Collision faux, Root Proxy Renderer coupé. L'ajouter à l'ancrage de `InitializeDestructible` (seulement si une GC est assignée).
- Matériaux : si le sous-lot 9.5-3 (tableaux de matériaux) est fait, ajouter `TrimMaterials` au profil ; sinon, matériaux de l'asset.
- La moulure ne reçoit jamais de coup directement : elle est dans les boîtes de blocage. Pas de vie propre.

### 6. Dépendants (porte, vitre) (validé)
- L'interface des dépendants **existe déjà en C++** : `UWB_DestructibleDependent` (`OnParentDamaged(Parent, ImpactLocation, ImpactNormal, Damage)`, `OnParentDestroyed(Parent)`), implémentée par `BP_M_PhysicObject`. Elle remplace `BPI_DestructibleDependent` : aucune création manuelle. (Corrigé le 2026-11-19, approuvé par l'utilisateur.)
- **Le mur appelle ses dépendants** (pas d'abonnement) : `BP_Destructible_Base` appelle déjà `NotifyDependentsDamaged` et `NotifyDependentsDestroyed` (`UWB_DestructibleLibrary`), qui trouvent les acteurs attachés qui implémentent l'interface. Le mur ne connaît pas leur type.
- **Dans les prefabs**, la porte ou la vitre est un **Child Actor Component** (`Dependent_Door`, `Dependent_Glass`) placé par l'artiste dans l'ouverture : le child actor est attaché au mur, donc `GetAttachedActors` le trouve. Pour un cas spécial, un acteur placé dans le niveau et attaché au mur fonctionne aussi.
- La porte ou la vitre a **sa propre collision** et reçoit ses propres coups (`ApplyImpact` sur elle) : un coup sur la porte ne touche jamais le mur. Si le mur meurt, il appelle `OnParentDestroyed` : la vitre éclate, la porte s'arrache.
- **Porte** : la Physics Constraint relie `DoorMesh` à un composant **`FrameAnchor`** de la porte elle-même (pas au mur ni à la moulure), pour garder la porte autonome. Collision coupée entre les deux corps de la contrainte. La porte ne doit pas toucher la moulure au repos (même jeu de 1 à 2 cm ; la moulure est en Debris, la porte doit l'ignorer ou ne pas la chevaucher).
- **`IsLocked` par instance** (validé) : une variable d'un Child Actor n'est pas toujours modifiable par instance du mur. Si ça bloque, ajouter sur le prefab de mur une variable instance `DoorStartsLocked` que le Construction Script pousse au child actor. Claude++ teste et choisit.

### 7. Ordre
1. Preset `DestructibleBlocker` dans `Config/DefaultEngine.ini` (Claude++, éditeur fermé). L'interface des dépendants existe déjà en C++ (voir §6) : aucune création manuelle.
2. Tags et liaison dynamique dans `BP_Destructible_Wall_Base` (point 1). Test : les murs du gym reçoivent les coups et s'effondrent comme avant.
3. Composant `GC_Trim` vide dans la base, ancrage et chute à `Destroyed` (point 5). Test : mur plein inchangé.
4. Monter `BP_Destructible_Concrete_A_Windows_A_01` avec 4 boîtes et `_Door_4m_01` avec 3 boîtes, plus une moulure placeholder. Test : le ballon et les joueurs passent par l'ouverture ; un coup sur chaque boîte abîme le mur ; la moulure ne bouge pas pendant l'oscillation et tombe à l'effondrement.
5. Suite des sous-lots 9.5-4 (vitre) et 9.5-6 (porte à pentures), branchés en Child Actor. Test : la porte arrachée laisse la moulure en place.
6. Rapport final de la section 8 du guide.

## Décisions (19 novembre)
- Le `Blocker` hérité devient la boîte n° 1 (pas de second système). **Validé.**
- La porte et la vitre viennent avec le prefab en Child Actor. **Validé.**
- `IsLocked` : plan B `DoorStartsLocked` si nécessaire. **Validé.**
- Moulure = **une seule** GC `GC_Trim` du mur, attachée à la racine, vide par défaut, sans oscillation, qui tombe à `Destroyed`. **Validé** (la variante une GC par côté a été écartée).
- Prefabs porte et fenêtre laissés enfants de `_Interior` / `_Exterior` ; une classe dédiée seulement si une logique propre apparaît.

## Décisions du 19 novembre, après la revue du plan (remplacent les points concernés plus haut)
- **Boîtes de blocage : liaison en C++.** Un Custom Event lié dynamiquement à `OnComponentHit` exige un paramètre `Hit` passé par référence, que les outils MCP ne savent pas créer. Le composant C++ `UWB_DestructibleBlockersComponent` (composant `Blockers` du mur) trouve les composants qui portent le tag `DestructibleBlocker`, active leurs Hit Events, ignore le mur lui-même et les acteurs qui y sont attachés, puis relaie le coup par l'event `OnBlockerHit` (→ `BuildImpactDataFromHit` → `ApplyImpact`). Il prévient dans le log si aucune boîte n'est trouvée. `SetBlockersCollisionEnabled` coupe toutes les boîtes à `Destroyed`. Ce composant sert aussi à la vitre et à la porte. Les variables Blueprint `BlockerComponents` et `BlockerTag` du §1 ne sont donc pas créées.
- **Moulure par tag, dans le prefab seulement** (remplace le §5, « une `GC_Trim` vide dans la base ») : l'artiste ajoute la GC de moulure seulement dans les prefabs qui en ont une, avec le tag `DestructibleTrim`. La fonction `CollapseTrims` de `BP_Destructible_Wall_Base` les fait tomber à `Destroyed`, juste après `CollapseSides`. Pas de `TrimMaterials` dans le profil : les matériaux se règlent sur le composant, dans le prefab. Un mur plein n'a aucun composant de moulure.
- **Penture** (remplace `FrameAnchor` au §6) : la contrainte tient la porte à un point fixe de l'espace (pas de deuxième corps). Le prefab de porte a un composant `DoorHingePivot` (tag `DoorHinge`, placé sur l'axe de la penture, Z = axe), parent de la moulure. Sans pivot, la penture reste où la contrainte est placée dans le Blueprint de la porte, avec un avertissement dans le log.
- **Porte en C++** : `UWB_HingedDoorProfile` (PDA C++ : angle, masse, amortissements, ressort de saloon, verrou, arrachage ; les réglages du ressort et du verrou se cachent avec `EditCondition`) et `UWB_HingedDoorComponent` (contrainte, verrou, défonçage, arrachage ; implémente l'interface des dépendants : arrachée quand le mur est détruit). La vie de la porte (seuil d'arrachage) reste le `MaxHealth` de son profil destructible, comme pour tout destructible. Verrouillée, la porte ne simule pas (elle agit comme un mur).
- **Dépendants Blueprint sans ajout manuel d'interface** : `UWB_DestructibleDependentComponent` implémente l'interface et relaie `ParentDamaged` / `ParentDestroyed` en events (utilisé par la vitre). La bibliothèque prévient aussi les composants qui implémentent l'interface.
- **Preset `DestructibleBlocker`** écrit dans `Config/DefaultEngine.ini`.

## Montage du 19 novembre (Claude++)
- Ouvertures mesurées sur les meshes : `Windows_A_01` X 40 à 360, Z 100 à 355 ; `Door_4m_01` X 80 à 320, Z 0 à 340 (mur de 400 × 400, Y ±30).
- Boîtes : fenêtre `Blocker` (montant gauche) + `Blocker_Right`, `Blocker_Top`, `Blocker_Bottom` ; porte `Blocker` (montant gauche) + `Blocker_Right`, `Blocker_Top`. Le `Blocker` hérité se redimensionne à la main (l'outil MCP ne crée pas la surcharge d'un composant hérité).
- Vitre : `BP_Destructible_Glass` (GlassBlocker 100 × 2 × 100 cm, `GC_Glass` vide en attendant une GC placeholder, composants `Blockers` et `Dependent`), profil `PDA_DestructibleGlassProfile` (enfant de `PDA_DestructibleProfile` : `ShatterImpulseRadius`, `ShatterImpulseStrength`, `DebrisLifetime`), `DA_DestructibleGlass_Default` (5 PV, surface Glass). Dans le prefab : Child Actor `Dependent_Glass`, mis à l'échelle de l'ouverture avec 1 cm de jeu.
- Porte : `BP_Destructible_HingedDoor_Base` (`DoorMesh` cube placeholder 238 × 5 × 338 cm qui simule, `HingeConstraint`, `HingedDoor`, `Blockers`), `DA_DestructibleDoor_Default` (150 PV, surface Wood), `DA_HingedDoor_Default` (verrouillable). Dans le prefab : `DoorHingePivot` (tag `DoorHinge`, X 81) → Child Actor `Dependent_Door`.
- `IsLocked` : plan B retenu, variable instance `DoorStartsLocked` (Category `Door`) sur le prefab, poussée à la porte par le Construction Script (`ApplyDoorLock`).
- Rayon d'impulsion de l'effondrement du mur : `CollapseImpulseRadius` (600) dans `PDA_DestructibleWallProfile`, plus de valeur en dur.
- Moulure placeholder : pas encore d'asset de GC ; le code (`CollapseTrims`, tag `DestructibleTrim`) est prêt.

## Porte : Dash, slam et verrou (décisions du 19 novembre, remplacent le « défonçage » du 9.5-6)
- **Verrou cassé** (PV du verrou à 0, Dash, ballon ou objet) : la porte est **décrochée** de ses pentures et devient un objet physique (plus de stade « défoncée, reste sur ses pentures »). Une porte décrochée suit les règles des objets physiques ; le Dash la pousse et passe.
- **Dash dans une porte déverrouillée** : pas de recul ; le dasheur passe au travers (porte ignorée pour le reste du Dash) et la porte s'ouvre violemment, loin de lui. Porte verrouillée : recul et dégâts au verrou, comme avant.
- **Priorité** : le premier dasheur prend la porte pendant `DashPriorityWindow`. Deux Dash dans la même frame : le plus gros impact gagne. Un dasheur du même côté passe aussi ; un dasheur du côté opposé est plaqué (Down) par la porte.
- **Slam** : la porte s'éloigne de sa position fermée plus vite que `SlamMinAngularSpeed` (Dash, ballon, objet). Un joueur qu'elle frappe est mis **Down** (pas Stunned), projeté dans le sens où la porte bouge (`ReceiveSlamHit`, `SlamKnockbackStrength` / `SlamKnockbackUpStrength` dans le profil de coup). Dès que la porte repart vers sa position fermée, elle est inoffensive. Un seul joueur par slam. Coéquipiers compris (la source est la porte, sans équipe).
- **Protection** : le dasheur prioritaire (et ceux du même côté) ne peut pas être plaqué par la porte pendant `DasherProtectionDuration`.
- **Double pénalité** : le premier coup gagne ; la table d'états refuse Down → Down et l'invulnérabilité suit le relevé.
- Backlog : son « porte verrouillée qui résiste » quand un joueur pousse une porte verrouillée.
