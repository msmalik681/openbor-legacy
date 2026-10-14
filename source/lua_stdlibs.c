#include <lauxlib.h>
#include <lualib.h>

/*
 * Open only the Lua standard libraries used by the engine's embedded
 * scripting environment. The legacy engine had no reason to expose the
 * host OS, filesystem I/O, or debug libraries to gameplay scripts.
 *
 * Package/loadlib is retained so existing Lua modules using require() keep
 * working; the engine-specific API is registered separately.
 */
void bor_lua_openlibs(lua_State *L)
{
    luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
    lua_pop(L, 1);

    luaL_requiref(L, LUA_LOADLIBNAME, luaopen_package, 1);
    lua_pop(L, 1);

    luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);
    lua_pop(L, 1);

    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(L, 1);

    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(L, 1);

    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(L, 1);

    luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(L, 1);
}
