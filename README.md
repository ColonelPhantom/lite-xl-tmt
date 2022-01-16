# lite-xl-tmt
Terminal emulator for Lite XL based on [libtmt](https://github.com/deadpixi/libtmt) (more specifically [lua-tmt](https://github.com/max1220/lua-tmt)).

## Build instructions
Place this repo in `~/.config/lite-xl/plugins`. Then run `make`.

You may need to ensure Lite is compiled against the system Lua installation, and perhaps adjust the Lua version that lua-tmt is linked against.

This plugin is only tested so far on the author's system. Note that Lite currently does not officially support native plugins at all.

The plugin is only tested on GNU/Linux. Other UNIX-like systems might work. Windows almost certainly will not.
