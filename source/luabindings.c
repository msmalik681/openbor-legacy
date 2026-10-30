#include "openbor.h"
#include "data.h"
#include "luabindings.h"
#include <lauxlib.h>
#include <string.h>
#include <stdlib.h>
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
static int bor_lua_stricmp(const char *left, const char *right);
extern int noshare;
extern int credits;
extern s_savedata savedata;
extern char musicname[128];
extern float musicfade[2];
extern int musicloop;
extern u32 musicoffset;
extern entity *self;
extern int g_lua_current_player_index;
extern u32 borTime;
extern char branch_name[MAX_NAME_LEN + 1];
extern int current_set;
extern int current_level;
extern int current_stage;
extern int current_palette;
extern float advancex;
extern float advancey;
extern float lasthitx;
extern float lasthitz;
extern float lasthita;
extern int lasthitt;
extern int lasthitc;
extern float blockade;
extern float scrollminz;
extern float scrollmaxz;
extern int light[2];
extern int shadowalpha;
extern unsigned char slowmotion[3];
extern int selectScreen;
extern unsigned int models_loaded;
extern unsigned int models_cached;
extern int ent_count;
extern int ent_max;
extern int gfx_y_offset;
extern int panel_height;
extern int pause;
extern entity *textbox;
extern entity *smartbomber;

/* Entity handle helpers are defined below the variant API. */
static void bor_lua_push_entity(lua_State *L, entity *ent);
static entity *bor_lua_get_live_entity(lua_State *L, int index);
static entity *bor_lua_get_live_entity_ptr(entity *candidate);

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


// changeopenborvariant(name, value)
// Implements the numeric legacy system-property setters.
static int lua_changeopenborvariant(lua_State *L) {
    const char *name;
    lua_Integer value;

    if (lua_gettop(L) != 2)
        return luaL_error(L, "changeopenborvariant expects exactly 2 arguments");

    name = luaL_checkstring(L, 1);

    /* The legacy API accepts entity pointers for these two properties. */
    if (!strcmp(name, "smartbomber") || !strcmp(name, "textbox")) {
        entity *ent = NULL;

        if (!lua_isnil(L, 2)) {
            ent = bor_lua_get_live_entity(L, 2);
            if (!ent || !ent->exists)
                return luaL_error(L, "changeopenborvariant: '%s' expects a live entity or nil", name);
        }

        if (!strcmp(name, "smartbomber"))
            smartbomber = ent;
        else
            textbox = ent;

        lua_pushboolean(L, 1);
        return 1;
    }

    value = luaL_checkinteger(L, 2);

    if (!strcmp(name, "elapsed_time")) {
        borTime = (u32)value;
    } else if (!strcmp(name, "levelpos")) {
        if (!level)
            return luaL_error(L, "changeopenborvariant: 'levelpos' requires a loaded level");
        level->pos = (int)value;
    } else if (!strcmp(name, "xpos")) {
        advancex = (float)value;
    } else if (!strcmp(name, "ypos")) {
        advancey = (float)value;
    } else if (!strcmp(name, "scrollminz")) {
        scrollminz = (float)value;
    } else if (!strcmp(name, "scrollmaxz")) {
        scrollmaxz = (float)value;
    } else if (!strcmp(name, "blockade")) {
        blockade = (float)value;
    } else if (!strcmp(name, "slowmotion")) {
        slowmotion[0] = (unsigned char)value;
    } else if (!strcmp(name, "slowmotion_duration")) {
        slowmotion[1] = (unsigned char)value;
    } else if (!strcmp(name, "lasthitx")) {
        lasthitx = (float)value;
    } else if (!strcmp(name, "lasthitz")) {
        lasthitz = (float)value;
    } else if (!strcmp(name, "lasthita")) {
        lasthita = (float)value;
    } else if (!strcmp(name, "lasthitc")) {
        lasthitc = (int)value;
    } else if (!strcmp(name, "lasthitt")) {
        lasthitt = (int)value;
    } else {
        return luaL_error(L, "changeopenborvariant: unsupported or unimplemented property '%s'", name);
    }

    lua_pushboolean(L, 1);
    return 1;
}

