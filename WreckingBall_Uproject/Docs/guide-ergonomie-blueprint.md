<!-- COPIE EN LECTURE SEULE. Original : claude/guide-ergonomie-blueprint.md dans le projet claude.ai « Wrecking Ball - Alagaesia ». Copiée le 2026-11-13. Ne pas modifier ici : le guide se met à jour uniquement dans la conversation dédiée, puis on recopie le fichier. -->

# Wrecking Ball — Guide d'ergonomie Blueprint (pour Claude)

À lire **avant** de créer ou de modifier un graph Blueprint dans le projet. Le but : qu'un humain ouvre le graph et comprenne le flux en le lisant de gauche à droite, sans démêler de câbles.

Références à imiter : `BP_GenericCharacter` (EventGraph, surtout la chaîne `Dash` et le bloc `DashAnim`) et `BP_SoccerPlayer` (EventGraph). Les termes Unreal restent en anglais.
Rappel : dans un graph Unreal, **Y augmente vers le bas**. La grille est de 16 px : toutes les positions sont des multiples de 16.

---

## 1. Le flux d'exécution (fils blancs)

1. **Gauche → droite, toujours.** L'Event est à gauche, chaque node exec suivant est à sa droite. Un node exec n'est jamais placé à gauche de celui qui l'appelle. Seule exception tolérée : le retour vers le `Break` d'un `ForEachLoopWithBreak`. Ce câble passe alors **sous** le bloc.
2. **Ligne droite.** Les nodes d'une même chaîne partagent le même Y, à ±16 px près, pour que le fil blanc soit horizontal. On aligne les **pins**, pas le haut des nodes :
   - node qui suit directement un Event / Custom Event : `y = y_event + 16` (ex. `Dash` à -816 → `Branch` à -800) ;
   - entre deux nodes impurs (Call Function, Set) : même Y, ou ±16 selon le type de node.
3. **Espacement horizontal serré.** L'écart entre deux nodes exec est juste assez grand pour loger les nodes de données qui alimentent le suivant. En pratique, 64 à 160 px de vide entre le bord droit d'un node et le bord gauche du suivant. Si une colonne est vide, on resserre.
4. **BeginPlay court.** BeginPlay → `Parent: BeginPlay` → appels à des fonctions ou Custom Events bien nommés (ex. `UI Sprint Initialization`, `UI KickEnergy Initialization`). La logique vit dans ces fonctions, pas dans BeginPlay.

## 2. Branchements : pas d'overlap, le « oui » en haut

Quand le flux se divise, chaque sortie part dans sa propre **rangée horizontale**. Les rangées sont empilées verticalement, dans l'ordre des pins de sortie, de haut en bas :

| Node | Rangée du haut | Rangée(s) du dessous |
|---|---|---|
| `Branch` | True | False |
| `ForEachLoop` | Loop Body | Completed |
| `Sequence` | Then 0 | Then 1, Then 2… |
| `Switch on Enum / Int / Name` | 1er case | cases suivants, dans l'ordre |
| `Timeline` | Update | Finished |
| `IsValid` (macro) | Is Valid | Is Not Valid |
| `Cast To` | succès | Cast Failed |
| Enhanced Input Action | Started / Triggered | Ongoing, puis Completed / Canceled |

```
[Event]──[Branch]─True──[A]──[B]──[C]
             │
             └False──[D]──[E]
```

- La rangée du haut garde le Y de la chaîne principale. La rangée suivante commence **sous le node le plus bas** de la rangée du dessus, avec environ 32 à 48 px d'air (dans les références : 144 px entre deux `Set WalkType` parallèles, et `SprintInitiate` / `SprintTerminate` empilés).
- Si **une seule** sortie est utilisée, le node suivant s'aligne sur **cette** pin. Ex. `Dash` : seul le False du `Branch` est branché, donc `Set Dashing` est à Y +48 du Branch et le fil reste droit.
- Deux rangées ne se croisent jamais. Si la rangée du bas doit rejoindre la ligne principale plus loin, elle remonte **après** la fin de la rangée du haut.

## 3. Les données (fils de couleur) : proximité

1. **Le getter vit à côté de son consommateur.** Un `Get Variable`, `Get Mesh`, `Get Actor Location`, etc. se place juste à gauche et un peu sous la pin qu'il alimente : 150 à 250 px à gauche, aligné sur sa pin à ±16 px.
   Exemple de référence (`DashAnim`) : `Get DashStartLocation` et `Get DashEndLocation` sont à environ 190 px à gauche de `Distance (Vector)`, à seulement 46 px l'un de l'autre.
