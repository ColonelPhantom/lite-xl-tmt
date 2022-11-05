# lite-xl-tmt
Terminal emulator for [Lite XL](https://github.com/lite-xl/lite-xl) based on [libtmt](https://github.com/deadpixi/libtmt) (more specifically [lua-tmt](https://github.com/max1220/lua-tmt)).

## Build instructions
Place this repo in `~/.config/lite-xl/plugins`, and rename it to `tmt`. (The full path should look like `~/.config/lite-xl/plugins/tmt/init.lua`, for example.) Then run `make`, or `make mingw` if you are on Windows.

Example:
```sh
cd ~/.config/lite-xl/plugins
git clone https://github.com/ColonelPhantom/lite-xl-tmt tmt
make # or "make mingw" if you use Windows
```

## Caveats
This plugin is not widely tested, use at your own risk.

The main author uses GNU/Linux, so Windows support is provided on a best-effort basis. (Should work, though.)

TMT is a fairly limited terminal emulator, so some advanced terminal programs will not work correctly.

There is no support for scrolling due to TMT limitations.
