# lite-xl-tmt
Terminal emulator for [Lite XL](https://github.com/lite-xl/lite-xl) based on [libtmt](https://github.com/deadpixi/libtmt) (more specifically [lua-tmt](https://github.com/max1220/lua-tmt)).

## Build instructions
Place this repo in `~/.config/lite-xl/plugins`, and rename it to `tmt`. (The full path should look like `~/.config/lite-xl/plugins/tmt/init.lua`, for example.) Then run `make`.

You may need to ensure Lite is compiled against the system Lua installation, and perhaps adjust the Lua version that lua-tmt is linked against.

## Caveats
This plugin is only tested so far on the author's system. Note that Lite XL currently does not officially support native plugins at all.

The plugin is only tested on GNU/Linux. Other UNIX-like systems might work. Windows almost certainly will not.

Resizing the terminal does not work well, it is necessary to run `resize` inside the terminal after resizing in order to inform client software.

There is no support for scrolling.
