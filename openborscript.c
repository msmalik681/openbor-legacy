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

// Map string arguments to indexes in a property-name array.
#define MAPSTRINGS(VAR, LIST, FAILMSG) \
if(VAR->vt == VT_STR) { \
	propname = StrCache_Get(VAR->strVal); \
	prop = searchList(LIST, propname, sizeof(LIST) / sizeof((LIST)[0])); \
	if(prop >= 0) { \
		ScriptVariant_ChangeType(VAR, VT_INTEGER); \
		VAR->lVal = prop; \
	} else { printf(FAILMSG, propname); } \
}

//////////////////////////////////////////////////////////
////////////   openbor functions
//////////////////////////////////////////////////////////

// ===== playerkeys =====
void mapstrings_playerkeys(ScriptVariant ** varlist, int paramCount) {
	char *propname;
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
		MAPSTRINGS(varlist[i], proplist, "Button name '%s' is not supported by playerkeys.");
	}
}


// Shared property names for background and foreground layer functions.
static const char *layer_property_list[] = {
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

// ===== getbglayerproperty ======
void mapstrings_getbglayerproperty(ScriptVariant ** varlist, int paramCount) {
	char *propname;
	int prop;

	if(paramCount < 2)
		return;
	MAPSTRINGS(varlist[1], layer_property_list,
		   "Property name '%s' is not supported by function getbglayerproperty.\n");
}


// ===== changebglayerproperty =====
void mapstrings_changebglayerproperty(ScriptVariant ** varlist, int paramCount) {
	char *propname;
	int prop;

	if(paramCount < 2)
		return;
	MAPSTRINGS(varlist[1], layer_property_list,
		   "Property name '%s' is not supported by function changebglayerproperty.\n");
}


// ===== getfglayerproperty =====
void mapstrings_getfglayerproperty(ScriptVariant ** varlist, int paramCount) {
	char *propname;
	int prop;

	if(paramCount < 2)
		return;
	MAPSTRINGS(varlist[1], layer_property_list,
		   "Property name '%s' is not supported by function getfglayerproperty.\n");
}


// ===== changefglayerproperty =====
void mapstrings_changefglayerproperty(ScriptVariant ** varlist, int paramCount) {
	char *propname;
	int prop;

	if(paramCount < 2)
		return;
	MAPSTRINGS(varlist[1], layer_property_list,
		   "Property name '%s' is not supported by function changefglayerproperty.\n");
}

