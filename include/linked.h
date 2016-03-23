/* Linked: A light-weight linked list library
 * Copyright (c) 2013 Cyphar
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef __LINKED_H__
#define __LINKED_H__

/* link structures */
struct link_node {
	void *content;
	size_t contentlen;

	struct link_node *prev;
	struct link_node *next;
};

struct link_t {
	struct link_node *chain;
	int length;
};

struct link_iter {
	void *content;

	struct link_iter_internal {
		struct link_t *link;
		struct link_node *node;
	} internal;
};

struct link_t *link_init(void);
void link_free(struct link_t *);

int link_insert(struct link_t *, int, void *, size_t);
int link_set(struct link_t *, int, void *, size_t);

/* macros for appending and prepending content */
#define link_append(link, content, len) link_insert(link, link->length - 1, content, len)
#define link_prepend(link, content, len) link_insert(link, 0, content, len)

void *link_get(struct link_t *, int);
void *link_pop(struct link_t *, int);

/* macros for getting first and last items */
#define link_top(link) link_get(link, 0)
#define link_end(link) link_get(link, link->length - 1)

/* macros for popping first and last items */
#define link_ptop(link) link_pop(link, 0)
#define link_pend(link) link_pop(link, link->length - 1)

int link_shorten(struct link_t *, int);
int link_truncate(struct link_t *, int);

struct link_iter *link_iter_init(struct link_t *);
int link_iter_next(struct link_iter *);
void link_iter_free(struct link_iter *);
#endif
