#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>
#include <time.h>

#include "tmt.h"

#define LUA_T_PUSH_S_N(S, N) lua_pushstring(L, S); lua_pushnumber(L, N); lua_settable(L, -3);
#define LUA_T_PUSH_S_I(S, N) lua_pushstring(L, S); lua_pushinteger(L, N); lua_settable(L, -3);
#define LUA_T_PUSH_S_B(S, N) lua_pushstring(L, S); lua_pushboolean(L, N); lua_settable(L, -3);
#define LUA_T_PUSH_S_S(S, S2) lua_pushstring(L, S); lua_pushstring(L, S2); lua_settable(L, -3);
#define LUA_T_PUSH_S_CF(S, CF) lua_pushstring(L, S); lua_pushcfunction(L, CF); lua_settable(L, -3);
#define CHECK_TMT(L, I, T) T=(tmt_t *)luaL_checkudata(L, I, "tmt"); if (T==NULL) { lua_pushnil(L); lua_pushfstring(L, "Argument %d must be a TMT", I); return 2; }


#define NS_IN_S 1000000000


typedef struct {
	TMT *vt;
	int w;
	int h;
	int update_lines;
	int update_bell;
	int update_answer;
	int update_cursor;
	char *answer;
} tmt_t;


static int l_write(lua_State *L) {
	struct timespec t;

	tmt_t *tmt;
	CHECK_TMT(L, 1, tmt)
	const char* str = luaL_checkstring(L, 2);

	const TMTPOINT *c = tmt_cursor(tmt->vt);

	tmt_write(tmt->vt, str, 0);

	lua_newtable(L);

	int counter = 1;

	if (clock_gettime(CLOCK_REALTIME, &t) == 0) {
		LUA_T_PUSH_S_N("time", (double)t.tv_sec + ((double)t.tv_nsec)/NS_IN_S)
	}

	if (tmt->update_lines) {
		tmt->update_lines = 0;

		lua_pushinteger(L, counter);
		lua_newtable(L);

		LUA_T_PUSH_S_S("type", "screen")

		lua_settable(L, -3);

		counter = counter + 1;
	}

	if (tmt->update_bell) {
		tmt->update_bell = 0;

		lua_pushinteger(L, counter);
		lua_newtable(L);

		LUA_T_PUSH_S_S("type", "bell")

		lua_settable(L, -3);

		counter = counter + 1;
	}

	if (tmt->update_answer) {
		tmt->update_answer = 0;

		lua_pushinteger(L, counter);
		lua_newtable(L);

		LUA_T_PUSH_S_S("type", "answer")
		LUA_T_PUSH_S_S("answer", tmt->answer)

		free(tmt->answer);
		tmt->answer = NULL;

		lua_settable(L, -3);

		counter = counter + 1;
	}

	if (tmt->update_cursor) {
		tmt->update_cursor = 0;

		lua_pushinteger(L, counter);
		lua_newtable(L);

		LUA_T_PUSH_S_S("type", "cursor")
		LUA_T_PUSH_S_I("x", c->c+1)
		LUA_T_PUSH_S_I("y", c->r+1)
		
		lua_settable(L, -3);

		counter = counter + 1;
	}

	return 1;
}



void insert_char_table(int x, lua_State *L, TMTCHAR c) {
	lua_pushinteger(L, x+1);
	lua_newtable(L);

	LUA_T_PUSH_S_I("char", c.c)
	LUA_T_PUSH_S_I("fg", (int)c.a.fg)
	LUA_T_PUSH_S_I("bg", (int)c.a.bg)

	LUA_T_PUSH_S_B("bold", c.a.bold? 1 : 0)
	LUA_T_PUSH_S_B("dim", c.a.dim? 1 : 0)
	LUA_T_PUSH_S_B("underline", c.a.underline? 1 : 0)
	LUA_T_PUSH_S_B("blink", c.a.blink? 1 : 0)
	LUA_T_PUSH_S_B("reverse", c.a.reverse? 1 : 0)
	LUA_T_PUSH_S_B("invisible", c.a.invisible? 1 : 0)

	lua_settable(L, -3);
}

void insert_lines(lua_State *L, TMT *vt) {
	const TMTSCREEN *s = tmt_screen(vt);
	lua_pushstring(L, "lines");
	lua_newtable(L);
	
	for (uint y=0; y < s->nline; y++) {
		lua_pushinteger(L, y+1);
		lua_newtable(L);

		LUA_T_PUSH_S_B("dirty", s->lines[y]->dirty? 1 : 0)

		for (uint x=0; x < s->ncol; x++) {
			insert_char_table(x, L, s->lines[y]->chars[x]);
		}
		lua_settable(L, -3);
	}
	
	lua_settable(L, -3);
}

