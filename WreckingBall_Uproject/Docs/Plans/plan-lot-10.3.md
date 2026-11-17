# Plan d'action — Lot 10.3 : recul du Dash contre une surface trop lourde

Préparé le 2026-11-16 à la fin de la conversation du lot 10.2, pour démarrer une nouvelle conversation. Devis de référence : `Docs/Devis/devis-phase-c.md` (lot 10.3).

---

## 1. Décisions déjà prises par l'utilisateur

1. **Nouvel état `Player.State.Recoiling`** (choix A) : court, aucune action, mouvement bloqué, fin → `Free`.
2. **Règle de l'angle (seuil 20°)** : on mesure l'angle, à l'horizontale, entre la direction du Dash et l'inverse de la normale de la surface frappée.
   - **≤ 20°** (frappe de face) : recul **et** `Stunned` (durée et animations du stun existant).
   - **> 20°** (frappe en biais) : recul seul, en `Recoiling`, **sans** stun.
3. **« Trop lourd »** (laissé à Claude) : objet sans destruction possible (mur statique, objet incassable) **ou** destructible que ce Dash n'a pas cassé (élan insuffisant).
4. Dasheur qui tient quelque chose : on ne s'en occupe pas (le Dash est déjà interdit en tenant un objet).
5. **Étapes 2 et 3 fusionnées** : recul (C++) et animation + ABP dans la même passe.

---

## 2. Étapes

### Étape 0 — Relevé (lecture seule, aucun changement)
- Lire comment le Dash bouge le joueur dans `BP_GenericCharacter` (Launch, vélocité forcée, timeline ?), où il se termine (`Dashing` → `Free`) et comment son scan d'impact (`ApplyDashImpact`) sait si la cible a cassé, ou pas.
- Lire `DA_Physics_Dash` (élan minimum, seuils).
- Rapport court à l'utilisateur avant d'écrire le code (où brancher la détection).

### Étape 1 — C++ (une seule compilation, éditeur fermé)
- **Tags** (`WB_PlayerStateTags`) : `Player.State.Recoiling`.
- **`UWB_PlayerStateProfile`** (constructeur) :
  - table de transitions :
    - `Dashing → Recoiling` oui ;
    - `Recoiling → Free` (fin), `Stunned`, `Down`, `Disabled` oui ;
    - tout le reste non ;
  - `Recoiling` ajouté à `StatesBlockingMovement` et à `StatesBlockingSprint`.
  - **Attention** : `DA_PlayerState_Default` existe déjà, donc le constructeur ne le mettra pas à jour. Il faut ajouter la ligne et les tags dans l'asset (par MCP ou à la main).
- **`UWB_PlayerHitProfile`**, Category `PlayerHit|Recoil` :
  - `RecoilSpeed` *(à calibrer, ~400)* ;
  - `RecoilUpSpeed` *(~150)* ;
  - `RecoilDuration` *(~0,35 s)* ;
  - `RecoilStunAngle` = 20 ;
  - `RecoilAnim` (`TObjectPtr<UAnimSequenceBase>`).
  - Commentaire `/** */` en anglais sur chaque propriété.
- **`UWB_PlayerHitComponent`** :
  - `bool ApplyDashRecoil(FVector HitNormal, FVector DashDirection)` (BlueprintCallable), qui retourne vrai si le joueur est étourdi ;
  - calcule l'angle, lance le recul, passe en `Recoiling` ou en `Stunned`, et pose un timer de fin → `Free` ;
  - phase d'animation exposée, comme `GetHitReactPhase`, si l'ABP en a besoin.

### Étape 2 — Prévention des « lancers fous » de `Launch Character`
Les risques et les parades à mettre en place :

