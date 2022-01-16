CFLAGS = -O2 -fPIC -Wall -Wextra -Wpedantic

# change these if you use a different Lua version
LUA_CFLAGS = -I/usr/include/lua5.2
LUA_LIBS = -llua5.2



all: tmt.so pty

tmt.so: lua_tmt.c
	$(CC) -shared -std=gnu99 -o tmt.so $(CFLAGS) $(LUA_CFLAGS) $(LUA_LIBS) -Wno-unused-variable lua_tmt.c tmt.h tmt.c

pty: pty.c
	$(CC) pty.c -o pty $(CFLAGS) -lutil

clean:
	rm tmt.so pty
