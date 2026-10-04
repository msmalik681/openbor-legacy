#include "openbor.h"
#include "data.h"
#include "openborscript.h"
#include "luabindings.h"
#include "soundmix.h"
#include "crashhandler.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>
/*
 * Common Lua -> C execution boundary.
 *
 * Lua errors are recoverable here. Native faults inside the C engine are
 * not caught by lua_pcall() and must be handled by the platform crash layer.
 */
static void bor_lua_log_error(lua_State *L, const char *context)
{
    const char *message = lua_tostring(L, -1);

    if (!message)
        message = "(non-string Lua error object)";

    printf("[LUA ERROR] %s: %s\n",
           context ? context : "unknown",
           message);

    /* Include the Lua call stack so the failing script/function is visible. */
    luaL_traceback(L, L, message, 1);
    if (lua_isstring(L, -1))
        printf("[LUA TRACEBACK] %s\n", lua_tostring(L, -1));

    lua_pop(L, 1);
    fflush(stdout);
}

int bor_lua_pcall(lua_State *L, int nargs, int nresults, const char *context)
{
    int status;

    if (!L)
        return LUA_ERRRUN;

    status = lua_pcall(L, nargs, nresults, 0);

    if (status != LUA_OK)
        bor_lua_log_error(L, context);

    return status;
}

int bor_lua_runbuffer(lua_State *L, const char *buffer, size_t size,
                      const char *source)
{
    int status;

    if (!L || !buffer)
        return LUA_ERRRUN;

    status = luaL_loadbufferx(L, buffer, size,
                             source ? source : "OpenBOR Lua buffer",
                             NULL);

    if (status != LUA_OK)
    {
        bor_lua_log_error(L, source ? source : "Lua compile");
        return status;
    }

    return bor_lua_pcall(L, 0, 0,
                         source ? source : "Lua chunk");
}



// Bring in the global execution pointer declared in openbor.c
extern lua_State *g_lua_engine_state;
extern s_level *level;
extern entity *self;
extern u32 borTime;

// openbor.log("message")
static int lua_openbor_log(lua_State *L) {
    const char *message = luaL_checkstring(L, 1);
    printf("%s", message);
    return 0;
}

// openbor.get_time()
static int lua_openbor_get_time(lua_State *L) {
    lua_pushinteger(L, (lua_Integer)borTime);
    return 1;
}

