<div align="center">
  <img src="https://avatars.githubusercontent.com/u/329755507?s=200&v=4" alt="QSWM" width="120" height="120">

  # QSWM

  Minimalist window manager for Xorg

  [![License: BSD 2-Clause](https://img.shields.io/badge/License-BSD%202--Clause-blue.svg)](./LICENSE)

  [![Русский](https://img.shields.io/badge/lang-Русский-lightgrey)](README.md)
  [![English](https://img.shields.io/badge/lang-English-blue)](README.en.md)
</div>

## About

QSWM is a minimalist window manager for Xorg that combines the core functionality of openbox with a smaller codebase. Supported platforms: Linux, FreeBSD.

## Features

**Window management**
 - Half-screen window: `Win+←` / `Win+→` / `Win+↑` / `Win+↓`
 - Two adjacent arrows for a quarter of the screen: `Win+←+↑`, `Win+←+↓`, `Win+→+↑`, `Win+→+↓`
 - `Win+Tab` - minimize all windows at once
 - `Ctrl+Tab` - unsnap window
 - `Alt+Tab` - switch windows

**Hotkeys**
[hotkeys] - 6 built-in actions, one binding per action (a repeated line overrides):
 - mod - modifier for the section (Alt by default)
 - altTab - window switcher overlay
 - close - close window
 - maximize - maximize/restore
 - minimize - minimize
 - launcher - run launcher_cmd from [general]
 - fullscreen - fullscreen mode
[exec] - arbitrary commands on any key combinations.
Example:
```
mods+key = command
```

**Appearance settings: windows, window switching**
`[appearance]`
- `titlebar_height` - window title bar height, px. Range 8–200
- `button_size` - title bar button size (−/□/×), px. Range 4–128
- `icon_left` - application icon to the left of the title bar buttons: `true`/`false`
- `alttab_position` - vertical position of the Alt+Tab overlay: `top`/`bottom`/`center`
- `alttab_icon_size` - cell/icon size in the Alt+Tab overlay, px. Range 16–256
- `titlebar_color` - title bar color
- `alttab_bg_color` - Alt+Tab overlay background color
`[icons]`
- `dir` - directory with application icons.


Startup commands are set in `~/.config/QSWM/autostart`
Everything is configured in `~/.config/QSWM/config.conf` after the first run.

## Build / Run

**FreeBSD**
```
sudo pkg install xcb-util-keysyms xcb-util-cursor xcb-util-renderutil pkgconf gmake
gmake

./QSWM
```

**Ubuntu/Debian**
```
sudo apt install build-essential pkg-config libxcb1-dev libxcb-keysyms1-dev libxcb-cursor-dev libxcb-render0-dev
make

./QSWM
```

## Technologies

- C11, XCB

## License

Distributed under the terms of the BSD 2-Clause license, as stated
in the file [LICENSE](./LICENSE).

```
Copyright (c) 2026, QSBSD Contributors
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
