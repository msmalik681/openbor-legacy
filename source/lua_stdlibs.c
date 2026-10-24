#include <lauxlib.h>
#include <lualib.h>

/*
 * Open the Lua standard libraries retained for the engine's embedded
 * scripting environment. Package/require, coroutine, UTF-8 helpers, host
 * filesystem I/O, OS, and debug libraries are omitted for this experiment.
 * The engine-specific API is registered separately.
 */
void bor_lua_openlibs(lua_State *L)
{
    luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
    lua_pop(L, 1);

    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(L, 1);

    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(L, 1);

    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(L, 1);
}