// openbor.get_player_health()
static int lua_openbor_get_player_health(lua_State *L) {
    extern s_player player[4];
    if (player[0].ent && player[0].ent->exists) {
        lua_pushinteger(L, (lua_Integer)player[0].ent->health);
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

// openbor.rand()
static int lua_openbor_rand(lua_State *L) {
    lua_pushinteger(L, (lua_Integer)rand32());
    return 1;
}

// openbor.drawstring(x, y, font, text, z)
static int lua_openbor_drawstring(lua_State *L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int font = (int)luaL_checkinteger(L, 3);
    const char *text = luaL_checkstring(L, 4);
    int z = (int)luaL_optinteger(L, 5, 0);

    font_printf(x, y, font, z, "%s", text);
    return 0;
}

// openbor.drawbox(x, y, width, height, z, color, lut)
static int lua_openbor_drawbox(lua_State *L) {
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int w = (int)luaL_checkinteger(L, 3);
    int h = (int)luaL_checkinteger(L, 4);
    int z = (int)luaL_checkinteger(L, 5);
    int color = (int)luaL_checkinteger(L, 6);
    int lut_index = (int)luaL_optinteger(L, 7, -1); 

    if (lut_index >= 0) {
        lut_index %= MAX_BLENDINGS + 1;
    }
    spriteq_add_box(x, y, w, h, z, color, lut_index);
    return 0;
}

/* A Lua lightuserdata value can become stale after an entity is destroyed.
 * Never dereference it until it has been found in the live entity table. */
static entity *bor_lua_get_live_entity_ptr(entity *candidate)
{
    extern entity *ent_list[MAX_ENTS];
    int i;

    if (!candidate)
        return NULL;

    for (i = 0; i < MAX_ENTS; ++i)
    {
        if (ent_list[i] == candidate)
            return candidate;
    }

    return NULL;
}

static entity *bor_lua_get_live_entity(lua_State *L, int index)
{
    if (!lua_islightuserdata(L, index) && !lua_isuserdata(L, index))
        return NULL;

    return bor_lua_get_live_entity_ptr((entity *)lua_touserdata(L, index));
}

// openbor.killentity(entity_pointer)
static int lua_openbor_killentity(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    if (ent && ent->exists) {
        kill(ent);
        lua_pushboolean(L, 1);
    } else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

// openbor.getentity(index)
static int lua_openbor_getentity(lua_State *L) {
    int idx = (int)luaL_checkinteger(L, 1);
    extern entity *ent_list[MAX_ENTS];

    if (idx >= 0 && idx < MAX_ENTS && ent_list[idx]) {
        lua_pushlightuserdata(L, (void *)ent_list[idx]);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// openbor.damageentity(target_ent, attacker_ent, force, drop, type)
static int lua_openbor_damageentity(lua_State *L) {
    // 1. Fetch and validate target entity
    entity *ent = bor_lua_get_live_entity(L, 1);
    if (!ent || !ent->exists) {
        lua_pushboolean(L, 0);
        return 1;
    }

    // 2. Fetch attacker entity (defaults to target if absent or null)
    entity *other = NULL;
    if (lua_isuserdata(L, 2)) {
        other = bor_lua_get_live_entity(L, 2);
    }
    if (!other) {
        other = ent;
    }

    // 3. Process optional combat parameters with precise defaults
    int force = (int)luaL_optinteger(L, 3, 1);
    int drop = (int)luaL_optinteger(L, 4, 0);
    int type = (int)luaL_optinteger(L, 5, ATK_NORMAL);
    
    entity *temp = self;
    self = ent;
    s_attack attack = emptyattack;
    attack.attack_force = force;
    attack.attack_drop  = drop;
    attack.attack_type  = type;
    
    if (drop) {
        attack.dropv[0] = 3.0f;
        attack.dropv[1] = 1.2f;
        attack.dropv[2] = 0.0f;
    }
    
    self->takedamage(other, &attack);
    self = temp;
    lua_pushboolean(L, 1);
    return 1;
}

// openbor.getEntityProperty(entity, property)
// Lightweight compatibility layer for the legacy getentityproperty() API.
static int lua_openbor_getentityproperty(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    const char *prop;

    if (!ent || !ent->exists) {
        lua_pushnil(L);
        return 1;
    }

    prop = luaL_checkstring(L, 2);

    if (!strcmp(prop, "health"))              lua_pushinteger(L, ent->health);
    else if (!strcmp(prop, "mp"))             lua_pushinteger(L, ent->mp);
    else if (!strcmp(prop, "maxhealth"))      lua_pushinteger(L, ent->modeldata.health);
    else if (!strcmp(prop, "maxmp"))          lua_pushinteger(L, ent->modeldata.mp);
    else if (!strcmp(prop, "x"))              lua_pushnumber(L, ent->x);
    else if (!strcmp(prop, "z"))              lua_pushnumber(L, ent->z);
    else if (!strcmp(prop, "a"))              lua_pushnumber(L, ent->a);
    else if (!strcmp(prop, "xdir"))           lua_pushnumber(L, ent->xdir);
    else if (!strcmp(prop, "zdir"))           lua_pushnumber(L, ent->zdir);
    else if (!strcmp(prop, "tossv"))           lua_pushnumber(L, ent->tossv);
    else if (!strcmp(prop, "base"))            lua_pushnumber(L, ent->base);
    else if (!strcmp(prop, "jumpz"))           lua_pushnumber(L, ent->jumpz);
    else if (!strcmp(prop, "jumpx"))           lua_pushnumber(L, ent->jumpx);
    else if (!strcmp(prop, "jumpv"))           lua_pushnumber(L, ent->jumpv);
    else if (!strcmp(prop, "direction"))       lua_pushinteger(L, ent->direction);
    else if (!strcmp(prop, "exists"))          lua_pushboolean(L, ent->exists);
    else if (!strcmp(prop, "dead"))            lua_pushboolean(L, ent->dead);
    else if (!strcmp(prop, "dying"))           lua_pushboolean(L, ent->dying);
    else if (!strcmp(prop, "attacking"))       lua_pushboolean(L, ent->attacking);
    else if (!strcmp(prop, "blocking"))        lua_pushboolean(L, ent->blocking);
    else if (!strcmp(prop, "falling"))         lua_pushboolean(L, ent->falling);
    else if (!strcmp(prop, "running"))         lua_pushboolean(L, ent->running);
    else if (!strcmp(prop, "frozen"))          lua_pushboolean(L, ent->frozen);
    else if (!strcmp(prop, "invincible"))      lua_pushboolean(L, ent->invincible);
    else if (!strcmp(prop, "projectile"))      lua_pushboolean(L, ent->projectile);
    else if (!strcmp(prop, "autokill"))        lua_pushboolean(L, ent->autokill);
    else if (!strcmp(prop, "seal"))            lua_pushboolean(L, ent->seal);
    else if (!strcmp(prop, "name"))            lua_pushstring(L, ent->name);
    else if (!strcmp(prop, "model"))           lua_pushstring(L, ent->model ? ent->model->name : "");
    else if (!strcmp(prop, "defaultmodel"))    lua_pushstring(L, ent->defaultmodel ? ent->defaultmodel->name : "");
    else if (!strcmp(prop, "spawntype"))        lua_pushinteger(L, ent->spawntype);
    else if (!strcmp(prop, "playerindex"))     lua_pushinteger(L, ent->playerindex);
    else if (!strcmp(prop, "map"))             lua_pushinteger(L, ent->map);
    else if (!strcmp(prop, "animnum"))         lua_pushinteger(L, ent->animnum);
    else if (!strcmp(prop, "animpos"))         lua_pushinteger(L, ent->animpos);
    else if (!strcmp(prop, "nextanim"))        lua_pushinteger(L, ent->nextanim);
    else if (!strcmp(prop, "nextthink"))       lua_pushinteger(L, ent->nextthink);
    else if (!strcmp(prop, "attackid"))        lua_pushinteger(L, ent->attack_id);
    else if (!strcmp(prop, "hitbyid"))         lua_pushinteger(L, ent->hit_by_attack_id);
    else if (!strcmp(prop, "speed"))           lua_pushnumber(L, ent->modeldata.speed);
    else if (!strcmp(prop, "height"))          lua_pushinteger(L, ent->modeldata.height);
    else if (!strcmp(prop, "alpha"))           lua_pushinteger(L, ent->modeldata.alpha);
    else if (!strcmp(prop, "setlayer"))        lua_pushinteger(L, ent->modeldata.setlayer);
    else if (!strcmp(prop, "mprate"))          lua_pushinteger(L, ent->modeldata.mprate);
    else if (!strcmp(prop, "mpdroprate"))      lua_pushinteger(L, ent->modeldata.mpdroprate);
    else if (!strcmp(prop, "chargerate"))      lua_pushinteger(L, ent->modeldata.chargerate);
    else if (!strcmp(prop, "guardpoints"))     lua_pushinteger(L, ent->modeldata.guardpoints[0]);
    else if (!strcmp(prop, "maxguardpoints"))  lua_pushinteger(L, ent->modeldata.guardpoints[1]);
    else if (!strcmp(prop, "jugglepoints"))    lua_pushinteger(L, ent->modeldata.jugglepoints[0]);
    else if (!strcmp(prop, "maxjugglepoints")) lua_pushinteger(L, ent->modeldata.jugglepoints[1]);
    else if (!strcmp(prop, "player"))           lua_pushinteger(L, ent->playerindex);
    else if (!strcmp(prop, "owner")) {
        if (ent->owner && ent->owner->exists) lua_pushlightuserdata(L, ent->owner); else lua_pushnil(L);
    }
    else if (!strcmp(prop, "parent")) {
        if (ent->parent && ent->parent->exists) lua_pushlightuserdata(L, ent->parent); else lua_pushnil(L);
    }
    else if (!strcmp(prop, "subentity")) {
        if (ent->subentity && ent->subentity->exists) lua_pushlightuserdata(L, ent->subentity); else lua_pushnil(L);
    }
    else if (!strcmp(prop, "opponent")) {
        if (ent->opponent && ent->opponent->exists) lua_pushlightuserdata(L, ent->opponent); else lua_pushnil(L);
    }
    else if (!strcmp(prop, "link")) {
        if (ent->link && ent->link->exists) lua_pushlightuserdata(L, ent->link); else lua_pushnil(L);
    }
    else {
        return luaL_error(L, "unknown entity property '%s'", prop);
    }

    return 1;
}

// openbor.setEntityProperty(entity, property, value)
static int lua_openbor_setentityproperty(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    const char *prop;

    if (!ent || !ent->exists) {
        lua_pushboolean(L, 0);
        return 1;
    }

    prop = luaL_checkstring(L, 2);

    if (!strcmp(prop, "health"))           ent->health = (int)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "mp"))          ent->mp = (int)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "x"))           ent->x = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "z"))           ent->z = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "a"))           ent->a = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "xdir"))        ent->xdir = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "zdir"))        ent->zdir = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "tossv"))       ent->tossv = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "base"))        ent->base = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "jumpz"))       ent->jumpz = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "jumpx"))       ent->jumpx = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "jumpv"))       ent->jumpv = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "direction"))   ent->direction = (char)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "map"))         ent->map = (char)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "nextanim"))    ent->nextanim = (unsigned int)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "nextthink"))   ent->nextthink = (unsigned int)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "autokill"))    ent->autokill = (char)lua_toboolean(L, 3);
    else if (!strcmp(prop, "invincible"))  ent->invincible = (char)lua_toboolean(L, 3);
    else if (!strcmp(prop, "frozen"))      ent->frozen = (char)lua_toboolean(L, 3);
    else if (!strcmp(prop, "seal"))        ent->seal = (char)lua_toboolean(L, 3);
    else {
        return luaL_error(L, "entity property '%s' is read-only or unsupported", prop);
    }

    lua_pushboolean(L, 1);
    return 1;
}