// openborvariant(name)
// First compatibility milestone: read-only legacy system properties.
static int lua_openborvariant(lua_State *L) {
    const char *name;
    extern s_player player[4];

    if (lua_gettop(L) != 1)
        return luaL_error(L, "openborvariant expects exactly 1 argument");

    name = luaL_checkstring(L, 1);

    if (!strcmp(name, "branchname")) {
        lua_pushstring(L, branch_name);
    } else if (!strcmp(name, "current_set")) {
        lua_pushinteger(L, current_set);
    } else if (!strcmp(name, "current_level")) {
        lua_pushinteger(L, current_level);
    } else if (!strcmp(name, "current_stage")) {
        lua_pushinteger(L, current_stage);
    } else if (!strcmp(name, "current_palette")) {
        lua_pushinteger(L, current_palette);
    } else if (!strcmp(name, "elapsed_time")) {
        lua_pushinteger(L, (lua_Integer)borTime);
    } else if (!strcmp(name, "ent_max")) {
        lua_pushinteger(L, ent_max);
    } else if (!strcmp(name, "in_level")) {
        /* Preserve the original legacy API's return semantics. */
        lua_pushboolean(L, level == NULL);
    } else if (!strcmp(name, "in_selectscreen")) {
        lua_pushboolean(L, selectScreen != 0);
    } else if (!strcmp(name, "xpos")) {
        if (!level) return luaL_error(L, "openborvariant: 'xpos' requires a loaded level");
        lua_pushnumber(L, advancex);
    } else if (!strcmp(name, "ypos")) {
        if (!level) return luaL_error(L, "openborvariant: 'ypos' requires a loaded level");
        lua_pushnumber(L, advancey);
    } else if (!strcmp(name, "lasthitx")) {
        lua_pushnumber(L, lasthitx);
    } else if (!strcmp(name, "lasthitz")) {
        lua_pushnumber(L, lasthitz);
    } else if (!strcmp(name, "lasthita")) {
        lua_pushnumber(L, lasthita);
    } else if (!strcmp(name, "lasthitt")) {
        lua_pushinteger(L, lasthitt);
    } else if (!strcmp(name, "lasthitc")) {
        lua_pushinteger(L, lasthitc);
    } else if (!strcmp(name, "hResolution")) {
        lua_pushinteger(L, videomodes.hRes);
    } else if (!strcmp(name, "vResolution")) {
        lua_pushinteger(L, videomodes.vRes);
    } else if (!strcmp(name, "lightx")) {
        lua_pushinteger(L, light[0]);
    } else if (!strcmp(name, "lightz")) {
        lua_pushinteger(L, light[1]);
    } else if (!strcmp(name, "shadowalpha")) {
        lua_pushinteger(L, shadowalpha);
    } else if (!strcmp(name, "shadowcolor")) {
        lua_pushinteger(L, colors.shadow);
    } else if (!strcmp(name, "pixelformat")) {
        lua_pushinteger(L, pixelformat);
    } else if (!strcmp(name, "totalram")) {
        lua_pushinteger(L, 64 * 1024);
    } else if (!strcmp(name, "freeram")) {
        lua_pushinteger(L, 63 * 1024);
    } else if (!strcmp(name, "usedram")) {
        lua_pushinteger(L, 1024);
    } else if (!strcmp(name, "slowmotion")) {
        lua_pushinteger(L, slowmotion[0]);
    } else if (!strcmp(name, "slowmotion_duration")) {
        lua_pushinteger(L, slowmotion[1]);
    } else if (!strcmp(name, "models_loaded")) {
        lua_pushinteger(L, (lua_Integer)models_loaded);
    } else if (!strcmp(name, "models_cached")) {
        lua_pushinteger(L, (lua_Integer)models_cached);
    } else if (!strcmp(name, "maxplayers")) {
        int set_index = current_set >= 0 ? current_set : 0;
        if (set_index >= MAX_DIFFICULTIES)
            return luaL_error(L, "openborvariant: 'maxplayers' current_set is out of range");
        lua_pushinteger(L, maxplayers[set_index]);
    } else if (!strcmp(name, "player") || !strcmp(name, "player1") ||
               !strcmp(name, "player2") || !strcmp(name, "player3") ||
               !strcmp(name, "player4")) {
        int index = !strcmp(name, "player") ? 0 : name[6] - '1';
        entity *player_ent = bor_lua_get_live_entity_ptr(player[index].ent);
        if (player_ent && player_ent->exists)
            bor_lua_push_entity(L, player_ent);
        else
            lua_pushnil(L);
    } else if (!strcmp(name, "count_enemies")) {
        lua_pushinteger(L, count_ents(TYPE_ENEMY));
    } else if (!strcmp(name, "count_npcs")) {
        lua_pushinteger(L, count_ents(TYPE_NPC));
    } else if (!strcmp(name, "count_players")) {
        lua_pushinteger(L, count_ents(TYPE_PLAYER));
    } else if (!strcmp(name, "count_entities")) {
        lua_pushinteger(L, ent_count);
    } else if (!strcmp(name, "game_paused")) {
        lua_pushboolean(L, pause != 0);
    } else if (!strcmp(name, "levelwidth")) {
        if (!level) return luaL_error(L, "openborvariant: 'levelwidth' requires a loaded level");
        lua_pushinteger(L, level->width);
    } else if (!strcmp(name, "levelheight")) {
        if (!level) return luaL_error(L, "openborvariant: 'levelheight' requires a loaded level");
        lua_pushinteger(L, panel_height);
    } else if (!strcmp(name, "numpalettes")) {
        if (!level) return luaL_error(L, "openborvariant: 'numpalettes' requires a loaded level");
        lua_pushinteger(L, level->numpalettes);
    } else if (!strcmp(name, "gfx_y_offset")) {
        if (!level) return luaL_error(L, "openborvariant: 'gfx_y_offset' requires a loaded level");
        lua_pushinteger(L, gfx_y_offset);
    } else if (!strcmp(name, "game_speed")) {
        if (!level) return luaL_error(L, "openborvariant: 'game_speed' requires a loaded level");
        lua_pushinteger(L, GAME_SPEED);
    } else if (!strcmp(name, "player_max_z")) {
        lua_pushinteger(L, PLAYER_MAX_Z);
    } else if (!strcmp(name, "player_min_z")) {
        lua_pushinteger(L, PLAYER_MIN_Z);
    } else {
        return luaL_error(L, "openborvariant: unsupported or unimplemented property '%s'", name);
    }

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
    int cache_index;

    if (!ent) {
        lua_pushnil(L);
        return;
    }

    /*
     * Reuse the Lua wrapper while it is still referenced. This preserves
     * Lua identity (including references stored in global variables) when
     * the same live engine entity is returned by different API functions.
     * The registry table uses weak values so it does not keep wrappers alive.
     */
    lua_getfield(L, LUA_REGISTRYINDEX, "OpenBOR.EntityCache");
    cache_index = lua_gettop(L);
    lua_pushlightuserdata(L, ent);
    lua_rawget(L, cache_index);
    if (!lua_isnil(L, -1)) {
        lua_remove(L, cache_index); /* cache table; leave the wrapper */
        return;
    }
    lua_pop(L, 1); /* cache miss */

    handle = (bor_lua_entity_handle *)lua_newuserdata(L, sizeof(*handle));
    handle->ptr = ent;
    luaL_getmetatable(L, BOR_LUA_ENTITY_METATABLE);
    lua_setmetatable(L, -2);

    /* cache[ent] = wrapper */
    lua_pushlightuserdata(L, ent);
    lua_pushvalue(L, -2);
    lua_rawset(L, cache_index);

    lua_remove(L, cache_index); /* leave only the new wrapper on the stack */
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
        if (which < 0 || which >= MAX_ATKS)
            lua_pushnil(L);
        else lua_pushnumber(L, ent->modeldata.offense_factors[which]);
    }
    else if (!strcmp(prop, "defense")) {
        int which = (int)luaL_optinteger(L, 3, 0);
        const char *field = luaL_optstring(L, 4, "factor");
        if (which < 0 || which >= MAX_ATKS)
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
static int lua_legacy_entity_type_mask(lua_State *L, int index, int *is_ground)
{
    const char *name = luaL_checkstring(L, index);

    *is_ground = 0;
    if (!strcmp(name, "ground")) {
        *is_ground = 1;
        return 0;
    }
    if (!strcmp(name, "enemy")) return TYPE_ENEMY;
    if (!strcmp(name, "npc")) return TYPE_NPC;
    if (!strcmp(name, "obstacle")) return TYPE_OBSTACLE;
    if (!strcmp(name, "player")) return TYPE_PLAYER;
    if (!strcmp(name, "shot")) return TYPE_SHOT;

    luaL_error(L, "changeentityproperty: unknown entity type '%s'", name);
    return 0;
}

static int lua_openbor_changeentityproperty(lua_State *L) {
    entity *ent = bor_lua_get_live_entity(L, 1);
    const char *prop;
    lua_Integer ivalue;

    if (!ent || !ent->exists) {
        lua_pushboolean(L, 0);
        return 1;
    }

    prop = luaL_checkstring(L, 2);

    if (!strcmp(prop, "type")) {
        ent->modeldata.type = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "candamage") || !strcmp(prop, "hostile") ||
               !strcmp(prop, "projectilehit")) {
        int i;
        int mask = 0;
        int ground = 0;
        for (i = 3; i <= lua_gettop(L); i++) {
            int is_ground = 0;
            mask |= lua_legacy_entity_type_mask(L, i, &is_ground);
            if (is_ground) ground = 1;
        }
        if (!strcmp(prop, "candamage")) {
            ent->modeldata.candamage = (short)mask;
            if (ground) ent->modeldata.ground = 1;
        } else if (!strcmp(prop, "hostile")) {
            ent->modeldata.hostile = (short)mask;
        } else {
            ent->modeldata.projectilehit = (short)mask;
        }
    } else if (!strcmp(prop, "health")) {
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
    } else if (!strcmp(prop, "weapon")) {
        int weapon = (int)luaL_checkinteger(L, 3);
        int anim_flag = lua_isnoneornil(L, 4) ? 0 : (int)luaL_checkinteger(L, 4);
        set_weapon(ent, weapon, anim_flag);
    } else if (!strcmp(prop, "trymove")) {
        int mode = (int)luaL_checkinteger(L, 3);
        if (mode == 1) ent->trymove = common_trymove;
        else if (mode == 2) ent->trymove = player_trymove;
        else ent->trymove = NULL;
    } else if (!strcmp(prop, "combostep")) {
        int index = (int)luaL_checkinteger(L, 3);
        if (index < 0 || index >= MAX_SPECIAL_INPUTS) return luaL_error(L, "combostep index out of range");
        ent->combostep[index] = (unsigned char)luaL_checkinteger(L, 4);
    } else if (!strcmp(prop, "mpset")) {
        ent->modeldata.mp = (int)luaL_checknumber(L, 3);
        if (!lua_isnoneornil(L, 4)) ent->modeldata.mpstable = (int)luaL_checknumber(L, 4);
        if (!lua_isnoneornil(L, 5)) ent->modeldata.mpstableval = (int)luaL_checknumber(L, 5);
        if (!lua_isnoneornil(L, 6)) ent->modeldata.mprate = (int)luaL_checknumber(L, 6);
    } else if (!strcmp(prop, "grabforce")) {
        ent->model->grabforce = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "iconposition")) {
        ent->modeldata.iconx = (short)luaL_checkinteger(L, 3);
        if (!lua_isnoneornil(L, 4)) ent->modeldata.icony = (short)luaL_checkinteger(L, 4);
    } else if (!strcmp(prop, "lifeposition")) {
        ent->modeldata.hpx = (short)luaL_checkinteger(L, 3);
        if (!lua_isnoneornil(L, 4)) ent->modeldata.hpy = (short)luaL_checkinteger(L, 4);
    } else if (!strcmp(prop, "nameposition")) {
        ent->modeldata.namex = (short)luaL_checkinteger(L, 3);
        if (!lua_isnoneornil(L, 4)) ent->modeldata.namey = (short)luaL_checkinteger(L, 4);
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
        ent->modeldata.guardpoints[0] = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "maxguardpoints")) {
        ent->modeldata.guardpoints[1] = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "jugglepoints")) {
        ent->modeldata.jugglepoints[0] = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "maxjugglepoints")) {
        ent->modeldata.jugglepoints[1] = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "komap")) {
        ent->modeldata.komap[0] = (char)luaL_checkinteger(L, 3);
        if (!lua_isnoneornil(L, 4)) ent->modeldata.komap[1] = (char)luaL_checkinteger(L, 4);
    } else if (!strcmp(prop, "rush_count")) {
        ent->rush[0] = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "rush_tally")) {
        ent->rush[1] = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "rush_time")) {
        ent->rushtime = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(prop, "offense")) {
        int which = (int)luaL_checkinteger(L, 3);
        if (which < 0 || which >= MAX_ATKS) return luaL_error(L, "offense index out of range");
        ent->modeldata.offense_factors[which] = (float)luaL_checknumber(L, 4);
    } else if (!strcmp(prop, "defense")) {
        int which = (int)luaL_checkinteger(L, 3);
        if (which < 0 || which >= MAX_ATKS) return luaL_error(L, "defense index out of range");
        ent->modeldata.defense_factors[which] = (float)luaL_checknumber(L, 4);
        if (!lua_isnoneornil(L, 5)) ent->modeldata.defense_pain[which] = (float)luaL_checknumber(L, 5);
        if (!lua_isnoneornil(L, 6)) ent->modeldata.defense_knockdown[which] = (float)luaL_checknumber(L, 6);
        if (!lua_isnoneornil(L, 7)) ent->modeldata.defense_blockpower[which] = (float)luaL_checknumber(L, 7);
        if (!lua_isnoneornil(L, 8)) ent->modeldata.defense_blockthreshold[which] = (float)luaL_checknumber(L, 8);
        if (!lua_isnoneornil(L, 9)) ent->modeldata.defense_blockratio[which] = (float)luaL_checknumber(L, 9);
        if (!lua_isnoneornil(L, 10)) ent->modeldata.defense_blocktype[which] = (float)luaL_checknumber(L, 10);
    } else if (!strcmp(prop, "stats")) {
        int which = (int)luaL_checkinteger(L, 3);
        int index = (int)luaL_checkinteger(L, 4);
        if (index < 0 || index >= 20) return luaL_error(L, "stats index out of range");
        if (which == 1) ent->stats[index] = (float)luaL_checknumber(L, 5);
        else ent->modeldata.stats[index] = (float)luaL_checknumber(L, 5);
    } else if (!strcmp(prop, "antigravity")) {
        float value = (float)luaL_checknumber(L, 3);
        ent->modeldata.antigravity = value;
        ent->antigravity = value;
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
    else if (!strcmp(prop, "staydown")) {
        ent->staydown[0] = (int)luaL_checkinteger(L, 3);
        if (!lua_isnoneornil(L, 4)) ent->staydown[1] = (int)luaL_checkinteger(L, 4);
        if (!lua_isnoneornil(L, 5)) ent->staydown[2] = (int)luaL_checkinteger(L, 5);
    } else if (!strcmp(prop, "stealth")) {
        ent->modeldata.stealth[0] = (short)luaL_checkinteger(L, 3);
        if (!lua_isnoneornil(L, 4)) ent->modeldata.stealth[1] = (short)luaL_checkinteger(L, 4);
    }
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

/*
 * Callback-scoped legacy local variables.
 * These values are installed by the engine immediately around a callback.
 */
#define BOR_LUA_LOCAL_VARS "OpenBOR.LocalVars"

void bor_lua_clear_localvars(lua_State *L)
{
    if (!L) return;
    lua_pushnil(L);
    lua_setfield(L, LUA_REGISTRYINDEX, BOR_LUA_LOCAL_VARS);
}

void bor_lua_set_localvar(lua_State *L, const char *name, int index)
{
    int abs_index;

    if (!L || !name) return;
    abs_index = (index > 0 || index <= LUA_REGISTRYINDEX)
        ? index : lua_gettop(L) + index + 1;

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_LOCAL_VARS);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, BOR_LUA_LOCAL_VARS);
    }

    lua_pushvalue(L, abs_index);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
}

/*
 * Legacy getlocalvar() compatibility. Callback-scoped values take precedence;
 * self and player retain their existing fallback behavior.
 */
static int lua_openbor_getlocalvar(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_LOCAL_VARS);
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, name);
        if (!lua_isnil(L, -1)) {
            lua_remove(L, -2);
            return 1;
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);

    if (!strcmp(name, "self")) {
        if (self && self->exists)
            bor_lua_push_entity(L, self);
        else
            lua_pushnil(L);
        return 1;
    }

    if (!strcmp(name, "player")) {
        if (g_lua_current_player_index >= 0)
            lua_pushinteger(L, (lua_Integer)g_lua_current_player_index);
        else
            lua_pushnil(L);
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

#define BOR_LUA_GLOBAL_VARS "OpenBOR.GlobalVars"
#define BOR_LUA_INDEXED_VARS "OpenBOR.IndexedVars"

static int lua_openbor_getindexedvar(lua_State *L)
{
    lua_Integer index = luaL_checkinteger(L, 1);

    if (index < 0) {
        lua_pushnil(L);
        return 1;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_INDEXED_VARS);
    lua_pushinteger(L, index);
    lua_rawget(L, -2);
    lua_remove(L, -2);
    return 1;
}

static int lua_openbor_setindexedvar(lua_State *L)
{
    lua_Integer index = luaL_checkinteger(L, 1);

    if (lua_gettop(L) < 2)
        return luaL_error(L, "setindexedvar expects an index and value");
    if (index < 0) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_INDEXED_VARS);
    lua_pushinteger(L, index);
    lua_pushvalue(L, 2);
    lua_rawset(L, -3);
    lua_pop(L, 1);

    lua_pushinteger(L, 1);
    return 1;
}

static int lua_openbor_clearindexedvar(lua_State *L)
{
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, BOR_LUA_INDEXED_VARS);
    return 0;
}

/* Legacy utility and variable functions retained from the original script API. */
static int lua_openbor_isempty(lua_State *L)
{
    if (lua_gettop(L) != 1)
        return luaL_error(L, "isempty expects exactly 1 argument");

    lua_pushinteger(L, lua_isnil(L, 1) ? 1 : 0);
    return 1;
}

static int lua_openbor_null(lua_State *L)
{
    (void)L;
    lua_pushnil(L);
    return 1;
}

static int lua_openbor_maxglobalvarindex(lua_State *L)
{
    int count = 0;

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_GLOBAL_VARS);
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2) != 0) {
            ++count;
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    lua_pushinteger(L, count - 1);
    return 1;
}

static int lua_openbor_setlocalvar(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);

    if (lua_gettop(L) < 2)
        return luaL_error(L, "setlocalvar expects a name and value");

    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_LOCAL_VARS);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, BOR_LUA_LOCAL_VARS);
    }

    lua_pushstring(L, name);
    lua_pushvalue(L, 2);
    lua_rawset(L, -3);
    lua_pop(L, 1);

    lua_pushinteger(L, 1);
    return 1;
}

