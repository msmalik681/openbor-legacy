/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

/////////////////////////////////////////////////////////////////////////////
//      Beats of Rage                                                           //
//      Side-scrolling beat-'em-up                                              //
/////////////////////////////////////////////////////////////////////////////

#include "debug.h"
#include "data.h"
#include "openbor.h"
#include "commands.h"
#include "models.h"
#include "movie.h"
#include "menus.h"
#include "luabindings.h"
#include "source/strswitch/stringswitch.h"
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define GET_ARG(z) arglist.count > z ? arglist.args[z] : ""
#define GET_ARG_LEN(z) arglist.count > z ? arglist.arglen[z] : 0
#define GET_ARGP(z) arglist->count > z ? arglist->args[z] : ""
#define GET_ARGP_LEN(z) arglist->count > z ? arglist->arglen[z] : 0
#define GET_INT_ARG(z) getValidInt(GET_ARG(z), filename, command)
#define GET_FLOAT_ARG(z) getValidFloat(GET_ARG(z), filename, command)
#define GET_INT_ARGP(z) getValidInt(GET_ARGP(z), filename, command)
#define GET_FLOAT_ARGP(z) getValidFloat(GET_ARGP(z), filename, command)

static const char *E_OUT_OF_MEMORY = "Error: Could not allocate sufficient memory.\n";
static int DEFAULT_OFFSCREEN_KILL = 3000;
void openbor_register_lua_api(void);

/////////////////////////////////////////////////////////////////////////////
//  Global Variables                                                        //
/////////////////////////////////////////////////////////////////////////////

s_level_entry *levelorder[MAX_DIFFICULTIES][MAX_LEVELS];
s_level *level = NULL;
s_screen *vscreen = NULL;
s_screen *background = NULL;
s_screen *bgbuffer = NULL;
char bgbuffer_updated = 0;
s_bitmap *texture = NULL;
s_videomodes videomodes;
s_sprite_list *sprite_list;
s_sprite_map *sprite_map;
s_anim_list *anim_list;
s_modelcache *model_cache;
static char lua_command_scratch[4096];
static char lua_frame_scratch[8192];

s_player_min_max_z_bgheight player_min_max_z_bgheight = {
	160, 232, 160
};

char custom_button_names[CB_MAX][16];
char disabledkey[CB_MAX] = { 0 };

int quit_game = 0;

int sprite_map_max_items = 0;
int cache_map_max_items = 0;


int startup_done = 0;
List *modelcmdlist = NULL;
List* modelsattackcmdlist = NULL;
List *modelstxtcmdlist = NULL;
List *levelcmdlist = NULL;
List *levelordercmdlist = NULL;
lua_State *g_lua_engine_state = NULL;

char *custBkgrds = NULL;
char *custLevels = NULL;
char *custModels = NULL;
char rush_names[2][MAX_NAME_LEN];
char branch_name[MAX_NAME_LEN + 1];
char set_names[MAX_DIFFICULTIES][MAX_NAME_LEN + 1];
unsigned char pal[MAX_PAL_SIZE] = { "" };
int blendfx[MAX_BLENDINGS] = { 0, 1, 0, 0, 0, 0 };

char blendfx_is_set = 0;
int fontmonospace[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

// move all blending effects here
unsigned char *blendings[MAX_BLENDINGS] = { NULL, NULL, NULL, NULL, NULL, NULL };

// function pointers to create the tables
palette_table_function blending_table_functions[MAX_BLENDINGS] =
    { palette_table_screen, palette_table_multiply, palette_table_overlay,
	palette_table_hardlight, palette_table_dodge, palette_table_half
};


int current_set = 0;
int current_level = 0;
int current_stage = 1;

float bgtravelled;
int traveltime;
int texttime;
float advancex;
float advancey;

float scrolldx;
float scrolldy;
float scrollminz;
float scrollmaxz;
float blockade;
float lasthitx;
float lasthitz;
float lasthita;
int lasthitt;
int lasthitc;

// used by gfx shadow
int light[2] = { 0, 0 };

int shadowalpha = BLEND_MULTIPLY + 1;

u32 interval = 0;
extern unsigned long seed;

s_samples samples = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,};

//------------------------------

int grab_attacks[5][2] = {
	{ANI_GRABATTACK, ANI_GRABATTACK2},
	{ANI_GRABFORWARD, ANI_GRABFORWARD2},
	{ANI_GRABUP, ANI_GRABUP2},
	{ANI_GRABDOWN, ANI_GRABDOWN2},
	{ANI_GRABBACKWARD, ANI_GRABBACKWARD2}
};

// background cache to speed up in-game menus
#if WII
s_screen *bg_cache[MAX_CACHED_BACKGROUNDS] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

