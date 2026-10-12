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
#include <stdlib.h>
#include <string.h>

void List_Set_UseStrdup(List *list, int use) {
	list->no_strdup = !use;
}

#ifdef DEBUG
void chklist(const List *list) {
	assert(list);
	assert(list->initdone == 1);
}
#endif

#define HASH_BUCKET_COUNT 256

static unsigned char strhash(const char *s) {
	ptrdiff_t tmp = 0;
	const char *p = s;
	while(*p)
		tmp += *p++ - 'A';
	return (size_t) tmp % HASH_BUCKET_COUNT;
}

#define BUCKET_INIT_NODES_ALLOC ((size_t) 2)
static void List_AddHash(List *list, Node *node) {
#ifdef DEBUG
	chklist(list);
#endif
	assert(node);
	if(!node->name)
		return;

	if(!list->buckets)
		list->buckets = calloc(HASH_BUCKET_COUNT, sizeof *list->buckets);

	unsigned char h = strhash(node->name);
	Bucket *bucket = list->buckets[h];
	if(!bucket) {
		bucket = calloc(1, sizeof *bucket);
		list->buckets[h] = bucket;
		bucket->nodes = calloc(BUCKET_INIT_NODES_ALLOC, sizeof *bucket->nodes);
		assert(bucket->nodes);
		bucket->size = BUCKET_INIT_NODES_ALLOC;
	}

	size_t capacity = bucket->size;
	assert(bucket->used <= capacity);
	if(bucket->used == capacity) {
		bucket->nodes = realloc(bucket->nodes, sizeof *bucket->nodes * (capacity * 2));
		assert(bucket->nodes);
		bucket->size = capacity * 2;
	}

	bucket->nodes[bucket->used++] = node;
}

static void List_RemoveHash(List *list, const Node *node) {
#ifdef DEBUG
	chklist(list);
#endif
	if(!node->name)
		return;
	unsigned char h = strhash(node->name);
	Bucket *bucket = list->buckets[h];
	assert(bucket);
	assert(bucket->used > 0);
	size_t i = 0;
	while(i < bucket->used && node != bucket->nodes[i])
		i++;
	if(i < bucket->used)
		bucket->nodes[i] = NULL;
}

static void List_FreeHashes(List *list) {
#ifdef DEBUG
	chklist(list);
#endif
	if(!list->buckets)
		return;
	for(size_t i = 0; i < 256; i++) {
		Bucket *bucket = list->buckets[i];
		if(bucket) {
			free(bucket->nodes);
			free(bucket);
		}
	}
	free(list->buckets);
	list->buckets = NULL;
}

int List_GotoLast(List *list) {
#ifdef DEBUG
	chklist(list);
#endif
	if(list->size == 0)
		return 0;
	list->current = list->last;
	return 1;
}

int List_GotoFirst(List *list) {
#ifdef DEBUG
	chklist(list);
#endif
	if(list->size == 0)
		return 0;
	list->current = list->first;
	return 1;
}

Node *List_GetCurrentNode(List *list) {
#ifdef DEBUG
	chklist(list);
#endif
	return list ? list->current : NULL;
}

void Node_Clear(Node *node) {
	if(!node)
		return;
	free(node->name);
	node->name = NULL;
}

void List_Init(List *list) {
	list->first = list->current = list->last = NULL;
	list->size = 0;
	list->buckets = NULL;
#ifdef DEBUG
	list->initdone = 1;
#endif
	list->no_strdup = 0;
}

void List_Clear(List *list) {
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

void List_InsertAfter(List *list, void *e, char *theName) {
#ifdef DEBUG
	chklist(list);
#endif
	Node *node = malloc(sizeof *node);
	Node *current = list->current;

	assert(node);
	node->value = e;
	node->name = list->no_strdup ? theName : strdup(theName);

	List_AddHash(list, node);

	if(list->size == 0) {
		node->prev = NULL;
		node->next = NULL;
		list->current = list->first = list->last = node;
	} else {
		node->next = current->next;
		node->prev = current;
		if(current->next)
			current->next->prev = node;
		current->next = node;
		if(current == list->last)
			list->last = node;
		list->current = node;
	}
	list->size++;
}

void List_Remove(List *list) {
#ifdef DEBUG
	chklist(list);
#endif
	Node *current = list->current;
	if(list->size == 0)
		return;
	if(list->size == 1) {
		List_RemoveHash(list, current);
		if(!list->no_strdup) {
			Node_Clear(current);
		}
		free(current);
		list->first = list->current = list->last = NULL;
	} else {
		int was_last = current == list->last;
		int was_first = current == list->first;
		if(current->prev)
			current->prev->next = current->next;
		if(current->next)
			current->next->prev = current->prev;
		Node *nptr = was_last ? current->prev : current->next;

		List_RemoveHash(list, current);

		if(!list->no_strdup) {
			Node_Clear(current);
		}
		free(current);

		if(was_last)
			list->last = nptr;
		if(was_first)
			list->first = nptr;
		list->current = nptr;
	}
	list->size--;
}

int List_GotoNext(List *list) {
#ifdef DEBUG
	chklist(list);
#endif
	if(list->size == 0 || list->current == list->last)
		return 0;
	list->current = list->current->next;
	return 1;
}

void *List_Retrieve(const List *list) {
#ifdef DEBUG
	chklist(list);
#endif
	return list->current ? list->current->value : NULL;
}

Node *List_GetNodeByName(List *list, const char *name) {
#ifdef DEBUG
	chklist(list);
#endif
	if(!name || !list->buckets)
		return NULL;

	unsigned char h = strhash(name);
	const Bucket *bucket = list->buckets[h];
	if(!bucket)
		return NULL;
	for(size_t i = 0; i < bucket->used; i++) {
		Node *nptr = bucket->nodes[i];
		if(nptr && strcmp(name, nptr->name) == 0)
			return nptr;
	}
	return NULL;
}

int List_FindByName(List *list, const char *name) {
#ifdef DEBUG
	chklist(list);
#endif
	Node *node = List_GetNodeByName(list, name);
	if(!node)
		return 0;
	list->current = node;
	return 1;
}

void List_Reset(List *list) {
#ifdef DEBUG
	chklist(list);
#endif
	list->current = list->first;
}

int List_GetSize(const List *list) {
#ifdef DEBUG
	chklist(list);
#endif
	return list->size;
}