static int lua_openbor_clearlocalvar(lua_State *L)
{
    lua_pushnil(L);
    lua_setfield(L, LUA_REGISTRYINDEX, BOR_LUA_LOCAL_VARS);
    return 0;
}

static int lua_openbor_clearglobalvar(lua_State *L)
{
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, BOR_LUA_GLOBAL_VARS);
    return 0;
}

/*
 * Legacy global-variable API.
 * Values are kept in a Lua registry table so entity userdata and other Lua
 * values retain their identity when stored and retrieved across callbacks.
 */

static int lua_openbor_globalvar(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_GLOBAL_VARS);
    lua_pushstring(L, name);
    lua_rawget(L, -2);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushstring(L, name);
        lua_pushinteger(L, 0);
        lua_rawset(L, -3);
    } else {
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return 0;
}

static int lua_openbor_getglobalvar(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_GLOBAL_VARS);
    lua_pushstring(L, name);
    lua_rawget(L, -2);
    lua_remove(L, -2);
    return 1;
}

static int lua_openbor_setglobalvar(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    luaL_checkany(L, 2);
    lua_getfield(L, LUA_REGISTRYINDEX, BOR_LUA_GLOBAL_VARS);
    lua_pushstring(L, name);
    lua_pushvalue(L, 2);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    lua_pushinteger(L, 1);
    return 1;
}

/* File-stream helpers retain the original level-scoped stream handles. */
static s_filestream *bor_lua_get_filestream(lua_State *L, int arg)
{
    lua_Integer index = luaL_checkinteger(L, arg);

    if (!level)
        luaL_error(L, "file stream operation requires a loaded level");
    if (index < 0 || index >= level->numfilestreams ||
        index >= LEVEL_MAX_FILESTREAMS || !level->filestreams[index].buf)
        luaL_error(L, "invalid file stream index");

    return &level->filestreams[index];
}

static int lua_openbor_openfilestream(lua_State *L)
{
    const char *filename = luaL_checkstring(L, 1);
    int location = (int)luaL_optinteger(L, 2, 0);
    s_filestream *stream;
    char path[512] = {0};
    char *buffer = NULL;
    size_t size = 0;

    if (!level)
        return luaL_error(L, "openfilestream requires a loaded level");
    if (level->numfilestreams >= LEVEL_MAX_FILESTREAMS)
        return luaL_error(L, "maximum file streams exceeded");

    if (location) {
        FILE *handle;
        long file_size;

        getBasePath(path, "Saves", 0);
        if (strlen(path) + strlen(filename) + 2 >= sizeof(path))
            return luaL_error(L, "file stream path is too long");
        strcat(path, "/");
        strcat(path, filename);

        handle = fopen(path, "rb");
        if (!handle)
            return luaL_error(L, "openfilestream could not open '%s'", path);
        if (fseek(handle, 0, SEEK_END) != 0 ||
            (file_size = ftell(handle)) < 0 ||
            fseek(handle, 0, SEEK_SET) != 0) {
            fclose(handle);
            return luaL_error(L, "openfilestream could not read '%s'", path);
        }

        size = (size_t)file_size;
        buffer = (char *)malloc(size + 1);
        if (!buffer) {
            fclose(handle);
            return luaL_error(L, "openfilestream ran out of memory");
        }
        if (size && fread(buffer, 1, size, handle) != size) {
            free(buffer);
            fclose(handle);
            return luaL_error(L, "openfilestream could not read '%s'", path);
        }
        fclose(handle);
        buffer[size] = '\0';
    } else {
        if (buffer_pakfile((char *)filename, &buffer, &size) != 1 || !buffer)
            return luaL_error(L, "invalid filename for openfilestream: '%s'", filename);
        if (size == 0)
            size = strlen(buffer);
    }

    stream = &level->filestreams[level->numfilestreams];
    stream->buf = buffer;
    stream->size = size;
    stream->pos = 0;
    lua_pushinteger(L, level->numfilestreams);
    ++level->numfilestreams;
    return 1;
}

static int lua_openbor_createfilestream(lua_State *L)
{
    s_filestream *stream;

    (void)L;
    if (!level)
        return luaL_error(L, "createfilestream requires a loaded level");
    if (level->numfilestreams >= LEVEL_MAX_FILESTREAMS)
        return luaL_error(L, "maximum file streams exceeded");

    stream = &level->filestreams[level->numfilestreams];
    stream->buf = (char *)malloc(1);
    if (!stream->buf)
        return luaL_error(L, "createfilestream ran out of memory");
    stream->buf[0] = '\0';
    stream->size = 0;
    stream->pos = 0;

    lua_pushinteger(L, level->numfilestreams);
    ++level->numfilestreams;
    return 1;
}

static int lua_openbor_getfilestreamline(lua_State *L)
{
    s_filestream *stream = bor_lua_get_filestream(L, 1);
    size_t end;

    if (stream->pos < 0 || (size_t)stream->pos > stream->size)
        return luaL_error(L, "file stream position is out of range");
    end = (size_t)stream->pos;

    while (end < stream->size && stream->buf[end] != '\n' &&
           stream->buf[end] != '\r')
        ++end;

    lua_pushlstring(L, stream->buf + stream->pos, end - (size_t)stream->pos);
    return 1;
}

static int lua_openbor_getfilestreamargument(lua_State *L)
{
    s_filestream *stream = bor_lua_get_filestream(L, 1);
    lua_Integer argument = luaL_checkinteger(L, 2);
    const char *type = luaL_checkstring(L, 3);
    size_t end = (size_t)stream->pos;
    char *line;
    char *value;

    if (stream->pos < 0 || (size_t)stream->pos > stream->size)
        return luaL_error(L, "file stream position is out of range");
    if (argument < 0)
        return luaL_error(L, "file stream argument index cannot be negative");

    while (end < stream->size && stream->buf[end] != '\n' &&
           stream->buf[end] != '\r')
        ++end;
    line = (char *)malloc(end - (size_t)stream->pos + 1);
    if (!line)
        return luaL_error(L, "getfilestreamargument ran out of memory");
    memcpy(line, stream->buf + stream->pos, end - (size_t)stream->pos);
    line[end - (size_t)stream->pos] = '\0';

    value = findarg(line, (int)argument);
    if (bor_lua_stricmp(type, "string") == 0) {
        lua_pushstring(L, value ? value : "");
    } else if (bor_lua_stricmp(type, "int") == 0) {
        lua_pushinteger(L, value ? (lua_Integer)atoi(value) : 0);
    } else if (bor_lua_stricmp(type, "float") == 0) {
        lua_pushnumber(L, value ? (lua_Number)atof(value) : 0);
    } else {
        free(line);
        return luaL_error(L, "getfilestreamargument type must be string, int, or float");
    }

    free(line);
    return 1;
}

static int lua_openbor_filestreamnextline(lua_State *L)
{
    s_filestream *stream = bor_lua_get_filestream(L, 1);
    size_t pos;

    if (stream->pos < 0 || (size_t)stream->pos > stream->size)
        return luaL_error(L, "file stream position is out of range");

    pos = (size_t)stream->pos;
    while (pos < stream->size && stream->buf[pos] != '\n' &&
           stream->buf[pos] != '\r')
        ++pos;
    while (pos < stream->size && (stream->buf[pos] == '\n' ||
           stream->buf[pos] == '\r'))
        ++pos;

    stream->pos = (int)pos;
    return 0;
}

static int lua_openbor_getfilestreamposition(lua_State *L)
{
    s_filestream *stream = bor_lua_get_filestream(L, 1);
    lua_pushinteger(L, stream->pos);
    return 1;
}

static int lua_openbor_setfilestreamposition(lua_State *L)
{
    s_filestream *stream = bor_lua_get_filestream(L, 1);
    lua_Integer position = luaL_checkinteger(L, 2);

    if (position < 0 || (lua_Unsigned)position > stream->size)
        return luaL_error(L, "file stream position is out of range");
    stream->pos = (int)position;
    return 0;
}

static int lua_openbor_savefilestream(lua_State *L)
{
    s_filestream *stream = bor_lua_get_filestream(L, 1);
    const char *filename = luaL_checkstring(L, 2);
    char path[512] = {0};
    char modname[128] = {0};
    const char *base;
    const char *slash;
    size_t mod_length;
    FILE *handle;

    if (lua_gettop(L) != 2)
        return luaL_error(L, "savefilestream expects a stream index and filename");

    base = strrchr(packfile, '/');
    slash = strrchr(packfile, '\\');
    if (!base || (slash && slash > base))
        base = slash;
    base = base ? base + 1 : packfile;

    mod_length = strlen(base);
    if (mod_length >= 4 && base[mod_length - 4] == '.')
        mod_length -= 4;
    if (mod_length >= sizeof(modname))
        mod_length = sizeof(modname) - 1;
    memcpy(modname, base, mod_length);
    modname[mod_length] = '\0';

    getBasePath(path, "Saves/", 0);
    if (strlen(path) + strlen(modname) + 2 >= sizeof(path))
        return luaL_error(L, "savefilestream path is too long");
    if (path[0] && path[strlen(path) - 1] == '/')
        path[strlen(path) - 1] = '\0';
    strcat(path, "/");
    strcat(path, modname);
    dirExists(path, 1);

    if (strlen(path) + strlen(filename) + 2 >= sizeof(path))
        return luaL_error(L, "savefilestream path is too long");
    strcat(path, "/");
    strcat(path, filename);

    handle = fopen(path, "wb");
    if (!handle)
        return luaL_error(L, "savefilestream could not open '%s' for writing", path);

    if (stream->size && fwrite(stream->buf, 1, stream->size, handle) != stream->size) {
        fclose(handle);
        return luaL_error(L, "savefilestream could not write '%s'", path);
    }
    if (fwrite("\r\n", 1, 2, handle) != 2) {
        fclose(handle);
        return luaL_error(L, "savefilestream could not finish writing '%s'", path);
    }
    fclose(handle);
    return 0;
}

static int lua_openbor_filestreamappend(lua_State *L)
{
    s_filestream *stream = bor_lua_get_filestream(L, 1);
    size_t append_length;
    size_t extra;
    const char *append;
    char *new_buffer;
    int append_type = (int)luaL_checkinteger(L, 3);

    if (append_type != 0 && append_type != 1)
        return luaL_error(L, "filestreamappend type must be 0 or 1");

    luaL_tolstring(L, 2, &append_length);
    append = lua_tostring(L, -1);
    extra = append_length + (append_type == 0 ? 3 : 1);

    new_buffer = (char *)realloc(stream->buf, stream->size + extra + 1);
    if (!new_buffer)
        return luaL_error(L, "filestreamappend ran out of memory");
    stream->buf = new_buffer;

    if (append_type == 0) {
        stream->buf[stream->size++] = '\r';
        stream->buf[stream->size++] = '\n';
    }
    memcpy(stream->buf + stream->size, append, append_length);
    stream->size += append_length;
    stream->buf[stream->size++] = ' ';
    stream->buf[stream->size] = '\0';
    lua_pop(L, 1);
    return 0;
}

static int lua_openbor_loadsample(lua_State *L)
{
    const char *filename = luaL_checkstring(L, 1);
    int log_errors = (int)luaL_optinteger(L, 2, 0);
    int sample;

    if (lua_gettop(L) < 1 || lua_gettop(L) > 2)
        return luaL_error(L, "loadsample expects a filename and optional log flag");

    sample = sound_load_sample((char *)filename, packfile, log_errors);
    lua_pushinteger(L, sample);
    return 1;
}

