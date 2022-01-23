#include <string.h>
#include <stdlib.h>
#include <locale.h>

#ifdef __WIN32
#include <windows.h>
#endif

#include <lua.h>
#include <lauxlib.h>

#include "tmt.h"


#define LUA_T_PUSH_S_I(S, N) (lua_pushinteger(L, N), lua_setfield(L, -2, S))
#define LUA_T_PUSH_S_B(S, N) (lua_pushboolean(L, N), lua_setfield(L, -2, S))
#define LUA_T_PUSH_S_S(K, V) (lua_pushstring(L, V), lua_setfield(L, -2, K))


#define API_TYPE_TMT "tmt"

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



// nobody likes wchar_t
static int wchar_convert(wchar_t c, char *s) {
#ifndef __WIN32
	return wctomb(s, c);
#else
	int ret = WideCharToMultiByte(CP_UTF8, 0, &c, 1, s, 4, NULL, NULL);
	return ret == 0 ? -1 : ret;
#endif
}



static int l_write(lua_State *L) {
	tmt_t *tmt = luaL_checkudata(L, 1, API_TYPE_TMT);
	const char* str = luaL_checkstring(L, 2);

	const TMTPOINT *c = tmt_cursor(tmt->vt);

	tmt_write(tmt->vt, str, 0);

	lua_newtable(L);
	LUA_T_PUSH_S_B("screen", tmt->update_lines);
	LUA_T_PUSH_S_B("bell", tmt->update_bell);
	if (tmt->update_answer) {
		LUA_T_PUSH_S_S("answer", tmt->answer);
		free(tmt->answer);
		tmt->answer = NULL;
	}

	if (tmt->update_cursor) {
		lua_newtable(L);
		LUA_T_PUSH_S_I("x", c->c + 1);
		LUA_T_PUSH_S_I("y", c->r + 1);
		lua_setfield(L, -2, "cursor");
	}

	tmt->update_lines = 0;
	tmt->update_bell = 0;
	tmt->update_answer = 0;
	tmt->update_cursor = 0;

	return 1;
}



void insert_char_table(lua_State *L, int x, TMTCHAR c) {
	lua_rawgeti(L, -1, x+1);

#ifdef __WIN32
	char cp[4];
#else
	char cp[MB_CUR_MAX];
#endif
	int sz = wchar_convert(c.c, cp);
	if (sz == -1)
		lua_pushliteral(L, " ");
	else
		lua_pushlstring(L, cp, sz);
	lua_setfield(L, -2, "char");

	LUA_T_PUSH_S_I("fg", (int)c.a.fg);
	LUA_T_PUSH_S_I("bg", (int)c.a.bg);

	LUA_T_PUSH_S_B("bold", c.a.bold);
	LUA_T_PUSH_S_B("dim", c.a.dim);
	LUA_T_PUSH_S_B("underline", c.a.underline);
	LUA_T_PUSH_S_B("blink", c.a.blink);
	LUA_T_PUSH_S_B("reverse", c.a.reverse);
	LUA_T_PUSH_S_B("invisible", c.a.invisible);

	lua_pop(L, 1);
}



static int l_get_screen(lua_State *L) {
	tmt_t *tmt = luaL_checkudata(L, 1, API_TYPE_TMT);
	if (!lua_istable(L, 2))
		lua_createtable(L, tmt->w * tmt->h, 2);

	lua_settop(L, 2);

	const TMTSCREEN *s = tmt_screen(tmt->vt);
	tmt->w = s->ncol;
	tmt->h = s->nline;

	LUA_T_PUSH_S_I("width", tmt->w);
	LUA_T_PUSH_S_I("height", tmt->h);

	// ensure enough objects for later
	int l = tmt->w * tmt->h;
	if (lua_rawlen(L, -1) < l) {
		for (int i = lua_rawlen(L, -1) + 1; i <= l; i++) {
			lua_newtable(L);
			lua_rawseti(L, -2, i);
		}
	}

	for (size_t y = 0; y < s->nline; y++) {
		if (!s->lines[y]->dirty) continue;
		for (size_t x = 0; x < s->ncol; x++) {
			insert_char_table(L, (y * s->ncol) + x, s->lines[y]->chars[x]);
		}
	}

	tmt_clean(tmt->vt); // reset dirty flags
	return 1;
}



static int l_get_cursor(lua_State *L) {
	tmt_t *tmt = luaL_checkudata(L, 1, API_TYPE_TMT);
	const TMTPOINT *c = tmt_cursor(tmt->vt);
	
	lua_pushinteger(L, c->c+1);
	lua_pushinteger(L, c->r+1);
	return 2;
}



static int l_set_size(lua_State *L) {
	tmt_t *tmt = luaL_checkudata(L, 1, API_TYPE_TMT);
	int w = lua_tointeger(L, 2);
	int h = lua_tointeger(L, 3);

	tmt->w = w;
	tmt->h = h;
	tmt_resize(tmt->vt, (size_t)h, (size_t)w);

	return 0;
}



static int l_get_size(lua_State *L) {
	tmt_t *tmt = luaL_checkudata(L, 1, API_TYPE_TMT);

	lua_pushinteger(L, tmt->w);
	lua_pushinteger(L, tmt->h);

	return 2;
}



void input_callback(tmt_msg_t m, TMT *vt, const void *a, void *p) {
	tmt_t *tmt = (tmt_t*)p;
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
			if (tmt->answer != NULL)
				free(tmt->answer);
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



static int l_new(lua_State *L) {
	int w = luaL_optinteger(L, 1, 80);
	int h = luaL_optinteger(L, 2, 24);

	tmt_t *tmt = (tmt_t *)lua_newuserdata(L, sizeof(tmt_t));
	luaL_setmetatable(L, API_TYPE_TMT);

	TMT *vt = tmt_open((size_t)h, (size_t)w, input_callback, tmt, NULL); 
	if (vt == NULL)
		luaL_error(L, "cannot create virtual terminal");

	tmt->vt = vt;

	tmt->w = w;
	tmt->h = h;
	tmt->update_lines = 0;
	tmt->update_bell = 0;
	tmt->update_answer = 0;
	tmt->update_cursor = 0;
	tmt->answer = NULL;

	return 1;
}



static int l_gc(lua_State *L) {
	tmt_t *tmt = luaL_checkudata(L, 1, API_TYPE_TMT);
	if (tmt->vt != NULL) {
		tmt_close(tmt->vt);
		tmt->vt = NULL;
	}
	if (tmt->answer != NULL) {
		free(tmt->answer);
		tmt->answer = NULL;
	}
	return 0;
}



static const luaL_Reg lib[] = {
	{ "new", l_new },
	{ "write", l_write },
	{ "get_screen", l_get_screen },
	{ "get_cursor", l_get_cursor },
	{ "get_size", l_get_size },
	{ "set_size", l_set_size },
	{ "__gc", l_gc },
	{ NULL, NULL }
};



int luaopen_tmt(lua_State *L) {
	setlocale(LC_CTYPE, "en_US.UTF-8");
	luaL_newmetatable(L, API_TYPE_TMT);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	luaL_setfuncs(L, lib, 0);

	return 1;
}