2. **Dupliquer plutôt que tirer un long câble.** Si une variable sert à deux endroits éloignés, on pose **un getter par consommateur**. Les références le font partout : `Get Mesh` ×3, `Get WalkType` ×4, `Get GhostingTimer` ×2. Un fil de données de plus d'environ 400 px est un signal d'alarme.
3. **Le node « self » se duplique comme un getter.** Un node `self` par consommateur, posé juste à côté de sa pin. On ne tire jamais un fil depuis un seul `self` vers plusieurs nodes éloignés.
4. **L'ordre vertical suit l'ordre des pins.** Le getter de la première pin d'entrée est au-dessus de celui de la deuxième (Start au-dessus de End, Location au-dessus de Rotation).
5. **Les chaînes pures** (math, break, conversions) se lisent aussi de gauche à droite, **sous** la ligne exec, dans l'espace entre les deux nodes exec concernés. Ex. `Get Actor Location` / `Get Actor Rotation` → `Get Forward Vector` → `vector * vector` → `vector + vector` → `Set DashEndLocation`.
6. Un node pur ne se place jamais au-dessus de la ligne exec s'il y a de la place en dessous. Exception : un « self » ou un getter d'une seule ligne peut se mettre juste au-dessus d'une pin haute.

## 4. Reroute nodes : le dernier recours

Je mettais beaucoup trop de reroutes, souvent par paires qui se superposent. Dans `BP_MenuCinematicManager`, presque chaque câble avait deux reroutes, et deux étaient exactement au même endroit. Ça vient du placement automatique sur une grille de 300 à 350 px, qui éloigne les nodes et ajoute un « coude » à chaque fil.

Règles :
- **Avant un reroute, on pose trois questions, dans cet ordre :**
  1. Est-ce que je peux **aligner** les deux pins (déplacer le node de 16 à 48 px) ? → plus de coude.
  2. Est-ce que les nodes ont vraiment besoin d'être **aussi loin** ? → on les rapproche.
  3. Est-ce que je peux **dupliquer le getter** (ou le `self`) près du consommateur ? → plus de long câble.
- Un reroute n'est justifié que si le câble passerait **à travers le corps d'un node**, ou s'il faut séparer deux fils parallèles qui se chevaucheraient. Maximum **un** reroute par câble en général, deux seulement pour contourner un bloc entier.
- Jamais de reroute sur un fil exec entre deux nodes voisins.
- Jamais deux reroutes à la même position, ni à moins d'environ 32 px l'un de l'autre sur le même fil.
- Les reroutes d'un même contournement s'alignent sur la même colonne X et sur les Y des pins concernées. Ex. dans `BP_GenericCharacter`, la paire sous `IsValid` est à Y -640 et emmène `Ghost` vers `DestroyComponent`.

## 5. Delegates et timers (fils rouges) : event tout près