static int lua_openbor_free(lua_State *L)
{
    if (lua_gettop(L) < 1)
        return luaL_error(L, "free expects at least 1 argument");

    /* Lua owns script values; explicit legacy free requests trigger collection. */
    lua_gc(L, LUA_GCCOLLECT, 0);
    return 0;
}

static int lua_openbor_fadeout(lua_State *L)
{
    int type;
    int speed;

    if (lua_gettop(L) != 2)
        return luaL_error(L, "fadeout expects 2 integer arguments");

    type = (int)luaL_checkinteger(L, 1);
    speed = (int)luaL_checkinteger(L, 2);
    fade_out(type, speed);
    return 0;
}

/* Legacy music and sample controls. */
static int lua_openbor_playmusic(lua_State *L)
{
    if (lua_gettop(L) == 0) {
        sound_close_music();
        return 0;
    }
    if (lua_gettop(L) > 3)
        return luaL_error(L, "playmusic expects a filename and up to 2 optional arguments");

    music((char *)luaL_checkstring(L, 1),
          lua_toboolean(L, 2),
          (long)luaL_optinteger(L, 3, 0));
    return 0;
}

static int lua_openbor_fademusic(lua_State *L)
{
    if (lua_gettop(L) < 1 || lua_gettop(L) > 4)
        return luaL_error(L, "fademusic expects a fade value and optional filename, loop, and offset");

    musicfade[0] = (float)luaL_checknumber(L, 1);
    musicfade[1] = (float)savedata.musicvol;

    if (lua_gettop(L) == 4) {
        const char *name = luaL_checkstring(L, 2);
        strncpy(musicname, name, sizeof(musicname) - 1);
        musicname[sizeof(musicname) - 1] = '\0';
        musicloop = (int)luaL_checkinteger(L, 3);
        musicoffset = (u32)luaL_checkinteger(L, 4);
    }
    return 0;
}

static int lua_openbor_setmusicvolume(lua_State *L)
{
    int left;
    int right;

    if (lua_gettop(L) == 0)
        return 0;
    if (lua_gettop(L) > 2)
        return luaL_error(L, "setmusicvolume expects 1 or 2 integer values");

    left = (int)luaL_checkinteger(L, 1);
    right = (int)luaL_optinteger(L, 2, left);
    sound_volume_music(left, right);
    return 0;
}

static int lua_openbor_setmusictempo(lua_State *L)
{
    if (lua_gettop(L) > 1)
        return luaL_error(L, "setmusictempo expects at most 1 integer value");
    if (lua_gettop(L) == 1)
        sound_music_tempo((int)luaL_checkinteger(L, 1));
    return 0;
}

static int lua_openbor_pausemusic(lua_State *L)
{
    if (lua_gettop(L) > 1)
        return luaL_error(L, "pausemusic expects at most 1 boolean value");
    if (lua_gettop(L) == 1)
        sound_pause_music(lua_toboolean(L, 1));
    return 0;
}

static int lua_openbor_playsample(lua_State *L)
{
    int id, priority, left, right, speed, loop;

    if (lua_gettop(L) != 6)
        return luaL_error(L, "playsample expects 6 integer arguments");

    id = (int)luaL_checkinteger(L, 1);
    priority = (int)luaL_checkinteger(L, 2);
    left = (int)luaL_checkinteger(L, 3);
    right = (int)luaL_checkinteger(L, 4);
    speed = (int)luaL_checkinteger(L, 5);
    loop = (int)luaL_checkinteger(L, 6);

    if (id < 0)
        return luaL_error(L, "playsample sample ID cannot be negative");

    if (loop)
        sound_loop_sample(id, (unsigned int)priority, left, right, (unsigned int)speed);
    else
        sound_play_sample(id, (unsigned int)priority, left, right, (unsigned int)speed);
    return 0;
}

static int lua_openbor_unloadsample(lua_State *L)
{
    if (lua_gettop(L) != 1)
        return luaL_error(L, "unloadsample expects exactly 1 integer argument");
    sound_unload_sample((int)luaL_checkinteger(L, 1));
    return 0;
}

/* Legacy engine state and entity action helpers. */
static int lua_openbor_changelight(lua_State *L)
{
    if (lua_gettop(L) < 2)
        return luaL_error(L, "changelight expects x and z values");

    if (!lua_isnil(L, 1))
        light[0] = (int)luaL_checkinteger(L, 1);
    if (!lua_isnil(L, 2))
        light[1] = (int)luaL_checkinteger(L, 2);
    return 0;
}

static int lua_openbor_changeshadowcolor(lua_State *L)
{
    if (lua_gettop(L) < 1 || lua_gettop(L) > 2)
        return luaL_error(L, "changeshadowcolor expects a color and optional alpha");

    colors.shadow = (int)luaL_checkinteger(L, 1);
    if (!lua_isnoneornil(L, 2))
        shadowalpha = (int)luaL_checkinteger(L, 2);
    return 0;
}

static int lua_openbor_changepalette(lua_State *L)
{
    int index = (int)luaL_checkinteger(L, 1);
    change_system_palette(index);
    return 0;
}

static int lua_openbor_jumptobranch(lua_State *L)
{
    extern int endgame;
    const char *name = luaL_checkstring(L, 1);

    if (lua_gettop(L) < 1 || lua_gettop(L) > 2)
        return luaL_error(L, "jumptobranch expects a branch name and optional immediate flag");

    strncpy(branch_name, name, MAX_NAME_LEN);
    branch_name[MAX_NAME_LEN] = '\0';
    if (!lua_isnoneornil(L, 2))
        endgame = (int)luaL_checkinteger(L, 2);
    return 0;
}

static int lua_openbor_bindentity(lua_State *L)
{
    entity *ent = bor_lua_get_live_entity(L, 1);
    entity *other = bor_lua_get_live_entity(L, 2);
    int i;
    int values[5] = {0, 0, 0, 0, 0};
    extern void adjust_bind(entity *e);

    if (lua_gettop(L) < 2 || lua_gettop(L) > 7)
        return luaL_error(L, "bindentity expects 2 entities and up to 5 optional offsets");

    if (!ent || !ent->exists)
        return 0;
    if (!other || !other->exists) {
        ent->bound = NULL;
        return 0;
    }

    ent->bound = other;
    for (i = 3; i <= lua_gettop(L); ++i) {
        if (!lua_isnil(L, i))
            values[i - 3] = (int)luaL_checkinteger(L, i);
    }
    if (lua_gettop(L) >= 3 && !lua_isnil(L, 3)) ent->bindoffset[0] = values[0];
    if (lua_gettop(L) >= 4 && !lua_isnil(L, 4)) ent->bindoffset[1] = values[1];
    if (lua_gettop(L) >= 5 && !lua_isnil(L, 5)) ent->bindoffset[2] = values[2];
    if (lua_gettop(L) >= 6 && !lua_isnil(L, 6)) ent->bindoffset[3] = values[3];
    if (lua_gettop(L) >= 7 && !lua_isnil(L, 7)) ent->bindanim = values[4];

    adjust_bind(ent);
    return 0;
}

static int lua_openbor_performattack(lua_State *L)
{
    entity *ent = bor_lua_get_live_entity(L, 1);
    int animation;
    int resetable;

    if (lua_gettop(L) < 1 || lua_gettop(L) > 3)
        return luaL_error(L, "performattack expects an entity and up to 2 optional integers");
    if (!ent || !ent->exists)
        return luaL_error(L, "performattack expects a live entity");

    ent->takeaction = common_attack_proc;
    ent->attacking = 1;
    ent->idling = 0;
    ent->drop = 0;
    ent->falling = 0;
    ent->inpain = 0;
    ent->blocking = 0;

    if (lua_gettop(L) >= 2) {
        animation = (int)luaL_checkinteger(L, 2);
        resetable = (int)luaL_optinteger(L, 3, 0);
        ent_set_anim(ent, animation, resetable);
    }
    return 0;
}

static int lua_openbor_setidle(lua_State *L)
{
    entity *ent = bor_lua_get_live_entity(L, 1);
    int animation = 0;
    int resetable = 0;
    int stalladd = 0;

    if (lua_gettop(L) < 1 || lua_gettop(L) > 4)
        return luaL_error(L, "setidle expects an entity and up to 3 optional integers");
    if (!ent || !ent->exists)
        return luaL_error(L, "setidle expects a live entity");

    ent->takeaction = NULL;
    ent->attacking = 0;
    ent->idling = 1;
    ent->drop = 0;
    ent->falling = 0;
    ent->inpain = 0;
    ent->blocking = 0;
    ent->nograb = 0;

    if (lua_gettop(L) >= 2) {
        animation = (int)luaL_checkinteger(L, 2);
        resetable = (int)luaL_optinteger(L, 3, 0);
        stalladd = (int)luaL_optinteger(L, 4, 0);
        ent_set_anim(ent, animation, resetable);
    }
    if (stalladd > 0)
        ent->stalltime = borTime + (u32)stalladd;
    return 0;
}

static int lua_openbor_updateframe(lua_State *L)
{
    entity *ent = bor_lua_get_live_entity(L, 1);
    int frame = (int)luaL_checkinteger(L, 2);
    void update_frame(entity *ent, int frame);

    if (lua_gettop(L) != 2)
        return luaL_error(L, "updateframe expects an entity and frame number");
    if (!ent || !ent->exists)
        return luaL_error(L, "updateframe expects a live entity");

    update_frame(ent, frame);
    return 0;
}

/* Legacy player property accessors. */
static int lua_openbor_getplayerproperty(lua_State *L)
{
    extern s_player player[4];
    int index = (int)luaL_optinteger(L, 1, 0);
    const char *property = luaL_checkstring(L, 2);
    entity *ent;

    if (index < 0 || index >= 4)
        return luaL_error(L, "getplayerproperty player index is out of range");

    ent = player[index].ent;
    if (!ent) {
        lua_pushnil(L);
        return 1;
    }

    if (!strcmp(property, "ent") || !strcmp(property, "entity")) {
        bor_lua_push_entity(L, ent);
    } else if (!strcmp(property, "name")) {
        lua_pushstring(L, player[index].name);
    } else if (!strcmp(property, "score")) {
        lua_pushinteger(L, (lua_Integer)player[index].score);
    } else if (!strcmp(property, "lives")) {
        lua_pushinteger(L, (lua_Integer)player[index].lives);
    } else if (!strcmp(property, "playkeys")) {
        lua_pushinteger(L, (lua_Integer)player[index].playkeys);
    } else if (!strcmp(property, "keys")) {
        lua_pushinteger(L, (lua_Integer)player[index].keys);
    } else if (!strcmp(property, "credits")) {
        lua_pushinteger(L, noshare ? player[index].credits : credits);
    } else if (!strcmp(property, "weaponum")) {
        lua_pushinteger(L, (lua_Integer)player[index].weapnum);
    } else {
        return luaL_error(L, "getplayerproperty: unsupported property '%s'", property);
    }

    return 1;
}

