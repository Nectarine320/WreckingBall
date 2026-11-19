# Wrecking Ball — Devis : dribble du ballon (le ballon roule au sol devant le joueur)

Créé le 2026-11-19. Brouillon bâti avec l'utilisateur, **pas encore autorisé**. Option C de l'analyse du 2026-11-19. Guide d'ergonomie Blueprint à appliquer, rapport de la section 8 à la fin.

## Problème
Aujourd'hui, un ballon possédé est tenu par la **Physics Handle** du personnage, comme tous les objets qu'on attrape (`BPC_GrabObject` → `GrabPhysicComponent` → `UWB_HeldObjectMotionComponent`). Résultat : le ballon **flotte**.
- **Hauteur imposée** : la handle tire le ballon vers un point à `BallCarryHeight` = 10 cm au-dessus des pieds. Le pivot de `SM_Ball_Soccer_01` est à sa base, donc le ballon est décollé du sol et la gravité ne joue plus.
- **Rotation bloquée** : `GrabComponentAtLocationWithRotation` + `SetTargetLocationAndRotation`. Le ballon ne peut pas rouler : il glisserait comme une rondelle.
- **Balancement d'un objet tenu** : sway de position et sway de rotation (9° / 5° / 7° autour du pivot à la base, soit environ 5 cm de déplacement du centre).

## But
Donner l'impression que le ballon **roule au sol** et qu'il est **contrôlé au pied** par le joueur qui le possède.

## Règles validées par l'utilisateur (2026-11-19)
1. **Plus de Physics Handle pour les ballons.** Le ballon reste en simulation physique complète : gravité, sol, rebonds, collisions. Un composant de dribble le pousse **à plat seulement** (X et Y) vers un point devant les pieds, et il roule.
2. **Contrôle collé.** Le ballon roule juste devant les pieds, avec un contrôle serré. Les **touches de balle** (le ballon part devant, le joueur le reprend en rythme) sont codées, mais **désactivées par défaut** (option booléenne), pour calibrer plus tard.
3. **Perte par distance.** Si le ballon s'éloigne trop du point de dribble (bloqué par un meuble, poussé par un adversaire, rebond), la possession se perd toute seule. Ça s'ajoute aux pertes actuelles : kick, hit / Dash (`ForceDrop`), drop.
4. **Profil par ballon.** Tous les ballons dribblent au sol (soccer, basket, bowling, plage, bombe, boule de neige, canne). Chaque `PDA_PhysicsProfile` de ballon peut pointer vers son propre profil de dribble. Sinon, le profil par défaut du composant s'applique.
5. **Point de dribble juste devant les pieds** : rayon de la capsule + rayon du ballon + un petit écart réglable. On ne garde pas les 125 cm de `CarryDistance`.
6. **Les autres objets ne changent pas** : Physics Handle + `UWB_HeldObjectMotionComponent`, comme aujourd'hui.

## Découpage

### Étape 1 · Composant de dribble et état de possession — **C++**
*Raison* : calcul exécuté à chaque frame, logique partagée par tous les joueurs et les bots, API physique plus complète en C++. *Coût* : nouveaux `.h` → **une seule compilation, éditeur fermé**. Les touches de balle sont fusionnées dans cette étape pour éviter une seconde compilation.

**`UWB_BallDribbleProfile`** (`Public/Ball/Dribble/`, `UPrimaryDataAsset`) : tous les réglages, chacun avec une Description en anglais. Valeurs par défaut à calibrer.
- **Point de dribble** :
  - `FrontGap` (cm) : écart entre la capsule et le ballon ;
  - `LeadTime` (s) : avance du point selon la vitesse du joueur, pour que le ballon reste devant en course.
- **Suivi** (à plat seulement) :
  - `FollowStiffness` (1/s) : vitesse à laquelle l'écart de position se referme ;
  - `FollowResponse` (1/s) : vitesse à laquelle le ballon prend la vitesse visée ;
  - `MaxAcceleration` (cm/s²) : plafond de la poussée, pour que le ballon ne traverse pas les meubles et qu'un obstacle puisse le bloquer.
