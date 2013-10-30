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

typedef struct link_node {
	void *content;
	size_t contentlen;

	struct link_node *prev;
	struct link_node *next;
} link_node;

struct link_t {
	link_node *chain;
	int length;
};

struct link_iter {
	void *content;

	struct link_iter_internal {
		struct link_t *link;
		link_node *node;
	} internal;
};

static void *pop_container = NULL;

struct link_t *link_init(void) {
	/* allocate new link */
	struct link_t *new = malloc(sizeof(struct link_t));

	/* allocate first link and set length */
	new->chain = malloc(sizeof(link_node));
	new->length = 1;

	/* initialise everything else to zero */
	new->chain->content = NULL;
	new->chain->contentlen = 0;

	new->chain->prev = NULL;
	new->chain->next = NULL;

	return new;
} /* link_init() */

void link_free(struct link_t *link) {
	if(!link) return;

	/* initialise parent and current links */
	link_node *parent = NULL, *current = link->chain;

	/* go to last link */
	while(current && current->next)
		current = current->next;

	/* go backwards through links and free them */
	while(current) {
		/* save parent */
		parent = current->prev;

		/* free the current link */
		free(current->content);
		free(current);

		current = parent;

		/* ensure no remaining links to freed link exist */
		if(current)
			current->next = NULL;
	}

	/* final setting to zero */
	link->chain = NULL;
	link->length = 0;

	free(link);

	/* freeing the pop_container (to keep valgrind happy) */
	free(pop_container);
	pop_container = NULL;
} /* link_free() */

link_node *link_node_get(struct link_t *link, int index) {
	if(!link || index >= link->length || index < 0)
		return NULL;

	/* initialise current and next links */
	link_node *current = link->chain;

	/* find link to append new link to */
	while(index && current) {
		/* get next link */
		current = current->next;
		index--;
	}

	/* index not found or link length is inaccurate */
	if(index)
		return NULL;

	return current;
} /* link_node_get() */

int link_insert(struct link_t *link, int pos, void *content, size_t contentlen) {
	if(pos >= link->length || pos < 0 || !content || contentlen < 1)
		return 1;

	/* find current link */
	link_node *current = link_node_get(link, pos), *next = NULL;

	/* allocate new link information */
	next = current->next;
	current->next = malloc(sizeof(link_node));

	/* allocate and copy content */
	current->next->content = malloc(contentlen);
	memcpy(current->next->content, content, contentlen);
	current->next->contentlen = contentlen;

	/* link up orphan and parent links */
	current->next->prev = current;
	current->next->next = next;

	/* update length */
	link->length++;
	return 0;
} /* link_insert() */

int link_remove(struct link_t *link, int pos) {
	if(!link || pos >= link->length || pos < 0)
		return 1;

	/* find current link */
	link_node *current = link_node_get(link, pos), *next = NULL, *prev = NULL;

	/* set previous and next links */
	prev = current->prev;
	next = current->next;

	/* free the selected link */
	free(current->content);
	free(current);

	/* link up orphan links, if they exist */
	if(next)
		next->prev = prev;
	if(prev)
		prev->next = next;

	/* update length */
	link->length--;
	return 0;
} /* link_remove() */

void *link_get(struct link_t *link, int pos) {
	if(!link || pos >= link->length || pos < 0)
		return NULL;

	/* find current link */
	link_node *current = link_node_get(link, pos);

	/* return the found content */
	return current->content;
} /* link_get() */

void *link_pop(struct link_t *link, int pos) {
	if(!link || pos >= link->length || pos < 0)
		return NULL;

	/* find current link */
	link_node *current = link_node_get(link, pos);

	/* copy the link's content to a temporary variable */
	pop_container = realloc(pop_container, current->contentlen);
	memcpy(pop_container, current->content, current->contentlen);

	/* delete the link and return its content */
	link_remove(link, pos);
	return pop_container;
} /* link_pop() */

int link_truncate(struct link_t *link, int pos) {
	if(!link || pos >= link->length || pos < 0)
		return 1;

	/* delete everything after the position */
	int position = pos + 1;
	while(!link_remove(link, position));

	/* not all of the links where deleted */
	if(link->length >= position)
		return 1;

	return 0;
} /* struct link_truncate() */

int link_shorten(struct link_t *link, int num) {
	if(!link || num >= link->length || num < 0)
		return 1;

	/* initialise the current and parent links */
	int left = num;
	link_node *current = link->chain, *parent = link->chain->prev;

	/* find the last link */
	while(current && current->next)
		current = current->next;

	/* go backwards and delete the given number of links */
	while(left && current) {
		/* save parent */
		parent = current->prev;

		/* free current link */
		free(current->content);
		free(current);

		/* update iterator and length */
		left--;
		link->length--;

		/* choose parent link and cut off freed link */
		current = parent;
		current->next = NULL;
	}

	/* not all links deleted */
	if(left)
		return 1;
	return 0;
} /* link_shorten() */

int link_length(struct link_t *link) {
	return link->length;
} /* link_length() */

struct link_iter *link_iter_init(struct link_t *link) {
	if(!link)
		return NULL;

	/* allocate iterator */
	struct link_iter *iter = malloc(sizeof(struct link_iter));

	/* set the iterator to first item */
	iter->content = link->chain->content;
	iter->internal.link = link;
	iter->internal.node = link->chain;

	return iter;
} /* link_iter_init() */

int link_iter_next(struct link_iter *iter) {
	if(!iter)
		return 1;

	if(iter->internal.node && iter->internal.node->next)
		/* select next link */
		iter->internal.node = iter->internal.node->next;
	else
		/* no links left */
		return 1;

	/* updated link */
	iter->content = iter->internal.node->content;
	return 0;
} /* link_iter_next() */

void link_iter_free(struct link_iter *iter) {
	/* wrapper to free iterator */
	free(iter);
} /* link_iter_free() */

void *link_iter_content(struct link_iter *iter) {
	return iter->content;
} /* link_iter_content() */
