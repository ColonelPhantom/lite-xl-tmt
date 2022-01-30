CFLAGS += -std=gnu99 -O2 -fPIC -Wall -Wextra -Wpedantic

# change these if you use a different Lua version
LUA_CFLAGS = $(CFLAGS) -I/usr/include/lua5.2
LUA_LIBS = -llua5.2

PTY_CFLAGS = $(CFLAGS)
PTY_LIBS = -lutil

SOEXT = so
SONAME = tmt.$(SOEXT)

PTY_SRC = pty.c

all: $(SONAME) pty

$(SONAME): lua_tmt.c tmt.c
	$(CC) -shared -o $@ $(LUA_CFLAGS) $(LUA_LIBS) $^

pty: $(PTY_SRC) minivt.c
	$(CC) -o $@ $(PTY_CFLAGS) $(PTY_LIBS) $^

mingw:
	$(MAKE) PTY_LIBS=-lwinpty SOEXT=dll PTY_SRC=pty.win.c

clean:
	rm -f $(SONAME) pty pty.exe

.PHONY: clean mingw all
