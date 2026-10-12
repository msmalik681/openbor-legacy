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

#include "List.h"
#include <assert.h>

void List_Set_UseStrdup(List* list, int use) {
	list->no_strdup = !use;
}

#ifdef DEBUG
void chklist(List * list) {
	assert(list);
	assert(list->initdone == 1);	// method called on uninitialised list
}
#endif

#ifdef USE_STRING_HASHES
static unsigned char strhash(char *s) {
	ptrdiff_t tmp = 0;
	char *p = s;
	while(*p) {
		tmp += *p - 'A';
		p++;
	}
	return (size_t) tmp % 256;
}

/* add a single node to the hash list */
#define BUCKET_INIT_NODES_ALLOC 2
static void List_AddHash(List * list, Node * node) {
#ifdef DEBUG
	chklist((List *) list);
#endif
	unsigned char h;
	size_t save;

	assert(node);
	if(!node->name)
		return;

	if(!list->buckets)
		list->buckets = calloc(1, sizeof(Bucket *) * 256);

	h = strhash((char *) node->name);
	if(!list->buckets[h]) {
		list->buckets[h] = calloc(1, sizeof(Bucket));
		list->buckets[h]->nodes = calloc(1, sizeof(Node *) * BUCKET_INIT_NODES_ALLOC);
		assert(list->buckets[h]->nodes != NULL);
		list->buckets[h]->size = BUCKET_INIT_NODES_ALLOC;
	}

	save = list->buckets[h]->size;
	assert(list->buckets[h]->used <= save);
	if(list->buckets[h]->used == save) {
		list->buckets[h]->nodes = realloc(list->buckets[h]->nodes, sizeof(Node *) * (save * 2));
		assert(list->buckets[h]->nodes != NULL);
		list->buckets[h]->size = save * 2;
	}

	list->buckets[h]->nodes[list->buckets[h]->used] = node;
	list->buckets[h]->used++;
}

/* Remove a node from the string-hash bucket. */
static void List_RemoveHash(List * list, Node * node) {
#ifdef DEBUG
	chklist((List *) list);
#endif
	unsigned char h;
	size_t i;
	if(!node->name)
		return;
	h = strhash((char *) node->name);
	assert(list->buckets[h]);
	assert(list->buckets[h]->used > 0);
	for(i = 0; i < list->buckets[h]->used; i++) {
		if(node == list->buckets[h]->nodes[i]) {
			list->buckets[h]->nodes[i] = NULL;
			break;
		}
	}
}

/* free everything related to the string hash list
usually you dont have to do it manually, since its called by List_Clear
but it won't hurt either */
static void List_FreeHashes(List * list) {
#ifdef DEBUG
	chklist((List *) list);
#endif
	int i;
	if(!list->buckets)
		return;
	for(i = 0; i < 256; i++) {
		if(list->buckets[i]) {
			free(list->buckets[i]->nodes);
			free(list->buckets[i]);
		}
	}
	free(list->buckets);
	list->buckets = NULL;
}

#endif

int List_GotoLast(List * list) {
#ifdef DEBUG
	chklist((List *) list);
#endif
	if(list->size)
		list->current = list->last;
	else
		return 0;
	return 1;
}

int List_GotoFirst(List * list) {
#ifdef DEBUG
	chklist((List *) list);
#endif
	if(list->size)
		list->current = list->first;
	else
		return 0;
	return 1;
}

Node *List_GetCurrentNode(List * list) {
#ifdef DEBUG
	chklist((List *) list);
#endif
	return list == NULL ? NULL : list->current;
}

void Node_Clear(Node * node) {
	if(!node)
		return;
	if(node->name) {
		free((void *) node->name);
		node->name = NULL;
	}
}

void List_Init(List * list) {
	list->first = list->current = list->last = NULL;
	list->size = 0;
#ifdef USE_STRING_HASHES
	list->buckets = NULL;
#endif
#ifdef DEBUG
	list->initdone = 1;
#endif
	list->no_strdup = 0;
}

