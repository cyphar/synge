/* ohmic: A fairly reliable hashmap "library"
 * Copyright (c) 2013 Cyphar
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * 1. The above copyright notice and this permission notice shall be included in
 *    all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include "ohmic.h"

ohm_t *ohm_init(int size, int (*hash_func)(void *, int, int)) {
	if(size < 1) return NULL;
	ohm_t *new_ohm = malloc(sizeof(ohm_t));

	new_ohm->table = malloc(sizeof(ohm_node *) * size);
	new_ohm->size = size;
	new_ohm->count = 0;

	/* initialise all entries as empty */
	int i;
	for(i = 0; i < size; i++)
		new_ohm->table[i] = NULL;

	if(hash_func)
		new_ohm->hash = hash_func;
	else
		new_ohm->hash = ohm_hash;

	return new_ohm;
} /* ohm_init() */

void ohm_free(ohm_t *hashmap) {
	if(!hashmap) return;

	ohm_node *current_node, *parent_node;

	int i;
	for(i = 0; i < hashmap->size; i++) {
		current_node = hashmap->table[i];
		while(current_node) {
			free(current_node->key);
			free(current_node->value);

			parent_node = current_node;
			current_node = current_node->next;

			free(parent_node);
		}
	}

	free(hashmap->table);
	free(hashmap);
} /* ohm_free() */

void *ohm_search(ohm_t *hashmap, void *key, int keylen) {
	if(!key || keylen < 1) return NULL;

	int index = hashmap->hash(key, keylen, hashmap->size);
	if(index < 0 || !hashmap->table[index])
		return NULL;

	ohm_node *current_node = hashmap->table[index];
	while(current_node) {
		if(!memcmp(current_node->key, key, keylen))
			return current_node->value;
		current_node = current_node->next;
	}

	return NULL;
} /* ohm_search() */

void *ohm_insert(ohm_t *hashmap, void *key, int keylen, void *value, int valuelen) {
	if(!key || keylen < 1 || !value || valuelen < 1) return NULL;

	int index = hashmap->hash(key, keylen, hashmap->size);
	if(index < 0) return NULL;

	ohm_node *parent_node = NULL, *current_node;
	current_node = hashmap->table[index];

	/* try and replace any existing key */
	while(current_node) {
		if(current_node->keylen == keylen)
			if(!memcmp(current_node->key, key, keylen)) {
				/* key found */
				if(current_node->valuelen != current_node->valuelen) {
					/* node value needs to change size */
					free(current_node->value);
					current_node->value = malloc(valuelen);
					if(!current_node->value)
						return NULL;
					current_node->valuelen = valuelen;
				}
				memcpy(current_node->value, value, valuelen);
				hashmap->count++;
				return current_node->value;
			}
		parent_node = current_node;
		current_node = current_node->next;
	}

	/* need to make a new key */
	current_node = malloc(sizeof(ohm_node));
	current_node->next = NULL;

	current_node->key = malloc(keylen);
	current_node->keylen = keylen;
	memcpy(current_node->key, key, keylen);

	current_node->value = malloc(valuelen);
	current_node->valuelen = valuelen;
	memcpy(current_node->value, value, valuelen);

	/* link to parent nodes or own bucket */
	if(parent_node)
		parent_node->next = current_node;
	else
		hashmap->table[index] = current_node;

	hashmap->count++;
	return current_node->value;
} /* ohm_insert() */

int ohm_remove(ohm_t *hashmap, void *key, int keylen) {
	if(!key || keylen < 1) return 1;

	int index = hashmap->hash(key, keylen, hashmap->size);
	if(index < 0) return 1;

	ohm_node *parent_node = NULL, *current_node;
	current_node = hashmap->table[index];

	/* try and replace any existing key */
	while(current_node) {
		if(current_node->keylen == keylen)
			if(!memcmp(current_node->key, key, keylen)) {
				/* key found */
				free(current_node->value);
				free(current_node->key);

				if(parent_node)
					parent_node->next = current_node->next;
				else
					hashmap->table[index] = current_node->next;

				free(current_node);
				hashmap->count--;
				return 0;
			}
		parent_node = current_node;
		current_node = current_node->next;
	}
	return 1;
} /* ohm_remove() */

ohm_t *ohm_resize(ohm_t *old_hm, int size) {
	if(!old_hm || size < 1) return NULL;

	ohm_t *new_hm = ohm_init(size, old_hm->hash);
	ohm_iter i = ohm_iter_init(old_hm);
	for(; i.key != NULL; ohm_iter_inc(&i))
		ohm_insert(new_hm, i.key, i.keylen, i.value, i.valuelen);

	ohm_free(old_hm);
	return new_hm;
} /* ohm_resize() */

ohm_iter ohm_iter_init(ohm_t *hashmap) {
	ohm_iter ret;

	ret.internal.hashmap = hashmap;
	ret.internal.node = NULL;
	ret.internal.index = -1;

	ohm_iter_inc(&ret);
	return ret;
} /* ohm_iter_init() */

/* depth-first node incrementor */
void ohm_iter_inc(ohm_iter *i) {
	ohm_t *hashmap = i->internal.hashmap;
	int index = i->internal.index;

	/* try to hop to next node down */
	if(i->internal.node && i->internal.node->next) {
		/* get next node down */
		i->internal.node = i->internal.node->next;
		i->key = i->internal.node->key;
		i->keylen = i->internal.node->keylen;
		i->value = i->internal.node->value;
		i->valuelen = i->internal.node->valuelen;
		return;
	}
	else index++;

	/* find next used bucket */
	while(index < hashmap->size && !hashmap->table[index])
		index++;

	if(index >= hashmap->size) {
		/* reached end of table */
		i->internal.node = NULL;
		i->internal.index = hashmap->size;

		i->key = NULL;
		i->keylen = 0;
		i->value = NULL;
		i->valuelen = 0;
		return;
	}

	/* update pointers to new index */
	i->internal.node = hashmap->table[index];
	i->internal.index = index;

	i->key = i->internal.node->key;
	i->keylen = i->internal.node->keylen;
	i->value = i->internal.node->value;
	i->valuelen = i->internal.node->valuelen;
} /* ohm_iter_inc() */

/* the djb2 hashing algorithm by Dan Bernstein */
int ohm_hash(void *key, int keylen, int size) {
	if(!key || keylen < 1 || size < 1) return -1;

	unsigned int hash = 5381;
	char c, *k = (char *) key;

	while((c = *k++))
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

	return hash % size; /* Probably a more Poisson range method exists, oh well. */
} /* ohm_hash() */
