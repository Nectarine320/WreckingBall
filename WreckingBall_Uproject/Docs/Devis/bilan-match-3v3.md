# Bilan — étape 15 (phase E) : match 3 contre 3

Rédigé le 2026-11-20 par Claude++ à la fin du chantier. Complète `devis-match-3v3.md`. Tests de l'utilisateur : « tout semble bien fonctionner » (2026-11-20). Les lots ont été regroupés à la demande de l'utilisateur : **lot A = lots 1 + 2 + 3**, **lot B = lots 4 + 5**, une compilation C++ chacun, puis des retouches demandées en cours de test.

---

## 1. État des lots

| Lot | État | Notes |
|---|---|---|
| 0 État des lieux | Fait | Voir §5 (constats). |
| 1 Six personnages, manettes | Fait | P1 = manette 1 + clavier, P2…P4 = manettes 2 à 4 connectées. ID impair (P1, P3) = rouge. COM avec leur propre `WB_SoccerPlayerState`. `ToggleBots` (Num*) met en pause le cerveau des COM. |
| 2 `BP_TeamSpawnPoint` | Fait | Posé dans Soccerfield, `Lv_998_GameplayGym` et `Lvl_Destruction_Gym`. Repli en cercle autour de (0, 0, 0). Ballon au coup d'envoi = (0, 0, 0) posé au sol. |
| 3 Passe et changement | Fait, avec écarts | Voir §3 (changement à la réception, 2ᵉ appui, reprise du COM porteur). |
| 4 Caméra ballon + porteur | Fait, avec ajouts | Data Asset caméra, porteur au tiers opposé, lissage de la direction, caméra figée ballon hors terrain. Pas de zoom min / max (non voulu). |
| 5 Indicateurs hors écran | Fait, avec écarts | Aussi pour les COM (sans texte), taille selon la distance, contour blanc. |
| V Tests | Fait par l'utilisateur | Blueprints touchés : compilés sans erreur ni avertissement. C++ : compilé sans avertissement (pas de passe « avertissements comme erreurs » forcée). |

---

## 2. Ce qui a été construit

