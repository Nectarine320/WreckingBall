# PC de benchmark (machine de référence)

Machine de l'utilisateur, relevée le 2026-11-13 (Gestionnaire des tâches). C'est la référence des mesures de performance du projet. Le jeu sort sur PC. Cible de **60 images par seconde**, plancher de **30** dans le pire cas.

## Matériel
| Pièce | Détail |
| --- | --- |
| CPU | Intel Core i9-9900 @ 3,10 GHz (turbo observé à 4,58 GHz), 8 cœurs / 16 threads, L3 de 16 Mo |
| GPU | NVIDIA GeForce RTX 2080, 8 Go de VRAM, pilote 32.0.15.9144 (2025-12-02), DirectX 12 (FL 12.2) |
| GPU intégré | Intel UHD Graphics (à ne pas utiliser pour Unreal) |
| RAM | 32 Go DDR4 à 2666 MT/s, 2 barrettes sur 4 emplacements |
| Disques | C : NVMe ; D : NVMe (le projet est sur D) ; G : SSD SATA ; E : USB |
| Résolution de test | **2560 × 1440 (1440p)** |

## Remarques au relevé
- **Mémoire déjà occupée à 78 à 79 %** (environ 25 Go sur 32), et 5,2 Go de VRAM utilisés au repos. C'est sûrement l'éditeur Unreal ouvert avec d'autres applications. Pour les mesures, fermer tout ce qui n'est pas nécessaire, et noter la mémoire utilisée au moment du test.
- Vérifier qu'Unreal tourne bien sur la RTX 2080 et non sur le GPU Intel : la commande `stat rhi` ou le log au démarrage affiche le nom de l'adaptateur.
- En 1440p, une RTX 2080 est plus souvent limitée par le GPU. Si les images par seconde baissent alors que le temps Game reste bas, la cause est le rendu (le coût des GC, par exemple) et pas la physique. `stat unit` permet de faire la différence.

## Protocole de mesure
- Mode **Standalone Game** (pas PIE), en 1440p, avec le même réglage de qualité à chaque mesure.
- Commandes : `stat fps`, `stat unit` (Game, Draw, GPU), `stat gpu`, `stat scenerendering`, stats Chaos ; `wb.Debris.Stats` (lot 9).
- Pour chaque mesure, noter la date, la carte, le prefab, le nombre de joueurs, le nombre de murs effondrés, les morceaux actifs, les props simulés, les images par seconde (moyenne et minimum) et le temps Game / GPU.

## Résultats
| Date | Scène | Joueurs | Murs effondrés | Morceaux actifs | Props simulés | Images/s moy. / min | Game ms | GPU ms | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