unsigned char bg_palette_cache[MAX_CACHED_BACKGROUNDS][MAX_PAL_SIZE];
#endif

int maxplayers[MAX_DIFFICULTIES] = { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 };
int ctrlmaxplayers[MAX_DIFFICULTIES] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

unsigned int num_levels[MAX_DIFFICULTIES];
unsigned int ifcomplete[MAX_DIFFICULTIES];
unsigned int num_difficulties;
unsigned int noshowhof[MAX_DIFFICULTIES];
unsigned int difflives[MAX_DIFFICULTIES];
unsigned int custfade[MAX_DIFFICULTIES];
unsigned int diffcreds[MAX_DIFFICULTIES];
unsigned int diffoverlap[MAX_DIFFICULTIES];
unsigned int typemp[MAX_DIFFICULTIES];
unsigned int continuescore[MAX_DIFFICULTIES];
char *(*skipselect)[MAX_DIFFICULTIES][MAX_PLAYERS] = NULL;
int cansave_flag[MAX_DIFFICULTIES];

int cameratype = 0;

u32 go_time = 0;
u32 borTime = 0;
u32 newtime = 0;
unsigned char slowmotion[3] = { 0, 2, 0 };

int disablelog = 0;
#define PLOG(fmt, args...) do { if(!disablelog) fprintf(stdout, fmt, ## args); } while (0)

int currentspawnplayer = 0;
int MAX_WALL_HEIGHT = 1000;
int saveslot = 0;
int current_palette = 0;
int fade = 24;
int credits = 0;
int gosound = 0;
int musicoverlap = 0;
int colorbars = 0;
int current_spawn = 0;
int level_completed = 0;
int nojoin = 0;
int groupmin = 0;
int groupmax = 0;
int selectScreen = 0;
int tospeedup = 0;
int reached[4] = { 0, 0, 0, 0 };

int noslowfx = 0;
int equalairpause = 0;
int hiscorebg = 0;
int completebg = 0;
s_loadingbar loadingbg[2] = { {0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0} };

int loadingmusic = 0;
int unlockbg = 0;
int pause = 0;
int nopause = 0;
int noscreenshot = 0;
int endgame = 0;

int forcecheatsoff = 0;
int cheats = 0;
int livescheat = 0;
int creditscheat = 0;
int healthcheat = 0;

int keyscriptrate = 0;

int showtimeover = 0;
int sameplayer = 0;
int PLAYER_LIVES = 3;
int CONTINUES = 5;
int colourselect = 0;
int autoland = 0;
int ajspecial = 0;
int nolost = 0;
int nocost = 0;
int mpstrict = 0;
int magic_type = 0;
entity *textbox = NULL;
entity *smartbomber = NULL;
int plife[4][2] = { {0, 0}, {0, 0}, {0, 0}, {0, 0} };
int plifeX[4][3] = { {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1} };
int plifeN[4][3] = { {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1} };
int picon[4][2] = { {0, 0}, {0, 0}, {0, 0}, {0, 0} };
int piconw[4][2] = { {0, 0}, {0, 0}, {0, 0}, {0, 0} };
int mpicon[4][2] = { {0, 0}, {0, 0}, {0, 0}, {0, 0} };
int pnameJ[4][7] = { {0, 0, 0, 0, 0, 0, -1}, {0, 0, 0, 0, 0, 0, -1}, {0, 0, 0, 0, 0, 0, -1}, {0, 0, 0, 0, 0, 0, -1} };
int pscore[4][7] = { {0, 0, 0, 0, 0, 0, -1}, {0, 0, 0, 0, 0, 0, -1}, {0, 0, 0, 0, 0, 0, -1}, {0, 0, 0, 0, 0, 0, -1} };
int pshoot[4][3] = { {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1} };
int prush[4][8] = { {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0} };
int psmenu[4][4] = { {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0} };
char musicname[128] = { "" };
float musicfade[2] = { 0, 0 };

int musicloop = 0;
u32 musicoffset = 0;

int timeloc[6] = { 0, 0, 0, 0, 0, -1 };

int timeicon = -1;
short timeicon_offsets[2] = { 0, 0 };
char timeicon_path[128] = { "" };

int bgicon = -1;
short bgicon_offsets[3] = { 0, 0, 0 };
char bgicon_path[128] = { "" };

int olicon = -1;
short olicon_offsets[3] = { 0, 0, 0 };
char olicon_path[128] = { "" };
int elife[4][2] = { {0, 0}, {0, 0}, {0, 0}, {0, 0} };
int ename[4][3] = { {0, 0, -1}, {0, 0, -1}, {0, 0, -1}, {0, 0, -1} };
int eicon[4][2] = { {0, 0}, {0, 0}, {0, 0}, {0, 0} };
int scomplete[6] = { 0, 0, 0, 0, 0, 0 };
int cbonus[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