### C++ (fondation, tout exposé à Blueprint)
| Classe / fonction | Rôle |
|---|---|
| `UWB_TeamRosterComponent` + `UWB_TeamRosterProfile` (`DA_TeamRoster_Default`) | Effectif de 6 (humains en place 1, COM sur les places libres), placement, remise en place après un but, `SwitchOrPass`, `SwitchToCharacter`, `ToggleBots`, joueur ajouté / retiré en cours de match, reprise du COM porteur. Sur `BP_SoccerGamemode`. |
| `AWB_TeamSpawnPoint` (`BP_TeamSpawnPoint`) | 3 places + point principal, aperçu éditeur à la couleur d'équipe ; `GetKickoffBallLocation`, `PlaceOnFloor`. |
| `UWB_PassProfile` (`DA_Pass_Default`) | Vitesse de passe (part du kick, vitesse d'arrivée, freinage au sol), `PassAimMode` Lead / Direct (B = position actuelle, interprétation de Claude++). |
| `UWB_PlayerCommandComponent` | `RequestPass`, `OnPassCommand`, `IsHoldingBall`, `ComputePassVelocity`. |
| `UWB_TeamLibrary` | `FindDirectionalAlly`, `IsHumanControlled`. |
| `UWB_BallPossessionSubsystem` | Événement `OnBallPossessionChanged` ; détection du rebond d'un ballon kické ; raison d'un refus de prise (log). |
| `UWB_BallDribbleComponent` / `UWB_BallDribbleProfile` | `CatchSound`, `CatchHeightGrace`, log des pertes. |
| `UWB_PlayerStateComponent` | Prise au contact de la capsule (dos, côtés), log des prises refusées. |
| `UWB_SoccerCameraProfile` (`DA_SoccerCamera_Default`) | Réglages caméra, `ApplyCameraProfile`, `ComputeCameraTarget` (porteur au tiers). |
| `UWB_OffscreenIndicatorWidget_Base`, `UWB_OffscreenIndicatorItem_Base`, `UWB_OffscreenIndicatorProfile` (`DA_OffscreenIndicator_Default`) | Pastilles hors écran. |
| `UWB_SoccerAIController_Base` | `bBrainPaused`, équipe lue par `GetTeamOf` (PlayerState d'abord). |
| `FWB_TeamDefinition.IndicatorColor` | Couleur des pastilles, par équipe (`DA_TeamDefinitions`). |

### Blueprints et assets
- `BP_SoccerGamemode` : `SetupMatchPlayers` (manettes + `BuildRoster`), `ResumeMatch` (remise en place), `StartGoalCelebration` sur les 6 personnages, events de l'effectif qui tiennent `Players` à jour.
- `BP_TP_SoccerPlayerController` : `IA_SwitchOrPass` (A / Q / Tab), Num* → effectif, `CreateOffscreenIndicators`, `bAutoManageActiveCameraTarget` = faux.
- `CAMM_SoccerPlayer` : la caméra partagée n'est créée qu'une fois (voir §5).
- `BP_SoccerCamera` : suivi du ballon actif via le profil, figée si le ballon est désactivé ; anciennes fonction et variables retirées.
- `BPC_GrabObject.PassBall`, `BP_GenericCharacter.OnPassCommand`.
- `BP_SpawnManager` : coup d'envoi et carrousel (touche 0) au même point, posés au sol.
- `WBP_OffscreenIndicators`, `WBP_OffscreenIndicatorItem`, `IA_SwitchOrPass`, mappings dans `IMC_SoccerDefault`.
- Soccerfield : `NavMeshBoundsVolume` + `RecastNavMesh` en pool de tuiles fixe (1024) pour que le NavMesh sauvegardé soit gardé au chargement.

---

## 3. Écarts au devis et décisions du chantier (19 et 20 novembre)

### Passe et changement de personnage
- **Le changement de personnage se fait à la réception**, pas au moment de la passe : on prend le receveur quand il touche le ballon. Une interception annule le changement.
- **2ᵉ appui pendant la passe** : on prend le receveur tout de suite.
- **Reprise du COM porteur** (`bTakeBotCarrier`, activé dans `DA_TeamRoster_Default`) : dès qu'un COM allié prend le ballon (dribble perdu, rebond, interception), un humain de l'équipe le prend : le dernier humain porteur de l'équipe, sinon le plus proche. S'applique aussi quand un COM ramasse un ballon libre (IA actives).
- **Vitesse de passe +35 %** : `MinKickShare` 0,54, `ArrivalSpeed` 540, `RollingDeceleration` 911.
- **Prise au contact** : un ballon libre qui touche la capsule (dos, côtés) est pris, en plus de la zone devant les pieds. Vaut pour tous les personnages.

### Prise de possession (ajustements du 20 novembre)
- `KickerRecatchDelay` 0,5 → **0,2 s**.
- **`CatchHeightGrace` = 0,5 s** : après une prise, `LoseHeight` est ignoré, le ballon retombe aux pieds (corrige le « collé dessus » : prise en l'air puis perte immédiate en boucle).
- **Rebond + vitesse lente** : un ballon kické qui a rebondi redevient rattrapable sous `KickedReboundReleaseSpeed` (600 cm/s, `DA_PlayerState_Default`).
- **Logs de diagnostic** activés sur `BP_GenericCharacter` (`bDebugLogBallCatch`, `bDebugLogLoss`) : à couper après le diagnostic.

### Caméra
- Plus de cadrage de tous les joueurs : **ballon libre = ballon**, **ballon tenu = milieu ballon + porteur placé dans le tiers opposé à sa direction** (`CarrierScreenOffsetHorizontal` / `Vertical`, réglés par l'utilisateur à 0,525).
- **Lissage de la direction** (`CarrierDirectionSmoothing`) : un demi-tour repasse par le centre.
- Valeurs de l'utilisateur : `ArmLength` 2200, `FieldOfView` 60, `CarrierDirectionMinSpeed` 175.
- Pas de zoom min / max : l'utilisateur trouve la distance fixe bonne.

### Indicateurs hors écran
- **Aussi pour les COM** (pastille de couleur sans texte, `bShowBots`), alors que le devis disait « pas d'indicateur pour les COM ».
- **Taille selon la distance** hors cadre (`SizeAtEdge` 56 → `SizeFar` 28 à `FarDistance` 800), **contour blanc** (`OutlineWidth` 3).

### Ballon
- Coup d'envoi en (0, 0, 0) **projeté sur le NavMesh puis posé au sol** (`PlaceOnFloor`, bas de la collision sur le plancher, quel que soit le pivot). Le carrousel (touche 0) utilise le même point.

---

## 4. Points ouverts
- **Boule de bowling** : le kicker n'arrive pas à la reprendre plusieurs secondes après son kick. Cause probable : `KickedReleaseSpeed` à 40 cm/s (reste « kickée » tant qu'elle roule). À confirmer avec le log « cannot catch », puis recalibrer les `KickedReleaseSpeed` des ballons (Bowling, Bomb).
- **Pastilles sous la bande du score** (haut de l'écran) : option `EdgeMarginTop` proposée, non faite.
- **`Lv_998_GameplayGym` sans NavMesh** : les COM n'y bougent pas.
- **Ménage** : `StartSpawnPoint` de `BP_SpawnManager` n'est plus utilisé.
- **Interface** : le score affiche « BLUE TEAM » à gauche alors que les rouges jouent à gauche à Soccerfield.
- **Personnage quitté** pendant un sprint ou une charge de kick : rien n'est relâché explicitement ; pas de problème signalé.
- **Passe version B** (`PassAimMode` Direct) : interprétée comme « vers la position actuelle » ; à confirmer.
- **Commit** de tout ce qui suit le lot A (caméra, lot B, retouches) : à faire par l'utilisateur.

---

## 5. Constats techniques à retenir
- **Saut de caméra au changement de personnage** : `Event OnPossess` du PlayerController appelle `CAMM_SoccerPlayer.SetUpSharedCamera`, qui créait une nouvelle `BP_SoccerCamera` en (0, 0, 0) à chaque possession. Corrigé par un `IsValid(CameraRef)`.
- **NavMesh rejeté au chargement** (« maxTiles mismatch ») avec la génération Dynamic : réglé par `bFixedTilePoolSize` sur le `RecastNavMesh` de la map.
- **MCP** : les opérateurs promus (`==`, `<`) et l'appel d'un Custom Event ne se créent pas node par node, mais passent par la DSL ; une fonction C++ `const` devient pure en Blueprint ; le getter d'une variable « Rendering » d'un autre acteur n'a pas de pin cible.

---

## 6. À reporter dans le projet claude.ai
- Devis maître : étape 15 **faite** (2026-11-20) ; prochaine étape = **phase F, étape 16** (`devis-npc.md`). Le passage de `devis-npc.md` sur le receveur IA (« démarquage devant le ballon ») reste valable ; la règle « reprise du COM porteur » (§3) est à prendre en compte dans la phase F.
- Devis `devis-match-3v3.md` : écarts du §3.
- Backlog : ligne `IsValid` de `BP_SoccerCamera` réglée.