static int lua_openbor_changeplayerproperty(lua_State *L)
{
    extern s_player player[4];
    int index = (int)luaL_optinteger(L, 1, 0);
    const char *property = luaL_checkstring(L, 2);
    entity *ent;
    lua_Integer value;

    if (lua_gettop(L) < 3)
        return luaL_error(L, "changeplayerproperty expects player, property, and value");
    if (index < 0 || index >= 4)
        return luaL_error(L, "changeplayerproperty player index is out of range");

    ent = player[index].ent;
    if (!ent) {
        lua_pushinteger(L, 1);
        return 1;
    }

    if (!strcmp(property, "weapon")) {
        int weapon = (int)luaL_checkinteger(L, 3);
        int animation = (int)luaL_optinteger(L, 4, 0);
        set_weapon(ent, weapon, animation);
    } else if (!strcmp(property, "name")) {
        const char *name = luaL_checkstring(L, 3);
        strncpy(player[index].name, name, sizeof(player[index].name) - 1);
        player[index].name[sizeof(player[index].name) - 1] = '\0';
    } else if (!strcmp(property, "score")) {
        value = luaL_checkinteger(L, 3);
        if (value < 0) value = 0;
        if (value > 999999999) value = 999999999;
        player[index].score = (unsigned int)value;
    } else if (!strcmp(property, "lives")) {
        player[index].lives = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(property, "playkeys")) {
        player[index].playkeys = (int)luaL_checkinteger(L, 3);
    } else if (!strcmp(property, "credits")) {
        value = luaL_checkinteger(L, 3);
        if (noshare)
            player[index].credits = (int)value;
        else
            credits = (int)value;
    } else {
        return luaL_error(L, "changeplayerproperty: unsupported property '%s'", property);
    }

    lua_pushinteger(L, 1);
    return 1;
}

/* Legacy entity targeting, collision queries, and toss helper. */
static int lua_openbor_tossentity(lua_State *L)
{
    entity *ent = bor_lua_get_live_entity(L, 1);
    float height = (float)luaL_optnumber(L, 2, 0);
    float speedx = (float)luaL_optnumber(L, 3, 0);
    float speedz = (float)luaL_optnumber(L, 4, 0);

    if (lua_gettop(L) < 1 || lua_gettop(L) > 4)
        return luaL_error(L, "tossentity expects an entity and up to 3 optional numbers");

    if (!ent || !ent->exists) {
        lua_pushinteger(L, 0);
        return 1;
    }

    ent->xdir = speedx;
    ent->zdir = speedz;
    toss(ent, height);
    lua_pushinteger(L, 1);
    return 1;
}

static int lua_openbor_findtarget(lua_State *L)
{
    entity *ent = bor_lua_get_live_entity(L, 1);
    entity *previous_self;
    entity *target;
    int animation = -1;

    if (lua_gettop(L) < 1 || lua_gettop(L) > 2)
        return luaL_error(L, "findtarget expects an entity and an optional animation");

    if (!ent || !ent->exists) {
        lua_pushnil(L);
        return 1;
    }

    if (!lua_isnoneornil(L, 2))
        animation = (int)luaL_checkinteger(L, 2);

    previous_self = self;
    self = ent;
    target = normal_find_target(animation);
    self = previous_self;

    bor_lua_push_entity(L, target);
    return 1;
}

static int lua_openbor_checkrange(lua_State *L)
{
    entity *ent = bor_lua_get_live_entity(L, 1);
    entity *target = bor_lua_get_live_entity(L, 2);
    int animation;

    if (lua_gettop(L) < 2 || lua_gettop(L) > 3)
        return luaL_error(L, "checkrange expects 2 entities and an optional animation");

    if (!ent || !target || !ent->exists || !target->exists) {
        lua_pushnil(L);
        return 1;
    }

    animation = lua_isnoneornil(L, 3) ? ent->animnum : (int)luaL_checkinteger(L, 3);
    lua_pushinteger(L, check_range(ent, target, animation));
    return 1;
}

static int lua_openbor_checkhole(lua_State *L)
{
    float x = (float)luaL_checknumber(L, 1);
    float z = (float)luaL_checknumber(L, 2);

    if (!level) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, checkhole(x, z) && checkwall(x, z) < 0 ? 1 : 0);
    return 1;
}

static int lua_openbor_checkwall(lua_State *L)
{
    float x = (float)luaL_checknumber(L, 1);
    float z = (float)luaL_checknumber(L, 2);
    int wall;

    if (!level) {
        lua_pushnumber(L, 0);
        return 1;
    }

    wall = checkwall_below(x, z, 100000);
    if (wall >= 0)
        lua_pushnumber(L, level->walls[wall].alt);
    else
        lua_pushnumber(L, 0);
    return 1;
}

static int lua_openbor_checkplatformbelow(lua_State *L)
{
    float x = (float)luaL_checknumber(L, 1);
    float z = (float)luaL_checknumber(L, 2);
    float a = (float)luaL_checknumber(L, 3);
    entity *platform;

    if (!level) {
        lua_pushnil(L);
        return 1;
    }

    platform = check_platform_below(x, z, a);
    bor_lua_push_entity(L, platform);
    return 1;
}

/* Legacy queued drawing primitives. */
static int lua_openbor_drawline(lua_State *L)
{
    int x1 = (int)luaL_checkinteger(L, 1);
    int y1 = (int)luaL_checkinteger(L, 2);
    int x2 = (int)luaL_checkinteger(L, 3);
    int y2 = (int)luaL_checkinteger(L, 4);
    int z = (int)luaL_checkinteger(L, 5);
    int color = (int)luaL_checkinteger(L, 6);
    int lut = (int)luaL_optinteger(L, 7, -1);

    if (lua_gettop(L) < 6 || lua_gettop(L) > 7)
        return luaL_error(L, "drawline expects 6 required arguments and 1 optional LUT");

    if (lut >= 0)
        lut %= MAX_BLENDINGS + 1;

    spriteq_add_line(x1, y1, x2, y2, z, color, lut);
    return 0;
}

static int lua_openbor_drawdot(lua_State *L)
{
    int x = (int)luaL_checkinteger(L, 1);
    int y = (int)luaL_checkinteger(L, 2);
    int z = (int)luaL_checkinteger(L, 3);
    int color = (int)luaL_checkinteger(L, 4);
    int lut = (int)luaL_optinteger(L, 5, -1);

    if (lua_gettop(L) < 4 || lua_gettop(L) > 5)
        return luaL_error(L, "drawdot expects 4 required arguments and 1 optional LUT");

    if (lut >= 0)
        lut %= MAX_BLENDINGS + 1;

    spriteq_add_dot(x, y, z, color, lut);
    return 0;
}

/* Legacy string and colour helpers. */
static int lua_openbor_strinfirst(lua_State *L)
{
    const char *source = luaL_checkstring(L, 1);
    const char *search = luaL_checkstring(L, 2);
    const char *match = strstr(source, search);

    if (match)
        lua_pushstring(L, match);
    else
        lua_pushinteger(L, -1);
    return 1;
}

static int lua_openbor_strinlast(lua_State *L)
{
    const char *source = luaL_checkstring(L, 1);
    const char *search = luaL_checkstring(L, 2);
    const char *match = NULL;
    const char *cursor;

    /* The original implementation searches for the first character of arg 2. */
    if (*search) {
        cursor = source;
        while ((cursor = strchr(cursor, *search)) != NULL) {
            match = cursor++;
        }
    }

    if (match)
        lua_pushstring(L, match);
    else
        lua_pushinteger(L, -1);
    return 1;
}

static int lua_openbor_strleft(lua_State *L)
{
    const char *source = luaL_checkstring(L, 1);
    lua_Integer count = luaL_checkinteger(L, 2);
    size_t length = strlen(source);

    if (count < 0)
        return luaL_error(L, "strleft character count cannot be negative");
    if ((lua_Unsigned)count > length)
        count = (lua_Integer)length;

    lua_pushlstring(L, source, (size_t)count);
    return 1;
}

static int lua_openbor_strlength(lua_State *L)
{
    const char *source = luaL_checkstring(L, 1);
    lua_pushinteger(L, (lua_Integer)strlen(source));
    return 1;
}

static int lua_openbor_strright(lua_State *L)
{
    const char *source = luaL_checkstring(L, 1);
    lua_Integer start = luaL_checkinteger(L, 2);
    size_t length = strlen(source);

    if (start < 0)
        return luaL_error(L, "strright start position cannot be negative");
    if ((lua_Unsigned)start > length)
        start = (lua_Integer)length;

    lua_pushstring(L, source + (size_t)start);
    return 1;
}

static int lua_openbor_rgbcolor(lua_State *L)
{
    int red = (int)luaL_checkinteger(L, 1);
    int green = (int)luaL_checkinteger(L, 2);
    int blue = (int)luaL_checkinteger(L, 3);

    if (lua_gettop(L) != 3)
        return luaL_error(L, "rgbcolor expects exactly 3 arguments");

    lua_pushinteger(L, (lua_Integer)_makecolour(red, green, blue));
    return 1;
}

/*
 * Resolve legacy OpenBOR constants by name for Lua scripts.
 * This first pass covers the static constants defined by this engine version;
 * dynamically allocated animation names are handled separately.
 */
/* ASCII case-insensitive comparisons, independent of platform stricmp APIs. */
static int bor_lua_ascii_lower(int ch)
{
    if (ch >= 'A' && ch <= 'Z')
        return ch - 'A' + 'a';
    return ch;
}

static int bor_lua_strnicmp(const char *left, const char *right, size_t count)
{
    size_t i;

    for (i = 0; i < count; ++i) {
        int a = bor_lua_ascii_lower((unsigned char)left[i]);
        int b = bor_lua_ascii_lower((unsigned char)right[i]);

        if (a != b)
            return a - b;
        if (a == '\0')
            return 0;
    }

    return 0;
}

static int bor_lua_stricmp(const char *left, const char *right)
{
    size_t length = strlen(left);
    int result = bor_lua_strnicmp(left, right, length);

    if (result != 0)
        return result;
    return left[length] == '\0' && right[length] == '\0' ? 0 :
           (left[length] == '\0' ? -1 : 1);
}

