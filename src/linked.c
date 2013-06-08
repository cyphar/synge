/* Linked: A light-weight linked list library
 * Copyright (C) 2013 Cyphar
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

#include <stdio.h>

#include <stdlib.h>
#include <string.h>

#include "linked.h"

static void *pop_container = NULL;

link_t *link_init(void) {
	link_t *new = malloc(sizeof(link_t));

	new->chain = malloc(sizeof(link_node));
	new->length = 1;

	new->chain->content = NULL;
	new->chain->contentlen = 0;

	new->chain->prev = NULL;
	new->chain->next = NULL;

	return new;
}

void link_free(link_t *link) {
	if(!link)
		return;

	link_node *parent = NULL, *current = link->chain;

	/* go to last link */
	while(current && current->next)
		current = current->next;

	/* go backwards through links and free them */
	while(current) {
		parent = current->prev;

		current->contentlen = 0;
		free(current->content);
		free(current);

		current = parent;

		if(current)
			current->next = NULL;
	}

	/* free topmost link */
	//free(link->chain);

	link->chain = NULL;
	link->length = 0;

	free(link);

	free(pop_container);
	pop_container = NULL;
}

int link_insert(link_t *link, int pos, void *content, int contentlen) {
	if(pos >= link->length || pos < 0 || !content || contentlen < 1)
		return 1;

	int position = pos;
	link_node *current = link->chain, *next = link->chain->next;

	while(position && current) {
		current = current->next;
		position--;
	}

	if(position || !current)
		return 1;

	next = current->next;
	current->next = malloc(sizeof(link_node));

	current->next->content = malloc(contentlen);
	memcpy(current->next->content, content, contentlen);

	current->next->contentlen = contentlen;

	current->next->prev = current;
	current->next->next = next;

	link->length++;
	return 0;
}

int link_remove(link_t *link, int pos) {
	if(pos >= link->length || pos < 0)
		return 1;

	int position = pos;
	link_node *current = link->chain, *prev = NULL, *next = link->chain->next;

	while(position && current) {
		current = current->next;
		position--;
	}

	if(position || !current)
		return 1;

	prev = current->prev;
	next = current->next;

	free(current->content);
	free(current);

	if(next)
		next->prev = prev;

	if(prev)
		prev->next = next;

	link->length--;
	return 0;
}

void *link_get(link_t *link, int pos) {
	if(pos >= link->length || pos < 0)
		return NULL;

	int position = pos;
	link_node *current = link->chain;

	while(position && current) {
		current = current->next;
		position--;
	}

	if(position || !current)
		return NULL;

	return current->content;
}

void *link_pop(link_t *link, int pos) {
	if(pos >= link->length || pos < 0)
		return NULL;

	int position = pos;
	link_node *current = link->chain;

	while(position && current) {
		current = current->next;
		position--;
	}

	if(position || !current)
		return NULL;

	pop_container = realloc(pop_container, current->contentlen);
	memcpy(pop_container, current->content, current->contentlen);

	link_remove(link, pos);
	return pop_container;
}

int link_truncate(link_t *link, int pos) {
	if(pos >= link->length || pos < 0)
		return 1;

	int position = pos;
	while(!link_remove(link, position));

	if(link->length >= position)
		return 1;

	return 0;
}

int link_shorten(link_t *link, int num) {
	if(num >= link->length || num < 0)
		return 1;

	int left = num;
	link_node *current = link->chain, *parent = link->chain->prev;

	while(current && current->next)
		current = current->next;

	while(left && current) {
		parent = current->prev;

		free(current->content);
		free(current);

		left--;
		current = parent;
		current->next = NULL;
	}

	if(left)
		return 1;
	return 0;
}

link_iter *link_iter_init(link_t *link) {
	if(!link)
		return NULL;

	link_iter *iter = malloc(sizeof(link_iter));

	iter->content = NULL;

	iter->internal.link = link;
	iter->internal.node = NULL;

	link_iter_next(iter);
	return iter;
}

int link_iter_next(link_iter *iter) {
	if(!iter)
		return 1;

	if(!iter->internal.node)
		iter->internal.node = iter->internal.link->chain;
	else
		iter->internal.node = iter->internal.node->next;

	if(!iter->internal.node)
		return 1;

	iter->content = iter->internal.node->content;
	return 0;
}