- **Roulement** :
  - `RollFollow` (0..1 par frame, ou vitesse par seconde) : à quel point la rotation suit le roulement idéal ;
  - `GroundTolerance` (cm) : distance au sol en dessous de laquelle on impose le roulement (en l'air, la rotation reste libre).
- **Contournement de la capsule** : `CapsuleClearance` (cm). Quand le joueur tourne brusquement, le ballon passe autour du joueur, pas à travers.
- **Rappel élastique** (test de l'utilisateur, ajouté le 2026-11-19) :
  - `bUseTether` = **faux par défaut** : éteint, aucun rappel, aucun calcul ; `TetherStiffness` = 0 coupe aussi le rappel ;
  - `TetherDistance` (cm) : au-delà de cette distance à plat du point de dribble, une force de rappel s'ajoute au suivi ;
  - `TetherStiffness` (1/s²) : force du rappel, proportionnelle à l'écart au-delà de `TetherDistance` (plus le ballon est loin, plus il revient fort) ;
  - `TetherMaxAcceleration` (cm/s²) : plafond du rappel, pour qu'un adversaire puisse encore arracher le ballon.
- **Perte** (la « corde » casse) :
  - `LoseDistance` (cm, plus grande que `TetherDistance`) : distance à plat au point de dribble au-delà de laquelle la possession se perd ;
  - `LoseDelay` (s) : temps passé au-delà avant la perte, pour tolérer un choc bref ;
  - `LoseHeight` (cm) : hauteur du ballon au-dessus des pieds au-delà de laquelle la possession se perd tout de suite.
- **Touches de balle** (option) :
  - `bUseTouches` = **faux par défaut** ;
  - `TouchLeadDistance` (cm) : distance jusqu'où le ballon part devant ;
  - `TouchSpeedFactor` : vitesse de la touche × vitesse du joueur ;
  - `TouchMinInterval` (s) : temps minimum entre deux touches.

**`UWB_BallDribbleComponent`** (`Public/Ball/Dribble/`, `UActorComponent`, `Blueprintable`, `BlueprintSpawnableComponent`), sur le personnage :
- `DefaultProfile` : profil utilisé quand le `PDA_PhysicsProfile` du ballon n'en donne pas. Il faut au moins l'un des deux. Sinon : avertissement dans le log, dribble refusé (`StartDribble` retourne faux), aucun repli silencieux.
- `StartDribble(Ball, BallProfile)` (`BlueprintCallable`, retourne un booléen) : trouve le corps simulé du ballon (`FindBallBody`), enregistre le joueur comme possesseur dans le subsystem et active le Tick.
- `StopDribble()` (`BlueprintCallable`) : libère le ballon, sans impulsion. Le kick et le drop ajoutent leur impulsion après.
- `IsDribbling()` et `GetDribbledBall()` (`BlueprintPure`).
- Dispatcher `OnDribbleLost(Ball)` : la perte par distance ou par hauteur appelle `StopDribble`, puis diffuse.
- **Tick en `TG_PrePhysics`**, actif seulement pendant le dribble :
  1. Centre du ballon = centre des bounds ou centre de masse, **pas** la position du component (pivot à la base).
  2. Point visé P = pieds + avant de la capsule × (rayon capsule + rayon ballon + `FrontGap`) + vitesse à plat du joueur × `LeadTime`.
  3. Si la ligne ballon → P traverse la capsule (rayon + `CapsuleClearance`), on décale la cible sur le côté pour contourner.
  4. Vitesse visée (à plat) = vitesse du joueur + (P − centre) × `FollowStiffness`. Accélération = (vitesse visée − vitesse du ballon) × `FollowResponse`, plafonnée à `MaxAcceleration`, appliquée avec `AddForce` (accélération, en X et Y seulement). **Rien en Z** : la gravité et le sol gèrent la hauteur.
  4b. Rappel élastique : si la distance à plat dépasse `TetherDistance`, on ajoute (écart − `TetherDistance`) × `TetherStiffness` vers P, plafonné à `TetherMaxAcceleration`.
  5. Roulement : si le ballon est au sol (selon `GroundTolerance`), la vitesse angulaire tend vers (Haut × vitesse à plat) ÷ rayon, selon `RollFollow`.
  6. Perte : distance à plat > `LoseDistance` pendant `LoseDelay`, ou hauteur > `LoseHeight` → `OnDribbleLost`.
  7. `bUseTouches` : au lieu du suivi continu, une impulsion vers l'avant quand le joueur rattrape le ballon (ballon trop près du point ou derrière), au plus une fois par `TouchMinInterval`. Entre deux touches, un léger amortissement seulement.
- **Collision ballon ↔ capsule du porteur** : la capsule ne doit pas bloquer ni pousser le ballon qu'elle dribble. Les adversaires, eux, doivent continuer à le bousculer. La méthode est choisie par Claude Code (paire de collisions ignorée côté Chaos si c'est possible, sinon le contournement du point 3 et le rappel élastique du point 4b suffisent : le ballon bousculé revient au pied). À valider au test 4.
- Debug : `bDebugDrawDribble` (point visé, vitesse visée, rayon de perte). Désactivé par défaut.

**`UWB_BallPossessionSubsystem`** :
- `SetBallDribbler(Ball, Player)`, `ClearBallDribbler(Ball)` et `GetBallPossessor(Ball)`.
- `IsBallHeld` = vrai si un joueur dribble le ballon **ou** si une Physics Handle le tient (le test actuel reste en plus). La traînée, le catch en marchant et l'interception en défense fonctionnent donc sans autre changement.

### Étape 2 · Données — **Data Asset** (+ une variable Blueprint)
*Raison* : réglage par l'équipe de design. *Coût* : aucune compilation. Je le fais via MCP.
- `DA_BallDribble_Default` (dans `Content/Blueprint/Ball/Data/`).
- `PDA_PhysicsProfile` (Blueprint) : nouvelle variable `DribbleProfile` (référence `WB_BallDribbleProfile`), Category `Ball|Dribble`, avec Description. Vide = profil par défaut du composant.
- Les DA par ballon (`DA_BallDribble_Bowling`, `_Beach`…) se créent au calibrage, au besoin. Le système les supporte dès l'étape 1.

### Étape 3 · Branchements — **Blueprint** (via MCP)
*Raison* : assemblage entre systèmes. *Coût* : aucune compilation.
- `BP_SoccerPlayer` (ou `BP_GenericCharacter`, selon l'endroit où vit `BPC_GrabObject`) : ajouter `UWB_BallDribbleComponent` avec `DefaultProfile` = `DA_BallDribble_Default`.
- `BPC_GrabObject` :
  - `PickUpSoccerBall` : `StartDribble` remplace `GrabPhysicComponent` et `StartHeldObjectMotion`. `ChangeHolding(Player.Holding.Ball)`, `SetPossessor` et `UpdateHeldObjectHolder` restent. Si `StartDribble` échoue, pas de possession.
  - `ThrowPhysicActor` (kick) : si le joueur dribble, `StopDribble`, puis on prend le corps du ballon comme `PhysicComponent` au lieu du composant tenu par la Physics Handle. Le reste ne change pas (`ApplyKickProfile`, impulsion, `MarkBallKicked`, VFX).
  - `ForceDrop` et `DropHeldItem` : même branchement (`StopDribble` au lieu de `ReleaseComponent` pour un ballon).
  - `OnDribbleLost` → `UpdateMasterBall` + `ClearPhysicActorData`, comme un drop sans impulsion.
- `UWB_HeldObjectMotionComponent` n'est plus appelé pour les ballons. `BallCarryHeight` et le paramètre `bIsBall` restent pour l'instant ; on les retire dans un nettoyage plus tard, après validation (lecture des valeurs avant tout retrait de `UPROPERTY`).

## Tests (par l'utilisateur)
1. **Au sol** : le ballon possédé touche le sol, ne flotte plus et ne balance plus.
2. **Roulement** : en marche et en course, le ballon roule dans le sens du déplacement, à la bonne vitesse (pas de glisse). À l'arrêt, il s'arrête devant les pieds et ne tourne plus.
3. **Virages** : demi-tour brusque, le ballon passe autour du joueur, pas à travers ; il reste contrôlé.
4. **Porteur** : la capsule du porteur ne pousse pas et ne bloque pas son propre ballon.
5. **Obstacles** : le ballon bloqué par un meuble ou un mur → perte de possession après `LoseDelay`. Le ballon ne traverse rien.
6. **Adversaire** : un adversaire qui marche dans le ballon le bouscule. Hit / Dash → perte comme aujourd'hui.
7. **Kick** : la force, la direction, le loft, la traînée, le VFX et le délai de re-catch du kickeur sont inchangés.
8. **Défense** : l'interception fonctionne ; on ne peut pas intercepter un ballon dribblé par un autre joueur.
9. **Marches, tapis, rampes** : le ballon suit le sol sans décoller.
10. **Autres ballons** : tous dribblent ; bowling et plage ont un comportement acceptable avec le profil par défaut.
11. **Bots** : ils ramassent, dribblent et kickent normalement.
12. **Autres objets** : pots, chaises, meubles traînés sont inchangés.

## Points ouverts
- **Animation `AS_BallCarry_Loop`** : si elle montre les mains qui tiennent le ballon, elle ne colle plus. Il faudra peut-être une animation de course au pied. À voir au test 2 (hors devis).
- **Charge du kick** : pendant la charge (`ShootChargeStarted`), le ballon reste au pied, comme pendant le dribble. Faut-il ralentir le joueur ou rapprocher le ballon ? À voir au test 7.
- **Rotation au kick** : la rotation du dribble reste au moment du kick, ou elle est remise à zéro ? À voir au test 7.
- **Touches de balle** : synchronisées avec les pas (`WB_AnimNotify_FootstepPuff`) ? À décider quand on activera l'option.
