/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD licence, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

/* Legacy string-to-index mapping helpers used by the engine. */

#include "openborscript.h"
#include "openbor.h"

// Define macro for string mapping
#define MAPSTRINGS(VAR, LIST, MAXINDEX, FAILMSG) \
if(VAR->vt == VT_STR) { \
	propname = (char*)StrCache_Get(VAR->strVal); \
	prop = searchList(LIST, propname, MAXINDEX); \
	if(prop >= 0) { \
		ScriptVariant_ChangeType(VAR, VT_INTEGER); \
		VAR->lVal = prop; \
	} else { printf(FAILMSG, propname); } \
}

s_variantnode **global_var_list = NULL;

int max_global_var_index = -1;

int max_global_vars = MAX_GLOBAL_VAR;

//////////////////////////////////////////////////////////
////////////   openbor functions
//////////////////////////////////////////////////////////

// ===== playerkeys =====
enum {
	_pk_anybutton,
	_pk_attack,
	_pk_attack2,
	_pk_attack3,
	_pk_attack4,
	_pk_esc,
	_pk_jump,
	_pk_movedown,
	_pk_moveleft,
	_pk_moveright,
	_pk_moveup,
	_pk_screenshot,
	_pk_special,
	_pk_start,
	_pk_the_end,
};

void mapstrings_playerkeys(ScriptVariant ** varlist, int paramCount) {
	char *propname = NULL;
	int i, prop;

	static const char *proplist[] =	// for args 2+
	{
		"anybutton",
		"attack",
		"attack2",
		"attack3",
		"attack4",
		"esc",
		"jump",
		"movedown",
		"moveleft",
		"moveright",
		"moveup",
		"screenshot",
		"special",
		"start",
	};

	for(i = 2; i < paramCount; i++) {
		MAPSTRINGS(varlist[i], proplist, _pk_the_end, "Button name '%s' is not supported by playerkeys.");
	}
}


// ===== getbglayerproperty ======
enum {
	_gbglp_alpha,
	_gbglp_amplitude,
	_gbglp_bgspeedratio,
	_gbglp_enabled,
	_gbglp_transparency,
	_gbglp_watermode,
	_gbglp_wavelength,
	_gbglp_wavespeed,
	_gbglp_xoffset,
	_gbglp_xratio,
	_gbglp_xrepeat,
	_gbglp_xspacing,
	_gbglp_zoffset,
	_gbglp_zratio,
	_gbglp_zrepeat,
	_gbglp_zspacing,
	_gbglp_the_end,
};

void mapstrings_getbglayerproperty(ScriptVariant ** varlist, int paramCount) {
	char *propname = NULL;
	int prop;

	static const char *proplist[] = {
		"alpha",
		"amplitude",
		"bgspeedratio",
		"enabled",
		"transparency",
		"watermode",
		"wavelength",
		"wavespeed",
		"xoffset",
		"xratio",
		"xrepeat",
		"xspacing",
		"zoffset",
		"zratio",
		"zrepeat",
		"zspacing",
	};

	if(paramCount < 2)
		return;
	MAPSTRINGS(varlist[1], proplist, _gbglp_the_end,
		   "Property name '%s' is not supported by function getbglayerproperty.\n");
}


// ===== changebglayerproperty =====
enum {
	_cbglp_alpha,
	_cbglp_amplitude,
	_cbglp_bgspeedratio,
	_cbglp_enabled,
	_cbglp_transparency,
	_cbglp_watermode,
	_cbglp_wavelength,
	_cbglp_wavespeed,
	_cbglp_xoffset,
	_cbglp_xratio,
	_cbglp_xrepeat,
	_cbglp_xspacing,
	_cbglp_zoffset,
	_cbglp_zratio,
	_cbglp_zrepeat,
	_cbglp_zspacing,
	_cbglp_the_end,
};

void mapstrings_changebglayerproperty(ScriptVariant ** varlist, int paramCount) {
	char *propname = NULL;
	int prop;

	static const char *proplist[] = {
		"alpha",
		"amplitude",
		"bgspeedratio",
		"enabled",
		"transparency",
		"watermode",
		"wavelength",
		"wavespeed",
		"xoffset",
		"xratio",
		"xrepeat",
		"xspacing",
		"zoffset",
		"zratio",
		"zrepeat",
		"zspacing",
	};

	if(paramCount < 2)
		return;
	MAPSTRINGS(varlist[1], proplist, _cbglp_the_end,
		   "Property name '%s' is not supported by function changebglayerproperty.\n");
}


// ===== getfglayerproperty =====
enum {
	_gfglp_alpha,
	_gfglp_amplitude,
	_gfglp_bgspeedratio,
	_gfglp_enabled,
	_gfglp_transparency,
	_gfglp_watermode,
	_gfglp_wavelength,
	_gfglp_wavespeed,
	_gfglp_xoffset,
	_gfglp_xratio,
	_gfglp_xrepeat,
	_gfglp_xspacing,
	_gfglp_zoffset,
	_gfglp_zratio,
	_gfglp_zrepeat,
	_gfglp_zspacing,
	_gfglp_the_end,
};

void mapstrings_getfglayerproperty(ScriptVariant ** varlist, int paramCount) {
	char *propname = NULL;
	int prop;

	static const char *proplist[] = {
		"alpha",
		"amplitude",
		"bgspeedratio",
		"enabled",
		"transparency",
		"watermode",
		"wavelength",
		"wavespeed",
		"xoffset",
		"xratio",
		"xrepeat",
		"xspacing",
		"zoffset",
		"zratio",
		"zrepeat",
		"zspacing",
	};

	if(paramCount < 2)
		return;
	MAPSTRINGS(varlist[1], proplist, _gfglp_the_end,
		   "Property name '%s' is not supported by function getfglayerproperty.\n");
}


// ===== changefglayerproperty =====
enum {
	_cfglp_alpha,
	_cfglp_amplitude,
	_cfglp_bgspeedratio,
	_cfglp_enabled,
	_cfglp_transparency,
	_cfglp_watermode,
	_cfglp_wavelength,
	_cfglp_wavespeed,
	_cfglp_xoffset,
	_cfglp_xratio,
	_cfglp_xrepeat,
	_cfglp_xspacing,
	_cfglp_zoffset,
	_cfglp_zratio,
	_cfglp_zrepeat,
	_cfglp_zspacing,
	_cfglp_the_end,
};

void mapstrings_changefglayerproperty(ScriptVariant ** varlist, int paramCount) {
	char *propname = NULL;
	int prop;

	static const char *proplist[] = {
		"alpha",
		"amplitude",
		"bgspeedratio",
		"enabled",
		"transparency",
		"watermode",
		"wavelength",
		"wavespeed",
		"xoffset",
		"xratio",
		"xrepeat",
		"xspacing",
		"zoffset",
		"zratio",
		"zrepeat",
		"zspacing",
	};

	if(paramCount < 2)
		return;
	MAPSTRINGS(varlist[1], proplist, _cfglp_the_end,
		   "Property name '%s' is not supported by function changefglayerproperty.\n");
}

