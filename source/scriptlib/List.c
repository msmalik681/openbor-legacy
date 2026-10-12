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
void chklist(const List *list) {
	assert(list);
	assert(list->initdone == 1);
}
#endif

static unsigned char strhash(const char *s) {
	ptrdiff_t tmp = 0;
	const char *p = s;
	while(*p)
		tmp += *p++ - 'A';
	return (size_t) tmp % 256;
}

#define BUCKET_INIT_NODES_ALLOC 2
static void List_AddHash(List * list, Node * node) {
#ifdef DEBUG
	chklist(list);
#endif
	unsigned char h;
	size_t save;

	assert(node);
	if(!node->name)
		return;

	if(!list->buckets)
		list->buckets = calloc(1, sizeof(Bucket *) * 256);

	h = strhash(node->name);
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

	list->buckets[h]->nodes[list->buckets[h]->used++] = node;
}

static void List_RemoveHash(List * list, Node * node) {
#ifdef DEBUG
	chklist(list);
#endif
	unsigned char h;
	size_t i;
	if(!node->name)
		return;
	h = strhash(node->name);
	assert(list->buckets[h]);
	assert(list->buckets[h]->used > 0);
	for(i = 0; i < list->buckets[h]->used; i++) {
		if(node == list->buckets[h]->nodes[i]) {
			list->buckets[h]->nodes[i] = NULL;
			break;
		}
	}
}

static void List_FreeHashes(List * list) {
#ifdef DEBUG
	chklist(list);
#endif
	size_t i;
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

int List_GotoLast(List * list) {
#ifdef DEBUG
	chklist(list);
#endif
	if(!list->size)
		return 0;
	list->current = list->last;
	return 1;
}

int List_GotoFirst(List * list) {
#ifdef DEBUG
	chklist(list);
#endif
	if(!list->size)
		return 0;
	list->current = list->first;
	return 1;
}

Node *List_GetCurrentNode(List * list) {
#ifdef DEBUG
	chklist(list);
#endif
	return list == NULL ? NULL : list->current;
}

void Node_Clear(Node * node) {
	if(!node)
		return;
	free(node->name);
	node->name = NULL;
}

void List_Init(List * list) {
	list->first = list->current = list->last = NULL;
	list->size = 0;
	list->buckets = NULL;
#ifdef DEBUG
	list->initdone = 1;
#endif
	list->no_strdup = 0;
}

void List_Clear(List * list) {
#ifdef DEBUG
	chklist(list);
#endif
	Node *nptr = list->first;
	while(nptr) {
		Node *next = nptr->next;
		if(!list->no_strdup)
			Node_Clear(nptr);
		free(nptr);
		nptr = next;
	}
	List_FreeHashes(list);
	List_Init(list);
}

void List_InsertAfter(List * list, void *e, char *theName) {
#ifdef DEBUG
	chklist(list);
#endif
	Node *nptr = malloc(sizeof(Node));

	assert(nptr != NULL);
	nptr->value = e;
	nptr->name = (list->no_strdup) ? theName : strdup(theName);

	List_AddHash(list, nptr);

	if(list->size == 0) {
		nptr->prev = NULL;
		nptr->next = NULL;
		list->current = list->first = list->last = nptr;
	} else {
		nptr->next = list->current->next;
		nptr->prev = list->current;
		if(list->current->next)
			list->current->next->prev = nptr;
		list->current->next = nptr;
		if(list->current == list->last)
			list->last = nptr;
		list->current = nptr;
	}
	list->size++;
}

void List_Remove(List * list) {
#ifdef DEBUG
	chklist(list);
#endif
	Node *nptr;
	if(list->size == 0)
		return;
	if(list->size == 1) {
		List_RemoveHash(list, list->current);
		if(!list->no_strdup) Node_Clear(list->current);
		free(list->current);
		list->first = list->current = list->last = NULL;
	} else {
		if(list->current->prev)
			list->current->prev->next = list->current->next;
		if(list->current->next)
			list->current->next->prev = list->current->prev;
		if(list->current == list->last)
			nptr = list->current->prev;
		else
			nptr = list->current->next;

		List_RemoveHash(list, list->current);

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
	chklist(list);
#endif
	if(list->current == list->last)
		return 0;
	list->current = list->current->next;
	return 1;
}

void *List_Retrieve(const List * list) {
#ifdef DEBUG
	chklist(list);
#endif
	return list->current ? list->current->value : NULL;
}

Node *List_GetNodeByName(List * list, const char *name) {
#ifdef DEBUG
	chklist(list);
#endif
	if(!name)
		return NULL;

	size_t i;
	unsigned char h = strhash(name);
	if(!list->buckets || !list->buckets[h])
		return NULL;
	for(i = 0; i < list->buckets[h]->used; i++) {
		Node *nptr = list->buckets[h]->nodes[i];
		if(nptr && strcmp(name, nptr->name) == 0)
			return nptr;
	}
	return NULL;
}

int List_FindByName(List * list, const char *name) {
#ifdef DEBUG
	chklist(list);
#endif

	Node *n = List_GetNodeByName(list, name);
	if(!n)
		return 0;
	list->current = n;
	return 1;
}

void List_Reset(List * list) {
#ifdef DEBUG
	chklist(list);
#endif
	list->current = list->first;
}

int List_GetSize(const List * list) {
#ifdef DEBUG
	chklist(list);
#endif
	return list->size;
}
