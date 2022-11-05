CFLAGS += -std=gnu99 -O2 -fPIC

PTY_LIBS = -lutil

SOEXT = so
SONAME = tmt.$(SOEXT)

PTY_SRC = pty.c

all: $(SONAME) pty

$(SONAME): lua_tmt.c tmt.c
	$(CC) -shared -o $@ $(CFLAGS) $^

pty: $(PTY_SRC) minivt.c
	$(CC) -o $@ $(CFLAGS) $(PTY_LIBS) $^

mingw:
	$(MAKE) PTY_LIBS=-lwinpty SOEXT=dll PTY_SRC=pty.win.c

clean:
	rm -f $(SONAME) pty pty.exe

.PHONY: clean mingw all
