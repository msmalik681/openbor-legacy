/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

#ifndef SCRIPTVARIANT_H
#define SCRIPTVARIANT_H

#include "types.h"

typedef enum {
	VT_EMPTY = 0,		//not initialized
	VT_INTEGER = 1,		//int/long
	VT_DECIMAL = 2,		//double
	VT_PTR = 5,		//void*
	VT_STR = 6,		//char*
} VARTYPE;

typedef struct {
	union			//value
	{
		s32 lVal;
		void *ptrVal;
		double dblVal;
		int strVal;
	};
	VARTYPE vt;		//variant type
} ScriptVariant;

char *StrCache_Get(int index);

void ScriptVariant_ChangeType(ScriptVariant * var, VARTYPE cvt);

#endif
