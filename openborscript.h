/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

#ifndef OPENBORSCRIPT_H
#define OPENBORSCRIPT_H

#include "ScriptVariant.h"

#define MAX_GLOBAL_VAR 2048
#define MAX_KEY_LEN    24

typedef struct {
	ScriptVariant value;
	char key[MAX_KEY_LEN];
} s_variantnode;

typedef struct Script {
    int dummy;
} Script;

extern s_variantnode **global_var_list;
extern int max_global_vars;
extern int max_global_var_index;


void mapstrings_playerkeys(ScriptVariant ** varlist, int paramCount);
void mapstrings_getbglayerproperty(ScriptVariant ** varlist, int paramCount);
void mapstrings_changebglayerproperty(ScriptVariant ** varlist, int paramCount);
void mapstrings_getfglayerproperty(ScriptVariant ** varlist, int paramCount);
void mapstrings_changefglayerproperty(ScriptVariant ** varlist, int paramCount);

#endif