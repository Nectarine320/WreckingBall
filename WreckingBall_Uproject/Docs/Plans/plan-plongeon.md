# Plongeon et défense : fiche des réglages

Créée le 2026-11-17, révisée le même jour. Le bouton plongeon (E au clavier, Y à la manette) fait deux choses selon le mouvement du joueur au moment de l'appui.

Cette fiche dit **où se trouve chaque réglage et ce qu'il change**. La distance et la vitesse du saut seront rééquilibrées souvent.

---

## 1. Règles du jeu (décisions de l'utilisateur, 2026-11-17)

### Quel mouvement
- **Appui en mouvement** (axe X ou Y du déplacement différent de zéro) : **plongeon**.
- **Appui sans mouvement** : **mode défense**, tant que le bouton reste enfoncé.
- Les deux restent disponibles en tout temps. Ils sont refusés si le joueur tient le ballon ou un objet, ou s'il n'est pas dans l'état Free.

### Plongeon
**Direction**
- Le stick a la priorité : le joueur plonge dans la direction du stick.
- Si le ballon (tiré ou non, à 12 m ou moins) a un point de sa trajectoire prévue dans le cône du stick, le plongeon va vers ce point.
  - Il prend le point atteignable le plus proche de la direction du stick.
  - S'il n'y en a aucun, il prend le point du cône le plus proche de ce que le saut peut atteindre.
- Aucun ballon dans le cône : le joueur plonge à la distance maximale, droit dans la direction du stick, en arche (`FreeDiveHeight`).

**Trajectoire**
- Vitesse à plat constante (deux fois plus rapide depuis le 2026-11-17 : `AirDuration` 0,75 → 0,375 s).
- Hauteur en parabole : le corps part vers le haut à pleine vitesse, ralentit jusqu'au sommet, puis retombe en accélérant jusqu'au sol.

**Le saut**
- Le joueur n'a aucun contrôle pendant le saut, comme pour le Dash.
- Il passe au travers des autres joueurs, sans leur donner de malus.

**Posture**
- Le joueur fait face au ballon s'il y en a un à portée. Sinon, il fait face à la direction du saut.
- Le corps se couche vers la direction du saut. Pour un saut de côté, il est couché sur le côté. Pour un saut vers le ballon, il est à plat ventre.

**Énergie**
- Le plongeon est refusé en Exhausted.
- Le départ coûte 30 %.
- Si le joueur touche le ballon : rien de plus, le plongeon coûte 30 % au total.
- Si le joueur rate le ballon : 30 % de plus à l'atterrissage, soit 60 % au total, puis un petit stun au sol de 0,25 s.
- Pas de recharge d'énergie pendant le plongeon. La recharge reprend après le relevé, avec le même délai que le Dash.

**Le reste**
- Au contact, le ballon rebondit loin du corps, comme sur un mur qui avance à la vitesse du saut. Un ballon immobile est donc poussé.
- Un flash blanc unique sur le mesh marque le départ, avec l'overlay `M_PlayerGraceFlash`.

### Mode défense
- Le joueur prend une **pose en X** : bras et jambes écartés.
- Il prend la pose **dans la direction qu'il regarde déjà** : il ne se tourne pas vers le ballon (décision du 2026-11-17). Cette orientation reste **fixe** ensuite : il se déplace en lignes droites sur ses axes.
- **Les touches suivent les axes du joueur, pas ceux de la caméra** (voulu) : un joueur tourné de 30° a son WASD tourné de 30°.
  - Chaque axe garde le sens le plus proche de celui de la caméra : D va toujours plutôt vers la droite de l'écran, et W plutôt vers le haut. Les contrôles ne s'inversent donc jamais, même quand le joueur fait face à la caméra.
  - A/D : pas de côté, à **70 %** de sa vitesse (50 % avant le 2026-11-17, +40 %).
  - W/S : avant ou arrière sur l'axe du regard, plus lentement (`DefenseForwardSpeedScale`, 50 % de la vitesse de côté, soit 35 % de la marche).