static int lua_openborconstant(lua_State *L)
{
    const char *name;

    if (lua_gettop(L) < 1)
        return luaL_error(L, "openborconstant expects a constant name");

    /* The legacy function passes integer arguments through unchanged. */
    if (lua_type(L, 1) == LUA_TNUMBER) {
        lua_Number number = lua_tonumber(L, 1);
        lua_Integer integer = lua_tointeger(L, 1);

        if (number != (lua_Number)integer)
            return luaL_error(L, "openborconstant expects a string or integer");
        lua_pushinteger(L, integer);
        return 1;
    }

    name = luaL_checkstring(L, 1);

    /* The legacy engine exposed this default as a global integer. */
    if (bor_lua_stricmp(name, "MAX_WALL_HEIGHT") == 0) {
        lua_pushinteger(L, 1000);
        return 1;
    }

    /* Keep this list aligned with the legacy constant map and engine headers. */
#define BOR_LUA_CONSTANT(symbol) \
    if (bor_lua_stricmp(name, #symbol) == 0) { lua_pushinteger(L, (lua_Integer)(symbol)); return 1; }
    BOR_LUA_CONSTANT(PIXEL_8);
    BOR_LUA_CONSTANT(PIXEL_x8);
    BOR_LUA_CONSTANT(PIXEL_16);
    BOR_LUA_CONSTANT(PIXEL_32);
    BOR_LUA_CONSTANT(COMPATIBLEVERSION);
    BOR_LUA_CONSTANT(CV_SAVED_GAME);
    BOR_LUA_CONSTANT(CV_HIGH_SCORE);
    BOR_LUA_CONSTANT(THINK_SPEED);
    BOR_LUA_CONSTANT(COUNTER_SPEED);
    BOR_LUA_CONSTANT(MAX_ENTS);
    BOR_LUA_CONSTANT(MAX_PANELS);
    BOR_LUA_CONSTANT(MAX_WEAPONS);
    BOR_LUA_CONSTANT(MAX_COLOUR_MAPS);
    BOR_LUA_CONSTANT(MAX_NAME_LEN);
    BOR_LUA_CONSTANT(LEVEL_MAX_SPAWNS);
    BOR_LUA_CONSTANT(LEVEL_MAX_PANELS);
    BOR_LUA_CONSTANT(LEVEL_MAX_HOLES);
    BOR_LUA_CONSTANT(LEVEL_MAX_WALLS);
    BOR_LUA_CONSTANT(MAX_LEVELS);
    BOR_LUA_CONSTANT(MAX_DIFFICULTIES);
    BOR_LUA_CONSTANT(MAX_SPECIALS);
    BOR_LUA_CONSTANT(MAX_ATCHAIN);
    BOR_LUA_CONSTANT(MAX_ATTACKS);
    BOR_LUA_CONSTANT(MAX_FOLLOWS);
    BOR_LUA_CONSTANT(MAX_PLAYERS);
    BOR_LUA_CONSTANT(MAX_ARG_LEN);
    BOR_LUA_CONSTANT(MAX_ANIS);
    BOR_LUA_CONSTANT(FLAG_ESC);
    BOR_LUA_CONSTANT(FLAG_START);
    BOR_LUA_CONSTANT(FLAG_MOVELEFT);
    BOR_LUA_CONSTANT(FLAG_MOVERIGHT);
    BOR_LUA_CONSTANT(FLAG_MOVEUP);
    BOR_LUA_CONSTANT(FLAG_MOVEDOWN);
    BOR_LUA_CONSTANT(FLAG_ATTACK);
    BOR_LUA_CONSTANT(FLAG_ATTACK2);
    BOR_LUA_CONSTANT(FLAG_ATTACK3);
    BOR_LUA_CONSTANT(FLAG_ATTACK4);
    BOR_LUA_CONSTANT(FLAG_JUMP);
    BOR_LUA_CONSTANT(FLAG_SPECIAL);
    BOR_LUA_CONSTANT(FLAG_SCREENSHOT);
    BOR_LUA_CONSTANT(FLAG_ANYBUTTON);
    BOR_LUA_CONSTANT(FLAG_FORWARD);
    BOR_LUA_CONSTANT(FLAG_BACKWARD);
    BOR_LUA_CONSTANT(SDID_MOVEUP);
    BOR_LUA_CONSTANT(SDID_MOVEDOWN);
    BOR_LUA_CONSTANT(SDID_MOVELEFT);
    BOR_LUA_CONSTANT(SDID_MOVERIGHT);
    BOR_LUA_CONSTANT(SDID_SPECIAL);
    BOR_LUA_CONSTANT(SDID_ATTACK);
    BOR_LUA_CONSTANT(SDID_ATTACK2);
    BOR_LUA_CONSTANT(SDID_ATTACK3);
    BOR_LUA_CONSTANT(SDID_ATTACK4);
    BOR_LUA_CONSTANT(SDID_JUMP);
    BOR_LUA_CONSTANT(SDID_START);
    BOR_LUA_CONSTANT(SDID_SCREENSHOT);
    BOR_LUA_CONSTANT(TYPE_NONE);
    BOR_LUA_CONSTANT(TYPE_PLAYER);
    BOR_LUA_CONSTANT(TYPE_ENEMY);
    BOR_LUA_CONSTANT(TYPE_ITEM);
    BOR_LUA_CONSTANT(TYPE_OBSTACLE);
    BOR_LUA_CONSTANT(TYPE_STEAMER);
    BOR_LUA_CONSTANT(TYPE_SHOT);
    BOR_LUA_CONSTANT(TYPE_TRAP);
    BOR_LUA_CONSTANT(TYPE_TEXTBOX);
    BOR_LUA_CONSTANT(TYPE_ENDLEVEL);
    BOR_LUA_CONSTANT(TYPE_NPC);
    BOR_LUA_CONSTANT(TYPE_PANEL);
    BOR_LUA_CONSTANT(SUBTYPE_NONE);
    BOR_LUA_CONSTANT(SUBTYPE_BIKER);
    BOR_LUA_CONSTANT(SUBTYPE_NOTGRAB);
    BOR_LUA_CONSTANT(SUBTYPE_ARROW);
    BOR_LUA_CONSTANT(SUBTYPE_TOUCH);
    BOR_LUA_CONSTANT(SUBTYPE_WEAPON);
    BOR_LUA_CONSTANT(SUBTYPE_NOSKIP);
    BOR_LUA_CONSTANT(SUBTYPE_FLYDIE);
    BOR_LUA_CONSTANT(SUBTYPE_BOTH);
    BOR_LUA_CONSTANT(SUBTYPE_PROJECTILE);
    BOR_LUA_CONSTANT(SUBTYPE_FOLLOW);
    BOR_LUA_CONSTANT(SUBTYPE_CHASE);
    BOR_LUA_CONSTANT(AIMOVE1_NORMAL);
    BOR_LUA_CONSTANT(AIMOVE1_CHASE);
    BOR_LUA_CONSTANT(AIMOVE1_CHASEZ);
    BOR_LUA_CONSTANT(AIMOVE1_CHASEX);
    BOR_LUA_CONSTANT(AIMOVE1_AVOID);
    BOR_LUA_CONSTANT(AIMOVE1_AVOIDZ);
    BOR_LUA_CONSTANT(AIMOVE1_AVOIDX);
    BOR_LUA_CONSTANT(AIMOVE1_WANDER);
    BOR_LUA_CONSTANT(AIMOVE1_NOMOVE);
    BOR_LUA_CONSTANT(AIMOVE1_BIKER);
    BOR_LUA_CONSTANT(AIMOVE1_STAR);
    BOR_LUA_CONSTANT(AIMOVE1_ARROW);
    BOR_LUA_CONSTANT(AIMOVE1_BOMB);
    BOR_LUA_CONSTANT(AIMOVE2_NORMAL);
    BOR_LUA_CONSTANT(AIMOVE2_IGNOREHOLES);
    BOR_LUA_CONSTANT(AIATTACK1_NORMAL);
    BOR_LUA_CONSTANT(AIATTACK1_LONG);
    BOR_LUA_CONSTANT(AIATTACK1_MELEE);
    BOR_LUA_CONSTANT(AIATTACK1_NOATTACK);
    BOR_LUA_CONSTANT(AIATTACK2_NORMAL);
    BOR_LUA_CONSTANT(AIATTACK2_DODGE);
    BOR_LUA_CONSTANT(AIATTACK2_DODGEMOVE);
    BOR_LUA_CONSTANT(FRONTPANEL_Z);
    BOR_LUA_CONSTANT(HOLE_Z);
    BOR_LUA_CONSTANT(NEONPANEL_Z);
    BOR_LUA_CONSTANT(SHADOW_Z);
    BOR_LUA_CONSTANT(SCREENPANEL_Z);
    BOR_LUA_CONSTANT(PANEL_Z);
    BOR_LUA_CONSTANT(MIRROR_Z);
    BOR_LUA_CONSTANT(PIT_DEPTH);
    BOR_LUA_CONSTANT(P2_STATS_DIST);
    BOR_LUA_CONSTANT(CONTACT_DIST_H);
    BOR_LUA_CONSTANT(CONTACT_DIST_V);
    BOR_LUA_CONSTANT(GRAB_DIST);
    BOR_LUA_CONSTANT(GRAB_STALL);
    BOR_LUA_CONSTANT(ATK_NORMAL);
    BOR_LUA_CONSTANT(ATK_NORMAL2);
    BOR_LUA_CONSTANT(ATK_NORMAL3);
    BOR_LUA_CONSTANT(ATK_NORMAL4);
    BOR_LUA_CONSTANT(ATK_BLAST);
    BOR_LUA_CONSTANT(ATK_BURN);
    BOR_LUA_CONSTANT(ATK_FREEZE);
    BOR_LUA_CONSTANT(ATK_SHOCK);
    BOR_LUA_CONSTANT(ATK_STEAL);
    BOR_LUA_CONSTANT(ATK_NORMAL5);
    BOR_LUA_CONSTANT(ATK_NORMAL6);
    BOR_LUA_CONSTANT(ATK_NORMAL7);
    BOR_LUA_CONSTANT(ATK_NORMAL8);
    BOR_LUA_CONSTANT(ATK_NORMAL9);
    BOR_LUA_CONSTANT(ATK_NORMAL10);
    BOR_LUA_CONSTANT(ATK_ITEM);
    BOR_LUA_CONSTANT(SCROLL_RIGHT);
    BOR_LUA_CONSTANT(SCROLL_DOWN);
    BOR_LUA_CONSTANT(SCROLL_LEFT);
    BOR_LUA_CONSTANT(SCROLL_UP);
    BOR_LUA_CONSTANT(SCROLL_BOTH);
    BOR_LUA_CONSTANT(SCROLL_LEFTRIGHT);
    BOR_LUA_CONSTANT(SCROLL_RIGHTLEFT);
    BOR_LUA_CONSTANT(SCROLL_INWARD);
    BOR_LUA_CONSTANT(SCROLL_OUTWARD);
    BOR_LUA_CONSTANT(SCROLL_INOUT);
    BOR_LUA_CONSTANT(SCROLL_OUTIN);
    BOR_LUA_CONSTANT(SCROLL_UPWARD);
    BOR_LUA_CONSTANT(SCROLL_DOWNWARD);
    BOR_LUA_CONSTANT(ANI_IDLE);
    BOR_LUA_CONSTANT(ANI_WALK);
    BOR_LUA_CONSTANT(ANI_JUMP);
    BOR_LUA_CONSTANT(ANI_LAND);
    BOR_LUA_CONSTANT(ANI_PAIN);
    BOR_LUA_CONSTANT(ANI_FALL);
    BOR_LUA_CONSTANT(ANI_RISE);
    BOR_LUA_CONSTANT(ANI_UPPER);
    BOR_LUA_CONSTANT(ANI_BLOCK);
    BOR_LUA_CONSTANT(ANI_JUMPATTACK);
    BOR_LUA_CONSTANT(ANI_JUMPATTACK2);
    BOR_LUA_CONSTANT(ANI_GET);
    BOR_LUA_CONSTANT(ANI_GRAB);
    BOR_LUA_CONSTANT(ANI_GRABATTACK);
    BOR_LUA_CONSTANT(ANI_GRABATTACK2);
    BOR_LUA_CONSTANT(ANI_THROW);
    BOR_LUA_CONSTANT(ANI_SPECIAL);
    BOR_LUA_CONSTANT(ANI_SPAWN);
    BOR_LUA_CONSTANT(ANI_DIE);
    BOR_LUA_CONSTANT(ANI_PICK);
    BOR_LUA_CONSTANT(ANI_JUMPATTACK3);
    BOR_LUA_CONSTANT(ANI_UP);
    BOR_LUA_CONSTANT(ANI_DOWN);
    BOR_LUA_CONSTANT(ANI_SHOCK);
    BOR_LUA_CONSTANT(ANI_BURN);
    BOR_LUA_CONSTANT(ANI_SHOCKPAIN);
    BOR_LUA_CONSTANT(ANI_BURNPAIN);
    BOR_LUA_CONSTANT(ANI_GRABBED);
    BOR_LUA_CONSTANT(ANI_SPECIAL2);
    BOR_LUA_CONSTANT(ANI_RUN);
    BOR_LUA_CONSTANT(ANI_RUNATTACK);
    BOR_LUA_CONSTANT(ANI_RUNJUMPATTACK);
    BOR_LUA_CONSTANT(ANI_ATTACKUP);
    BOR_LUA_CONSTANT(ANI_ATTACKDOWN);
    BOR_LUA_CONSTANT(ANI_ATTACKFORWARD);
    BOR_LUA_CONSTANT(ANI_ATTACKBACKWARD);
    BOR_LUA_CONSTANT(ANI_RISEATTACK);
    BOR_LUA_CONSTANT(ANI_DODGE);
    BOR_LUA_CONSTANT(ANI_ATTACKBOTH);
    BOR_LUA_CONSTANT(ANI_GRABFORWARD);
    BOR_LUA_CONSTANT(ANI_GRABFORWARD2);
    BOR_LUA_CONSTANT(ANI_JUMPFORWARD);
    BOR_LUA_CONSTANT(ANI_GRABDOWN);
    BOR_LUA_CONSTANT(ANI_GRABDOWN2);
    BOR_LUA_CONSTANT(ANI_GRABUP);
    BOR_LUA_CONSTANT(ANI_GRABUP2);
    BOR_LUA_CONSTANT(ANI_SELECT);
    BOR_LUA_CONSTANT(ANI_DUCK);
    BOR_LUA_CONSTANT(ANI_FAINT);
    BOR_LUA_CONSTANT(ANI_CANT);
    BOR_LUA_CONSTANT(ANI_THROWATTACK);
    BOR_LUA_CONSTANT(ANI_CHARGEATTACK);
    BOR_LUA_CONSTANT(ANI_VAULT);
    BOR_LUA_CONSTANT(ANI_JUMPCANT);
    BOR_LUA_CONSTANT(ANI_JUMPSPECIAL);
    BOR_LUA_CONSTANT(ANI_BURNDIE);
    BOR_LUA_CONSTANT(ANI_SHOCKDIE);
    BOR_LUA_CONSTANT(ANI_PAIN2);
    BOR_LUA_CONSTANT(ANI_PAIN3);
    BOR_LUA_CONSTANT(ANI_PAIN4);
    BOR_LUA_CONSTANT(ANI_FALL2);
    BOR_LUA_CONSTANT(ANI_FALL3);
    BOR_LUA_CONSTANT(ANI_FALL4);
    BOR_LUA_CONSTANT(ANI_DIE2);
    BOR_LUA_CONSTANT(ANI_DIE3);
    BOR_LUA_CONSTANT(ANI_DIE4);
    BOR_LUA_CONSTANT(ANI_CHARGE);
    BOR_LUA_CONSTANT(ANI_BACKWALK);
    BOR_LUA_CONSTANT(ANI_SLEEP);
    BOR_LUA_CONSTANT(ANI_PAIN5);
    BOR_LUA_CONSTANT(ANI_PAIN6);
    BOR_LUA_CONSTANT(ANI_PAIN7);
    BOR_LUA_CONSTANT(ANI_PAIN8);
    BOR_LUA_CONSTANT(ANI_PAIN9);
    BOR_LUA_CONSTANT(ANI_PAIN10);
    BOR_LUA_CONSTANT(ANI_FALL5);
    BOR_LUA_CONSTANT(ANI_FALL6);
    BOR_LUA_CONSTANT(ANI_FALL7);
    BOR_LUA_CONSTANT(ANI_FALL8);
    BOR_LUA_CONSTANT(ANI_FALL9);
    BOR_LUA_CONSTANT(ANI_FALL10);
    BOR_LUA_CONSTANT(ANI_DIE5);
    BOR_LUA_CONSTANT(ANI_DIE6);
    BOR_LUA_CONSTANT(ANI_DIE7);
    BOR_LUA_CONSTANT(ANI_DIE8);
    BOR_LUA_CONSTANT(ANI_DIE9);
    BOR_LUA_CONSTANT(ANI_DIE10);
    BOR_LUA_CONSTANT(ANI_TURN);
    BOR_LUA_CONSTANT(ANI_RESPAWN);
    BOR_LUA_CONSTANT(ANI_FORWARDJUMP);
    BOR_LUA_CONSTANT(ANI_RUNJUMP);
    BOR_LUA_CONSTANT(ANI_JUMPLAND);
    BOR_LUA_CONSTANT(ANI_JUMPDELAY);
    BOR_LUA_CONSTANT(ANI_HITWALL);
    BOR_LUA_CONSTANT(ANI_GRABBACKWARD);
    BOR_LUA_CONSTANT(ANI_GRABBACKWARD2);
    BOR_LUA_CONSTANT(ANI_GRABWALK);
    BOR_LUA_CONSTANT(ANI_GRABBEDWALK);
    BOR_LUA_CONSTANT(ANI_GRABWALKUP);
    BOR_LUA_CONSTANT(ANI_GRABBEDWALKUP);
    BOR_LUA_CONSTANT(ANI_GRABWALKDOWN);
    BOR_LUA_CONSTANT(ANI_GRABBEDWALKDOWN);
    BOR_LUA_CONSTANT(ANI_GRABTURN);
    BOR_LUA_CONSTANT(ANI_GRABBEDTURN);
    BOR_LUA_CONSTANT(ANI_GRABBACKWALK);
    BOR_LUA_CONSTANT(ANI_GRABBEDBACKWALK);
    BOR_LUA_CONSTANT(ANI_SLIDE);
    BOR_LUA_CONSTANT(ANI_RUNSLIDE);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN);
    BOR_LUA_CONSTANT(ANI_DUCKATTACK);
    BOR_LUA_CONSTANT(PLAYER_MIN_Z);
    BOR_LUA_CONSTANT(PLAYER_MAX_Z);
    BOR_LUA_CONSTANT(BGHEIGHT);
    BOR_LUA_CONSTANT(ANI_RISE2);
    BOR_LUA_CONSTANT(ANI_RISE3);
    BOR_LUA_CONSTANT(ANI_RISE4);
    BOR_LUA_CONSTANT(ANI_RISE5);
    BOR_LUA_CONSTANT(ANI_RISE6);
    BOR_LUA_CONSTANT(ANI_RISE7);
    BOR_LUA_CONSTANT(ANI_RISE8);
    BOR_LUA_CONSTANT(ANI_RISE9);
    BOR_LUA_CONSTANT(ANI_RISE10);
    BOR_LUA_CONSTANT(ANI_RISEB);
    BOR_LUA_CONSTANT(ANI_RISES);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN2);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN3);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN4);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN5);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN6);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN7);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN8);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN9);
    BOR_LUA_CONSTANT(ANI_BLOCKPAIN10);
    BOR_LUA_CONSTANT(ANI_BLOCKPAINB);
    BOR_LUA_CONSTANT(ANI_BLOCKPAINS);
    BOR_LUA_CONSTANT(ANI_CHIPDEATH);
    BOR_LUA_CONSTANT(ANI_GUARDBREAK);
    BOR_LUA_CONSTANT(ANI_RISEATTACK2);
    BOR_LUA_CONSTANT(ANI_RISEATTACK3);
    BOR_LUA_CONSTANT(ANI_RISEATTACK4);
    BOR_LUA_CONSTANT(ANI_RISEATTACK5);
    BOR_LUA_CONSTANT(ANI_RISEATTACK6);
    BOR_LUA_CONSTANT(ANI_RISEATTACK7);
    BOR_LUA_CONSTANT(ANI_RISEATTACK8);
    BOR_LUA_CONSTANT(ANI_RISEATTACK9);
    BOR_LUA_CONSTANT(ANI_RISEATTACK10);
    BOR_LUA_CONSTANT(ANI_RISEATTACKB);
    BOR_LUA_CONSTANT(ANI_RISEATTACKS);