// openbor.isEntityValid(entity)
// Lightweight validity check for the light-userdata entity handle.
static int lua_openbor_isentityvalid(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    lua_pushboolean(L, ent && ent->exists);
    return 1;
}

// openbor.isEntityAlive(entity)
static int lua_openbor_isentityalive(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    lua_pushboolean(L, ent && ent->exists && !ent->dead);
    return 1;
}

// openbor.getEntityHealth(entity)
static int lua_openbor_getentityhealth(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    if (!ent || !ent->exists) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, (lua_Integer)ent->health);
    return 1;
}

// openbor.getEntityTarget(entity)
// OpenBOR's built-in target relationship is represented by entity->opponent.
static int lua_openbor_getentitytarget(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    if (!ent || !ent->exists || !ent->opponent || !bor_lua_get_live_entity_ptr(ent->opponent) || !ent->opponent->exists) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlightuserdata(L, (void *)ent->opponent);
    return 1;
}

// openbor.setEntityTarget(entity, target)
static int lua_openbor_setentitytarget(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    entity *target = NULL;

    if (!ent || !ent->exists) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!lua_isnoneornil(L, 2)) {
        target = bor_lua_get_live_entity(L, 2);
        if (!target || !target->exists) {
            return luaL_error(L, "invalid target entity");
        }
    }

    ent->opponent = target;
    lua_pushboolean(L, 1);
    return 1;
}

