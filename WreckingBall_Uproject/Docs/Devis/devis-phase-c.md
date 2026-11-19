# Wrecking Ball — Devis de la phase C (joueur)

Créé le 2026-11-15. **Révisé le 2026-11-17 (17 h 40)** après l'état des lieux du projet : étapes 9 et 10 implémentées (en C++), ajouts hors devis intégrés (défense, plongeon, énergie), nouveau lot P placé en tête de ce qui reste. **Révisé le 2026-11-18** : lots V, P et D faits ; lot 11.1 refait en « possession automatique et ballon kické » (le tap / maintien est abandonné) ; lot 11.2 fait (malus fixe) ; lot 12.1 fait (surfaces de plancher, `Lvl_Floor_Gym`) ; lot 12.2 (Dash en root motion) suspendu ; question 2 réglée ; pas chassés de la Défense faits. **Phase C terminée le 2026-11-18** (reste : 12.2 suspendu ; manipulation du sofa et passes entre coéquipiers renvoyées à de prochains devis de l'utilisateur). Couvre les étapes 9 à 12 du devis maître.

Exécution par Claude++, **un lot à la fois** ; commit Git avant et après chaque lot (par l'utilisateur) ; test de l'utilisateur à la fin de chaque lot. `claude/guide-ergonomie-blueprint.md` appliqué partout, rapport de la section 8 à la fin de chaque lot. Règles du `CLAUDE.md` (dossier du projet), dont **« Aucune valeur de réglage cachée dans le C++ »** (2026-11-17).

Valeurs marquées *(à calibrer)* : provisoires, réglées par l'utilisateur au test. Aucun nouvel enum Blueprint : Gameplay Tags.

---

## Ordre de travail

| # | Lot | État |
| --- | --- | --- |
| 1 | **V — Validation des étapes 9 et 10** (+ défense, plongeon, énergie) | fait (test de l'utilisateur, 2026-11-17) |
| 2 | **P — Profils et énergie** (prérequis de l'étape 11) | fait (2026-11-17) |
| 3 | **D — Finition du mode Défense** (mouvement relatif à la caméra) | fait (2026-11-17) ; pas chassés faits le 2026-11-18 (`BS_Defense_Move`, 4 anims placeholder) |
| 4 | Étape 11 — Ballon hybride, GrabMode, nouveau feel | fait : 11.1 à 11.4 (2026-11-18 ; sofa en `DA_Physics_Thrown_6_UltraHeavy`) |
| 5 | Étape 12 — Surfaces de plancher, Dash en Root Motion | 12.1 fait (2026-11-18, testé) ; **12.2 suspendu** (2026-11-18) |

---

## Ce qui est en place (état relevé le 2026-11-17)

### Architecture
L'état du joueur est porté par quatre composants C++ du module `WreckingBall`, branchés sur `BP_GenericCharacter` :
- `WB_PlayerStateComponent` : `CurrentState`, `CurrentHolding`, `ChangePlayerState`, `ChangeHolding`, dispatchers `OnPlayerStateChanged`, `OnHoldingChanged`, `OnActionRefused`, `ActionCharge` (ralenti du kick / lancer), texte de debug. Profils `PlayerStateProfile` (`DA_PlayerState_Default`) et `ActionCostProfile` (`DA_PlayerActionCost_Default`).
- `WB_PlayerCommandComponent` : relais des commandes (`OnMove / Dash / Grab / KickPress / KickRelease`).
- `WB_PlayerHitComponent` : receveur d'impact, Stunned / Down, recul, grâce (`DA_PlayerHit_Default`).
- `WB_PlayerDiveComponent` : plongeon et défense, prédiction du ballon, blocage (`DA_PlayerDive_Default`).
Côté Blueprint, `BP_GenericCharacter` garde l'assemblage : `RequestMove / Dash / Grab / KickOrThrow / Look / Dive`, `ReleaseKickOrThrow`, `ReleaseDive`, `ApplyImpact`, `TryDashHitPlayer`, `EndDashPassThrough`, `TryDashRecoil`, sprint et énergie. `BP_SoccerGamemode` : `SetAllPlayersState`, `StartGoalCelebration`.

### Étape 9 — État du joueur : implémentée
Lots 9.1 à 9.3 en place (tags, porte d'entrée unique, commandes publiques, Disabled au 3-2-1 et en fin de match, célébration / bouderie par équipe). Variables `Legacy` retirées. **À vérifier au lot V** : lot 9.4 (HUD et AnimBP abonnés aux dispatchers, sans lecture au tick).

### Étape 10 — Étourdissement, chute, rebonds : implémentée
Objet lancé = Stunned, ballon jamais ; Dash avec élan (`DashMinRunUpDistancePlayer`) = Down ; traversée du joueur touché ; mur détruit = passe au travers ; mur qui tient = rebond par réflexion (état `Recoiling`).

### Ajouts hors devis initial (2026-11-16 et 17)
- **Mode Défense** (`Player.State.Defending`) : bouton de défense à l'arrêt ; le joueur bloquait un ballon kické par l'adversaire (`OnDefenseBallBlocked`) ; depuis le lot 11.1, il l'attrape (`OnDefenseBallCaught`). Animations `AS_Defense_Idle / Pose` ; depuis le 2026-11-18, le déplacement passe par `BS_Defense_Move` (pas chassés, voir lot D) et `AS_Defense_Walk` ne sert plus. Règles du mouvement : `claude/devis-mode-defense.md`.
- **Plongeon** (`Player.State.Diving`) : bouton de défense + direction ; le joueur s'élance pour intercepter le ballon (trajectoire prédite, cible choisie). Coûte de l'énergie : départ 30 %, +30 % si le plongeon rate *(à calibrer)*. Depuis le 2026-11-17, le corps se couche sur le côté, le regard vers l'horizon (`bLieOnSide`, `DA_PlayerDive_Default`).
- **Barre d'énergie refaite** : commune au sprint, au Dash (40 %) et au plongeon ; `WB_EnergyBarWidget_Base` (couleurs pleine / vide / épuisée, clignotement, flash de recharge).
- **Bouderie** devenue un état (`Player.State.Sulking`) au lieu du seul paramètre `IsSulking`.

### Ajouts hors devis (2026-11-18)
- **Menu de debug des ballons** : touche 0 (hors build packagé) dans `BP_TP_SoccerPlayerController` → `BP_SpawnManager.DebugCycleBall` : liste circulaire des 8 `DA_Ball_*` (`DebugBallTypes`), le ballon réapparaît au centre.
- **VFX du kick** : `NS_KickBlast` (éclat de bande dessinée piloté par shader, `Art/VFX/KickBlast/`), lancé par `ThrowPhysicActor`.
- **Courbe de la célébration** : `CelebrationSpeedCurve` (`DA_PlayerState_Default`, X = avancée 0..1, Y = multiplicateur de la vitesse de marche) remplace `CelebrationStartSpeedMultiplier`, `CelebrationEndSpeedMultiplier` et `CelebrationRiseExponent` (même feel par défaut).
- **Vérification des valeurs cachées** : les valeurs du Dash sont dans `DA_PlayerDash_Default` (lot P) ; les paramètres de `WB_GrabLibrary` viennent des variables `Throw*` de `BPC_GrabObject` ; `GetHandsMidpoint` et `GetThrowArcTarget` ne sont plus appelées (à retirer lors d'un ménage).

---

## Décisions communes (validées le 2026-11-15, mises à jour le 2026-11-17)

1. **Deux familles de tags, exclusives chacune.**
   - `Player.State.*` : `Free`, `Dashing`, `Kicking`, `Throwing`, `Recoiling`, `Defending`, `Diving`, `Stunned`, `Down`, `Disabled`, `Celebrating`, `Sulking`.
   - `Player.Holding.*` : `None`, `Ball`, `Object`.
2. **Une seule porte d'entrée** : `ChangePlayerState` / `ChangeHolding` du `WB_PlayerStateComponent`. Personne d'autre n'écrit l'état.
3. **Commandes publiques** filtrées par l'état ; le PlayerController (et un futur AIController) n'appelle que ces commandes.
4. **Pas de Dash en tenant quelque chose**, refus silencieux (aucun son, aucun FX).
5. **Kicking / Throwing** : moment du lâcher donné par l'animation (notify `Release`, sinon durée de secours du profil) ; pas d'autre action pendant ; déplacement ralenti jusqu'à l'arrêt selon la charge.
6. **Disabled** : 3-2-1 (objet gardé, ni lancé ni kické) et fin du match (`ForceDrop` de tout, joueur figé). Posé et retiré par `BP_SoccerGamemode` seulement.
7. **Réaction au but selon le but où entre le ballon** : l'équipe qui marque célèbre, l'équipe qui encaisse boude (`Sulking`, pleine capacité de mouvement). But contre son camp : pas de cas à part.
8. **Celebrating** (~2 s *(à calibrer)*) : course auto dans la dernière direction, le stick ne fait que diriger, aucune action.
9. **Sulking** : même durée que la célébration ; boucle marche / course blendée avec une emote de bouderie (haut du corps).
10. **Priorité des états** : `Disabled` > `Down` > `Stunned` > `Celebrating` = `Sulking` > actions (`Dashing`, `Kicking`, `Throwing`, `Recoiling`, `Defending`, `Diving`) > `Free`. Les exceptions vivent dans la table de transitions.
11. **Dash sur un joueur** : le dasheur garde sa pleine vitesse ; élan minimum `DashMinRunUpDistancePlayer`, séparé de `DashMinRunUpDistance` (destructibles).
12. **Dash sur un mur** : détruit = traverse ; tient = rebond par réflexion.
13. **Le ballon n'étourdit jamais.** Un ballon kické n'est captable qu'en Défense, jusqu'à ce qu'il ralentisse sous son seuil (lot 11.1) ; le plongeon ne le prend jamais, il rebondit.
15. **Possession automatique** (2026-11-18) : le ballon se prend en marchant dessus, sans bouton ; le bouton de prise (`IA_Grab`) ne sert qu'aux objets physiques (lot 11.1).
14. **Aucune valeur de réglage cachée dans le C++** (2026-11-17) : valeurs dans des Data Assets, pas de repli silencieux, un système = un seul endroit (voir `CLAUDE.md`).

### Table de transitions (référence : `DA_PlayerState_Default.AllowedTransitions`)

| De \ Vers | Free | Dashing | Kick / Throw | Recoiling | Defending | Diving | Stunned | Down | Disabled | Celebrating | Sulking |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Free | — | oui | oui | non | oui | oui | oui | oui | oui | oui | oui |
| Dashing | oui | — | non | oui | non | non | oui | oui | oui | oui | oui |
| Kick / Throw | oui | non | — | non | non | non | oui | oui | oui | oui | oui |
| Recoiling | oui | non | non | — | non | non | oui | oui | oui | oui | oui |
| Defending | oui | non | non | non | — | non | oui | oui | oui | oui | oui |
| Diving | oui | non | non | non | non | — | oui | oui | oui | oui | oui |
| Stunned | oui | non | non | non | non | non | — | oui | oui | oui | oui |
| Down | oui | non | non | non | non | non | non | — | oui | oui | oui |
| Disabled | oui | non | non | non | non | non | non | non | — | oui | oui |
| Celebrating | oui | non | non | non | non | non | non | non | oui | — | non |
| Sulking | oui | non | non | non | non | non | non | non | oui | non | — |

Conditions en plus de la table : Dashing seulement si `Holding = None` ; Kick / Throw seulement si `Holding ≠ None`.
**Point à trancher au lot V** : la table permet `Dashing → Celebrating / Sulking` et `Stunned / Down → Celebrating` (un but marqué pendant l'action). À confirmer comme voulu ; et `Defending → Diving` est refusé (le plongeon part-il seulement de `Free` ?).

---

## Lot V — Validation des étapes 9 et 10 (test de l'utilisateur)

Rien à coder, sauf correctifs trouvés au test. Claude++ commence par une lecture : lot 9.4 (HUD et AnimBP par dispatchers), et relevé des points de la table ci-dessus.

Protocole (2 à 4 joueurs) :
- Feel général inchangé ; Dash sans aucun effet en tenant ballon ou objet ; kick et lancer ralentissent jusqu'à l'arrêt.
- 3-2-1 : aucun bouton n'a d'effet (Défense et plongeon compris), ballon tenu gardé ; chrono à 0 : tout le monde lâche tout et reste figé.
- But : l'équipe qui marque court ~2 s en se dirigeant, l'autre boude en bougeant normalement ; but contre son camp : même chose.
- Objet lancé fort = étourdi sans chute ; faible = rien ; ballon = jamais étourdi.
- Dash avec élan sur un joueur = au sol, il lâche le ballon, le dasheur continue ; collé (< 35 cm) = rien ; sur un étourdi = au sol ; objet sur un joueur au sol = rien ; pas de chute en boucle (grâce).
- Dash qui casse un mur = traverse ; contre un mur qui tient : de face = rebond arrière, à 45° = rebond à 45°.
- **Nouveaux états** : se faire dasher pendant une Défense et pendant un plongeon ; plongeon sans énergie (refusé ?) ; plongeon qui touche (30 %) et qui rate (60 %) ; Défense qui bloque un kick adverse ; énergie : sprint, épuisement, recharge, Dash refusé épuisé.

---

## Lot P — Profils et énergie (prérequis de l'étape 11)

But : toute l'énergie du joueur dans **un seul Data Asset**, l'affichage de la barre dans **un Data Asset visuel**, et plus aucune valeur de gameplay cachée dans le C++. Rapport : valeurs avant / après pour chaque champ déplacé (le feel ne doit pas changer). Relever les valeurs sur **chaque CDO enfant** (`BP_SoccerPlayer`) avant de bouger quoi que ce soit.

### Recensement du C++ (2026-11-17)

**A. Replis silencieux** (`GetDefault<…>()` quand le DA est vide) → remplacer par avertissement dans le log + action coupée ; vérifier que chaque DA est assigné.
| Fichier | Profil |
| --- | --- |
| `Player/State/WB_PlayerStateComponent.cpp` l. 75, 80 | `WB_PlayerStateProfile`, `WB_PlayerActionCostProfile` |
| `Player/Command/WB_PlayerCommandComponent.cpp` l. 159 | `WB_PlayerActionCostProfile` (coût du Dash) |
| `Player/Hit/WB_PlayerHitComponent.cpp` l. 58 | `WB_PlayerHitProfile` |
| `Player/Dive/WB_PlayerDiveComponent.cpp` l. 61 | `WB_PlayerDiveProfile` |
| `AI/WB_SoccerAIController_Base.cpp` l. 47 | `WB_SoccerAIProfile` |

**B. Lecture de variables Blueprint par leur nom, avec secours en dur** : `WB_PlayerCommandComponent.cpp` l. 164-177 lit `DashScanDistance`, `Dash Speed`, `DashMinRunUpDistancePlayer` (secours 350 cm, 0,2 s, 35 cm), ainsi que `Sprint Lifetime Percent` et `WalkType` (`E_WalkSpeed`). → Valeurs dans un profil (énergie ou `WB_PlayerDashProfile`, au choix dans l'analyse) ; l'énergie courante et le mode de marche deviennent des valeurs du C++ exposées en lecture.

**C. Littéraux de gameplay ou de feel dans les `.cpp`** → un champ de profil chacun, avec Description.
| Fichier | Valeur | Rôle |
| --- | --- | --- |
| `UI/WB_EnergyBarWidget_Base.cpp` l. 14 | 0,4 | luminosité de la partie vide en épuisement |
| `Player/Dive/WB_PlayerDiveComponent.cpp` l. 25 | 24 | points de prédiction du ballon |
| idem l. 199, 274 | 0,45 ; clamp 0,1-0,9 | moment du sommet du saut |
| idem l. 205, 407, 482 | 0,2 s | durée minimale en l'air |
| idem l. 223 | 0,05 | `MinInterceptFraction` minimal |
| idem l. 257 | × 10 | poids du temps dans le choix de la cible |
| idem l. 287 | +100 / −200 cm | trace de sol à l'atterrissage |
| idem l. 410 | 0,2 s | décollage sans animation |
| idem l. 530 | 0,1 | `RecoveryPlayRate` minimal |
| `Player/Hit/WB_PlayerHitComponent.cpp` l. 189 | 0,05 | tolérance « de face » du recul |
| idem l. 269 | 0,1 | angle minimal de rebond contre un mur |
| idem l. 293 | 0,05 s | `RecoilDuration` minimal |
| `Player/State/WB_PlayerStateComponent.cpp` l. 260 | puissance 3 | courbe de vitesse de la célébration (→ `UCurveFloat`) |
| `AI/WB_SoccerAIController_Base.cpp` l. 409, 497, 397 | 0,7 ; 0,8 ; 4 pas | pente « sol », cône de tir réduit, balayage de la ligne de Dash |
| `AI/WB_SoccerBotManagerComponent.cpp` l. 264 | (150, 150, 300) | projection sur la navigation au spawn |
| `Impact/Destructible/WB_WobbleComponent.cpp` l. 53 | 0,1 | test du sens de l'oscillation |
| `Impact/Budget/WB_PhysicsBudgetManager_Base.cpp` l. 26-27 | 1/30 s, 0,05 s | cadence du rétrécissement des débris |
Clamps 0..1, gardes de division (0,01) et constantes mathématiques : restent dans le code.

**D. Paramètres par défaut de `WB_GrabLibrary.h`** (`ForearmLength` 17, `BlendInCharge` 0,2, `ArcHeight` 25, `OverheadHeight` 15, `OverheadForward` 10, `ClearanceMargin` 5) → branchés depuis un profil.

**E. Déjà conformes** : couleurs et durées de `WB_EnergyBarWidget_Base` (à déplacer dans le DA visuel) ; `WB_SoccerBotManagerComponent` (`MaxPlayers`, `MinPlayers`, `PlayerStartClearance`, `SpawnBesideDistance`) ; `WB_AnimNotify_FootstepPuff.MinSpeed` ; `WB_WobbleTypes`.

**Côté Blueprint (`BP_GenericCharacter`), à unifier** : `Sprint Lifetime Default` (5 s), `SprintSpeed Multiplier` (2), `ExaustionDuration` (3 s), `Walk Speed Exhausted` (300), `EnergyRecoveryDelay` (1,25 s), `DashCooldown` (0,55 s), timeline `SprintingLifeSpan` (vitesse de recharge).

### Sous-lots
- **P.1 — Profil d'énergie (C++, `.h` modifiés : éditeur fermé, une seule compilation)** : `WB_PlayerActionCostProfile` agrandi ou remplacé par `WB_PlayerEnergyProfile` (choix dans l'analyse) : coût du sprint par seconde, vitesse de recharge, délai avant recharge, durée et seuil de sortie de l'épuisement, vitesse épuisé, multiplicateur de sprint, cooldown du Dash, coûts Dash / plongeon. `DA_PlayerEnergy_Default` créé avec les valeurs actuelles.
- **P.2 — Branchement Blueprint** : `BP_GenericCharacter` lit le profil ; variables remplacées retirées (Find References d'abord) ; timeline de recharge retirée si elle ne sert plus.
- **P.3 — Profil visuel** : `DA_EnergyBarVisual` (couleurs, clignotement, flash, luminosité épuisée, matériau) lu par `WB_EnergyBarWidget_Base`, partagé par `WBP_Sprintbar` et `WBP_PlayerBar`.
- **P.4 — Ménage** des sections A à D.

**Test** : feel identique (sprint, épuisement, recharge, Dash, plongeon touché / raté) ; changer une valeur du DA en PIE change le jeu ; retirer un DA → avertissement dans le log, action coupée, pas de crash ; barre aux mêmes couleurs.

---

## Lot D — Finition du mode Défense

Référence : `claude/devis-mode-defense.md` (règles validées le 2026-11-17). Rappel : entrée à l'arrêt seulement ; pivot unique vers le ballon s'il est à ≤ 12 m, puis orientation figée ; WASD / stick **relatifs au yaw de la caméra**, en ligne droite, quelle que soit l'orientation du corps ; l'AnimBP reçoit l'angle vitesse / orientation pour les pas chassés.
**Pas chassés (2026-11-18)** : `BS_Defense_Move` (Blend Space 2D : `DefenseMoveAngle` −180..180 en boucle × `Speed` 0..300). À vitesse 0, `AS_Defense_Idle` (mains en X) ; à 300, `AS_Defense_Step_Forward / _Back / _Left / _Right` (pas de crabe en boucle, petits pas avant / recul, coudes repliés, mains devant la poitrine). Remplace `AS_Defense_Walk` dans `ABP_SoccerPlayer`.
Retour de l'utilisateur (2026-11-17) : l'axage actuel suit encore en partie la position et la rotation du personnage, pas assez la caméra ; « pas parfait, à arranger au fur et à mesure ». Claude++ relève le calcul actuel dans le rapport avant de corriger. Réglages (`DefenseBallTrackRadius`, `DefenseTurnSpeed`) dans `DA_PlayerDive_Default` (règle du lot P).
**Question ouverte** : quel bouton active la Défense, et lequel le plongeon (« Y » et « Y + direction », ou X) ?

---

## Étape 11 — Ballon hybride, GrabMode, nouveau feel

### Lot 11.1 — Possession automatique et ballon kické (fait le 2026-11-18, à tester)
Révisé le 2026-11-17 : remplace « tap = kick au sol, maintien = prise ». Le tap / maintien, puis un mode « freestyle » (ballon jamais tenu, dribble des bots) ont été essayés et retirés le même jour.

**Règles (utilisateur, 2026-11-17)**
- **Possession automatique** : le ballon s'attache au joueur dès qu'il entre dans la zone de capture devant lui, sans bouton. Conditions : état `Free`, mains vides, ballon ni tenu ni kické. La capture est vérifiée en continu tant que le ballon est dans la zone (un ballon qui ralentit dans la zone est capté).
- **Bouton de prise** (`IA_Grab`, ex-`IA_PickupSoccerBall`, à la pression) : objets physiques seulement, jamais le ballon. `IA_PickupObject` (touche C, inutilisé) supprimé.
- **Ballon kické** : un kick met le ballon en mode kické ; personne ne le capte en marchant. Le mode prend fin quand la vitesse du ballon passe sous `KickedReleaseSpeed` (par ballon).
- **Délai du tireur** : le tireur ne recapte pas son propre ballon pendant `KickerRecatchDelay` *(0,5 s)*, même après un kick trop faible pour le mode kické.
- **Défense = interception** : un joueur en `Defending` attrape le ballon (kické ou non, non tenu) qui touche son corps ; il sort de la Défense et passe en `Free` avec le ballon. Remplace le bloc (rebond).
- **Plongeon** : ne capte jamais le ballon, qui rebondit sur le joueur.
- **Aucune capture** en `Dashing`, `Stunned`, `Down`, `Disabled`, `Kicking`, `Throwing`, `Recoiling`, `Diving`, ni avec un objet en main. Un dasheur capte le ballon lâché par le porteur tombé à la fin de son Dash, s'il est encore à portée.
- **Ballon lâché** par un porteur tombé : pas kické, libre tout de suite.

**Réalisation**
- C++ : `UWB_BallPossessionSubsystem` (`Ball/`) : mode kické et tireur par ballon, fin du mode par la vitesse, `IsBallHeld` (Physics Handle d'un joueur), `MarkBallKicked`, `ClearBallKick`, `IsBallKicked`.
- C++ : `WB_PlayerStateComponent` teste la zone de capture à chaque frame (seulement en `Free`, mains vides) et envoie `OnBallCatchRequested(Ball)`.
- C++ : `WB_PlayerDiveComponent` : en Défense, le contact attrape (`OnDefenseBallCaught(Ball)`, ex-`OnDefenseBallBlocked`).
- Blueprint : `BP_GenericCharacter.CatchBall(Ball)` (vérifie le Possessor, puis `BPC_GrabObject.PickUpSoccerBall`) reçoit les deux dispatchers. `ThrowPhysicActor` appelle `MarkBallKicked`. `GrabRequested` va directement à `PickUpPhysicObject`.
- Bots : `SeekBall` marche droit dans le ballon (`GrabRange` = distance où le bot quitte son chemin), la capture est automatique.
- Réglages : `DA_PlayerState_Default` › `PlayerState|Possession` (`BallClass`, `BallCatchRadius` *(60 cm)*, `BallCatchForwardOffset` *(40 cm)*, `KickerRecatchDelay` *(0,5 s)*) ; `PDA_PhysicsProfile` › `Kick` › `KickedReleaseSpeed` *(400 cm/s, les 8 ballons)*.
- Reste : les réglages `DefenseBlock*` de `DA_PlayerDive_Default` gardent leur nom mais définissent la zone d'attrapé.

### Lot 11.2 — Pénalités de possession (fait le 2026-11-17, à tester)
- But : limiter le porteur pour que les autres puissent le rattraper. Possession illimitée : la pénalité ne fait jamais lâcher le ballon.
- Fait : malus fixe `HoldingBallMoveSpeedMultiplier` *(0,9 = −10 %)* dans `DA_PlayerState_Default` › `PlayerState|Movement` (avec les autres vitesses, décision de l'utilisateur), ballon seulement, combiné avec le ralenti de la charge.
- Plus tard, si besoin : courbe temps tenu → multiplicateur (plancher *(0,6)*), remise à zéro au lâcher.

### Lot 11.3 — Kick en Vel Change faux et nouveau feel des ballons (fait le 2026-11-18)
- `ThrowPhysicActor` : `AddImpulse` avec Vel Change = faux (la masse compte), via `ApplyKickProfile` (plafond `MaxKickSpeed` × masse ; sans profil, `PhysicActorThrowingStrength` 20000).
- `PDA_PhysicsProfile` : `KickImpulse` remplace `KickImpulseMultiplier` (20000 × l'ancien multiplicateur, même vitesse au départ : Soccer 30000, Thrown_2 22000, Thrown_3 26000, Thrown_4 32000, Thrown_5 40000, Thrown_6 50000).
- `KickedReleaseSpeed` à environ 40 % d'un kick moyen : Beach 1000, SnowBall 570, Soccer 430, Dodgeball 400, CanFood 235, Basketball 220, Bomb 115, Bowling 40 *(à calibrer)*.

### Lot 11.3b — Traînée du ballon (fait le 2026-11-18)
- `NS_BallTrail` (`Art/VFX/BallTrail/`), attaché au ballon par `BP_PO_Master_Ball_01.InitializeBallTrail`, piloté par la variable Niagara `TrailActive`.
- C++ : `UWB_BallPossessionSubsystem::RegisterBallTrail` ; la traînée s'allume quand le ballon dépasse `TrailStartSpeed` ou reçoit un choc d'au moins `TrailImpactSpeed` (variation de vitesse en une frame : kick, Dash, mur…), et s'éteint sous `TrailStopSpeed` ou quand le ballon est tenu.
- Seuils par ballon dans `PDA_PhysicsProfile` (relatifs au kick, plancher de 400 pour le départ) *(à calibrer)*.

### Lot 11.4 — GrabMode Centered / AtPoint (prototype sofa, fait le 2026-11-18)
- `GrabMode` dans `PDA_PhysicsProfile` (tag `Grab.Mode.Centered` / `Grab.Mode.AtPoint`), avec `GrabLinearStiffness` / `GrabLinearDamping`. `Centered` = actuel ; `AtPoint` sur `DA_Physics_Thrown_5_VeryHeavy` et le nouveau `DA_Physics_Thrown_6_UltraHeavy` (250 kg, assigné à `BP_PO_Sofa_A`).
- `AtPoint` : prise au point de collision le plus proche (Physics Handle), rotation libre, ralenti selon la masse (`GrabbedMoveSpeedCurve` de `DA_PlayerState_Default`). La distance de prise se mesure au point de collision le plus proche, pas au pivot (le sofa se prend sous tous les angles).
- Pose du joueur gardée telle quelle pour le prototype. **Une manipulation avec les mains toujours au contact de la surface fera l'objet d'un devis de l'utilisateur** (ne pas y toucher d'ici là).

**Test de fin d'étape 11** : marcher sur le ballon = possession ; le bouton ne prend que les objets ; le tireur ne recapte pas avant 0,5 s ; tir rapide non captable en marchant, captable en ralentissant ; Défense = interception, plongeon = rebond ; le porteur se fait rattraper ; feel des 8 ballons validé un par un ; sofa traîné de façon crédible ; mur de plâtre toujours cassé en 2 kicks pleins.

---

## Étape 12 — Surfaces de plancher et Dash en Root Motion

### Lot 12.1 — DA_SurfaceMovement (fait et testé le 2026-11-18)
- C++ (`Player/Surface/`) : `UWB_SurfaceMovementProfile` (`SurfaceType`, `MaxWalkSpeedMultiplier`, `MaxAccelerationMultiplier`, `BrakingDecelerationMultiplier`, `GroundFrictionMultiplier`, `DashDistanceMultiplier`), `UWB_SurfaceMovementTable` (`Surfaces`, `DefaultProfile`) et `UWB_SurfaceMovementComponent` sur `BP_GenericCharacter` (joueurs et bots).
- Le composant lit le Physical Material sous les pieds (`CurrentFloor`), applique le profil seulement quand la surface change, toujours depuis les valeurs de base (jamais en cumul) ; la vitesse passe par `WB_PlayerStateComponent.SetSurfaceSpeedMultiplier` (combinée avec état et possession). En l'air, la dernière surface reste. Sans table : avertissement, aucun effet.
- Limite : la vitesse passe par l'échelle de l'input, un `MaxWalkSpeedMultiplier` au-dessus de 1 n'a pas d'effet.
- 4 sols : `DA_SurfaceMovement_Wood / Tile / Carpet / Ice` dans `DA_SurfaceMovementTable` (`Blueprint/Character/Data/Surface/`), pas de profil par défaut (autres sols inchangés). Nouvelle surface physique `Tile` (`SurfaceType12`, `DefaultEngine.ini`) et `PM_Tile`. Calibrage laissé à l'équipe de design.
- Map de test `Level/Art/Lvl_Floor_Gym` : éclairage, ballon, buts, limites, 4 zones de sol étiquetées (`MI_FloorTest_*`, dossier `FloorZones`).

### Lot 12.2 — Dash en Apply Root Motion Constant Force (suspendu le 2026-11-18)
**Suspendu (décision de l'utilisateur, 2026-11-18)** : le feel du Dash actuel convient ; la root motion risquerait de le changer sans gain visible pour le prototype. Le Dash actuel glisse déjà plus loin sur les sols à faible freinage (glace, carrelage). Si un sol demande un Dash plus long ou plus court, `DashDistanceMultiplier` (lot 12.1, `UWB_SurfaceMovementComponent::GetDashDistanceMultiplier`) peut être branché sur le Dash actuel, sans root motion. Les valeurs du Dash sont déjà dans `DA_PlayerDash_Default` (lot P).

Plan d'origine, gardé pour une reprise éventuelle :
- Le Dash actuel remplacé par `Apply Root Motion Constant Force` ; distance et vitesse mesurées avant / après ; le scan d'impact (deux élans minimums) suit la nouvelle trajectoire. Distance × surface.
- Traversées et rebond (`Recoiling`) gardent le même comportement. `Dashing` → `Free` au callback de fin de la root motion.
- Le plongeon garde son propre mouvement (hors lot), sauf décision contraire.

**Test de fin d'étape 12** : vitesse et glisse selon le sol ; Dash au même feel sur sol normal ; plus long sur glace ; dégâts, poussées, traversées et rebonds inchangés. (12.1 testé le 2026-11-18 dans `Lvl_Floor_Gym` ; le calibrage des sols est laissé à l'équipe de design.)

---

## Questions ouvertes
1. ~~Boutons : Défense et plongeon (lot D).~~ Réglé le 2026-11-17 : une seule Input Action, `IA_DefensiveDive` (E, manette Y) ; à l'arrêt = Défense, avec direction = plongeon.
2. ~~Transitions vers `Celebrating` / `Sulking` depuis une action ou Stunned / Down (lot V).~~ Réglé le 2026-11-18 : un but met fin tout de suite à Stunned / Down, pour les deux équipes (`Celebrating` ou `Sulking`). Pendant un plongeon, le joueur finit son plongeon et se relève, puis célèbre ou boude pour toute la durée (`WB_PlayerStateComponent`, état en attente annulé par tout autre changement d'état).
3. ~~Plongeon et interception (lot 11.1).~~ Réglé le 2026-11-17 : le plongeon ne prend jamais le ballon, il rebondit ; la Défense l'attrape.
4. Passes entre coéquipiers : un ballon kické n'est captable qu'en Défense, donc une passe rapide n'est reçue qu'en Défense ou une fois ralentie. Faut-il laisser les coéquipiers du tireur capter un ballon kické ? Réponse de l'utilisateur (2026-11-18) : oui, mais dans un prochain devis complet qu'il fournira.