static int l_get_screen(lua_State *L) {
	tmt_t *tmt;
	CHECK_TMT(L, 1, tmt)
	TMT *vt = tmt->vt;

	lua_newtable(L);
	
	LUA_T_PUSH_S_I("width", tmt->w);
	LUA_T_PUSH_S_I("height", tmt->h);
	
	insert_lines(L, vt);
	
	return 1;
}



static int l_get_cursor(lua_State *L) {
	tmt_t *tmt;
	CHECK_TMT(L, 1, tmt)
	TMT *vt = tmt->vt;
	const TMTPOINT *c = tmt_cursor(vt);
	
	lua_pushinteger(L, c->c+1);
	lua_pushinteger(L, c->r+1);
	return 2;
}



static int l_set_size(lua_State *L) {
	tmt_t *tmt;
	CHECK_TMT(L, 1, tmt)
	TMT *vt = tmt->vt;

	int w = lua_tointeger(L, 2);
	int h = lua_tointeger(L, 3);

	tmt->w = w;
	tmt->h = h;
	tmt_resize(vt, (size_t)h, (size_t)w);

	return 0;
}



static int l_get_size(lua_State *L) {
	tmt_t *tmt;
	CHECK_TMT(L, 1, tmt);

	lua_pushinteger(L, tmt->w);
	lua_pushinteger(L, tmt->h);

	return 2;
}



void input_callback(tmt_msg_t m, TMT *vt, const void *a, void *p) {
	//lua_State *L = (lua_State*)p;
	tmt_t *tmt = (tmt_t*)p;
	//CHECK_TMT(L, 1, tmt)

	if (tmt != NULL) {
		switch (m){
			case TMT_MSG_BELL:
				tmt->update_bell = 1;
				break;
			case TMT_MSG_UPDATE:
				/* the screen image changed; a is a pointer to the TMTSCREEN */
				tmt->update_lines = 1;
				break;
			case TMT_MSG_ANSWER:
				/* the terminal has a response to give to the program; a is a
				 * pointer to a string */
				tmt->answer = strdup((const char *)a);
				tmt->update_answer = 1;
				break;
			case TMT_MSG_MOVED:
				/* the cursor moved; a is a pointer to the cursor's TMTPOINT */
				tmt->update_cursor = 1;
				break;
			case TMT_MSG_CURSOR:
				break;
		}
	}
}

static int l_new(lua_State *L) {
	tmt_t *tmt = (tmt_t *)lua_newuserdata(L, sizeof(tmt_t));
	int w = lua_tointeger(L, 1);
	int h = lua_tointeger(L, 2);

	TMT *vt = tmt_open((size_t)h, (size_t)w, input_callback, tmt, NULL);

	tmt->vt = vt;

	tmt->w = w;
	tmt->h = h;
	tmt->update_lines = 0;
	tmt->update_bell = 0;
	tmt->update_answer = 0;
	tmt->update_cursor = 0;

	if (luaL_newmetatable(L, "tmt")) {
		lua_pushstring(L, "__index");
		lua_newtable(L);
		LUA_T_PUSH_S_CF("write", l_write)
		LUA_T_PUSH_S_CF("get_screen", l_get_screen)
		LUA_T_PUSH_S_CF("get_cursor", l_get_cursor)
		LUA_T_PUSH_S_CF("get_size", l_get_size)
		LUA_T_PUSH_S_CF("set_size", l_set_size)
		lua_settable(L, -3);
	}

	lua_setmetatable(L, -2);
	return 1;
}



LUALIB_API int luaopen_plugins_tmt_tmt(lua_State *L) {
	lua_newtable(L);
	LUA_T_PUSH_S_CF("new", l_new)

	lua_pushstring(L, "special_keys");
	lua_newtable(L);
	LUA_T_PUSH_S_S("KEY_UP", TMT_KEY_UP)
	LUA_T_PUSH_S_S("KEY_DOWN", TMT_KEY_DOWN)
	LUA_T_PUSH_S_S("KEY_RIGHT", TMT_KEY_RIGHT)
	LUA_T_PUSH_S_S("KEY_LEFT", TMT_KEY_LEFT)
	LUA_T_PUSH_S_S("KEY_HOME", TMT_KEY_HOME)
	LUA_T_PUSH_S_S("KEY_END", TMT_KEY_END)
	LUA_T_PUSH_S_S("KEY_INSERT", TMT_KEY_INSERT)
	LUA_T_PUSH_S_S("KEY_BACKSPACE", TMT_KEY_BACKSPACE)
	LUA_T_PUSH_S_S("KEY_ESCAPE", TMT_KEY_ESCAPE)
	LUA_T_PUSH_S_S("KEY_BACK_TAB", TMT_KEY_BACK_TAB)
	LUA_T_PUSH_S_S("KEY_PAGE_UP", TMT_KEY_PAGE_UP)
	LUA_T_PUSH_S_S("KEY_PAGE_DOWN", TMT_KEY_PAGE_DOWN)
	lua_settable(L, -3);

	return 1;
}
