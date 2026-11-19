# Bilan — lot 9.5 (destruction) : variantes de murs, fenêtres, colonne, porte à pentures

Rédigé le 2026-11-19 par Claude++ à la fin du lot. Complète `devis-destruction-lot9-5.md` et son addenda `devis-destruction-lot9-5-ouvertures.md`. Le placement dans le gym et le calibrage (PV, oscillation, porte) sont **hors mandat** : ils feront l'objet d'une passe à part. Le rapport final de la section 8 du guide sera produit par Claude Desktop.

---

## 1. État des sous-lots

| Sous-lot | État | Notes |
|---|---|---|
| 9.5-1 Renommage générique du mur | Fait (avant ce chantier) | |
| 9.5-2 Remontée dans la base, dépendants | Fait | Interface des dépendants en C++ (`UWB_DestructibleDependent`) au lieu de `BPI_DestructibleDependent`. Debounce par source confirmé. |
| 9.5-3 Couches et matériaux | Fait | Mur extérieur : à `Destroyed`, seul le côté destructible tombe ; la vitre éclate, la porte s'arrache. |
| 9.5-4 Fenêtre | Fait | Prefabs `Windows_A/B/C_01`, vitre en static mesh + Niagara, cadre en GC. |
| 9.5-5 Colonne | Fait (avant ce chantier) | Listes de matériaux par instance alignées sur les murs le 19 novembre. |
| 9.5-6 Porte à pentures | Fait, avec écarts au devis | Voir §3 (Dash, slam, verrou). Stade 3 (éclatement en GC) toujours en attente. |
| 9.5-7 Placement, calibrage, ménage | Partiel | Bilan de la base : §2. Placement et calibrage : hors mandat. |

---

## 2. Bilan de la base (`BP_Destructible_Base`)

### Ce que la base porte (commun à tous les destructibles)
- **Cycle de dégâts** : `ApplyImpact` (interface `BPI_ImpactReceiver`) → `GetSourceProfile` → `TryRegisterHit` (debounce par source) → `CanTakeDamageFrom` → calcul des dégâts → `ComputeNewHealth` → `SetDestructibleState` (Intact / Damaged / Destroyed, dispatcher `OnStateChanged`).
- **Réactions** : `ReactToDamage` (prévient les dépendants, lance l'oscillation si l'acteur a un `UWB_WobbleComponent`), `PlayMinorHitWobble` (coups refusés), `PlayImpactFeedback`, `GetFeedbackSound` / `GetFeedbackFX`.
- **États** : `OnEnterDamaged`, `OnEnterDestroyed` (prévient les dépendants, feedback `Feedback.Destruction`). Les enfants surchargent `OnEnterDestroyed` pour leur propre effondrement.
- **Hooks faits pour être surchargés** : `CanTakeDamageFrom`, `IsSideDestructible`, `GetAttachSurfaceForSide`.
- **Outils** : `BuildImpactDataFromHit`, `GetStateForHealth`, `PrintImpactDebug`.
- **Profil** : `PDA_DestructibleProfile` (PV, seuils, courbe énergie → dégâts, debounce, feedback, oscillation). Enfants : `PDA_DestructibleWallProfile`, `PDA_DestructibleColumnProfile`, `PDA_DestructibleGlassProfile`.

### Fondation C++ (réutilisée par plusieurs enfants)
| Classe / fonction | Rôle | Utilisée par |
|---|---|---|
| `UWB_DestructibleBlockersComponent` | Boîtes de blocage par tag `DestructibleBlocker`, relaie les coups (`OnBlockerHit`), ignore l'acteur et ses dépendants | Mur, vitre, porte |
| `UWB_DestructibleDependentComponent` | Rend un Blueprint dépendant sans ajouter l'interface à la main (`ParentDamaged`, `ParentDestroyed`) | Vitre |
| `UWB_WobbleComponent` | Oscillation un axe ou quatre faces, interruption sans saut | Mur, colonne, vitre |
| `UWB_HingedDoorComponent` + `UWB_HingedDoorProfile` | Penture, ressort, verrou, Dash au travers, slam, arrachage | Porte |
| `UWB_DestructibleLibrary` | Dépendants (acteurs et composants), effondrement d'une GC, côté d'impact, `SyncMaterialOverrides` | Tous |