Pour `Set Timer by Event`, `Bind Event to …`, `Create Event` :
- Le Custom Event branché sur la pin rouge `Event` se place **juste en dessous du node qui fait le binding** (ou dans la même colonne que l'event qui le contient). Le fil rouge doit être court et presque vertical. Ex. `InitializeGhosting` (y -288) avec `Ghosting` posé dans la même colonne X, juste en dessous.
- Si la logique de l'event appelé vit ailleurs dans le graph, je **crée un Custom Event relais** à côté du binding. Par exemple, un event `Initialize…` dont le seul travail est de faire le `Set Timer by Event` et de stocker le handle, placé juste au-dessus de l'event appelé. C'est moins optimal, mais c'est voulu et approuvé : la lisibilité compte plus que le coût négligeable d'un appel supplémentaire.
- Le handle retourné (`Return Value`) va directement dans un `Set …Timer` placé à droite, sur la même ligne.
- Contre-exemple à ne pas refaire : `OnSequenceFinished` à (0, 689) relié à `Bind Event to OnFinished` à (2021, 16), soit un fil rouge de 2000 px.

### Modèle : animation par code (sans Timeline)

Pour une animation calculée image par image (secousse, rebond, déplacement de lettres…) :
1. **`Play…Anim`** (ex. `PlayShakeAnim`) initialise l'état, puis lance un `Set Timer by Event` à **0,016 s, en boucle** (`Looping` = vrai), et stocke le handle.
2. L'event **`…Step`** (ex. `ShakeAnimStep`) est branché sur la pin rouge et placé **juste sous le node `Set Timer by Event`**, pour garder le fil rouge court. Il avance le temps écoulé, applique la valeur, et arrête le timer (`Clear and Invalidate Timer by Handle`) quand l'animation est finie.
3. **Les calculs mathématiques vont dans une fonction dédiée** (ex. `GetShakeOffset`), pure si possible. L'event `…Step` reste lisible : avancer le temps → appeler la fonction → appliquer le résultat → tester la fin.

Les deux events contiennent `Anim` (section 7) et vont dans un bloc à Comment jaune.

## 6. Blocs et commentaires colorés

Chaque chaîne d'event = **un bloc**, entouré d'un Comment de la couleur de sa catégorie. Les blocs sont empilés verticalement, avec environ 200 à 400 px de vide entre eux. Un bloc qui continue loin (ex. la fin du Dash : ghosting, cooldown) prolonge sa propre ligne vers la droite, il ne descend pas dans le bloc suivant.

**Les Comments servent aux events de l'EventGraph.** Une fonction n'est pas encadrée par un Comment : elle est documentée par son champ **Description** (section 7).

Légende du projet (couleurs mesurées dans les BP de référence, avec Alpha 0.5, FontSize 18 et MoveMode GroupMovement). Ces couleurs sont déjà dans les **favoris** de l'éditeur : dans les rapports, je donne la couleur **par son nom seulement** (bleu, rouge, vert, jaune, Do Not Delete), jamais en RGBA. Les valeurs RGBA ne servent que si je règle `CommentColor` par outil.

| Nom | Usage | CommentColor (R, G, B, A) |
|---|---|---|
| **bleu** — Begin Play / Initializer | BeginPlay, events d'initialisation | (0, 0.153, 1, 0.5) |
| **rouge** — Input Event | Enhanced Input Actions, events déclenchés par un input | (1, 0, 0.065, 0.5) |
| **vert** — Event Tick | Tick, timers récurrents, mises à jour continues | (0, 1, 0.125, 0.5) |
| **jaune** — Animation | Timelines, animations par code, lerp, montages, feedback visuel | (2, 1.759, 0.167, 0.5) |
| **Do Not Delete** (noir) | Code nécessaire qu'on pourrait croire mort | (0.0095, 0.0006, 0.0104, 0.5) |
| (gris, défaut / légende) | Cadre neutre | (0.15, 0.15, 0.15, 0.5) |

- Le titre du Comment dit **ce que fait le bloc** (« Dash – scan des destructibles », « Sprint – consommation d'énergie »), pas le nom du node.
- **Limite de l'outillage MCP** : je ne peux ni écrire le texte d'un Comment, ni régler sa taille ou sa position (un Comment créé atterrit vide en 0,0). Je ne crée donc **pas** de Comment par MCP. À la fin de chaque tâche, je donne la liste des blocs à encadrer (couleur par son nom + titre), pour que tu les poses à la main, ou je les pose par le contrôle de l'écran si tu me le demandes. Je peux par contre modifier `CommentColor` et `FontSize` sur un Comment existant.
- **Pas de Comments sur les Blueprints de test temporaires** (ex. `BP_Destructible_TestCube`, appelé à être supprimé).

## 7. Variables, fonctions et Custom Events : nommer et documenter

### Variables
1. **Toujours une Category.** Toute variable créée est rangée dans une Category du panneau My Blueprint (`set_variable_category`), jamais laissée dans la catégorie par défaut (le nom du Blueprint). La Category regroupe par **système** : `Sprint`, `Dash`, `Kick`, `UI`, `Timers`, `Match`… Je réutilise une Category existante du BP avant d'en créer une nouvelle.
2. **Une Description dès que l'effet n'est pas évident.** Si le nom seul ne dit pas *à quoi sert* la variable, ni *dans quelle unité* elle est, ni *quand elle change*, elle reçoit une Description (tooltip). Elle répond en une ou deux phrases à : qu'est-ce que ça contrôle, unité ou plage, qui la modifie.
   - Pas besoin de description : `Dashing` (bool), `WalkType`.
   - Description obligatoire : `DashScanDistance` (« Distance in cm in front of the player where the Dash looks for destructibles. »), `SprintTerminationLerp`, `KickStrenghtPercentDefault`, tout handle de timer (« Handle of the timer that refreshes the sprint bar; paused when the bar is full. »).
3. **Widget déjà placé ailleurs.** Quand j'ajoute une variable à un widget qui est déjà placé dans d'autres WBP, les instances placées gardent 0 ou vide au lieu de la valeur par défaut (vu avec la secousse et les textes des slots). Je remets donc la variable aux valeurs par défaut sur **chaque instance placée** (`reset_properties`), puis je compile et sauvegarde les WBP concernés.

### Fonctions
- Une fonction est documentée par son champ **Description**, pas par un Comment. La Description dit ce que fait la fonction, ses entrées et sorties si elles ne sont pas évidentes, et quand l'appeler.

### Langue des Descriptions
- Les Descriptions de **variables et de fonctions sont écrites en anglais**.

### Limite de l'outillage MCP
- S'il n'existe pas d'outil pour écrire une Description (variable ou fonction), je ne la saute pas en silence. Je la donne dans le rapport final (format en section 8), pour que tu la colles, ou je la pose par le contrôle de l'écran si tu me le demandes.

### Custom Events
1. **Self-explanatory.** Le nom dit **ce que fait l'event**, pas comment il est appelé. On lit le nom et on sait ce qui va se passer, sans ouvrir le graph. Format : verbe + sujet (ou sujet + action, selon la convention déjà présente dans le BP), en PascalCase.
   - Oui : `InitializeGhosting`, `UpdateSprintUI`, `BallDepossession`, `SprintingEnergyRecoveryOvertime`.
   - Non : `Event1`, `DoStuff`, `Handle`, `OnTimer`, `Custom_0`.
2. **Anim dans le nom.** Un Custom Event qui joue, pilote ou dépend d'une **Timeline** ou d'une **animation** (Timeline, animation par code, montage, lerp visuel, feedback animé) contient le mot **`Anim`** dans son nom. Ex. `DashAnim`, `PlayShakeAnim`, `ShakeAnimStep`. Un event `Anim` va dans un bloc à Comment jaune.
3. **Les events relais** (section 5) portent un nom qui dit ce qu'ils déclenchent : `InitializeUpdateSprintUI` lance le timer qui appelle `UpdateSprintUI`.

### Composants universels et classes parentes
1. **Nouvelle fonctionnalité = option désactivée par défaut.** Quand j'ajoute une fonctionnalité à un composant de base partagé (ex. `WBP_BaseButton`), elle passe par une option booléenne à **faux par défaut** (ex. `UseBlockedState`). Les usages existants ne changent pas tant qu'on n'active pas l'option.
2. **Fonction vide ou neutre dans une classe parente = voulu.** Une fonction de base qui renvoie une valeur neutre (`CanAccept` = vrai) ou qui est vide (`OnAcceptRefused`) est un **point d'accroche pour les enfants**. Ce n'est pas du code mort : on ne la supprime pas, on ne la signale pas comme un problème, et sa Description dit qu'elle est faite pour être surchargée.

## 8. Méthode de travail avec les outils MCP

1. **Construire la logique** (`write_graph_dsl`, `create_node`, `connect_pins`).
   - Pour chaque variable créée : Category tout de suite, Description si nécessaire, et `reset_properties` sur les instances déjà placées si c'est un widget (section 7).
   - Pour chaque fonction : Description en anglais.
   - Pour chaque Custom Event : vérifier le nom (self-explanatory, `Anim` si Timeline ou animation).
2. **Leçons sur le DSL (`write_graph_dsl`) :**
   - Il **ajoute** des nodes, il ne remplace pas le graph. Pour réécrire une fonction, je purge d'abord ses nodes, **sans supprimer le node de retour** d'une fonction qui a des sorties : il porte les paramètres de sortie.
   - J'utilise des **arguments nommés** : les arguments positionnels tombent souvent sur la pin `self`.
3. **Nettoyer après l'écriture :**
   - supprimer les `MakeLiteral` et reporter leur valeur directement dans la pin ;
   - supprimer les reroutes générés et reconnecter les pins directement ;
   - ne pas faire confiance au placement automatique (`arrange_nodes`, ou le layout du DSL) : il produit la grille de 300 à 350 px et les paires de reroutes.
4. **Refaire la mise en page selon un plan** (écrit avant de bouger les nodes, puis appliqué avec `set_node_position`) :
   - colonnes exec de gauche à droite, avec le Y de la ligne exec principale de chaque bloc ;
   - rangées des branchements empilées (section 2) ;
   - chaînes pures à gauche de leur consommateur : X = X_consommateur - 150 à 250, Y = Y de la pin ;
   - getters et `self` dupliqués au lieu de longs câbles ;
   - positions sur la grille de 16.
5. **Vérifier avec `get_node_infos`** avant de dire que c'est fini :
   - aucun node exec à gauche de son appelant ;
   - aucun reroute qui ne contourne rien, et aucun reroute superposé ;
   - aucun fil de données plus long que ~400 px, aucun fil rouge plus long que ~400 px ;
   - True / Loop Body / case 0 au-dessus des autres sorties ;
   - positions multiples de 16 ;
   - chaque nouvelle variable a une Category (`get_variable_category` ne renvoie pas une chaîne vide).
6. **Limites connues de l'outillage :**
   - un Custom Event **ne se renomme pas** par outil : il faut le renommer à la main (F2). Je choisis donc le bon nom dès la création, et je liste dans le rapport ceux qui seraient à renommer ;
   - l'**appel au parent** dans un event surchargé (fonction parente sans sortie) ne peut pas être créé par outil. **On évite ces surcharges.** Si une surcharge est indispensable, on passe par une fonction avec une sortie (l'appel au parent est alors créé automatiquement), ou je te demande d'ajouter l'appel à la main.
7. **Rapport final** (format validé le 2026-11-13) :
   - **Message concis** : dire ce qui a changé, avec une ligne sur pourquoi ou à quoi ça sert. Les explications détaillées seulement si tu les demandes.
   - **Blocs à encadrer** : couleur **par son nom** + titre (pas pour les Blueprints de test temporaires, section 6).
   - **Descriptions à coller**, si je n'ai pas pu les écrire :
     - une section par Blueprint, avec le nom du Blueprint en titre, dans une boîte de code ;
     - puis un tableau à trois colonnes : **Variable / fonction** (nom en boîte de code, avec « (fonction) » ou « (fonction, surcharge) » si besoin) | **Effet** (court, en français) | **Description** (en anglais, dans une boîte de code) ;
     - tu copies la Description par un triple-clic sur la boîte.

     Exemple :

     `BP_Destructible_Wall`

     | Variable / fonction | Effet | Description |
     |---|---|---|
     | `DashScanDistance` | Portée du scan du Dash | `Distance in cm in front of the player where the Dash looks for destructibles.` |
     | `ApplyDamage` (fonction, surcharge) | Applique les dégâts reçus | `Applies incoming damage and breaks the wall when health reaches zero.` |

   - **Custom Events à renommer** à la main, s'il y en a.
   - **Ce que je n'ai pas pu placer proprement.**

## 9. Checklist rapide

- [ ] Lecture gauche → droite, fils blancs droits
- [ ] True / Loop Body / Then 0 / Update en haut, sans overlap
- [ ] Getters et `self` collés à leur pin, dupliqués si éloignés
- [ ] Pas de reroute inutile ni superposé, pas de `MakeLiteral` restant
- [ ] Fil rouge court : Custom Event juste sous le binding ou le timer, ou event relais
- [ ] Animation par code : `Play…Anim` → timer 0,016 s en boucle → `…Step` juste dessous → maths dans une fonction
- [ ] BeginPlay court, la logique est dans des fonctions
- [ ] Chaque variable dans une Category, avec une Description en anglais si son effet n'est pas évident
- [ ] Nouvelle variable sur un widget déjà placé : `reset_properties` sur les instances
- [ ] Chaque fonction a une Description en anglais (pas de Comment)
- [ ] Custom Events self-explanatory ; `Anim` dans le nom si Timeline ou animation
- [ ] Composant de base : nouvelle option désactivée par défaut
- [ ] Pas de Comments sur les Blueprints de test temporaires
- [ ] Rapport concis ; Comments à poser (couleur par son nom + titre) ; Descriptions en tableau par Blueprint (Variable / fonction | Effet | Description en boîte de code)

---
*Créé le 2026-11-01 à partir de l'analyse des positions de nodes de `BP_GenericCharacter`, `BP_SoccerPlayer`, `BP_MenuCinematicManager` et `BP_MenuCinematicTransition`. Section 7 ajoutée le 2026-11-01. Mise à jour du 2026-11-02 : leçons des autres conversations du projet (couleurs par leur nom, Descriptions en anglais, fonctions sans Comment, `reset_properties`, composants universels, animation par code, `self` dupliqué, leçons sur le DSL et limites des outils). Mise à jour du 2026-11-13 : format du rapport final (message concis, Descriptions en tableau par Blueprint) et pas de Comments sur les Blueprints de test temporaires. À mettre à jour uniquement dans la conversation dédiée à l'édition du guide.*