// openbor.getEntityPosition(entity) -> x, z, a
// OpenBOR uses x/z for ground position and a for altitude.
static int lua_openbor_getentityposition(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    if (!ent || !ent->exists) {
        return luaL_error(L, "invalid entity");
    }

    lua_pushnumber(L, (lua_Number)ent->x);
    lua_pushnumber(L, (lua_Number)ent->z);
    lua_pushnumber(L, (lua_Number)ent->a);
    return 3;
}

// openbor.setEntityPosition(entity, x, z, a)
static int lua_openbor_setentityposition(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    if (!ent || !ent->exists) {
        lua_pushboolean(L, 0);
        return 1;
    }

    ent->x = (float)luaL_checknumber(L, 2);
    ent->z = (float)luaL_checknumber(L, 3);
    ent->a = (float)luaL_checknumber(L, 4);

    lua_pushboolean(L, 1);
    return 1;
}

// openbor.changeAnimation(entity, animation, resetable)
static int lua_openbor_changeanimation(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    int animation = (int)luaL_checkinteger(L, 2);
    int resetable = (int)luaL_optinteger(L, 3, 1);

    if (!ent || !ent->exists) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (animation < 0 || animation >= dyn_anim_custom_maxvalues.max_animations ||
        !validanim(ent, animation)) {
        return luaL_error(L, "invalid animation %d for entity", animation);
    }

    ent_set_anim(ent, animation, resetable);
    lua_pushboolean(L, 1);
    return 1;
}

