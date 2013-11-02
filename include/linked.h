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
