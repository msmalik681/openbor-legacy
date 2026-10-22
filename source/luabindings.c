#include "openbor.h"
#include "data.h"
#include "luabindings.h"
#include <lauxlib.h>
#include <string.h>
extern int sprite_map_max_items;
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
    printf("%s\n", message);
    fflush(stdout);
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

typedef struct bor_lua_entity_handle {
    entity *ptr;
} bor_lua_entity_handle;

#define BOR_LUA_ENTITY_METATABLE "OpenBOR.Entity"

static void bor_lua_push_entity(lua_State *L, entity *ent)
{
    bor_lua_entity_handle *handle;

    if (!ent) {
        lua_pushnil(L);
        return;
    }

    handle = (bor_lua_entity_handle *)lua_newuserdata(L, sizeof(*handle));
    handle->ptr = ent;
    luaL_getmetatable(L, BOR_LUA_ENTITY_METATABLE);
    lua_setmetatable(L, -2);
}

static entity *bor_lua_get_live_entity(lua_State *L, int index)
{
    entity *candidate = NULL;

    /* Accept raw lightuserdata for compatibility, but expose entity handles
     * as full userdata so Lua can safely use ent.health-style property access. */
    if (lua_islightuserdata(L, index)) {
        candidate = (entity *)lua_touserdata(L, index);
    } else if (lua_isuserdata(L, index)) {
        bor_lua_entity_handle *handle;
        int matches;

        if (!lua_getmetatable(L, index))
            return NULL;
        luaL_getmetatable(L, BOR_LUA_ENTITY_METATABLE);
        matches = lua_rawequal(L, -1, -2);
        lua_pop(L, 2);
        if (!matches)
            return NULL;

        handle = (bor_lua_entity_handle *)lua_touserdata(L, index);
        if (!handle)
            return NULL;
        candidate = handle->ptr;
    } else {
        return NULL;
    }

    return bor_lua_get_live_entity_ptr(candidate);
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
        bor_lua_push_entity(L, ent_list[idx]);
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

// getentityproperty(entity, property, ...)
// Lua implementation of the legacy OpenBOR entity-property query API.
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
    else if (!strcmp(prop, "exists"))          lua_pushinteger(L, ent->exists ? 1 : 0);
    else if (!strcmp(prop, "dead"))            lua_pushinteger(L, ent->dead ? 1 : 0);
    else if (!strcmp(prop, "dying"))           lua_pushinteger(L, ent->dying ? 1 : 0);
    else if (!strcmp(prop, "attacking"))       lua_pushinteger(L, ent->attacking ? 1 : 0);
    else if (!strcmp(prop, "blocking"))        lua_pushinteger(L, ent->blocking ? 1 : 0);
    else if (!strcmp(prop, "falling"))         lua_pushinteger(L, ent->falling ? 1 : 0);
    else if (!strcmp(prop, "running") && lua_gettop(L) < 3) lua_pushinteger(L, ent->running ? 1 : 0);
    else if (!strcmp(prop, "frozen"))          lua_pushinteger(L, ent->frozen ? 1 : 0);
    else if (!strcmp(prop, "invincible"))      lua_pushinteger(L, ent->invincible ? 1 : 0);
    else if (!strcmp(prop, "projectile"))      lua_pushinteger(L, ent->projectile ? 1 : 0);
    else if (!strcmp(prop, "autokill"))        lua_pushinteger(L, ent->autokill ? 1 : 0);
    else if (!strcmp(prop, "seal"))            lua_pushinteger(L, ent->seal ? 1 : 0);
    else if (!strcmp(prop, "name"))            lua_pushstring(L, ent->name);
    else if (!strcmp(prop, "model"))           lua_pushstring(L, ent->model ? ent->model->name : "");
    else if (!strcmp(prop, "defaultmodel"))    lua_pushstring(L, ent->defaultmodel ? ent->defaultmodel->name : "");
    else if (!strcmp(prop, "spawntype"))        lua_pushinteger(L, ent->spawntype);
    else if (!strcmp(prop, "playerindex"))     lua_pushinteger(L, ent->playerindex);
    else if (!strcmp(prop, "map")) {
        int i;
        lua_Integer map_index = 0;
        for (i = 0; i < ent->modeldata.maps_loaded; ++i) {
            if (ent->colourmap == ent->modeldata.colourmap[i]) {
                map_index = i + 1;
                break;
            }
        }
        lua_pushinteger(L, map_index);
    }
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
        if (bor_lua_get_live_entity_ptr(ent->owner) && ent->owner->exists) bor_lua_push_entity(L, ent->owner); else lua_pushnil(L);
    }
    else if (!strcmp(prop, "parent")) {
        if (bor_lua_get_live_entity_ptr(ent->parent) && ent->parent->exists) bor_lua_push_entity(L, ent->parent); else lua_pushnil(L);
    }
    else if (!strcmp(prop, "subentity")) {
        if (bor_lua_get_live_entity_ptr(ent->subentity) && ent->subentity->exists) bor_lua_push_entity(L, ent->subentity); else lua_pushnil(L);
    }
    else if (!strcmp(prop, "opponent")) {
        if (bor_lua_get_live_entity_ptr(ent->opponent) && ent->opponent->exists) bor_lua_push_entity(L, ent->opponent); else lua_pushnil(L);
    }
    else if (!strcmp(prop, "link")) {
        if (bor_lua_get_live_entity_ptr(ent->link) && ent->link->exists) bor_lua_push_entity(L, ent->link); else lua_pushnil(L);
    }
    /* Additional properties supported by the original legacy API. */
    else if (!strcmp(prop, "animal"))            lua_pushinteger(L, ent->modeldata.animal);
    else if (!strcmp(prop, "animhits")) {
        if (ent->animation) lua_pushinteger(L, ent->animation->animhits); else lua_pushnil(L);
    }
    else if (!strcmp(prop, "tosstime"))           lua_pushinteger(L, ent->toss_time);
    else if (!strcmp(prop, "mpstableval"))        lua_pushinteger(L, ent->modeldata.mpstableval);
    else if (!strcmp(prop, "mpstable"))           lua_pushinteger(L, ent->modeldata.mpstable);
    else if (!strcmp(prop, "mapcount"))           lua_pushinteger(L, ent->modeldata.maps_loaded + 1);
    else if (!strcmp(prop, "mapdefault"))         lua_pushinteger(L, ent->map);
    else if (!strcmp(prop, "hmapl"))              lua_pushinteger(L, ent->modeldata.hmap1);
    else if (!strcmp(prop, "hmapu"))              lua_pushinteger(L, ent->modeldata.hmap2);
    else if (!strcmp(prop, "defaultname"))        lua_pushstring(L, ent->defaultmodel ? ent->defaultmodel->name : "");
    else if (!strcmp(prop, "type"))               lua_pushinteger(L, ent->modeldata.type);
    else if (!strcmp(prop, "subtype"))            lua_pushinteger(L, ent->modeldata.subtype);
    else if (!strcmp(prop, "throwdamage"))        lua_pushinteger(L, ent->modeldata.throwdamage);
    else if (!strcmp(prop, "throwdist"))          lua_pushnumber(L, ent->modeldata.throwdist);
    else if (!strcmp(prop, "throwframewait"))     lua_pushinteger(L, ent->modeldata.throwframewait);
    else if (!strcmp(prop, "throwheight"))        lua_pushnumber(L, ent->modeldata.throwheight);
    else if (!strcmp(prop, "animheight")) {
        if (ent->animation) lua_pushinteger(L, ent->animation->height); else lua_pushnil(L);
    }
    else if (!strcmp(prop, "animationid"))        lua_pushinteger(L, ent->animnum);
    else if (!strcmp(prop, "animating"))          lua_pushinteger(L, ent->animating);
    else if (!strcmp(prop, "jumping"))            lua_pushinteger(L, ent->jumping ? 1 : 0);
    else if (!strcmp(prop, "idling"))             lua_pushinteger(L, ent->idling ? 1 : 0);
    else if (!strcmp(prop, "drop"))               lua_pushinteger(L, ent->drop ? 1 : 0);
    else if (!strcmp(prop, "getting"))            lua_pushinteger(L, ent->getting ? 1 : 0);
    else if (!strcmp(prop, "turning"))            lua_pushinteger(L, ent->turning ? 1 : 0);
    else if (!strcmp(prop, "charging"))           lua_pushinteger(L, ent->charging ? 1 : 0);
    else if (!strcmp(prop, "inpain"))             lua_pushinteger(L, ent->inpain ? 1 : 0);
    else if (!strcmp(prop, "toexplode"))          lua_pushinteger(L, ent->toexplode ? 1 : 0);
    else if (!strcmp(prop, "freezetime"))         lua_pushinteger(L, ent->freezetime);
    else if (!strcmp(prop, "invinctime"))         lua_pushinteger(L, ent->invinctime);
    else if (!strcmp(prop, "rush_count"))         lua_pushinteger(L, ent->rush[0]);
    else if (!strcmp(prop, "rush_tally"))         lua_pushinteger(L, ent->rush[1]);
    else if (!strcmp(prop, "rush_time"))          lua_pushinteger(L, ent->rushtime);
    else if (!strcmp(prop, "knockdowncount"))     lua_pushnumber(L, ent->knockdowncount);
    else if (!strcmp(prop, "lifespancountdown"))  lua_pushnumber(L, ent->lifespancountdown);
    else if (!strcmp(prop, "subject_to_screen"))  lua_pushinteger(L, ent->modeldata.subject_to_screen);
    else if (!strcmp(prop, "subject_to_minz"))    lua_pushinteger(L, ent->modeldata.subject_to_minz);
    else if (!strcmp(prop, "subject_to_maxz"))    lua_pushinteger(L, ent->modeldata.subject_to_maxz);
    else if (!strcmp(prop, "subject_to_wall"))    lua_pushinteger(L, ent->modeldata.subject_to_wall);
    else if (!strcmp(prop, "subject_to_hole"))    lua_pushinteger(L, ent->modeldata.subject_to_hole);
    else if (!strcmp(prop, "subject_to_gravity")) lua_pushinteger(L, ent->modeldata.subject_to_gravity);
    else if (!strcmp(prop, "subject_to_obstacle"))lua_pushinteger(L, ent->modeldata.subject_to_obstacle);
    else if (!strcmp(prop, "subject_to_platform"))lua_pushinteger(L, ent->modeldata.subject_to_platform);
    else if (!strcmp(prop, "no_adjust_base"))     lua_pushinteger(L, ent->modeldata.no_adjust_base);
    else if (!strcmp(prop, "noaicontrol"))        lua_pushinteger(L, ent->noaicontrol);
    else if (!strcmp(prop, "nodieblink"))         lua_pushinteger(L, ent->modeldata.nodieblink);
    else if (!strcmp(prop, "bounce"))             lua_pushinteger(L, ent->modeldata.bounce);
    else if (!strcmp(prop, "falldie"))            lua_pushinteger(L, ent->modeldata.falldie);
    else if (!strcmp(prop, "antigravity"))        lua_pushnumber(L, ent->antigravity);
    else if (!strcmp(prop, "path"))               lua_pushstring(L, ent->modeldata.path ? ent->modeldata.path : "");
    else if (!strcmp(prop, "grabbing")) {
        if (bor_lua_get_live_entity_ptr(ent->grabbing) && ent->grabbing->exists) bor_lua_push_entity(L, ent->grabbing); else lua_pushnil(L);
    }
    else if (!strcmp(prop, "aimove"))             lua_pushinteger(L, ent->modeldata.aimove);
    else if (!strcmp(prop, "aggression"))         lua_pushinteger(L, ent->modeldata.aggression);
    else if (!strcmp(prop, "aiattack"))           lua_pushinteger(L, ent->modeldata.aiattack);
    else if (!strcmp(prop, "scroll"))             lua_pushnumber(L, ent->modeldata.scroll);
    else if (!strcmp(prop, "damage_on_landing"))  lua_pushinteger(L, ent->damage_on_landing);
    else if (!strcmp(prop, "detect"))             lua_pushinteger(L, ent->modeldata.stealth[1]);
    else if (!strcmp(prop, "stealth"))            lua_pushinteger(L, ent->modeldata.stealth[0]);
    else if (!strcmp(prop, "stalltime"))          lua_pushinteger(L, ent->stalltime);
    else if (!strcmp(prop, "gfxshadow"))          lua_pushinteger(L, ent->modeldata.gfxshadow);
    else if (!strcmp(prop, "nograb"))             lua_pushinteger(L, ent->nograb);
    else if (!strcmp(prop, "nodrop"))             lua_pushinteger(L, ent->modeldata.nodrop);
    else if (!strcmp(prop, "nopain"))             lua_pushinteger(L, ent->modeldata.nopain);
    else if (!strcmp(prop, "running_speed"))      lua_pushnumber(L, ent->modeldata.runspeed);
    else if (!strcmp(prop, "running_jumpy"))      lua_pushnumber(L, ent->modeldata.runjumpheight);
    else if (!strcmp(prop, "running_jumpx"))      lua_pushnumber(L, ent->modeldata.runjumpdist);
    else if (!strcmp(prop, "running_land"))       lua_pushinteger(L, ent->modeldata.runhold);
    else if (!strcmp(prop, "running_movez"))      lua_pushinteger(L, ent->modeldata.runupdown);
    else if (!strcmp(prop, "sealtime"))           lua_pushinteger(L, ent->sealtime);
    else if (!strcmp(prop, "blockback"))          lua_pushinteger(L, ent->modeldata.blockback);
    else if (!strcmp(prop, "blockpain"))          lua_pushinteger(L, ent->modeldata.blockpain);
    else if (!strcmp(prop, "weapent")) {
        if (bor_lua_get_live_entity_ptr(ent->weapent) && ent->weapent->exists) bor_lua_push_entity(L, ent->weapent); else lua_pushnil(L);
    }
    else if (!strcmp(prop, "blink"))              lua_pushinteger(L, ent->blink ? 1 : 0);
    else if (!strcmp(prop, "animation") || !strcmp(prop, "animation.handle")) {
        if (ent->animation) lua_pushlightuserdata(L, ent->animation); else lua_pushnil(L);
    }
    else if (!strcmp(prop, "colourmap")) {
        if (ent->colourmap) lua_pushlightuserdata(L, ent->colourmap); else lua_pushnil(L);
    }
    else if (!strcmp(prop, "colourtable")) {
        int which = (int)luaL_checkinteger(L, 3);
        if (which < 0 || which >= ent->modeldata.maps_loaded) lua_pushnil(L);
        else if (ent->modeldata.colourmap[which]) lua_pushlightuserdata(L, ent->modeldata.colourmap[which]);
        else lua_pushnil(L);
    }
    else if (!strcmp(prop, "aiflag")) {
        const char *flag = luaL_optstring(L, 3, "");
        if (!strcmp(flag, "dead")) lua_pushinteger(L, ent->dead ? 1 : 0);
        else if (!strcmp(flag, "jumping")) lua_pushinteger(L, ent->jumping ? 1 : 0);
        else if (!strcmp(flag, "idling")) lua_pushinteger(L, ent->idling ? 1 : 0);
        else if (!strcmp(flag, "drop")) lua_pushinteger(L, ent->drop ? 1 : 0);
        else if (!strcmp(flag, "attacking")) lua_pushinteger(L, ent->attacking ? 1 : 0);
        else if (!strcmp(flag, "getting")) lua_pushinteger(L, ent->getting ? 1 : 0);
        else if (!strcmp(flag, "turning")) lua_pushinteger(L, ent->turning ? 1 : 0);
        else if (!strcmp(flag, "charging")) lua_pushinteger(L, ent->charging ? 1 : 0);
        else if (!strcmp(flag, "blocking")) lua_pushinteger(L, ent->blocking ? 1 : 0);
        else if (!strcmp(flag, "falling")) lua_pushinteger(L, ent->falling ? 1 : 0);
        else if (!strcmp(flag, "running")) lua_pushinteger(L, ent->running ? 1 : 0);
        else if (!strcmp(flag, "inpain")) lua_pushinteger(L, ent->inpain ? 1 : 0);
        else if (!strcmp(flag, "projectile")) lua_pushinteger(L, ent->projectile ? 1 : 0);
        else if (!strcmp(flag, "frozen")) lua_pushinteger(L, ent->frozen ? 1 : 0);
        else if (!strcmp(flag, "freezetime")) lua_pushinteger(L, ent->freezetime);
        else if (!strcmp(flag, "toexplode")) lua_pushinteger(L, ent->toexplode ? 1 : 0);
        else if (!strcmp(flag, "animating")) lua_pushinteger(L, ent->animating);
        else if (!strcmp(flag, "blink")) lua_pushinteger(L, ent->blink ? 1 : 0);
        else if (!strcmp(flag, "invincible")) lua_pushinteger(L, ent->invincible ? 1 : 0);
        else if (!strcmp(flag, "autokill")) lua_pushinteger(L, ent->autokill ? 1 : 0);
        else return luaL_error(L, "unknown aiflag '%s'", flag);
    }
    else if (!strcmp(prop, "animvalid")) {
        int anim = (int)luaL_optinteger(L, 3, 0);
        lua_pushinteger(L, validanim(ent, anim) ? 1 : 0);
    }
    else if (!strcmp(prop, "dropframe") || !strcmp(prop, "landframe")) {
        int anim = (int)luaL_optinteger(L, 3, 0);
        if (!validanim(ent, anim)) lua_pushnil(L);
        else if (!strcmp(prop, "dropframe")) lua_pushinteger(L, ent->modeldata.animation[anim]->dropframe);
        else lua_pushinteger(L, ent->modeldata.animation[anim]->landframe[0]);
    }
    else if (!strcmp(prop, "energycost")) {
        int anim = (int)luaL_checkinteger(L, 3);
        int arg = (int)luaL_checkinteger(L, 4);
        if (!validanim(ent, anim) || arg < 0 || arg >= MAX_ATTACKS)
            return luaL_error(L, "getentityproperty: invalid energycost animation or argument");
        lua_pushinteger(L, ent->modeldata.animation[anim]->energycost[arg]);
    }
    else if (!strcmp(prop, "komap")) {
        int which = (int)luaL_checkinteger(L, 3);
        if (which < 0 || which > 1) lua_pushnil(L);
        else lua_pushinteger(L, ent->modeldata.komap[which]);
    }
    else if (!strcmp(prop, "offense")) {
        int which = (int)luaL_optinteger(L, 3, 0);
        if (which < 0 || which >= (int)(sizeof(ent->modeldata.offense_factors) / sizeof(ent->modeldata.offense_factors[0])))
            lua_pushnil(L);
        else lua_pushnumber(L, ent->modeldata.offense_factors[which]);
    }
    else if (!strcmp(prop, "defense")) {
        int which = (int)luaL_optinteger(L, 3, 0);
        const char *field = luaL_optstring(L, 4, "factor");
        if (which < 0 || which >= (int)(sizeof(ent->modeldata.defense_factors) / sizeof(ent->modeldata.defense_factors[0])))
            lua_pushnil(L);
        else if (!strcmp(field, "factor")) lua_pushnumber(L, ent->modeldata.defense_factors[which]);
        else if (!strcmp(field, "blockpower")) lua_pushnumber(L, ent->modeldata.defense_blockpower[which]);
        else if (!strcmp(field, "blockratio")) lua_pushnumber(L, ent->modeldata.defense_blockratio[which]);
        else if (!strcmp(field, "blockthreshold")) lua_pushnumber(L, ent->modeldata.defense_blockthreshold[which]);
        else if (!strcmp(field, "blocktype")) lua_pushnumber(L, ent->modeldata.defense_blocktype[which]);
        else if (!strcmp(field, "knockdown")) lua_pushnumber(L, ent->modeldata.defense_knockdown[which]);
        else if (!strcmp(field, "pain")) lua_pushnumber(L, ent->modeldata.defense_pain[which]);
        else return luaL_error(L, "unknown defense property '%s'", field);
    }
    else if (!strcmp(prop, "flash")) {
        int which = (int)luaL_checkinteger(L, 3);
        if (which == 1) lua_pushinteger(L, ent->modeldata.noatflash);
        else if (which == 2) lua_pushinteger(L, ent->modeldata.bflash);
        else lua_pushinteger(L, ent->modeldata.flash);
    }
    else if (!strcmp(prop, "icon")) {
        int which = (int)luaL_checkinteger(L, 3);
        int sprite = ent->modeldata.icon;
        if (which == 1) sprite = ent->modeldata.icondie;
        else if (which == 2) sprite = ent->modeldata.iconget;
        else if (which == 3) sprite = ent->modeldata.iconmp[0];
        else if (which == 4) sprite = ent->modeldata.iconmp[1];
        else if (which == 5) sprite = ent->modeldata.iconmp[2];
        else if (which == 6) sprite = ent->modeldata.iconpain;
        else if (which == 7) sprite = ent->modeldata.iconw;
        else if (which == 8) sprite = ent->modeldata.iconx;
        else if (which == 9) sprite = ent->modeldata.icony;
        if (sprite < 0 || sprite >= sprite_map_max_items || !sprite_map || !sprite_map[sprite].sprite) {
            lua_pushinteger(L, -1);
        } else {
            sprite_map[sprite].sprite->centerx = sprite_map[sprite].centerx;
            sprite_map[sprite].sprite->centery = sprite_map[sprite].centery;
            lua_pushlightuserdata(L, sprite_map[sprite].sprite);
        }
    }
    else if (!strcmp(prop, "sprite")) {
        int sprite;
        if (!ent->animation || ent->animpos < 0 || ent->animpos >= ent->animation->numframes) {
            lua_pushnil(L);
        } else {
            sprite = ent->animation->sprite[ent->animpos];
            if (sprite < 0 || sprite >= sprite_map_max_items || !sprite_map || !sprite_map[sprite].sprite) {
                lua_pushnil(L);
            } else {
                sprite_map[sprite].sprite->centerx = sprite_map[sprite].centerx;
                sprite_map[sprite].sprite->centery = sprite_map[sprite].centery;
                lua_pushlightuserdata(L, sprite_map[sprite].sprite);
            }
        }
    }
    else if (!strcmp(prop, "edelay")) {
        const char *field = luaL_checkstring(L, 3);
        if (!strcmp(field, "mode")) lua_pushinteger(L, ent->modeldata.edelay.mode);
        else if (!strcmp(field, "factor")) lua_pushnumber(L, ent->modeldata.edelay.factor);
        else if (!strcmp(field, "cap_min")) lua_pushinteger(L, ent->modeldata.edelay.cap_min);
        else if (!strcmp(field, "cap_max")) lua_pushinteger(L, ent->modeldata.edelay.cap_max);
        else if (!strcmp(field, "range_min")) lua_pushinteger(L, ent->modeldata.edelay.range_min);
        else if (!strcmp(field, "range_max")) lua_pushinteger(L, ent->modeldata.edelay.range_max);
        else return luaL_error(L, "unknown edelay property '%s'", field);
    }
    else if (!strcmp(prop, "running")) {
        const char *field = luaL_checkstring(L, 3);
        if (!strcmp(field, "speed")) lua_pushnumber(L, ent->modeldata.runspeed);
        else if (!strcmp(field, "jumpy")) lua_pushnumber(L, ent->modeldata.runjumpheight);
        else if (!strcmp(field, "jumpx")) lua_pushnumber(L, ent->modeldata.runjumpdist);
        else if (!strcmp(field, "land")) lua_pushinteger(L, ent->modeldata.runhold);
        else if (!strcmp(field, "movez")) lua_pushinteger(L, ent->modeldata.runupdown);
        else return luaL_error(L, "unknown running property '%s'", field);
    }
    else if (!strcmp(prop, "bbox")) {
        int anim = (int)luaL_checkinteger(L, 4);
        int frame = (int)luaL_checkinteger(L, 5);
        int coord = (int)luaL_checkinteger(L, 6);
        (void)luaL_checkinteger(L, 3); /* legacy bbox index argument */
        if (!validanim(ent, anim) || frame < 0 ||
            frame >= ent->modeldata.animation[anim]->numframes || coord < 0 || coord >= 5 ||
            !ent->modeldata.animation[anim]->bbox_coords)
            lua_pushnil(L);
        else lua_pushinteger(L, ent->modeldata.animation[anim]->bbox_coords[frame][coord]);
    }
    else if (!strcmp(prop, "range")) {
        const char *field = luaL_checkstring(L, 3);
        int anim = (int)luaL_checkinteger(L, 4);
        int idx = -1;
        if (!validanim(ent, anim)) lua_pushnil(L);
        else {
            if (!strcmp(field, "xmin")) idx = 0;
            else if (!strcmp(field, "xmax")) idx = 1;
            else if (!strcmp(field, "zmin")) idx = 2;
            else if (!strcmp(field, "zmax")) idx = 3;
            else if (!strcmp(field, "amin")) idx = 4;
            else if (!strcmp(field, "amax")) idx = 5;
            else if (!strcmp(field, "bmin")) idx = 6;
            else if (!strcmp(field, "bmax")) idx = 7;
            if (idx < 0) return luaL_error(L, "unknown range property '%s'", field);
            lua_pushinteger(L, ent->modeldata.animation[anim]->range[idx]);
        }
    }
    else if (!strcmp(prop, "stats")) {
        int type = (int)luaL_checkinteger(L, 3);
        int idx = (int)luaL_checkinteger(L, 4);
        if (idx < 0 || idx >= 20) lua_pushnil(L);
        else if (type == 1) lua_pushnumber(L, ent->stats[idx]);
        else lua_pushnumber(L, ent->modeldata.stats[idx]);
    }
    else if (!strcmp(prop, "staydown")) {
        int which = 0;
        if (lua_isstring(L, 4)) {
            const char *field = lua_tostring(L, 4);
            if (!strcmp(field, "staydownatk")) which = 1;
            else if (strcmp(field, "staydown") != 0)
                return luaL_error(L, "unknown staydown property '%s'", field);
        } else {
            which = (int)luaL_optinteger(L, 4, 0);
        }
        if (which < 0 || which > 1) lua_pushnil(L);
        else lua_pushnumber(L, ent->staydown[which]);
    }
    else if (!strcmp(prop, "staydownatk")) lua_pushnumber(L, ent->staydown[1]);
    else if (!strcmp(prop, "dot")) {
        int which = (int)luaL_checkinteger(L, 3);
        const char *field = luaL_checkstring(L, 4);
        if (which < 0 || which >= MAX_DOTS) lua_pushnil(L);
        else if (!strcmp(field, "time")) lua_pushinteger(L, ent->dot_time[which]);
        else if (!strcmp(field, "mode")) lua_pushinteger(L, ent->dot[which]);
        else if (!strcmp(field, "force")) lua_pushinteger(L, ent->dot_force[which]);
        else if (!strcmp(field, "rate")) lua_pushinteger(L, ent->dot_rate[which]);
        else if (!strcmp(field, "type")) lua_pushinteger(L, ent->dot_atk[which]);
        else if (!strcmp(field, "owner")) {
            if (bor_lua_get_live_entity_ptr(ent->dot_owner[which]) && ent->dot_owner[which]->exists) bor_lua_push_entity(L, ent->dot_owner[which]);
            else lua_pushnil(L);
        }
        else return luaL_error(L, "unknown dot property '%s'", field);
    }
    else if (!strcmp(prop, "attack")) {
        const char *field = luaL_checkstring(L, 3);
        int anim = (int)luaL_checkinteger(L, 5);
        int frame = (int)luaL_checkinteger(L, 6);
        int arg = (int)luaL_optinteger(L, 7, 0);
        s_attack *attack;
        (void)luaL_checkinteger(L, 4); /* legacy attack-box index argument */
        if (!validanim(ent, anim) || frame < 0 ||
            frame >= ent->modeldata.animation[anim]->numframes ||
            !ent->modeldata.animation[anim]->attacks ||
            !ent->modeldata.animation[anim]->attacks[frame]) {
            lua_pushnil(L);
        } else {
            attack = ent->modeldata.animation[anim]->attacks[frame];
            if (!strcmp(field, "blockflash")) lua_pushinteger(L, attack->blockflash);
            else if (!strcmp(field, "blocksound")) lua_pushinteger(L, attack->blocksound);
            else if (!strcmp(field, "blast")) lua_pushinteger(L, attack->blast);
            else if (!strcmp(field, "coords")) {
                if (arg < 0 || arg >= 5) lua_pushnil(L); else lua_pushinteger(L, attack->attack_coords[arg]);
            }
            else if (!strcmp(field, "counterattack")) lua_pushinteger(L, attack->counterattack);
            else if (!strcmp(field, "direction")) lua_pushinteger(L, attack->force_direction);
            else if (!strcmp(field, "dol")) lua_pushinteger(L, attack->damage_on_landing);
            else if (!strcmp(field, "dot")) lua_pushinteger(L, attack->dot);
            else if (!strcmp(field, "dotforce")) lua_pushinteger(L, attack->dot_force);
            else if (!strcmp(field, "dotindex")) lua_pushinteger(L, attack->dot_index);
            else if (!strcmp(field, "dotrate")) lua_pushinteger(L, attack->dot_rate);
            else if (!strcmp(field, "dottime")) lua_pushinteger(L, attack->dot_time);
            else if (!strcmp(field, "drop")) lua_pushinteger(L, attack->attack_drop);
            else if (!strcmp(field, "dropv")) {
                if (arg < 0 || arg >= 3) lua_pushnil(L); else lua_pushnumber(L, attack->dropv[arg]);
            }
            else if (!strcmp(field, "force")) lua_pushinteger(L, attack->attack_force);
            else if (!strcmp(field, "forcemap")) lua_pushinteger(L, attack->forcemap);
            else if (!strcmp(field, "freeze")) lua_pushinteger(L, attack->freeze);
            else if (!strcmp(field, "freezetime")) lua_pushinteger(L, attack->freezetime);
            else if (!strcmp(field, "grab")) lua_pushinteger(L, attack->grab);
            else if (!strcmp(field, "grabdistance")) lua_pushinteger(L, attack->grab_distance);
            else if (!strcmp(field, "guardcost")) lua_pushinteger(L, attack->guardcost);
            else if (!strcmp(field, "hitflash")) lua_pushinteger(L, attack->hitflash);
            else if (!strcmp(field, "hitsound")) lua_pushinteger(L, attack->hitsound);
            else if (!strcmp(field, "jugglecost")) lua_pushinteger(L, attack->jugglecost);
            else if (!strcmp(field, "maptime")) lua_pushinteger(L, attack->maptime);
            else if (!strcmp(field, "noblock")) lua_pushinteger(L, attack->no_block);
            else if (!strcmp(field, "noflash")) lua_pushinteger(L, attack->no_flash);
            else if (!strcmp(field, "nopain")) lua_pushinteger(L, attack->no_pain);
            else if (!strcmp(field, "otg")) lua_pushinteger(L, attack->otg);
            else if (!strcmp(field, "pause")) lua_pushinteger(L, attack->pause_add);
            else if (!strcmp(field, "seal")) lua_pushinteger(L, attack->seal);
            else if (!strcmp(field, "sealtime")) lua_pushinteger(L, attack->sealtime);
            else if (!strcmp(field, "staydown")) lua_pushlightuserdata(L, attack->staydown);
            else if (!strcmp(field, "steal")) lua_pushinteger(L, attack->steal);
            else if (!strcmp(field, "type")) lua_pushinteger(L, attack->attack_type);
            else return luaL_error(L, "unknown attack property '%s'", field);
        }
    }
    else {
        return luaL_error(L, "unsupported legacy entity property '%s'", prop);
    }

    return 1;
}

