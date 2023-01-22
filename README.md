# lite-xl-tmt
Terminal emulator for [Lite XL](https://github.com/lite-xl/lite-xl) based on [libtmt](https://github.com/deadpixi/libtmt) (more specifically [lua-tmt](https://github.com/max1220/lua-tmt)).

## Installation instructions
- Head over to the [latest release](https://github.com/ColonelPhantom/lite-xl-tmt/releases/latest) page.
- Download the appropriate zip/tar file for your platform (only Windows and Linux, and 64 bit only).
- Then place the `tmt` folder that is in the archive into your plugins folder.
- You should now have a Lua file at `$LITE_XL_USERDIR/plugins/tmt/init.lua`
- Simply restart Lite XL and hit Ctrl+\` to get a terminal!

Note that this plugin only works on Lite XL 2.1.1 and higher.

## Building from source
Place this repo in `~/.config/lite-xl/plugins`, and rename it to `tmt`. (The full path should look like `~/.config/lite-xl/plugins/tmt/init.lua`, for example.) Then run `make`, or `make mingw` if you are on Windows.

Example:
```sh
git clone https://github.com/ColonelPhantom/lite-xl-tmt tmt
make # or "make mingw" if you use Windows
```

Note: the repo contains some binaries for WinPTY stuff, you may need to replace or rebuild those depending on your needs (e.g. 32 bit support).

## Caveats
This plugin is not widely tested, use at your own risk.

The main author uses GNU/Linux, so Windows support is provided on a best-effort basis. (Should work, though.)

TMT is a fairly limited terminal emulator, so some advanced terminal programs will not work correctly.

There is no support for scrolling due to TMT limitations.
