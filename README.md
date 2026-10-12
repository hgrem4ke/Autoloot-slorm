# AutoLoot pour The Slormancer

Ramène automatiquement sur le héros l'or, l'essence, les objets de quête, les matériaux
(slormites, slormandrites, slormelines) et le slorm ancien. L'équipement peut aussi être attiré,
en option.

## Installation

Il faut [Aurie Framework](https://github.com/AurieFramework/Aurie) et
[YYToolkit](https://github.com/AurieFramework/YYToolkit) **v5.0.0c** déjà installés dans le jeu.

Copie `x64/Release/AutoLoot.dll` dans `The Slormancer\mods\Aurie\`.

Pour régler le mod à la souris dans le jeu, installe aussi
[ModMenu](https://github.com/hgrem4ke/ModMenu-slorm). Sans lui, AutoLoot fonctionne quand même,
avec le clavier et le fichier `AutoLoot.ini`.

## Réglages

**Dans le jeu, à la souris** (avec ModMenu) : menu Options → bouton **M** → encadré **AUTOLOOT**.

**Dans le jeu, au clavier** :

| Touche | Effet |
|--------|-------|
| F6 | Autoloot activé / désactivé |
| F7 | Portée plus courte |
| F8 | Portée plus longue |
| F9 | Attirer aussi l'équipement : oui / non |

**Hors du jeu** : le fichier `AutoLoot.ini` est créé à côté de la DLL au premier lancement.

| Réglage | Valeurs |
|---------|---------|
| Enabled | 1 = autoloot actif, 0 = désactivé |
| Range | portée en pixels, 0 = toute la salle |
| Equipment | 1 = attire aussi l'équipement (il faut toujours appuyer sur la touche pour le ramasser) |
| Slormandrites | 1 = ramène les slormandrites, 0 = les laisse au sol |
| ElderSlorm | 1 = ramène le slorm ancien (orbe violet), 0 = le laisse au sol |
| Sound | 1 = petit bip quand on change un réglage au clavier |
| Debug | 1 = messages détaillés, et touche Home pour lister le loot autour du héros |

## Compilation

Visual Studio 2022 ou plus récent, avec « Développement Desktop en C++ ». Ouvre `AutoLoot.vcxproj`
et compile en **Release / x64**. Les en-têtes d'Aurie, de YYToolkit v5.0.0c et de ModMenu sont dans
`include/`.

## Licence

AGPL-3.0, voir [LICENSE](LICENSE). Le dossier `include/` contient les en-têtes d'Aurie et de
YYToolkit (AGPL-3.0, AurieFramework).

Mod non officiel, sans lien avec les développeurs du jeu. Fais une sauvegarde de ta partie avant le
premier essai.