// openbor.setEntityProperty(entity, property, value)
static int lua_openbor_changeentityproperty(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    const char *prop;
    lua_Integer ivalue;

    if (!ent || !ent->exists) {
        lua_pushboolean(L, 0);
        return 1;
    }

    prop = luaL_checkstring(L, 2);

    if (!strcmp(prop, "health")) {
        ivalue = luaL_checkinteger(L, 3);
        if (ivalue > ent->modeldata.health) ivalue = ent->modeldata.health;
        if (ivalue < 0) ivalue = 0;
        ent->health = (int)ivalue;
    } else if (!strcmp(prop, "mp")) {
        ivalue = luaL_checkinteger(L, 3);
        if (ivalue > ent->modeldata.mp) ivalue = ent->modeldata.mp;
        if (ivalue < 0) ivalue = 0;
        ent->mp = (int)ivalue;
    } else if (!strcmp(prop, "x"))          ent->x = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "z"))             ent->z = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "a"))             ent->a = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "xdir"))          ent->xdir = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "zdir"))          ent->zdir = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "tossv"))         ent->tossv = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "base"))          ent->base = (float)luaL_checknumber(L, 3);
    else if (!strcmp(prop, "direction"))     ent->direction = (char)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "map"))           ent->map = (char)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "nextanim"))      ent->nextanim = (unsigned int)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "nextthink"))     ent->nextthink = (unsigned int)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "autokill"))      ent->autokill = (char)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "invincible"))   ent->invincible = (char)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "frozen"))        ent->frozen = (char)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "seal"))          ent->seal = (char)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "position")) {
        ent->x = (float)luaL_checknumber(L, 3);
        if (!lua_isnoneornil(L, 4)) ent->z = (float)luaL_checknumber(L, 4);
        if (!lua_isnoneornil(L, 5)) ent->a = (float)luaL_checknumber(L, 5);
    } else if (!strcmp(prop, "velocity")) {
        ent->xdir = (float)luaL_checknumber(L, 3);
        if (!lua_isnoneornil(L, 4)) ent->zdir = (float)luaL_checknumber(L, 4);
        if (!lua_isnoneornil(L, 5)) ent->tossv = (float)luaL_checknumber(L, 5);
    } else if (!strcmp(prop, "animation")) {
        int anim = (int)luaL_checkinteger(L, 3);
        int reset = lua_isnoneornil(L, 4) ? 1 : (int)luaL_checkinteger(L, 4);
        ent_set_anim(ent, anim, reset);
    } else if (!strcmp(prop, "animpos")) {
        ent->animpos = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "maxhealth")) {
        ivalue = luaL_checkinteger(L, 3);
        ent->modeldata.health = (int)(ivalue < 0 ? 0 : ivalue);
        if (ent->health > ent->modeldata.health) ent->health = ent->modeldata.health;
    } else if (!strcmp(prop, "maxmp")) {
        ivalue = luaL_checkinteger(L, 3);
        ent->modeldata.mp = (int)(ivalue < 0 ? 0 : ivalue);
        if (ent->mp > ent->modeldata.mp) ent->mp = ent->modeldata.mp;
    } else if (!strcmp(prop, "speed")) {
        ent->modeldata.speed = (float)luaL_checknumber(L, 3);
    } else if (!strcmp(prop, "setlayer")) {
        ent->modeldata.setlayer = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "throwdamage")) {
        ent->modeldata.throwdamage = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "throwdist")) {
        ent->modeldata.throwdist = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "throwframewait")) {
        ent->modeldata.throwframewait = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "throwheight")) {
        ent->modeldata.throwheight = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "alpha")) {
        ent->modeldata.alpha = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "guardpoints")) {
        ent->modeldata.guardpoints = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "maxguardpoints")) {
        ent->modeldata.guardpoints = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "jugglepoints")) {
        ent->modeldata.jugglepoints = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "maxjugglepoints")) {
        ent->modeldata.jugglepoints = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "komap")) {
        ent->modeldata.komap = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "antigravity")) {
        ent->modeldata.antigravity = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "maptime")) {
        ent->maptime = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "tosstime")) {
        ent->toss_time = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "invinctime")) {
        ent->invinctime = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "lifespancountdown")) {
        ent->lifespancountdown = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "freezetime")) {
        ent->freezetime = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "bounce")) {
        ent->modeldata.bounce = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "antigrab")) {
        ent->model->antigrab = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "nodrop")) {
        ent->modeldata.nodrop = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "nopain")) {
        ent->modeldata.nopain = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "no_adjust_base")) {
        ent->modeldata.no_adjust_base = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "noaicontrol")) {
        ent->noaicontrol = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "nodieblink")) {
        ent->modeldata.nodieblink = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "falldie")) {
        ent->modeldata.falldie = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "aiflag")) {
        const char *flag = luaL_checkstring(L, 3);
        int value = (int)luaL_checkinteger(L, 4);
        if (!strcmp(flag, "dead")) ent->dead = value;
        else if (!strcmp(flag, "jumping")) ent->jumping = value;
        else if (!strcmp(flag, "idling")) ent->idling = value;
        else if (!strcmp(flag, "drop")) ent->drop = value;
        else if (!strcmp(flag, "attacking")) ent->attacking = value;
        else if (!strcmp(flag, "getting")) ent->getting = value;
        else if (!strcmp(flag, "turning")) ent->turning = value;
        else if (!strcmp(flag, "charging")) ent->charging = value;
        else if (!strcmp(flag, "blocking")) ent->blocking = value;
        else if (!strcmp(flag, "falling")) ent->falling = value;
        else if (!strcmp(flag, "running")) ent->running = value;
        else if (!strcmp(flag, "inpain")) ent->inpain = value;
        else if (!strcmp(flag, "projectile")) ent->projectile = value;
        else if (!strcmp(flag, "frozen")) ent->frozen = value;
        else if (!strcmp(flag, "toexplode")) ent->toexplode = value;
        else if (!strcmp(flag, "animating")) ent->animating = value;
        else if (!strcmp(flag, "blink")) ent->blink = value;
        else if (!strcmp(flag, "invincible")) ent->invincible = value;
        else if (!strcmp(flag, "autokill")) ent->autokill = value;
        else return luaL_error(L, "changeentityproperty: unknown aiflag '%s'", flag);
    } else if (!strcmp(prop, "dead"))            ent->dead = (int)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "blink"))             ent->blink = (int)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "attacking"))         ent->attacking = (int)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "projectile"))        ent->projectile = (int)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "damage_on_landing")) ent->damage_on_landing = (int)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "stalltime"))         ent->stalltime = (int)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "staydown"))          ent->staydown = (int)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "stealth"))           ent->modeldata.stealth = (int)luaL_checkinteger(L, 3);
    else if (!strcmp(prop, "gfxshadow"))         ent->modeldata.gfxshadow = (int)luaL_checkinteger(L, 3);
    else {
        return luaL_error(L, "changeentityproperty: property '%s' is not implemented yet", prop);
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* Entity userdata metatable: property reads reuse the legacy getter, and
 * writes reuse the supported setter so both interfaces stay consistent. */
static int lua_openbor_entity_index(lua_State *L)
{
    if (lua_type(L, 2) != LUA_TSTRING) {
        lua_pushnil(L);
        return 1;
    }
    return lua_openbor_getentityproperty(L);
}

static int lua_openbor_entity_newindex(lua_State *L)
{
    if (lua_type(L, 2) != LUA_TSTRING)
        return luaL_error(L, "entity property name must be a string");

    lua_openbor_changeentityproperty(L);
    lua_pop(L, 1);
    return 0;
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
    if (!ent || !ent->exists || !bor_lua_get_live_entity_ptr(ent->opponent) || !ent->opponent->exists) {
        lua_pushnil(L);
        return 1;
    }
    bor_lua_push_entity(L, ent->opponent);
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

    bor_lua_push_entity(L, ent);
    return 1;
}

// openbor.get_self()
static int lua_openbor_get_self(lua_State *L) {
    if (self && self->exists) {
        bor_lua_push_entity(L, self);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

// Global table registration index called during startup()
void openbor_register_lua_api(void) {
    if (!g_lua_engine_state) return;

    /* Register before any entity handles are created. */
    luaL_newmetatable(g_lua_engine_state, BOR_LUA_ENTITY_METATABLE);
    lua_pushcfunction(g_lua_engine_state, lua_openbor_entity_index);
    lua_setfield(g_lua_engine_state, -2, "__index");
    lua_pushcfunction(g_lua_engine_state, lua_openbor_entity_newindex);
    lua_setfield(g_lua_engine_state, -2, "__newindex");
    lua_pop(g_lua_engine_state, 1);

    /*
     * Register the API as global Lua functions. Scripts should call
     * spawn(...), get_self(), log(), etc., without an openbor. prefix.
     */
    lua_pushcfunction(g_lua_engine_state, lua_openbor_log);
    lua_setglobal(g_lua_engine_state, "log");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_rand);
    lua_setglobal(g_lua_engine_state, "rand");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawstring);
    lua_setglobal(g_lua_engine_state, "drawstring");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawbox);
    lua_setglobal(g_lua_engine_state, "drawbox");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_killentity);
    lua_setglobal(g_lua_engine_state, "killentity");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_get_time);
    lua_setglobal(g_lua_engine_state, "get_time");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_get_player_health);
    lua_setglobal(g_lua_engine_state, "get_player_health");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentity);
    lua_setglobal(g_lua_engine_state, "getentity");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_damageentity);
    lua_setglobal(g_lua_engine_state, "damageentity");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_get_self);
    lua_setglobal(g_lua_engine_state, "get_self");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changeentityproperty);
    lua_setglobal(g_lua_engine_state, "changeentityproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_isentityvalid);
    lua_setglobal(g_lua_engine_state, "isEntityValid");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_isentityalive);
    lua_setglobal(g_lua_engine_state, "isEntityAlive");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentityhealth);
    lua_setglobal(g_lua_engine_state, "getEntityHealth");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentitytarget);
    lua_setglobal(g_lua_engine_state, "getEntityTarget");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setentitytarget);
    lua_setglobal(g_lua_engine_state, "setEntityTarget");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentityposition);
    lua_setglobal(g_lua_engine_state, "getEntityPosition");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setentityposition);
    lua_setglobal(g_lua_engine_state, "setEntityPosition");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changeanimation);
    lua_setglobal(g_lua_engine_state, "changeAnimation");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_spawn);
    lua_setglobal(g_lua_engine_state, "spawn");

    /* Legacy property access is also available as a lowercase global. */
    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentityproperty);
    lua_setglobal(g_lua_engine_state, "getentityproperty");
}
