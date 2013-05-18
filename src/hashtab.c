/*
 * hashtab.c - Simple, Reliable C Hashtable
 */

#include <stdlib.h>
#include <string.h>
#include "hashtab.h"

hashtab_t *ht_init(size_t size, int (*hash_func) (void *, size_t, size_t))
{
    hashtab_t *new_ht = (hashtab_t *) malloc(sizeof(hashtab_t));

    new_ht->arr =
	(hashtab_node_t **) malloc(sizeof(hashtab_node_t *) * size);

    new_ht->size = size;
    new_ht->count = 0;

    /* all entries are empty */
    int i = 0;
    for (i = 0; i < (int) size; i++) {
	new_ht->arr[i] = NULL;
    }

    if (hash_func == NULL)
	new_ht->hash_func = &ht_hash;
    else
	new_ht->hash_func = hash_func;

    return new_ht;
}

void *ht_search(hashtab_t * hashtable, void *key, size_t keylen)
{
    int index = ht_hash(key, keylen, hashtable->size);

    if (hashtable->arr[index] == NULL)
	return NULL;

    hashtab_node_t *last_node = hashtable->arr[index];
    while (last_node != NULL) {
	/* only compare matching keylens */
	if (last_node->keylen == keylen) {
	    /* compare keys */
	    if (memcmp(key, last_node->key, keylen) == 0) {
		return last_node->value;
	    }
	}

	last_node = last_node->next;
    }

    return NULL;
}

void *ht_insert(hashtab_t * hashtable,
		void *key, size_t keylen, void *value, size_t vallen)
{
    int index = ht_hash(key, keylen, hashtable->size);

    hashtab_node_t *next_node, *last_node;
    next_node = hashtable->arr[index];
    last_node = NULL;

    /* Search for an existing key. */
    while (next_node != NULL) {
	/* only compare matching keylens */
	if (next_node->keylen == keylen) {
	    /* compare keys */
	    if (memcmp(key, next_node->key, keylen) == 0) {
		/* this key already exists, replace it */
		if (next_node->vallen != vallen) {
		    /* new value is a different size */
		    free(next_node->value);
		    next_node->value = malloc(vallen);
		    if (next_node->value == NULL)
			return NULL;
		}
		memcpy(next_node->value, value, vallen);
		next_node->vallen = vallen;
		return next_node->value;
	    }
	}

	last_node = next_node;
	next_node = next_node->next;
    }

    /* create a new node */
    hashtab_node_t *new_node;
    new_node = (hashtab_node_t *) malloc(sizeof(hashtab_node_t));
    if (new_node == NULL)
	return NULL;

    /* get some memory for the new node data */
    new_node->key = malloc(keylen);
    new_node->value = malloc(vallen);
    if (new_node->key == NULL || new_node->key == NULL) {
	free(new_node->key);
	free(new_node->value);
	free(new_node);
	return NULL;
    }

    /* copy over the value and key */
    memcpy(new_node->key, key, keylen);
    memcpy(new_node->value, value, vallen);
    new_node->keylen = keylen;
    new_node->vallen = vallen;

    /* no next node */
    new_node->next = NULL;

    /* Tack the new node on the end or right on the table. */
    if (last_node != NULL)
	last_node->next = new_node;
    else
	hashtable->arr[index] = new_node;

    hashtable->count++;
    return new_node->value;
}

/* delete the given key from the hashtable */
void ht_remove(hashtab_t * hashtable, void *key, size_t keylen)
{
    hashtab_node_t *last_node, *next_node;
    int index = ht_hash(key, keylen, hashtable->size);
    next_node = hashtable->arr[index];
    last_node = NULL;

    while (next_node != NULL) {
	if (next_node->keylen == keylen) {
	    /* compare keys */
	    if (memcmp(key, next_node->key, keylen) == 0) {
		/* free node memory */
		free(next_node->value);
		free(next_node->key);

		/* adjust the list pointers */
		if (last_node != NULL)
		    last_node->next = next_node->next;
		else
		    hashtable->arr[index] = next_node->next;

		/* free the node */
		free(next_node);
		break;
	    }
	}

	last_node = next_node;
	next_node = next_node->next;
    }
}

/* grow the hashtable */
void *ht_grow(hashtab_t * old_ht, size_t new_size)
{
    /* create new hashtable */
    hashtab_t *new_ht = ht_init(new_size, old_ht->hash_func);
    if (new_ht == NULL)
	return NULL;

    void *ret;			/* captures return values */

    /* Iterate through the old hashtable. */
    hashtab_iter_t ii;
    ht_iter_init(old_ht, &ii);
    for (; ii.key != NULL; ht_iter_inc(&ii)) {
	ret = ht_insert(new_ht, ii.key, ii.keylen, ii.value, ii.vallen);
	if (ret == NULL) {
	    /* Insert failed. Destroy new hashtable and return. */
	    ht_destroy(new_ht);
	    return NULL;
	}
    }

    /* Destroy the old hashtable. */
    ht_destroy(old_ht);

    return new_ht;
}

/* free all resources used by the hashtable */
void ht_destroy(hashtab_t * hashtable)
{
    hashtab_node_t *next_node, *last_node;

    /* Free each linked list in hashtable. */
    int i;
    for (i = 0; i < (int) hashtable->size; i++) {
	next_node = hashtable->arr[i];
	while (next_node != NULL) {
	    /* destroy node */
	    free(next_node->key);
	    free(next_node->value);
	    last_node = next_node;
	    next_node = next_node->next;
	    free(last_node);
	}
    }

    free(hashtable->arr);
    free(hashtable);
}

/* iterator initilaize */
void ht_iter_init(hashtab_t * hashtable, hashtab_iter_t * ii)
{
    /* stick in initial bookeeping data */
    ii->internal.hashtable = hashtable;
    ii->internal.node = NULL;
    ii->internal.index = -1;

    /* have iterator point to first element */
    ht_iter_inc(ii);
}

/* iterator increment  */
void ht_iter_inc(hashtab_iter_t * ii)
{
    hashtab_t *hashtable = ii->internal.hashtable;
    int index = ii->internal.index;

    /* attempt to grab the next node */
    if (ii->internal.node == NULL || ii->internal.node->next == NULL)
	index++;
    else {
	/* next node in the list */
	ii->internal.node = ii->internal.node->next;
	ii->key = ii->internal.node->key;
	ii->value = ii->internal.node->value;
	ii->keylen = ii->internal.node->keylen;
	ii->vallen = ii->internal.node->vallen;
	return;
    }

    /* find next node */
    while (hashtable->arr[index] == NULL && index < (int) hashtable->size)
	index++;

    if (index >= (int) hashtable->size) {
	/* end of hashtable */
	ii->internal.node = NULL;
	ii->internal.index = (int) hashtable->size;

	ii->key = NULL;
	ii->value = NULL;
	ii->keylen = 0;
	ii->vallen = 0;
	return;
    }

    /* point to the next item in the hashtable */
    ii->internal.node = hashtable->arr[index];
    ii->internal.index = index;
    ii->key = ii->internal.node->key;
    ii->value = ii->internal.node->value;
    ii->keylen = ii->internal.node->keylen;
    ii->vallen = ii->internal.node->vallen;
}

int ht_hash(void *key, size_t keylen, size_t hashtab_size)
{
    int sum = 0;

    /* very simple hash function for now */
    int i;
    for (i = 0; i < (int) keylen; i++) {
	sum += ((unsigned char *) key)[i] * (i + 1);
    }

    return (sum % (int) hashtab_size);
}
