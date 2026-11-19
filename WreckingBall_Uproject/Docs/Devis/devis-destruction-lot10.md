# Devis destruction — lot 10 : feedback par surface et effondrement

**Version 4, 2026-11-15, 13 h : alignée sur `CLAUDE.md` et sur le guide d'ergonomie (mise à jour du lot 9.5). Prêt à remettre à Claude++** (= Claude Code). Étape 8 du devis maître, **dernier lot de la phase B** (destruction et terrain). Une fois ce lot fait, l'essentiel de la phase B est accompli et on passe à la phase C (le joueur, étape 9).

## 0. Prérequis et règles de travail
- **Benchmark du lot 9** (section 8 de `Docs/Devis/devis-destruction-lot9.md`, par l'utilisateur, en Standalone) : à faire **avant** ce lot si possible. Les sons et les Niagara ajoutent du coût, et il faut connaître la marge (cible 60 images/s, plancher 30).
- **Copie locale** : l'utilisateur dépose ce devis dans `Docs/Devis/devis-destruction-lot10.md` (lecture seule pour Claude++, comme le guide).
- **Commits** : l'utilisateur commite avant et après le lot, et après chaque sous-lot testé. Claude++ ne commite pas sans qu'on le lui demande.
- **Analyse d'abord** (CLAUDE.md) : avant de commencer, Claude++ donne pour chaque sous-lot une ligne « **C++ ou Blueprint** », avec la raison (critères de CLAUDE.md) et le coût (compilation éditeur fermé, création manuelle évitée…). Il signale les sous-lots à fusionner ou à redécouper. **L'utilisateur tranche avant le début.** Les pistes de la section 4 sont des suggestions, pas des décisions.
- **Autonomie dans l'éditeur** : avec `unreal-mcp`, Claude++ fait lui-même les branchements Blueprint, la création des assets placeholders et le remplissage des data assets. Restent à l'utilisateur : tests en jeu, calibrage, commits.
- Avant de toucher un Blueprint : lire `Docs/guide-ergonomie-blueprint.md` et l'appliquer en entier. Chaque sous-lot se termine par le rapport de la section 8 du guide, dans le format de rapport de CLAUDE.md (titres typés (C++), (Blueprint), (Data Asset), (Config)…, section « À vérifier / À faire de ton côté »). Au début du chantier, lister les créations manuelles restantes.
- **C++** : chaque `UPROPERTY` / `UFUNCTION` exposée a une `Category` et un commentaire `/** */` en anglais. Les changements de `.h` d'un même sous-lot sont regroupés en une seule compilation (éditeur fermé).
- **Blueprint** : chaque nouvelle variable a une Category et, si son effet n'est pas évident, une Description en anglais. Nouvelle option dans une classe de base (`BP_Destructible_Base`, `BP_M_PhysicObject`, `PDA_…`) = booléen **désactivé par défaut** quand elle change un comportement existant.
- **Instances déjà placées** : après l'ajout d'une variable, d'un champ de profil ou d'un composant à un Blueprint déjà placé dans le gym, vérifier les instances et remettre la valeur par défaut (`reset_properties` ; `set_properties` si c'est une propriété C++).
- Pas de nouvel enum Blueprint (Gameplay Tags ou booléens). Réglages dans les profils (PDA / DA), pas en dur.

## 1. Le but
Rendre la destruction **lisible et satisfaisante** : chaque coup sonne et réagit selon la matière frappée, chaque destructible qui cède a son moment, et le joueur responsable le **sent** dans sa manette.

## 2. Décisions de l'utilisateur (2026-11-15)
1. **Assets** : aucun son ni Niagara dans le projet. Tout est en **placeholder**, à remplacer par les assets de l'équipe.
2. **Manette** : vibration (Force Feedback) **dans la manette du joueur qui détruit**, pas des autres. Dash = le joueur qui dashe. **Plus tard (étape 10)** : un Dash dans un autre joueur fait vibrer **les deux** manettes.
3. **Secousse de caméra** : **réglable, activable et désactivable**, et **subtile**. Elle pourrait être retirée si on détruit trop de choses : elle doit pouvoir se couper d'une case, sans toucher au code.
4. **Intensité** : **deux sons, léger et lourd**, pas de gradation. (Le système actuel lourd / léger est gardé.)
5. **Objets accrochés** : **un son quand ils se décrochent** (obligatoire). Un son quand ils touchent le sol : oui, selon le sol et la force de l'impact.
6. **Débris** : **pas de son** quand les morceaux touchent le sol (trop de sons en même temps). Un seul son à la destruction.
7. **Objets lancés** (pneu, etc.) : l'utilisateur ne voit pas de dégâts nets quand il les lance sur un mur. À diagnostiquer et corriger en premier (sous-lot 10-0).

## 3. Ce qui existe déjà
- `PDA_ImpactFeedback` (4 Maps : son lourd, son léger, FX lourd, FX léger, par Physical Surface) et `DA_ImpactFeedback_Default` (**Maps vides**).
- `BP_Destructible_Base.PlayImpactFeedback` (surface de l'impact, sinon celle du profil ; lourd = impact accepté, léger = refusé), `GetFeedbackSound` / `GetFeedbackFX` (repli sur `SurfaceType_Default`).
- Physical Surfaces 1 à 11 (Plaster … Character) et leurs PM.
- Champs prévus au devis destruction : `CollapseSound`, `CollapseFX`.
- Événement `OnPieceRemoved(Collection, PieceIndex)` du gestionnaire C++ (lot 9) : **non utilisé ici** (décision 6).
- **Fondation C++ du lot 9.5** : interface des dépendants, oscillation (`UWB_WobbleComponent`), effondrement d'une GC. Familles : mur (couches, côté protégé), vitre, colonne, porte à pentures (verrou défoncé, arrachée), objets accrochés via l'interface C++ `WB_DestructibleDependent` (remplace le `BPI_DestructibleDependent` prévu au devis du lot 9.5). Lire le rapport du lot 9.5 et `Source/WreckingBall/` pour les noms exacts : les moments de destruction (4.3) se branchent de préférence sur les événements déjà émis par ce code.
- **Objets lancés** : tout enfant de `BP_M_PhysicObject` est déjà une source d'impact (`BPI_ImpactSource`, lot 5). `GetPhysicsProfile` renvoie son `PhysicsPreset`, sinon `DA_Physics_Object_Default` (25 kg, `DamageMultiplier` 1, `MinImpactSpeed` 200, kick neutre). `CanDealImpactDamage` est vrai s'il n'est pas tenu (et hors délai de grâce après une chute). Dégâts = énergie (½ × masse du profil × vitesse normale², en J) passée dans `CRV_EnergyToDamage_Default` (courbe en S : 0 J → 0 PV, 20 000 J → 100 PV ; environ 3 900 J pour 10 PV), × `DamageMultiplier` ; sous `MinImpactSpeed`, dégâts nuls ; sous `MinDamagePerHit` (10), le coup est refusé. Le ballon de soccer (70 kg, kick ×1,5) fait 39 à 67 PV.

## 4. Ce qui est à construire

### 4.0 Diagnostic des objets lancés (sous-lot 10-0)
**Hypothèse** : le code le permet déjà ; c'est surtout une question de données. Un objet sans profil compte pour 25 kg avec un lancer moins rapide qu'un kick, donc ses dégâts restent souvent sous le seuil de 10 PV.
**Diagnostic de Claude++ (2026-11-15)** : la cause principale est l'écart entre la masse réelle et la masse du profil. `ThrowPhysicActor` calcule la vitesse avec la **masse réelle** du mesh (vitesse = charge × 2000 / (masse / 10), plafonnée à 2000 × charge, en cm/s), mais les dégâts utilisent la **masse du profil** (25 kg sans profil). Résultat : les objets lourds (pneu et cône forcés à 150 kg) partent à 1,3–5,3 m/s et ne font aucun dégât, et les objets légers (carton 0,6 kg) partent à 20–80 m/s et comptent pour 25 kg (16 à 100 PV, plus que le ballon). Correctif retenu : 5 paliers de poids (`DA_Physics_Thrown_1_VeryLight` à `DA_Physics_Thrown_5_VeryHeavy`), dont la masse et l'amortissement sont aussi appliqués au mesh par `ApplyPhysicsPreset` (amortissement angulaire 5, comme les réglages faits à la main dans le projet).
À vérifier par Claude++, sur le pneu puis sur quelques objets lançables représentatifs :
1. La classe parent (doit être un enfant de `BP_M_PhysicObject` ; sinon, il n'est pas une source d'impact).
2. Son `PhysicsPreset` (vide = profil par défaut à 25 kg).
3. Sa vitesse réelle au moment du choc (le lancer par `BPC_GrabObject.ThrowPhysicActor`, Vel Change vrai) et les dégâts calculés : affichage de debug **temporaire** de `ApplyImpact` (vitesse, masse, énergie, dégâts, accepté ou non), derrière un booléen désactivé par défaut, ou retiré à la fin du sous-lot. Pas de Comment sur ce code temporaire.
4. Que le choc touche bien le `Blocker` (collision de l'objet lancé vs `Blocker`, Hit Events).

Correctif attendu, selon le diagnostic :
- des **profils par famille d'objets** (`DA_Physics_Tire`, etc.) avec une masse et un `DamageMultiplier` qui donnent un résultat lisible ; cible à valider avec l'utilisateur : un objet lourd lancé ≈ un kick de soccer, un objet léger fait un feedback léger sans dégâts ;
- ou une hausse du profil par défaut, si la plupart des objets sont mal servis.
Si un profil est assigné aux objets déjà placés dans le gym, vérifier chaque instance (section 0).
Tableau des objets testés (masse, vitesse, dégâts avant / après) dans le rapport.

**Piste** : Blueprint et Data Assets (réglage, aucun critère C++).

### 4.1 Placeholders
- Dossier dédié (par exemple `/Game/Audio/Placeholder/` et `/Game/FX/Placeholder/`, Claude++ propose selon la structure du projet).
- **Sons** : un léger et un lourd par surface utilisée (plâtre, verre, bois, brique / béton, `Default`), plus les sons de « moments » (4.3). Si aucun son n'existe dans le moteur ou le projet, Claude++ le signale : l'utilisateur fournira des fichiers WAV temporaires. Chaque son passe par un **Sound Cue ou un MetaSound avec variation aléatoire de hauteur et de volume** (le même coup répété ne doit pas fatiguer l'oreille), pour que le remplacement par les vrais assets se fasse sans toucher au code. Claude++ signale ce qui ne peut pas se créer par MCP (création manuelle à lister).
- **Niagara** : systèmes simples (poussière, éclats), une couleur par matière si possible.

### 4.2 Feedback d'impact par surface
- Remplir `DA_ImpactFeedback_Default` (les 4 Maps) pour les surfaces ci-dessus.
- **Côté protégé** (lot 9.5) : feedback léger de la matière du côté frappé, pas celle du profil.
- Le feedback léger ne se répète pas à chaque frame de contact (règle du lot 4, à garder).

**Piste** : Blueprint et Data Asset (le système existe déjà).

### 4.3 Moments de destruction
Un feedback par événement, réglé dans le profil de chaque famille. Le son et le FX partent du **centre de la partie qui cède**, pas du point d'impact. **Un seul son par événement** : aucun son de chute des débris (décision 6).

| Événement | Son | FX |
| --- | --- | --- |
| Mur : effondrement d'un côté | `CollapseSound` | `CollapseFX` (poussière le long du côté) |
| Vitre : éclatement | verre | éclats |
| Colonne : chute de l'enrobage | `CollapseSound` | poussière à la base |
| Porte : verrou défoncé | craquement de bois | éclats de bois |
| Porte : arrachée | son lourd | poussière |
| Objet accroché : décroché | `DetachSound` (obligatoire) | aucun |
| Objet accroché : touche le sol | selon la surface du sol (4.4) | aucun |

**Piste** : le centre de la partie qui cède d'une GC (bornes des morceaux) relève de l'API GC, absente en Blueprint → calcul en C++, dans la fondation du lot 9.5 (par exemple une fonction `BlueprintPure` qui renvoie le centre). Le choix du son et du FX, et leur lecture, peuvent rester en Blueprint (profils).

### 4.4 Objet accroché qui touche le sol
- Après le décrochage, au premier contact (Hit) assez fort : son selon la **surface du sol** frappé (Physical Surface du Hit), en réutilisant `PDA_ImpactFeedback` (son léger ou lourd selon un seuil de vitesse).
- Réglages dans `PDA_PhysicsProfile` (Category `Attached|Feedback`, Descriptions en anglais avec l'unité) : `FloorImpactMinSpeed` (cm/s ; sous ce seuil, aucun son), `FloorImpactHeavySpeed` (cm/s ; au-dessus, son lourd), `FloorImpactCooldown` (s ; pas de son répété quand l'objet rebondit).
- Le son s'arrête de jouer quand l'objet est immobile (pas de son en continu).

**Piste** : logique réutilisée par tous les objets accrochés → candidat C++ (un composant, par exemple `UWB_FloorImpactFeedbackComponent`, ajouté aux objets accrochés). En Blueprint, l'event de Hit se crée avec `add_component_bound_event`, pas avec le DSL.

### 4.5 Vibration de la manette du joueur responsable
- **Qui est responsable** : Dash = le personnage qui dashe. Ballon ou objet lancé = **le dernier joueur qui l'a lancé**. Claude++ vérifie si le projet garde déjà ce dernier joueur (le but marqué l'utilise peut-être) ; sinon, l'ajouter à `BP_M_PhysicObject` (`LastInstigator`, Category et Description, réglé au lancer et au kick dans `BPC_GrabObject`) et le transmettre dans `STR_ImpactData` (champ `Instigator`, si la struct ne l'a pas).
- Deux vibrations (Force Feedback Effect placeholder) : **légère** à un impact accepté, **forte** à une destruction (mur, vitre, colonne, porte). Rien sur les impacts refusés.
- Réglages globaux (4.6) : activable, intensité.
- Pas de joueur responsable (objet tombé seul) : pas de vibration.
- **Prévoir pour l'étape 10** : la fonction qui fait vibrer prend un joueur en paramètre (par exemple `PlayImpactHaptics(PlayerPawn, IsStrong)`), pour que le Dash d'un joueur dans un autre puisse faire vibrer les deux manettes sans refaire le code.

**Piste** : `PlayImpactHaptics` a un paramètre de type objet (le joueur), impossible à créer par MCP en Blueprint, et sert à plusieurs systèmes → candidat C++ (bibliothèque de fonctions `BlueprintCallable`). Ajouter un champ à `STR_ImpactData` (struct Blueprint) : Claude++ vérifie si l'outil le permet ; sinon, création manuelle à signaler. Ne pas convertir la struct en C++ sans l'accord de l'utilisateur.

### 4.6 Secousse de caméra (subtile, réglable)
- La caméra est commune (`BP_SoccerCamera`) : la secousse touche tout le monde, d'où la prudence.
- Seulement aux **destructions** (pas aux impacts).
- **Réglages globaux**, dans un seul data asset (Claude++ propose : des champs dans `PDA_ImpactFeedback`, Category `Feedback|CameraShake` et `Feedback|Haptics`, ou un nouveau `DA_FeedbackSettings`). Chaque champ a une Description en anglais (effet, unité ou plage) :
  - `bEnableCameraShake` (case à cocher) ;
  - `CameraShakeScale` (très bas par défaut : subtil) ;
  - `CameraShakeMaxPerSecond` ou un cooldown (plusieurs murs qui tombent en même temps ne s'additionnent pas) ;
  - `bEnableHaptics`, `HapticsScale`.
- Un Camera Shake placeholder (petite amplitude, courte durée).

**Piste** : le plafond par seconde (état partagé entre tous les destructibles) peut aller dans la même bibliothèque ou un sous-système C++ que 4.5, pour regrouper la compilation. Les réglages restent dans un data asset.

### 4.7 Budget du feedback
- **Son** : une Sound Concurrency par famille (nombre maximal de voix, les plus vieilles coupées) et une atténuation commune.
- **Niagara** : FX courts, un plafond de FX simultanés, mise en commun des systèmes (pooling).
- Mesurer avec les stats (`stat sounds`, `stat niagara`) à la fin du sous-lot 10-4.

## 5. Sous-lots
0. **10-0** Diagnostic et correctif des objets lancés (4.0). Court, en premier : il change ce que le joueur ressent avant tout feedback.
1. **10-1** Placeholders (4.1) + feedback d'impact par surface (4.2) + budget du son (4.7).
2. **10-2** Moments de destruction (4.3) + objets accrochés : décrochage et sol (4.4).
3. **10-3** Joueur responsable + vibration (4.5) + secousse de caméra (4.6).
4. **10-4** Calibrage avec l'utilisateur, mesure du coût, bilan de la phase B dans le devis maître.

Regrouper en une compilation (éditeur fermé) les `.h` de 10-2, puis ceux de 10-3 ; si le C++ est retenu pour les deux, Claude++ peut proposer une seule compilation pour 10-2 et 10-3.

Chaque sous-lot : analyse C++ ou Blueprint validée, guide d'ergonomie, rapport (section 8 du guide + format de CLAUDE.md), test de l'utilisateur, commit par l'utilisateur.

## 6. Hors champ
- Sons de chute des débris (décision 6).
- Vibration des deux joueurs quand un Dash frappe un joueur : **étape 10** (le joueur devient un `BPI_ImpactReceiver`).
- Le trail du ballon (étape 11).
- Les sons des joueurs et du match (but, sifflet, foule).
- Le stade 3 de la porte.
- Une gradation continue du son (décision 4).