### Ce qui reste propre à chaque enfant
- **Mur** (`BP_Destructible_Wall_Base`) : côtés A/B et pivots, fissures, `BreakSideAtImpact`, `CollapseSide(s)`, moulures et cadres par tag (`CollapseTrims`), coupure des boîtes (`DisableBlockersIfOpen`), listes de matériaux (côtés, milieu).
- **Colonne** : poutre et enrobage, oscillation quatre faces, `CollapseCover`, listes de matériaux.
- **Vitre** (`BP_Destructible_Glass`) : `ShatterGlass` (cache la vitre, coupe ses boîtes, Niagara orienté), oscillation quand le mur est frappé.
- **Porte** (`BP_Destructible_HingedDoor_Base`) : lien `OnDamaged` → verrou, `OnEnterDestroyed` → arrachage, tag `NoGrab`.

### Candidats à remonter plus tard (non faits)
- Le bloc **`OnBlockerHit` → `BuildImpactDataFromHit` → `ApplyImpact`** est copié dans le mur, la vitre et la porte. Il pourrait vivre une seule fois dans la base (composant `Blockers` + event), si la colonne adopte aussi les boîtes par tag.
- Les **listes de matériaux par instance** (`SyncMaterialOverrides`) sont appelées séparément dans le mur, la colonne et le prefab de porte. Un schéma commun (une liste par composant tagué) éviterait de les recâbler à chaque nouveau type.

---

## 3. Écarts au devis et décisions du chantier (19 novembre)

