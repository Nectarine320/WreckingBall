# Wrecking Ball — Correctif : mouvement en mode Défense relatif à la caméra

Créé le 2026-11-17 (14 h 40), modifié à 14 h 55 (orientation : visée unique, plus de suivi du ballon). À remettre à Claude++. Guide d'ergonomie Blueprint appliqué, rapport de la section 8 à la fin.

## Problème (test de l'utilisateur, 2026-11-17)
En mode Défense, quand le joueur se déplace en arc, A et D restent « collés » à un mouvement qui devrait passer par W et S : l'input est lu par rapport au corps du joueur (qui garde son orientation), et non par rapport à la caméra.

## Règles validées par l'utilisateur
1. **Entrée** : appui sur le bouton de Défense **sans être en déplacement** (input de mouvement nul ou sous la dead zone). En mouvement, l'appui ne fait rien. *(Sortie : au relâchement du bouton, à confirmer par Claude++ dans le code existant ; ne pas changer le comportement actuel.)*
2. **Orientation — visée unique à l'entrée** (changement du 2026-11-17, 14 h 53) :
   - Ballon à **≤ 12 m (1200 cm)** au moment d'entrer : le joueur **pivote une seule fois** pour faire face au ballon (rotation interpolée vers l'angle visé). Une fois l'angle atteint, **l'orientation est figée** : le joueur **ne suit plus le ballon**, même si celui-ci bouge.
   - Ballon à **> 12 m** : pas de pivot, le joueur garde la direction qu'il avait en entrant.
   - Dans les deux cas, l'orientation reste fixe jusqu'à la sortie de la Défense.
3. **Déplacement** : WASD (et stick gauche) **relatifs à la caméra exacte**, en mouvements directionnels (pas d'arc autour du ballon). W = haut de l'écran, S = bas, A = gauche, D = droite, quelle que soit l'orientation du personnage. Mouvements latéraux et frontaux libres.

## Implémentation demandée
- Direction de mouvement en Défense = `ForwardVector` et `RightVector` du **yaw de la caméra active** (`BP_SoccerCamera` / caméra du joueur), aplatis au sol (Z = 0, normalisés) × axes de l'input. Même calcul pour le clavier et la manette.
- `bOrientRotationToMovement` (ou l'équivalent du code actuel) coupé pendant la Défense ; rétabli à la sortie.
- À l'entrée : `DefenseTargetYaw` = yaw vers le ballon (si ≤ `DefenseBallTrackRadius`) sinon yaw actuel ; `RInterpTo` jusqu'à l'atteindre (vitesse `DefenseTurnSpeed`), puis plus aucune rotation. Pas de recalcul de la cible ensuite. Le déplacement est permis pendant le pivot.
- Animation : l'AnimBP reçoit l'angle entre la vitesse et l'orientation du joueur (`CalculateDirection`) pour le blend des pas chassés ; aucun autre usage de l'orientation du corps pour lire l'input.
  - **Fait le 2026-11-18** : `DefenseMoveAngle` (`WB_PlayerDiveComponent::GetDefenseMoveAngle`) et `Speed` pilotent `BS_Defense_Move` (Blend Space 2D) dans `ABP_SoccerPlayer`. À l'arrêt : `AS_Defense_Idle` (mains en X) ; en déplacement : `AS_Defense_Step_Forward / _Back / _Left / _Right` (pas de crabe, petits pas avant / recul, coudes repliés, mains devant la poitrine). `AS_Defense_Walk` n'est plus utilisé.
- Réglages `DefenseBallTrackRadius` = 1200 et `DefenseTurnSpeed` *(à calibrer)* : dans `DA_PlayerDive_Default` (règle « aucune valeur cachée dans le C++ », révision du 2026-11-17), avec Description.
- L'état `Player.State.Defending` existe (étape 9 faite) : entrée / sortie par `ChangePlayerState`.

## Test
- Immobile + bouton → Défense ; en mouvement + bouton → rien.
- Ballon à moins de 12 m : le joueur pivote vers le ballon, puis reste figé dans cet angle même si le ballon bouge ; au-delà de 12 m : aucun pivot.
- En Défense : W / A / S / D suivent toujours l'écran, mouvements en ligne droite, sans « jam » ; les pas chassés suivent le mouvement.
- Sortie de Défense : déplacement normal inchangé.
