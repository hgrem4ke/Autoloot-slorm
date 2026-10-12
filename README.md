# AutoLoot for The Slormancer

Automatically pulls gold, essence, quest items, materials (slormites, slormandrites, slormelines)
and the elder slorm onto the hero. Equipment can be pulled too, as an option.

## Installation

You need [Aurie Framework](https://github.com/AurieFramework/Aurie) and
[YYToolkit](https://github.com/AurieFramework/YYToolkit) **v5.0.0c** already installed in the game.

Copy `x64/Release/AutoLoot.dll` into `The Slormancer\mods\Aurie\`.

To change the settings with the mouse in game, also install
[ModMenu](https://github.com/hgrem4ke/ModMenu-slorm). Without it, AutoLoot still works, with the
keyboard and the `AutoLoot.ini` file.

## Settings

**In game, with the mouse** (with ModMenu): Options menu → **M** button → **AUTOLOOT** tile.
The page is in English by default; ModMenu's **EN / FR** switch turns it to French.

**In game, with the keyboard**:

| Key | Effect |
|-----|--------|
| F6 | Autoloot on / off |
| F7 | Shorter range |
| F8 | Longer range |
| F9 | Also pull equipment: yes / no |

**Outside the game**: the `AutoLoot.ini` file is created next to the DLL on first launch.

| Setting | Values |
|---------|--------|
| Enabled | 1 = autoloot on, 0 = off |
| Range | range in pixels, 0 = whole room |
| Equipment | 1 = also pulls equipment (you still have to press the key to pick it up) |
| Slormandrites | 1 = pulls slormandrites, 0 = leaves them on the ground |
| ElderSlorm | 1 = pulls the elder slorm (purple orb), 0 = leaves it on the ground |
| Sound | 1 = short beep when a setting is changed with the keyboard |
| Debug | 1 = detailed messages, and Home key to list the loot around the hero |

## Building

Visual Studio 2022 or newer, with "Desktop development with C++". Open `AutoLoot.vcxproj` and build
**Release / x64**. The Aurie, YYToolkit v5.0.0c and ModMenu headers are in `include/`.

## License

AGPL-3.0, see [LICENSE](LICENSE). The `include/` folder contains the Aurie and YYToolkit headers
(AGPL-3.0, AurieFramework).

Unofficial mod, not affiliated with the game's developers. Back up your save before the first try.