### Ouvertures (addenda)
- Boîtes de blocage **par tag**, liées en C++ (l'outil MCP ne crée pas un paramètre `Hit` par référence). Preset `DestructibleBlocker` dans `DefaultEngine.ini`. Rendu des boîtes : rouge (TSL 5°, 100 %, 50 %), épaisseur 2,5.
- Les boîtes s'arrêtent au **bord intérieur du cadre** ; leur profondeur ne couvre pas le dépassement du cadre (option c). Pas de collision sur les meneaux.
- **Cadres** de porte et de fenêtre en **GC** (`GC_Frame`, tag `DestructibleTrim`), font partie du mur : on ne peut pas les casser sans casser le mur ; ils tombent dès que le mur passe à `Destroyed`.
- **Un mur qui a un côté indestructible reste un mur** : ses boîtes restent actives à `Destroyed` (`DisableBlockersIfOpen`). La vitre éclate et le cadre tombe ; on passe par l'ouverture, pas par le mur.
- Fenêtres : profil dédié `DA_Concrete_A_Windows_01`, **côté B indestructible** par défaut.
- Effondrement : rayon d'impulsion dans le profil (`CollapseImpulseRadius`), origine = centre du mur.

### Vitre
- **Static mesh + Niagara** au lieu d'une GC (transparence et éclats minces mal gérés par Chaos, budget de débris préservé). `NS_PH_GlassShatter_Burst` : nuage du footstep + 4 cubes projetés dans le sens du ballon.
- Oscillation propre à la vitre, déclenchée quand son mur est frappé.

### Porte (remplace le « défonçage » du 9.5-6)
- **Verrou cassé** : la porte est **décrochée** et devient un objet physique (plus de stade « défoncée, reste sur ses pentures »).
- **Dash dans une porte déverrouillée** : pas de recul ; la porte s'ouvre de force jusqu'à `DashOpenAngle` (90°, cinématique), le dasheur passe au travers. Porte verrouillée : recul et dégâts au verrou, comme avant.
- **Priorité** : le premier dasheur prend la porte pendant `DashPriorityWindow` ; même frame : le plus gros impact gagne. Même côté : passe aussi ; côté opposé : plaqué.
- **Slam** : porte qui s'ouvre plus vite que `SlamMinAngularSpeed` (Dash, ballon, objet) → le joueur frappé est mis **Down**, projeté dans le sens de la porte (`ReceiveSlamHit`). Inoffensive dès qu'elle revient vers la position fermée. Le dasheur prioritaire est protégé ; les coéquipiers ne le sont pas.
- Mur détruit : la porte est arrachée à l'opposé du dernier coup reçu par le mur, et bascule (`TearOffPushHeight`).
- Penture : pivot `DoorHingePivot` (tag `DoorHinge`) dans le prefab ; contrainte vers un point fixe (pas de deuxième corps).
- Porte placeholder (cube) de 215,5 × 304 cm, à 25 cm du sol, dans l'ouverture du cadre en bois.
- **Pas de grab** : tag d'acteur `NoGrab`, respecté par `BPC_GrabObject`.
- `IsLocked` par instance : variable `DoorStartsLocked` sur le prefab de mur-porte (plan B).

### Matériaux par instance
- Chaque liste (côtés A/B et milieu du mur, porte, poutre et enrobage de la colonne) a **exactement** le nombre de slots de sa pièce et affiche les matériaux en usage. Vider une case revient au matériau du mesh.

---

## 4. Restes et backlog

**Mandat suivant (annoncé par l'utilisateur)**
- Portes **doubles** et portes **coulissantes** (`BP_Destructible_SlidingDoor_Base`).
- **Son de porte verrouillée** : cue de feedback quand un joueur pousse une porte verrouillée, avec un délai réglable.

**Hors mandat / passe à part**
- Placement d'une instance de chaque variante dans le gym, calibrage (PV, oscillation, porte).

**Petits restes connus**
- Prefab `Windows_B_01` : `Blocker` hérité à régler à la main — Location (45, 0, 200), Box Extent (45, 30, 200). L'instance du gym est déjà corrigée.
- Bande de collision invisible laissée par le cadre tombé sur un mur dont le côté B est indestructible.
- `CollapseImpulseStrength` est encore une variable du Blueprint du mur (devrait aller dans le profil).
- `DA_DestructibleWall_Exterior` référence un mesh introuvable (`SM_Wall_Middle_Wood_A_Stats-Final`).
- Stade 3 de la porte (éclatement en GC) : en attente.

**Abandonné (décision de l'utilisateur)** : boîtes amincies pour ouvrir la porte au-delà de 90°, règle de grab « pas l'objet sous ses pieds ».

---

## 5. À coller dans le projet claude.ai

### Devis maître — ligne de la phase B
> **Lot 9.5 — fait (2026-11-19).** Variantes de murs (fenêtres A/B/C avec vitre Niagara et cadre en GC, mur-porte avec porte à pentures), colonne, couches destructibles, dépendants. Fondation C++ : boîtes de blocage par tag, composant dépendant, oscillation, porte à pentures (Dash au travers, slam → Down, verrou cassé → porte décrochée). Placement et calibrage reportés à une passe à part. Détails : `Docs/Devis/bilan-lot9-5.md`.
>
> **À venir (prochain mandat, phase B)** : ajout des **portes doubles** (deux battants à pentures, assets `SM_Door_Double_*` des artistes) et des **portes coulissantes** (`BP_Destructible_SlidingDoor_Base`), avec le son de porte verrouillée.

### Devis vivant — décisions à reporter
> - Porte : le verrou cassé **décroche** la porte (remplace le « défonçage »). Dash dans une porte déverrouillée : on passe, la porte s'ouvre de force à 90°. Porte qui slamme : Down, projeté dans le sens de la porte, coéquipiers compris.
> - Vitre : static mesh + Niagara, jamais de GC (transparence).
> - Cadres de porte et de fenêtre : GC qui font partie du mur ; un mur avec un côté indestructible reste un mur (boîtes actives), seuls la vitre et le cadre tombent.
> - Fenêtres : côté B indestructible par défaut.
> - Objets non attrapables : tag d'acteur `NoGrab`.