#undef BOR_LUA_CONSTANT

    /*
     * Legacy scripts also use numbered animation constants such as
     * ANI_ATTACK1 and ANI_FREESPECIAL2. These map through the engine's
     * dynamic animation-index tables rather than directly to enum values.
     */
    {
        struct bor_lua_dynamic_constant {
            const char *prefix;
            const int *values;
            int max_count;
        } dynamic_constants[] = {
            {"ANI_DOWN", dyn_anims.animdowns ? dyn_anims.animdowns : default_dyn_anims.animdowns, MAX_DOWNS},
            {"ANI_UP", dyn_anims.animups ? dyn_anims.animups : default_dyn_anims.animups, MAX_UPS},
            {"ANI_BACKWALK", dyn_anims.animbackwalks ? dyn_anims.animbackwalks : default_dyn_anims.animbackwalks, MAX_BACKWALKS},
            {"ANI_WALK", dyn_anims.animwalks ? dyn_anims.animwalks : default_dyn_anims.animwalks, MAX_WALKS},
            {"ANI_IDLE", dyn_anims.animidles ? dyn_anims.animidles : default_dyn_anims.animidles, MAX_IDLES},
            {"ANI_FALL", dyn_anims.animfalls ? dyn_anims.animfalls : default_dyn_anims.animfalls, MAX_ATKS},
            {"ANI_RISE", dyn_anims.animrises ? dyn_anims.animrises : default_dyn_anims.animrises, MAX_ATKS},
            {"ANI_RISEATTACK", dyn_anims.animriseattacks ? dyn_anims.animriseattacks : default_dyn_anims.animriseattacks, MAX_ATKS},
            {"ANI_PAIN", dyn_anims.animpains ? dyn_anims.animpains : default_dyn_anims.animpains, MAX_ATKS},
            {"ANI_DIE", dyn_anims.animdies ? dyn_anims.animdies : default_dyn_anims.animdies, MAX_ATKS},
            {"ANI_ATTACK", dyn_anims.animattacks ? dyn_anims.animattacks : default_dyn_anims.animattacks, dyn_anims.animattacks ? MAX_ATTACKS : 4},
            {"ANI_FOLLOW", dyn_anims.animfollows ? dyn_anims.animfollows : default_dyn_anims.animfollows, dyn_anims.animfollows ? MAX_FOLLOWS : 4},
            {"ANI_FREESPECIAL", dyn_anims.animspecials ? dyn_anims.animspecials : default_dyn_anims.animspecials, dyn_anims.animspecials ? MAX_SPECIALS : 8}
        };
        size_t i;

        for (i = 0; i < sizeof(dynamic_constants) / sizeof(dynamic_constants[0]); ++i) {
            size_t prefix_length = strlen(dynamic_constants[i].prefix);
            char *end = NULL;
            long animation_index;

            if (bor_lua_strnicmp(name, dynamic_constants[i].prefix, prefix_length) != 0)
                continue;

            /*
             * The legacy lookup requires a numeric suffix for dynamic
             * animation names, including ANI_ATTACK1 and ANI_FREESPECIAL1.
             */
            if (name[prefix_length] < '0' || name[prefix_length] > '9')
                continue;

            animation_index = strtol(name + prefix_length, &end, 10);
            if (end == name + prefix_length || *end != '\0')
                continue;
            if (animation_index < 1 ||
                animation_index > dynamic_constants[i].max_count) {
                continue;
            }

            /*
             * The legacy implementation offsets numbered RISE, RISEATTACK,
             * PAIN, and DIE constants into the upper attack-animation slots.
             */
            if (i >= 6 && i <= 9) {
                long first_slot = MAX_ATKS - STA_ATKS + 1;
                if (animation_index < first_slot)
                    animation_index = first_slot;
            }

            if (!dynamic_constants[i].values) {
                lua_pushnil(L);
                return 1;
            }

            if (animation_index > dynamic_constants[i].max_count) {
                lua_pushnil(L);
                return 1;
            }

            lua_pushinteger(L, (lua_Integer)dynamic_constants[i].values[animation_index - 1]);
            return 1;
        }
    }

    /* The legacy implementation clears the result when no constant matches. */
    lua_pushnil(L);
    return 1;
}

// getlevelproperty(name)
// Preserve the legacy getter's case-insensitive property lookup and integer results.
static int lua_openbor_getlevelproperty(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);

    if (!level)
        return luaL_error(L, "getlevelproperty requires a loaded level");

    if (!stricmp(name, "bgspeed"))
        lua_pushinteger(L, (lua_Integer)level->bgspeed);
    else if (!stricmp(name, "cameraxoffset"))
        lua_pushinteger(L, level->cameraxoffset);
    else if (!stricmp(name, "camerazoffset"))
        lua_pushinteger(L, level->camerazoffset);
    else
        return luaL_error(L, "Property name '%s' is not supported by function getlevelproperty", name);

    return 1;
}