void List_Clear(List * list) {
#ifdef DEBUG
	chklist((List *) list);
#endif

	//Delete all the Nodes in the list.
	Node *nptr = list->first;
	list->current = list->first;

	while(list->current) {
		list->current = list->current->next;
		if(!list->no_strdup) Node_Clear(nptr);
		free(nptr);
		nptr = list->current;
	}


#ifdef USE_STRING_HASHES
	List_FreeHashes(list);
#endif

	List_Init(list);
}
// Insertion function.
void List_InsertAfter(List * list, void *e, char *theName) {
#ifdef DEBUG
	chklist((List *) list);
#endif
//Construct a new Node and fill it with the appropriate value
	Node *nptr = (Node *) malloc(sizeof(Node));

	assert(nptr != NULL);
	nptr->value = e;
	nptr->name = (list->no_strdup) ? theName : NAME(theName);

#ifdef USE_STRING_HASHES
	List_AddHash(list, nptr);
#endif

	if(list->size == 0) {
		nptr->prev = NULL;
		nptr->next = NULL;
		list->current = list->first = list->last = nptr;
	} else {
		nptr->next = list->current->next;
		nptr->prev = list->current;
		if(list->current->next != NULL)
			list->current->next->prev = nptr;
		list->current->next = nptr;
		if(list->current == list->last)
			list->last = nptr;
		list->current = nptr;
	}
	list->size++;
}

//removes the current node and sets current to next, if applicable
void List_Remove(List * list) {
#ifdef DEBUG
	chklist((List *) list);
#endif
	Node *nptr;
	if(list->size == 0) {
		//OutputDebugString("Attempted to remove from empty list.\n");
		return;
	} else if(list->size == 1) {
#ifdef USE_STRING_HASHES
		List_RemoveHash(list, list->current);
#endif
		if(!list->no_strdup) Node_Clear(list->current);
		free(list->current);
		list->first = list->current = list->last = NULL;
	} else {
		if(list->current->prev != NULL)
			list->current->prev->next = list->current->next;
		if(list->current->next)
			list->current->next->prev = list->current->prev;
		if(list->current == list->last)
			nptr = list->current->prev;
		else
			nptr = list->current->next;

#ifdef USE_STRING_HASHES
		List_RemoveHash(list, list->current);
#endif

		if(!list->no_strdup) Node_Clear(list->current);
		free(list->current);

		if(list->current == list->last)
			list->last = nptr;
		if(list->current == list->first)
			list->first = nptr;
		list->current = nptr;
	}
	list->size--;
}

int List_GotoNext(List * list) {
#ifdef DEBUG
	chklist((List *) list);
#endif
	if(list->current != list->last)
		list->current = list->current->next;
	else
		return 0;
	return 1;
}

void *List_Retrieve(const List * list) {
#ifdef DEBUG
	chklist((List *) list);
#endif
	if(list->current)
		return list->current->value;
	else
		return NULL;
}


/* returns the first node of which name is equal to theName */
Node *List_GetNodeByName(List * list, char *name) {
#ifdef DEBUG
	chklist((List *) list);
#endif
	Node *nptr;
	if(!name)
		return NULL;

#ifdef USE_STRING_HASHES
	size_t i;
	unsigned char h = strhash(name);
	if(!list->buckets || !list->buckets[h])
		return 0;
	for(i = 0; i < list->buckets[h]->used; i++) {
		nptr = list->buckets[h]->nodes[i];
		if(nptr && strcmp(name, nptr->name) == 0) {
			return list->buckets[h]->nodes[i];
		}
	}
	return NULL;
#else
	nptr = list->first;

	while(nptr) {
		if(nptr->name) {
			if(strcmp(nptr->name, name) == 0)
				return nptr;
		}
		nptr = nptr->next;
	}
	return NULL;
#endif

}

/* SIDE EFFECTS: sets list->current to the first found node */
int List_FindByName(List * list, char *name) {
#ifdef DEBUG
	chklist((List *) list);
#endif

	Node *n = List_GetNodeByName(list, name);
	if(n) {
		list->current = n;
		return 1;
	}
	return 0;
}

void List_Reset(List * list) {
#ifdef DEBUG
	chklist(list);
#endif
	list->current = list->first;
}

int List_GetSize(const List * list) {
#ifdef DEBUG
	chklist((List *) list);
#endif
	return list->size;
}