| Risque | Parade |
| --- | --- |
| Plusieurs Hits sur la même frame ou les suivantes qui additionnent les recul | Un seul déclenchement : `ApplyDashRecoil` ne fait rien hors de `Dashing`. L'entrée en `Recoiling` est refusée si on y est déjà (table). |
| La vitesse du Dash qui s'ajoute au recul | Arrêter le Dash d'abord (timer et vitesse : `StopMovementImmediately`), puis `LaunchCharacter` avec **`bXYOverride = true` et `bZOverride = true`** pour remplacer la vitesse au lieu d'additionner. |
| Normale de sol, de plafond ou d'arête qui lance vers le haut ou le bas | Normale aplatie (Z = 0, normalisée). On ignore le recul si la normale est surtout verticale (\|Z\| > 0,7). La hauteur vient seulement de `RecoilUpSpeed`. |
| Vitesse démesurée | Vitesse fixe du profil (pas proportionnelle à l'impact), avec un clamp. |
| Le scan du Dash qui continue pendant le recul | Le scan ne s'exécute qu'en `Dashing`. |
| Glissade sans fin après le recul | Le timer `RecoilDuration` → `Free`. Le freinage du CharacterMovement reste par défaut, à vérifier au test. |
| Objets physiques qui poussent le joueur | La capsule est cinématique : les objets simulés ne la poussent pas. Rien à faire, à confirmer au test. |

### Étape 3 — Blueprint et animation (éditeur rouvert)
- **`BP_GenericCharacter`** : à l'endroit trouvé à l'étape 0, quand la cible ne casse pas (ou est statique), appeler `ApplyDashRecoil(ImpactNormal, DashDirection)`. Prévoir un `IsValid` partout.
- **Animation placeholder `AS_Dash_Recoil`** (`Animation/Dash/`), environ 0,4 s, pipeline habituel (voir la section 4) :
  - choc ;
  - buste rejeté en arrière ;
  - bras qui s'écartent ;
  - petit retour.
- Référence de l'animation dans `RecoilAnim` du `DA_PlayerHit_Default`.
- **`ABP_SoccerPlayer`** :
  - nouvelle variable `IsRecoiling` (dispatcher `UpdateStateAnimParameters`, `MatchesTag Player.State.Recoiling`) ;
  - BLB plein corps placé **au-dessus du Dash** dans la chaîne (priorité plus haute que Dash) ;
  - le cas de face utilise le stun existant (`IsStunned`).
- Guide d'ergonomie : Descriptions, Category, Comments, rapport de la section 8.

### Étape 4 — Test (utilisateur)
- Dash de face sur un mur : recul et stun.
- Dash en biais (> 20°) : recul seul, pas de stun.
- Dash qui casse un mur : aucun recul.
- Dash sur un joueur : comportement du 10.2 inchangé (`Down`).
- Dash contre un coin, un poteau ou une arête : pas d'envol.
- Plusieurs Dashs à la suite : pas de cumul.

### Docs à proposer à l'utilisateur (le devis local est en lecture seule)
- Ajouter `Recoiling` aux décisions communes, à l'ordre de priorité (entre `Stunned` et `Dashing`) et à la table de transitions.
- Ajouter au lot 10.3 la règle des 20° et l'animation de rebond.

---

## 3. Travail du lot 10.2 à reporter dans les docs claude.ai (hors devis)

Fait pendant la conversation du lot 10.2, testé par l'utilisateur, pas encore dans le devis ni dans le backlog :

- **Animations placeholder** (rig `CR_SoccerPlayer_FK`, séquences `LS_Anim_*`, sorties `AS_*`) :
  - HitReact : Stunned Enter / Loop / GetUp, Down Fall / Enter / Loop / GetUp ;
  - Kick et Throw : Windup / Release ;
  - Dash_Loop ;
  - Celebrating / Sulking ;
  - Carry_Loop ;
  - **BallCarry_Loop** : jogging en bonds de 30 cm, plein corps ;
  - **BallIdle_Loop** : même cycle sur place, bras en A.
- **ABP_SoccerPlayer** :
  - **Haut du corps** : SM → BLB Carry. Avec `IsHoldingBall`, l'ancien BallCarry du haut du corps est maintenant masqué (à retirer au ménage). Puis Throw (windup piloté par `ActionCharge`), Sulking, Celebrating, Inertialization.
  - **Layered Blend** depuis `Bone`.
  - **Plein corps**, de la priorité la plus basse à la plus haute :
    1. **Ball locomotion** (`IsBallLocomotion` = tient le ballon, au sol, pas de throw ; dedans, `IsBallIdle` choisit l'idle ou le jog ; Play Rate 3,06 ; Sync Group `BallLocomotion`) ;
    2. Kick ;
    3. Stunned ;
    4. Down ;
    5. Dash.
- **C++** :
  - `UWB_AnimNotify_FootstepPuff` (« Footstep Puff ») : fait apparaître `NS_FootstepPuff` au pied, vers l'arrière de la course, rien sous `MinSpeed` ; dépendance `Niagara` dans le Build.cs ;
  - vitesse de célébration en courbe (montée rapide jusqu'à mi-parcours, puis retour graduel) ;
  - kick chargé à fond = le joueur ne se déplace plus, mais pivote.
- **VFX** : `NS_FootstepPuff`, petite bouffée blanche de 5 nuages, 0,6 s. La courbe `ScaleColor` RGB est à 1.
- **Blueprint** : `BPC_GrabObject.UpdateHeldObjectHolder` fait ignorer à la capsule l'objet tenu (`IgnoreComponentWhenMoving`), donc le ballon ne freine plus la course.

### Petits restes ouverts
- Guide §8 non appliqué sur les derniers ajouts. Il manque :
  - les Comments des blocs `IsBallIdle` / `IsBallLocomotion` et de `UpdateHeldObjectHolder` ;
  - les Descriptions de `IsBallIdle`, `IsBallLocomotion`, `IsHoldingBall`.
- Mise en page automatique (DSL) à retoucher : `ForceDrop`, `PlayHitFeedback`, `UpdateHeldObjectHolder`.
- Footstep Puff à poser dans `AS_BallCarry_Loop` et `AS_BallIdle_Loop` : frame 0 avec `Foot_r`, frame 17 avec `Foot_l`.
- Poings fermés : impossible sans re-rig dans le logiciel 3D (ni os de doigts ni morph target).
- Conflit possible entre les timelines de fin de sprint et la vitesse de célébration : jamais testé.
- Backlog : glisser un peu pendant `Stunned` sur une surface glissante.

---

## 4. Techniques d'animation (rappel pour la nouvelle conversation)

La mémoire de Claude (`placeholder-anim-pipeline`) contient la méthode complète. Les points clés :

- **Une clé à chaque frame**, avec slerp et déroulage des angles d'Euler (sinon les membres tournent de 360°).
- **Ne jamais appeler `create_level_sequence` sur une séquence déjà commitée.** Pour retoucher, on rouvre la séquence, on refait les clés, on exporte.
- **Export** : `convert_to_possessable` → `export_anim_sequence` → `convert_to_spawnable`. Un nouvel asset se fait en dupliquant `FM_AnimationRoot_Jump`.
- **Vérification** : `SkeletalMeshActor` temporaire en `AnimationSingleNode` à z = 20000, capture, PNG décodé dans le scratchpad, puis suppression de l'acteur.
- **Pour faire bouger le corps verticalement**, l'animation doit être plein corps : les jambes pendent de `Root`, pas de `Bone`.
- **Play Rate et Sync Group** se règlent dans la structure `Node` du lecteur (`method` = `SyncGroup`).
