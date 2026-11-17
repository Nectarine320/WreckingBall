# Plongeon de défense : fiche des réglages

Créée le 2026-11-17. Le plongeon est le saut de garde du corps vers un ballon tiré par l'équipe adverse. Touches : E au clavier, Y à la manette.

Cette fiche dit **où se trouve chaque réglage et ce qu'il change**. La distance et la vitesse du saut seront rééquilibrées souvent.

---

## 1. Règles du jeu (décisions de l'utilisateur, 2026-11-17)

**Quand le plongeon est permis**
- Seulement si un ballon tiré ou lancé par l'équipe adverse est en vol. Sinon l'appui ne fait rien.
- Refusé si le joueur tient le ballon ou un objet. Un Force Drop pour tenter de sauver le ballon est à revoir selon les tests.
- Refusé en Exhausted.

**Le saut**
- Le joueur n'a aucun contrôle pendant le saut, comme pour le Dash.
- Il passe au travers des autres joueurs, sans leur donner de malus.

**Visée et posture**
- Le stick choisit le côté : le plongeon vise le point de la trajectoire prévue du ballon le plus proche de la direction du stick, dans un cône.
- Stick relâché : le plongeon vise le point du trajet atteint le plus tôt.
- Aucun point atteignable : le joueur plonge à la distance maximale, dans la direction du stick.
- Il fait face au ballon, et le corps se couche vers la direction du saut. Pour un saut de côté, il est couché sur le côté. Pour un saut vers le ballon, il est à plat ventre.

**Énergie**
- Le départ coûte 30 %.
- Si le joueur touche le ballon : rien de plus, le plongeon coûte 30 % au total.
- Si le joueur rate le ballon : 30 % de plus à l'atterrissage, soit 60 % au total, puis un petit stun au sol de 0,5 s.
- Pas de recharge d'énergie pendant le plongeon. La recharge reprend après le relevé, avec le même délai que le Dash.

**Le reste**
- Au contact, le ballon rebondit loin du corps, comme sur un mur.
- Un flash blanc unique sur le mesh marque le départ, avec l'overlay `M_PlayerGraceFlash`.

---

## 2. Où sont les réglages

### `DA_PlayerDive_Default` (classe `UWB_PlayerDiveProfile`)
Ce Data Asset est branché sur le composant `PlayerDiveComponent` de `BP_GenericCharacter`.

| Réglage | Défaut | Ce qu'il change |
|---|---|---|
| `AirDuration` | 0,75 s | Durée du saut, du décollage au contact avec le sol. C'est le réglage principal de la **vitesse**. |
| `MinDistance` / `MaxDistance` | 200 / 500 cm | Bornes de la **distance** du saut. `MaxDistance` sert aussi quand aucun point n'est atteignable. |
| `MinHeight` / `MaxHeight` | 50 / 225 cm | Bornes de la **hauteur** du corps au sommet du saut. Le saut vise la hauteur du ballon, limitée à ces bornes. |
| `StickConeHalfAngle` | 30° | Demi-angle du cône autour du stick (30° donne un cône de 60°). |
| `PlanningReach` | 70 cm | Tolérance de visée : écart maximal entre le corps et le ballon prévu pour qu'un point compte comme atteignable. |
| `MinInterceptFraction` / `MaxInterceptFraction` | 0,2 / 0,85 | Moments du saut où le corps peut rencontrer le ballon, en fraction de `AirDuration`. |
| `BodyTilt` | 80° | Inclinaison du corps pendant le saut (90° = couché à plat). |
| `BodyTiltInTime` | 0,2 s | Temps que met le corps à se coucher au décollage. |
| `LyingHeight` | 25 cm | Hauteur du centre du corps couché au sol. |
| `BodyRadius` / `ArmReach` | 25 / 45 cm | Taille du corps pour détecter le contact avec le ballon (bras tendus compris). |
| `BallBounceRestitution` | 0,6 | Part de la vitesse que le ballon garde quand il rebondit sur le joueur. |
| `MissStunDuration` | 0,5 s | Temps au sol après un raté, entre l'atterrissage et le relevé. |
| `MinBallSpeed` | 400 cm/s | En dessous de cette vitesse, le ballon ne compte pas comme tiré. |
| `MaxBallDistance` | 1500 cm | Au-delà de cette distance, le ballon est ignoré. |
| `bOnlyEnemyBall` | vrai | Faux : le plongeon marche sur n'importe quel ballon, ce qui est utile pour tester seul. |
| `FlashMaterial` / `FlashDuration` | `M_PlayerGraceFlash` / 0,1 s | Flash du départ. |
| `JumpAnim`, `AirAnim`, `LandAnim`, `GetUpAnim` | `AS_Dive_*` | Animations. La longueur de chacune fixe la durée de sa phase, sauf `AirAnim`, qui boucle. |

### `DA_PlayerActionCost_Default` (classe `UWB_PlayerActionCostProfile`)
Ce Data Asset est branché sur le composant `PlayerStateComponent` de `BP_GenericCharacter`.

| Réglage | Défaut | Ce qu'il change |
|---|---|---|
| `DashEnergyCost` | 0,4 | Énergie d'un Dash. |
| `DiveStartEnergyCost` | 0,3 | Énergie prise au départ du plongeon. |
| `DiveMissEnergyCost` | 0,3 | Énergie prise en plus quand le plongeon rate le ballon. |

---

## 3. Phases (`EWB_DivePhase`, lues par `ABP_SoccerPlayer`)

| Phase | Durée | Animation |
|---|---|---|
| `Takeoff` | longueur de `AS_Dive_Jump` (au plus `AirDuration`) | `AS_Dive_Jump` |
| `Air` | reste de `AirDuration` | `AS_Dive_Air` (boucle) |
| `Land` | longueur de `AS_Dive_Land`, plus `MissStunDuration` après un raté | `AS_Dive_Land` (finit couché) |
| `GetUp` | longueur de `AS_Dive_GetUp` | `AS_Dive_GetUp`. Le corps se redresse par code. |

L'état reste `Player.State.Diving` du décollage à la fin du relevé, puis passe à `Free`. Un coup, un but ou `Disabled` coupe le plongeon.

## 4. Débogage
- `bDebugDrawDive` sur `PlayerDiveComponent` dessine :
  - la trajectoire prévue du ballon (en orange) ;
  - le point visé (en vert, ou en rouge s'il n'est pas atteignable) ;
  - le trajet (en cyan) ;
  - le corps pendant le saut (en jaune, puis en vert après le contact).
