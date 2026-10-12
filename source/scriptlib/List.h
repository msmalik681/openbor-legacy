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

// Optional index lookup support.
#define USE_INDEX

// Optional string lookup support.
#define USE_STRING_HASHES

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//A macro to simplify iterating through all this lists.
#define FOREACH( x, y ) { \
	  size = List_GetSize(&x); \
	  List_Reset(&x); \
	  for (i = 0; i < size; i++){ \
		 y \
		 List_GotoNext(&x); \
	  } \
   }


#define NAME(s) ((s)?(strdup(s)):NULL)

typedef struct Node {
	struct Node *prev;	//pointer to previous Node
	struct Node *next;	//pointer to next Node
	void *value;		//data stored in a Node
	char *name;		//optional name of the Node
} Node;

#ifdef USE_INDEX
typedef struct LIndex {
	size_t size;
	size_t used;
	Node **nodes;
	ptrdiff_t *indices;
} LIndex;
#endif

#ifdef USE_STRING_HASHES
typedef struct Bucket {
	size_t size;
	size_t used;
	Node **nodes;
} Bucket;
#endif

typedef struct List {
	//Data members
	Node *first;
	Node *current;
	Node *last;
	size_t size;
#ifdef USE_INDEX
	LIndex **mindices;
#endif
#ifdef USE_STRING_HASHES
	Bucket **buckets;
#endif
#ifdef DEBUG
	int initdone;
#endif
	int no_strdup:1;
} List;

void List_Set_UseStrdup(List* list, int use);
void Node_Clear(Node * node);
void List_Init(List * list);
void List_Clear(List * list);
void List_InsertAfter(List * list, void *e, char *theName);
void List_Remove(List * list);
int List_GotoNext(List * list);
int List_GotoLast(List * list);
int List_GotoFirst(List * list);
void *List_Retrieve(const List * list);
int List_FindByName(List * list, char *name);
void List_Reset(List * list);
int List_GetSize(const List * list);

Node *List_GetNodeByName(List * list, char *Name);
Node *List_GetCurrentNode(List * list);
#ifdef USE_INDEX
void List_AddIndex(List * list, Node * node, size_t index);
void List_RemoveLastIndex(List * list);
void List_CreateIndices(List * list);
void List_FreeIndices(List * list);
unsigned char ptrhash(void *value);	// need to export that as well for unittest.
#endif

#endif
