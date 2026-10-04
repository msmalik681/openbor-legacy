#ifndef LUABINDINGS_H
#define LUABINDINGS_H

#include <stddef.h>
#include <lua.h>

// Forward declaration signature
void openbor_register_lua_api(void);

/* Execute Lua code through the common protected boundary. */
int bor_lua_pcall(lua_State *L, int nargs, int nresults, const char *context);
int bor_lua_runbuffer(lua_State *L, const char *buffer, size_t size, const char *source);

#endif