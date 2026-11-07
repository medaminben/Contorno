---
title: Messages et problèmes
order: 7
summary: Ce que dit le panneau de messages, et que faire quand le résultat semble faux.
---

## Lignes habituelles {#usual}

| Ligne | Signification |
|---|---|
| cowhide.jpg importé, 1600 x 1200 px. | La photo est affichée ; sa taille en pixels. |
| Caméra : … | D'où vient l'échelle. Voir [D'où vient l'échelle](camera#line). |
| Analyse en cours… | L'analyse tourne ; la barre d'activité avance. |
| 1 objet · 1.411 mm/px · 3.2 s | Le résultat : les objets, l'échelle et le temps pris. |
| Une partie de la peau a été trouvée à partir de la table autour, … | L'IA n'a pris qu'une partie de la peau, par exemple les taches sombres d'une peau tachetée. Le contour a été trouvé à la place à partir de la table autour de la peau. Vérifiez-le avant d'enregistrer. |
| Aucun objet trouvé | Rien sur la photo n'est assez grand. L'afficheur montre 0.0 cm². |
| … enregistré / … ouvert | Le fichier écrit ou lu. |

## Le contour IA est indisponible {#ai}

*Contour IA indisponible : modèle introuvable* — le fichier du modèle IS-Net manque. Le message énumère les endroits où il a été cherché. Dans une compilation depuis les sources, compilez la cible isnet_model ; dans une installation, réinstallez.

*Contour IA indisponible : Vision a été compilé sans LibTorch* — cette copie du programme a été compilée sans l'IA. Elle montre la photo mais ne mesure pas : elle ne se rabat jamais sur une méthode moins exacte sans le dire.

## Le contour est faux {#outline}

- **Il suit une ombre.** Éclairez la table plus uniformément, ou par le dessus.
- **Il entame une partie claire de la peau.** Utilisez une table dont la couleur diffère davantage de celle de la peau.
- **Il laisse de côté une grande partie unie d'une peau bicolore.** L'IA peut prendre une grande zone uniforme, comme le blanc d'une peau de vache, pour la table. Contorno trouve alors la peau à partir de la table autour, et le panneau de messages le dit. Il faut pour cela de la table tout autour de la peau : là où la peau atteint le bord de la photo, le contour de l'IA est gardé. Vérifiez toujours le contour avant d'enregistrer.
- **Il englobe deux peaux.** Deux objets qui se touchent forment un seul objet. Laissez un écart entre eux, ou mesurez-les l'un après l'autre.
- **Il englobe la table à côté d'un tapis.** Un tapis ou un tissu plus petit que la photo montre deux couleurs au bord de la photo, et la table à côté du tapis peut être prise pour de la peau. Utilisez un tapis ou un tissu qui remplit toute la photo, ou aucun.
- **Une marque ou une tache est découpée comme un trou.** Seuls les trous à travers lesquels on voit la table devraient être découpés. Signalez la photo au support.

## La surface est fausse {#surface}

- Vérifiez d'abord la `Distance à la table`. Une erreur de 1 % sur la distance fait une erreur de 2 % sur la surface.
- Vérifiez la `Focale` : la vraie, pas l'équivalent 35 mm.
- Lisez la ligne de caméra : prend-elle les valeurs de la photo ou des réglages, et sont-elles justes ?
- Vérifiez avec un objet de taille connue (voir [Mesurer une peau](measure#check)).

## Le manuel ne s'ouvre pas {#manual}

Le ? cherche ce manuel à côté du programme et l'ouvre dans Chromium ou Google Chrome (Linux) ou Microsoft Edge (Windows), sinon dans le navigateur par défaut. Si aucun n'y parvient, un message donne le fichier du manuel : ouvrez-le dans n'importe quel navigateur.
