# Vita3K

![C/C++ CI](https://github.com/Vita3K/Vita3K/workflows/C/C++%20CI/badge.svg)
[![Vita3K discord server](https://img.shields.io/discord/408916678911459329?color=5865F2&label=Vita3K%20&logo=discord&logoColor=white)](https://discord.gg/6aGwQzh)

## Introduction

Vita3K is an experimental PlayStation Vita emulator for Windows, Linux and macOS.

* [Website](https://vita3k.org/) (information for users)
* [Wiki](https://github.com/Vita3K/Vita3K/wiki) (information for developers)
* [**Discord**](https://discord.gg/MaWhJVH) (recommended)
* IRC `#vita3k` on **freenode** ([Web-based IRC client](https://webchat.freenode.net/?channels=%23vita3k))
* [Patreon](https://www.patreon.com/Vita3K) (support the project)

## Compatibility

The emulator currently runs some homebrew programs and some decrypted commercial games.

- [Homebrew compatibility page](https://vita3k.org/compatibility-homebrew.html)
- [Commercial compatibility page](https://vita3k.org/compatibility.html)

|               **Persona 4 Golden** by Atlus                   |                     **A Rose in the Twilight** by Nippon Ichi Software                         |
| :-----------------------------------------------------------: | :--------------------------------------------------------------------------------------------: |
| ![Persona 4 Golden screenshot](./_readme/screenshots/P4G.png) | ![A Rose in the Twilight screenshot](./_readme/screenshots/A%20Rose%20in%20the%20Twilight.png) |

|                  **Alone with You** by Benjamin Rivers                     |                 **VA-11 HALL-A** by Sukeban Games                    |
| :------------------------------------------------------------------------: | :------------------------------------------------------------------: |
| ![Alone with You screenshot](./_readme/screenshots/Alone%20With%20You.png) | ![VA-11 HALL-A screenshot](./_readme/screenshots/VA-11%20HALL-A.png) |

|              **Fruit Ninja** by Halfbrick Studios                  |                **Jetpack Joyride** by Halfbrick Studios                    |
| :----------------------------------------------------------------: | :------------------------------------------------------------------------: |
| ![Fruit Ninja Screenshot](./_readme/screenshots/Fruit%20Ninja.png) | ![Jetpack Joyride Screenshot](./_readme/screenshots/Jetpack%20Joyride.png) |

## License

Vita3K is licensed under the **GPLv2** license. This is largely dictated by external dependencies, most notably Unicorn.

## Downloads
* Windows
  * [Scoop](https://scoop.sh/#/apps?q=vita3k&s=0&d=1&o=true)
  * Requirements:
    * [Microsoft Visual C++ 2015-2022 Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe)
* Linux
  * Arch based:
    * [vita3k-bin](https://aur.archlinux.org/packages/vita3k-bin)<sup><small>AUR</small></sup>
    * [vita3k-git](https://aur.archlinux.org/packages/vita3k-git)<sup><small>AUR</small></sup>
  * Requirements:
    * xdg-desktop-portal
* Others
  * [Download Artifact](https://github.com/Vita3K/Vita3K/actions?query=event%3Apush+is%3Asuccess+branch%3Amaster)

## Building

Please see [`building.md`](./building.md).

## Running
Specify the path to a .vpk file as the first command line argument, or run `Vita3K --help` from the command-line for a full list of options.
For more detailed instructions on running/installing games on all platforms, please read the **#info-faq** channel on our [Discord Server](https://discord.gg/MaWhJVH).

## Bugs and issues
The project is in an early stage, so please be mindful when opening new issues. Most games will have some problems, whether that be crashes, glitches, low compatibility or poor performance.

## Thanks
Thanks to those who offered advice or otherwise made this project possible, such as Davee, korruptor, Rinnegatamante, ScHlAuChi, Simon Kilroy, TheFlow, xerpi, xyz, Yifan Lu and many others.

## Donations
If you would like to show your appreciation or help fund development, the project has a [Patreon](https://www.patreon.com/Vita3K) page.

## Supporters
Thank you to the following supporters:
* Mored1984
* soiaf

If you support us on Patreon and would like your name added, please get in touch or open a Pull Request.

## Note
The purpose of this emulator is **not** to enable illegal activity. You can dump games by using [NoNpDrm](https://github.com/TheOfficialFloW/NoNpDrm)/[FAGDec](https://github.com/CelesteBlue-dev/PSVita-RE-tools/tree/master/FAGDec/build). Homebrew programs can be found at [VitaDB](https://vitadb.rinnegatamante.it/).

PlayStation, PlayStation Vita and PS Vita are trademarks of Sony Interactive Entertainment Inc. This emulator is not related to or endorsed by Sony, or derived from confidential materials belonging to Sony.
