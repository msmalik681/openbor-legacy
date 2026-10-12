/*
 * List library.
 * 90% (C) 2011 anallyst
 * 10% (C) unknown author
 *
 * license to use and redistribute is granted under the same terms as OpenBOR itself.
 * as long as anallyst is granted to use and redistribute the OpenBOR source code,
 * and maintain his own fork.
 * 
 * the whole smart stuff going on here was written by anallyst.
 * at the time anallyst touched this code, this was a very simple single linked list.
 * double linked list functionality is (C) anallyst.
 * 
 */

#ifndef LIST_H
#define LIST_H

#define USE_STRING_HASHES

#include <stddef.h>

typedef struct Node {
	struct Node *prev;
	struct Node *next;
	void *value;
	char *name;
} Node;

typedef struct Bucket {
	size_t size;
	size_t used;
	Node **nodes;
} Bucket;

typedef struct List {
	Node *first;
	Node *current;
	Node *last;
	size_t size;
	Bucket **buckets;
#ifdef DEBUG
	int initdone;
#endif
	int no_strdup:1;
} List;

void List_Set_UseStrdup(List *list, int use_strdup);
void Node_Clear(Node *node);
void List_Init(List *list);
void List_Clear(List *list);
void List_InsertAfter(List *list, void *value, char *name);
void List_Remove(List *list);
int List_GotoNext(List *list);
int List_GotoLast(List *list);
int List_GotoFirst(List *list);
void *List_Retrieve(const List *list);
int List_FindByName(List *list, const char *name);
void List_Reset(List *list);
int List_GetSize(const List *list);
Node *List_GetNodeByName(const List *list, const char *name);
Node *List_GetCurrentNode(List *list);

#endif