- Pas de sprint, pas d'action.
- Le ballon qui le touche rebondit, comme pendant le plongeon.
- Aucun coût d'énergie : la vitesse réduite est le seul malus.
- **Fin du mode** :
  - quand le joueur relâche le bouton ;
  - quand un autre état prend le relais : un Dash qui le met Down ou Stunned, un but, `Disabled`.

---

## 2. Où sont les réglages

### `DA_PlayerDive_Default` (classe `UWB_PlayerDiveProfile`)
Ce Data Asset est branché sur le composant `PlayerDiveComponent` de `BP_GenericCharacter`.

**Cible et saut**

| Réglage | Défaut | Ce qu'il change |
|---|---|---|
| `BallClass` | `BP_PO_Master_Ball_01` | Ballons visés par le plongeon et bloqués par la défense. |
| `MaxBallDistance` | 1200 cm | Portée du ballon. Au-delà, le plongeon l'ignore. |
| `AirDuration` | 0,375 s | Durée du saut, du décollage au contact avec le sol. C'est le réglage principal de la **vitesse**. |
| `MinDistance` / `MaxDistance` | 200 / 800 cm | Bornes de la **distance** du saut. `MaxDistance` sert aussi quand le saut va droit dans la direction du stick. |
| `MinHeight` / `MaxHeight` | 50 / 225 cm | Bornes de la **hauteur** du corps au sommet du saut. Le saut vise la hauteur du ballon, limitée à ces bornes. |
| `FreeDiveHeight` | 140 cm | Hauteur du corps au sommet d'un plongeon sans ballon visé. Au-dessus de 90 cm (debout), le saut fait une arche. C'est le réglage principal de la **hauteur de l'arche**. |

**Visée**

| Réglage | Défaut | Ce qu'il change |
|---|---|---|
| `StickConeHalfAngle` | 30° | Demi-angle du cône autour du stick (30° donne un cône de 60°). Un ballon dans ce cône attire le plongeon. |
| `PlanningReach` | 70 cm | Tolérance de visée : écart maximal entre le corps et le ballon prévu pour qu'un point compte comme atteignable. |
| `MinInterceptFraction` / `MaxInterceptFraction` | 0,2 / 0,85 | Moments du saut où le corps peut rencontrer le ballon, en fraction de `AirDuration`. |

**Corps et ballon**

| Réglage | Défaut | Ce qu'il change |
|---|---|---|
| `BodyTilt` | 80° | Inclinaison du corps pendant le saut (90° = couché à plat). |
| `BodyTiltInTime` | 0,2 s | Temps que met le corps à se coucher au décollage. |
| `LyingHeight` | 25 cm | Hauteur du centre du corps couché au sol. |
| `BodyRadius` / `ArmReach` | 25 / 45 cm | Taille du corps pour détecter le contact avec le ballon pendant le plongeon (bras tendus compris). |
| `BallBounceRestitution` | 0,6 | Part de la vitesse que le ballon garde quand il rebondit sur le joueur. Vaut pour le plongeon et la défense. |

**Défense**

| Réglage | Défaut | Ce qu'il change |
|---|---|---|
| `DefenseForwardSpeedScale` | 0,5 | Vitesse de W/S (avant, arrière) en part de la vitesse de côté de A/D. |
| `DefenseBlockHalfWidth` | 70 cm | Demi-largeur de la zone de blocage : bras et jambes écartés. |
| `DefenseBlockHalfThickness` | 25 cm | Demi-épaisseur de la zone de blocage, de l'avant à l'arrière. |
| `DefenseBlockExtraHeight` | 20 cm | Hauteur ajoutée au-dessus de la capsule par les mains levées. |

**Atterrissage, feedback et animations**

| Réglage | Défaut | Ce qu'il change |
|---|---|---|
| `MissStunDuration` | 0,25 s | Temps au sol après un raté, entre l'atterrissage et le relevé. |
| `RecoveryPlayRate` | 2 | Vitesse de lecture de l'atterrissage et du relevé (2 = deux fois plus court). `ABP_SoccerPlayer` la lit (variable `DiveRecoveryPlayRate`) pour la Play Rate de `AS_Dive_Land` et `AS_Dive_GetUp`. |
| `FlashMaterial` / `FlashDuration` | `M_PlayerGraceFlash` / 0,1 s | Flash du départ. |
| `JumpAnim`, `AirAnim`, `LandAnim`, `GetUpAnim` | `AS_Dive_*` | Animations. La longueur de chacune fixe la durée de sa phase, sauf `AirAnim`, qui boucle. |

