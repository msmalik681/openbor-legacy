/*
 * OpenBOR - http://www.LavaLit.com
 * -----------------------------------------------------------------------
 * Licensed under the BSD license, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2011 OpenBOR Team
 */

#include "ScriptVariant.h"
#include <stdlib.h>
#define MAX_STR_VAR_LEN    64

// use string cache to cut the memory usage down, because not all variants are string, no need to give each of them an array
#define STRCACHE_INC      64
static char **strcache = NULL;
static int strcache_size = 0;
static int strcache_top = -1;
static int *strcache_index = NULL;

//clear the string cache
static void StrCache_Clear() {
	int i;
	if(strcache) {
		for(i = 0; i < strcache_size; i++) {
			free(strcache[i]);
			strcache[i] = NULL;
		}
		free(strcache);
		strcache = NULL;
	}
	if(strcache_index) {
		free(strcache_index);
		strcache_index = NULL;
	}
	strcache_size = 0;
	strcache_top = -1;
}

// int the string cache
static void StrCache_Init() {
	int i;
	StrCache_Clear();	// just in case
	strcache = malloc(sizeof(char *) * STRCACHE_INC);
	strcache_index = malloc(sizeof(int) * STRCACHE_INC);
	for(i = 0; i < STRCACHE_INC; i++) {
		strcache[i] = malloc((MAX_STR_VAR_LEN + 1) * sizeof(char));
		strcache[i][0] = 0;
		strcache_index[i] = i;
	}
	strcache_size = STRCACHE_INC;
	strcache_top = strcache_size - 1;
}

static void StrCache_Collect(int index) {
	if(strcache_size) {
		strcache_top++;
		if(strcache_top < strcache_size) {
			strcache_index[strcache_top] = index;
		}
	}
}

static int StrCache_Pop() {
	char **temp;
	int *tempi;
	int i;
	if(strcache_size == 0) {
		StrCache_Init();
	}
	if(strcache_top < 0)	// realloc
	{
		temp = malloc(sizeof(char *) * (strcache_size + STRCACHE_INC));
		for(i = strcache_size; i < strcache_size + STRCACHE_INC; i++) {
			temp[i] = malloc((MAX_STR_VAR_LEN + 1) * sizeof(char));
			temp[i][0] = 0;
		}
		for(i = 0; i < strcache_size; i++) {
			temp[i] = strcache[i];
		}
		free(strcache);
		strcache = temp;
		tempi = malloc(sizeof(int) * (strcache_size + STRCACHE_INC));
		for(i = 0; i < STRCACHE_INC; i++) {
			tempi[i] = strcache_size + i;
		}
		free(strcache_index);
		strcache_index = tempi;
		strcache_size += STRCACHE_INC;
		strcache_top += STRCACHE_INC;
	}
	return strcache_index[strcache_top--];
}

char *StrCache_Get(int index) {
	if(index < strcache_size) {
		return strcache[index];
	}
	return NULL;
}

void ScriptVariant_ChangeType(ScriptVariant * var, VARTYPE cvt) {
	if(var->vt == cvt)
		return;

	if(var->vt == VT_STR) {
		if(cvt != VT_STR) {
			StrCache_Collect(var->strVal);
		}
	} else if(cvt == VT_STR) {
		var->strVal = StrCache_Pop();
	}
	var->vt = cvt;
}