// changelevelproperty(name, value)
// Preserve the legacy setter's case-sensitive names and return behavior.
static int lua_openbor_changelevelproperty(lua_State *L) {
    const char *name;
    lua_Integer value;

    if (lua_gettop(L) < 2) {
        lua_pushnil(L);
        return 1;
    }
    if (lua_type(L, 1) != LUA_TSTRING) {
        lua_pushnil(L);
        return 1;
    }

    name = lua_tostring(L, 1);
    if (!level)
        return luaL_error(L, "changelevelproperty requires a loaded level");

    /* The legacy setter reports success for unknown names without changing anything. */
    lua_pushinteger(L, 1);
    if (!strcmp(name, "rock")) {
        if (lua_isnumber(L, 2)) {
            value = lua_tointeger(L, 2);
            level->rocking = (int)value;
        } else {
            lua_pop(L, 1);
            lua_pushinteger(L, 0);
        }
    } else if (!strcmp(name, "bgspeed")) {
        if (lua_isnumber(L, 2)) {
            value = lua_tointeger(L, 2);
            level->bgspeed = (float)value;
        } else {
            lua_pop(L, 1);
            lua_pushinteger(L, 0);
        }
    } else if (!strcmp(name, "cameraxoffset")) {
        if (lua_isnumber(L, 2)) {
            value = lua_tointeger(L, 2);
            level->cameraxoffset = (int)value;
        } else {
            lua_pop(L, 1);
            lua_pushinteger(L, 0);
        }
    } else if (!strcmp(name, "camerazoffset")) {
        if (lua_isnumber(L, 2)) {
            value = lua_tointeger(L, 2);
            level->camerazoffset = (int)value;
        } else {
            lua_pop(L, 1);
            lua_pushinteger(L, 0);
        }
    }

    return 1;
}

// playerkeys(playerindex, newkey?, key1, key2, ...)
// Preserve legacy button names, key-state selection, and bitmask return values.
static int lua_openbor_playerkeys(lua_State *L) {
    static const struct {
        const char *name;
        u32 flag;
    } buttons[] = {
        {"anybutton", FLAG_ANYBUTTON},
        {"attack", FLAG_ATTACK},
        {"attack2", FLAG_ATTACK2},
        {"attack3", FLAG_ATTACK3},
        {"attack4", FLAG_ATTACK4},
        {"esc", FLAG_ESC},
        {"jump", FLAG_JUMP},
        {"movedown", FLAG_MOVEDOWN},
        {"moveleft", FLAG_MOVELEFT},
        {"moveright", FLAG_MOVERIGHT},
        {"moveup", FLAG_MOVEUP},
        {"screenshot", FLAG_SCREENSHOT},
        {"special", FLAG_SPECIAL},
        {"start", FLAG_START}
    };
    int top = lua_gettop(L);
    int index = 0;
    int newkey = 0;
    int i;
    u32 keys;

    if (top < 3) {
        lua_pushnil(L);
        return 1;
    }

    if (lua_isnumber(L, 1))
        index = (int)lua_tointeger(L, 1);
    if (lua_isnumber(L, 2))
        newkey = (int)lua_tointeger(L, 2);

    /* The legacy code did not validate the player index; guard it in Lua. */
    if (index < 0 || index >= MAX_PLAYERS) {
        lua_pushinteger(L, 0);
        return 1;
    }

    if (newkey == 1)
        keys = player[index].newkeys;
    else if (newkey == 2)
        keys = player[index].releasekeys;
    else
        keys = player[index].keys;

    for (i = 3; i <= top; i++) {
        lua_Integer button = -1;
        u32 flag = 0;
        int j;

        if (lua_isnumber(L, i)) {
            button = lua_tointeger(L, i);
        } else if (lua_type(L, i) == LUA_TSTRING) {
            const char *name = lua_tostring(L, i);
            for (j = 0; j < (int)(sizeof(buttons) / sizeof(buttons[0])); j++) {
                if (!stricmp(name, buttons[j].name)) {
                    flag = buttons[j].flag;
                    button = j;
                    break;
                }
            }
        }

        if (button >= 0 && button < (lua_Integer)(sizeof(buttons) / sizeof(buttons[0]))) {
            if (!flag)
                flag = buttons[button].flag;
            lua_pushinteger(L, (lua_Integer)(keys & flag));
        } else {
            lua_pushinteger(L, 0);
        }

        if (lua_tointeger(L, -1) == 0)
            return 1;
        /* Keep the last checked button's bitmask, as the legacy API does. */
        if (i < top)
            lua_remove(L, -1);
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

    /* Weak-value cache: stable wrapper identity without retaining dead wrappers. */
    lua_newtable(g_lua_engine_state);
    lua_newtable(g_lua_engine_state);
    lua_pushstring(g_lua_engine_state, "v");
    lua_setfield(g_lua_engine_state, -2, "__mode");
    lua_setmetatable(g_lua_engine_state, -2);
    lua_setfield(g_lua_engine_state, LUA_REGISTRYINDEX, "OpenBOR.EntityCache");

    /* Indexed variables use a Lua table with legacy zero-based indices. */
    lua_newtable(g_lua_engine_state);
    lua_setfield(g_lua_engine_state, LUA_REGISTRYINDEX, BOR_LUA_INDEXED_VARS);

    /* Strong-value storage for legacy global variables. */
    lua_newtable(g_lua_engine_state);
    lua_setfield(g_lua_engine_state, LUA_REGISTRYINDEX, BOR_LUA_GLOBAL_VARS);

    /*
     * Register the API as global Lua functions. Scripts should call
     * spawn(...), get_self(), log(), etc., without an openbor. prefix.
     */
    lua_pushcfunction(g_lua_engine_state, lua_openbor_isempty);
    lua_setglobal(g_lua_engine_state, "isempty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_null);
    lua_setglobal(g_lua_engine_state, "NULL");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_maxglobalvarindex);
    lua_setglobal(g_lua_engine_state, "maxglobalvarindex");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setlocalvar);
    lua_setglobal(g_lua_engine_state, "setlocalvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_clearlocalvar);
    lua_setglobal(g_lua_engine_state, "clearlocalvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_clearglobalvar);
    lua_setglobal(g_lua_engine_state, "clearglobalvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawline);
    lua_setglobal(g_lua_engine_state, "drawline");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_drawdot);
    lua_setglobal(g_lua_engine_state, "drawdot");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_strinfirst);
    lua_setglobal(g_lua_engine_state, "strinfirst");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_strinlast);
    lua_setglobal(g_lua_engine_state, "strinlast");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_strleft);
    lua_setglobal(g_lua_engine_state, "strleft");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_strlength);
    lua_setglobal(g_lua_engine_state, "strlength");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_strright);
    lua_setglobal(g_lua_engine_state, "strright");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_rgbcolor);
    lua_setglobal(g_lua_engine_state, "rgbcolor");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_free);
    lua_setglobal(g_lua_engine_state, "free");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_fadeout);
    lua_setglobal(g_lua_engine_state, "fadeout");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_playmusic);
    lua_setglobal(g_lua_engine_state, "playmusic");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_fademusic);
    lua_setglobal(g_lua_engine_state, "fademusic");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setmusicvolume);
    lua_setglobal(g_lua_engine_state, "setmusicvolume");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setmusictempo);
    lua_setglobal(g_lua_engine_state, "setmusictempo");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_pausemusic);
    lua_setglobal(g_lua_engine_state, "pausemusic");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_loadsample);
    lua_setglobal(g_lua_engine_state, "loadsample");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_playsample);
    lua_setglobal(g_lua_engine_state, "playsample");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_unloadsample);
    lua_setglobal(g_lua_engine_state, "unloadsample");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changelight);
    lua_setglobal(g_lua_engine_state, "changelight");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changeshadowcolor);
    lua_setglobal(g_lua_engine_state, "changeshadowcolor");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changepalette);
    lua_setglobal(g_lua_engine_state, "changepalette");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_jumptobranch);
    lua_setglobal(g_lua_engine_state, "jumptobranch");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_bindentity);
    lua_setglobal(g_lua_engine_state, "bindentity");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_performattack);
    lua_setglobal(g_lua_engine_state, "performattack");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setidle);
    lua_setglobal(g_lua_engine_state, "setidle");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_updateframe);
    lua_setglobal(g_lua_engine_state, "updateframe");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getplayerproperty);
    lua_setglobal(g_lua_engine_state, "getplayerproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changeplayerproperty);
    lua_setglobal(g_lua_engine_state, "changeplayerproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_tossentity);
    lua_setglobal(g_lua_engine_state, "tossentity");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_findtarget);
    lua_setglobal(g_lua_engine_state, "findtarget");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_checkrange);
    lua_setglobal(g_lua_engine_state, "checkrange");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_checkhole);
    lua_setglobal(g_lua_engine_state, "checkhole");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_checkwall);
    lua_setglobal(g_lua_engine_state, "checkwall");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_checkplatformbelow);
    lua_setglobal(g_lua_engine_state, "checkplatformbelow");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_openfilestream);
    lua_setglobal(g_lua_engine_state, "openfilestream");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_createfilestream);
    lua_setglobal(g_lua_engine_state, "createfilestream");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getfilestreamline);
    lua_setglobal(g_lua_engine_state, "getfilestreamline");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getfilestreamargument);
    lua_setglobal(g_lua_engine_state, "getfilestreamargument");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_filestreamnextline);
    lua_setglobal(g_lua_engine_state, "filestreamnextline");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getfilestreamposition);
    lua_setglobal(g_lua_engine_state, "getfilestreamposition");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setfilestreamposition);
    lua_setglobal(g_lua_engine_state, "setfilestreamposition");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_savefilestream);
    lua_setglobal(g_lua_engine_state, "savefilestream");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_filestreamappend);
    lua_setglobal(g_lua_engine_state, "filestreamappend");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getindexedvar);
    lua_setglobal(g_lua_engine_state, "getindexedvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setindexedvar);
    lua_setglobal(g_lua_engine_state, "setindexedvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_clearindexedvar);
    lua_setglobal(g_lua_engine_state, "clearindexedvar");

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

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getlocalvar);
    lua_setglobal(g_lua_engine_state, "getlocalvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_globalvar);
    lua_setglobal(g_lua_engine_state, "globalvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getglobalvar);
    lua_setglobal(g_lua_engine_state, "getglobalvar");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_setglobalvar);
    lua_setglobal(g_lua_engine_state, "setglobalvar");

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
    lua_pushcfunction(g_lua_engine_state, lua_openborvariant);
    lua_setglobal(g_lua_engine_state, "openborvariant");

    lua_pushcfunction(g_lua_engine_state, lua_changeopenborvariant);
    lua_setglobal(g_lua_engine_state, "changeopenborvariant");

    lua_pushcfunction(g_lua_engine_state, lua_openborconstant);
    lua_setglobal(g_lua_engine_state, "openborconstant");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getentityproperty);
    lua_setglobal(g_lua_engine_state, "getentityproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_getlevelproperty);
    lua_setglobal(g_lua_engine_state, "getlevelproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_changelevelproperty);
    lua_setglobal(g_lua_engine_state, "changelevelproperty");

    lua_pushcfunction(g_lua_engine_state, lua_openbor_playerkeys);
    lua_setglobal(g_lua_engine_state, "playerkeys");
}