// openbor.spawn(model, x, z, a, direction)
// The public Lua API uses a model name; x/z/a match OpenBOR's native spawn().
static int lua_openbor_spawn(lua_State *L) {
    const char *model = luaL_checkstring(L, 1);
    float x = (float)luaL_optnumber(L, 2, 0.0);
    float z = (float)luaL_optnumber(L, 3, 0.0);
    float a = (float)luaL_optnumber(L, 4, 0.0);
    int direction = (int)luaL_optinteger(L, 5, 1);
    entity *ent;
    extern entity *ent_list[MAX_ENTS];

    /*
     * spawn() expects the entity pool to have been allocated. The select
     * screen can have a Lua VM but no active entity pool, so calling the
     * native spawn() there can dereference a NULL ent_list entry.
     */
    if (!level) {
        return luaL_error(L, "spawn called without an active level");
    }

    if (!ent_list[0]) {
        return luaL_error(L, "spawn called before entity pool initialization");
    }

    if (!model[0]) {
        return luaL_error(L, "spawn called with an empty model name");
    }

    ent = spawn(x, z, a, direction, (char *)model, -1, NULL);
    if (!ent || !ent->exists) {
        lua_pushnil(L);
        return 1;
    }

    lua_pushlightuserdata(L, (void *)ent);
    return 1;
}

// openbor.get_self()
static int lua_openbor_get_self(lua_State *L) {
    if (self && self->exists) {
        lua_pushlightuserdata(L, (void *)self);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// Global table registration index called during startup()
void openbor_register_lua_api(void) {
    if (!g_lua_engine_state) return;

    lua_newtable(g_lua_engine_state);
    
    lua_pushcfunction(g_lua_engine_state, lua_openbor_log);
    lua_setfield(g_lua_engine_state, -2, "log");
    
    lua_pushcfunction(g_lua_engine_state, lua_openbor_rand);
    lua_setfield(g_lua_engine_state, -2, "rand");
    
    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawstring);
    lua_setfield(g_lua_engine_state, -2, "drawstring");
    
    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawbox);
    lua_setfield(g_lua_engine_state, -2, "drawbox");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_killentity);
    lua_setfield(g_lua_engine_state, -2, "killentity");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_get_time);
    lua_setfield(g_lua_engine_state, -2, "get_time");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_get_player_health);
    lua_setfield(g_lua_engine_state, -2, "get_player_health");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentity);
    lua_setfield(g_lua_engine_state, -2, "getentity");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_damageentity);
    lua_setfield(g_lua_engine_state, -2, "damageentity");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_get_self);
    lua_setfield(g_lua_engine_state, -2, "get_self");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentityproperty);
    lua_setfield(g_lua_engine_state, -2, "getEntityProperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setentityproperty);
    lua_setfield(g_lua_engine_state, -2, "setEntityProperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_isentityvalid);
    lua_setfield(g_lua_engine_state, -2, "isEntityValid");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_isentityalive);
    lua_setfield(g_lua_engine_state, -2, "isEntityAlive");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentityhealth);
    lua_setfield(g_lua_engine_state, -2, "getEntityHealth");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentitytarget);
    lua_setfield(g_lua_engine_state, -2, "getEntityTarget");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setentitytarget);
    lua_setfield(g_lua_engine_state, -2, "setEntityTarget");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentityposition);
    lua_setfield(g_lua_engine_state, -2, "getEntityPosition");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setentityposition);
    lua_setfield(g_lua_engine_state, -2, "setEntityPosition");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changeanimation);
    lua_setfield(g_lua_engine_state, -2, "changeAnimation");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_spawn);
    lua_setfield(g_lua_engine_state, -2, "spawn");
    
    lua_setglobal(g_lua_engine_state, "openbor");
}