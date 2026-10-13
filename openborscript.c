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
enum { _pk_the_end = 14 };

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
enum { _gbglp_the_end = 16 };

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
enum { _cbglp_the_end = 16 };

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
enum { _gfglp_the_end = 16 };

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
enum { _cfglp_the_end = 16 };

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

