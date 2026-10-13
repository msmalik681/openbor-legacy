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

typedef struct Script {
    int dummy;
} Script;


void mapstrings_playerkeys(ScriptVariant ** varlist, int paramCount);
void mapstrings_getbglayerproperty(ScriptVariant ** varlist, int paramCount);
void mapstrings_changebglayerproperty(ScriptVariant ** varlist, int paramCount);
void mapstrings_getfglayerproperty(ScriptVariant ** varlist, int paramCount);
void mapstrings_changefglayerproperty(ScriptVariant ** varlist, int paramCount);

#endif