### `DA_PlayerState_Default` (classe `UWB_PlayerStateProfile`)

| Réglage | Défaut | Ce qu'il change |
|---|---|---|
| `DefenseMoveSpeedMultiplier` | 0,7 | Vitesse en mode défense, en part de la vitesse de marche. |

### `DA_PlayerActionCost_Default` (classe `UWB_PlayerActionCostProfile`)
Ce Data Asset est branché sur le composant `PlayerStateComponent` de `BP_GenericCharacter`.

| Réglage | Défaut | Ce qu'il change |
|---|---|---|
| `DashEnergyCost` | 0,4 | Énergie d'un Dash. |
| `DiveStartEnergyCost` | 0,3 | Énergie prise au départ du plongeon. |
| `DiveMissEnergyCost` | 0,3 | Énergie prise en plus quand le plongeon rate le ballon. |

---

## 3. Phases du plongeon (`EWB_DivePhase`, lues par `ABP_SoccerPlayer`)

| Phase | Durée | Animation |
|---|---|---|
| `Takeoff` | longueur de `AS_Dive_Jump` (au plus `AirDuration`) | `AS_Dive_Jump` |
| `Air` | reste de `AirDuration` | `AS_Dive_Air` (boucle) |
| `Land` | longueur de `AS_Dive_Land` ÷ `RecoveryPlayRate`, plus `MissStunDuration` après un raté | `AS_Dive_Land` (finit couché) |
| `GetUp` | longueur de `AS_Dive_GetUp` ÷ `RecoveryPlayRate` | `AS_Dive_GetUp`. Le corps se redresse par code. |

L'état reste `Player.State.Diving` du décollage à la fin du relevé, puis passe à `Free`. Un coup, un but ou `Disabled` coupe le plongeon.

## 4. Mode défense (`Player.State.Defending`)
- **Animations** (dans `Animation/Defense`, choisies par `ABP_SoccerPlayer`) :
  - `AS_Defense_Idle` (boucle de 1 s) : pose en X, bras qui pulsent, buste un peu penché vers l'avant ;
  - `AS_Defense_Walk` (boucle de 0,67 s) : pas chassé en crabe, avec les jambes qui s'ouvrent et se ferment en alternance et le buste qui se balance ;
  - choix : `IsDefenseMoving` (`IsDefending` et `Speed` > 10 cm/s), avec un fondu de 0,15 s ;
  - `AS_Defense_Pose` est la pose fixe d'origine. Elle n'est plus utilisée, mais sert de référence.
- **Déplacement** : en mode défense, `RequestMove` appelle `AddDefenseMoveInput` au lieu de `Move`, qui suit la caméra. L'input est appliqué dans les axes du joueur. La rotation automatique (`bOrientRotationToMovement`) est coupée pendant le mode et revient à sa valeur d'avant à la fin.
- **Entrées** : `IA_Dive` n'a plus de trigger (Pressed et Tap retirés). Started appelle `RequestDive`, et Completed ou Canceled appelle `ReleaseDive`.
- **Manette** : Y (`Gamepad_FaceButton_Top`) est mappé sur `IA_Dive` dans `IMC_SoccerDefault`. `IMC_PickupPhysicObject` (ajouté par `BPC_GrabObject`, priorité 1) ne doit pas mapper Y : il le prenait avant `IA_Dive` (retiré le 2026-11-17).

## 5. Débogage
- `bDebugDrawDive` sur `PlayerDiveComponent` dessine :
  - la trajectoire prévue du ballon (en orange) ;
  - le point visé :
    - en vert s'il est atteignable ;
    - en rouge si le ballon est dans le cône mais hors d'atteinte ;
    - en blanc s'il n'y a pas de ballon dans le cône ;
  - le trajet (en cyan) ;
  - le corps pendant le saut (en jaune, puis en vert après le contact) ;
  - la zone de blocage de la défense (en jaune).
