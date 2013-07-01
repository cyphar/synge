/* ohmic: a fairly reliable hashmap library
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

#ifndef __OHMIC_H__
#define __OHMIC_H__

/* opaque structures */
typedef struct ohm_t ohm_t;
typedef struct ohm_iter ohm_iter;

/* basic hashmap functionality */
ohm_t *ohm_init(int, int (*)(void *, size_t));
void ohm_free(ohm_t *);

void *ohm_search(ohm_t *, void *, size_t);

void *ohm_insert(ohm_t *, void *, size_t, void *, size_t);
int ohm_remove(ohm_t *, void *, size_t);

ohm_t *ohm_resize(ohm_t *, int);

/* functions to iterate (correctly) through the hashmap */
ohm_iter ohm_iter_init(ohm_t *);
void ohm_iter_inc(ohm_iter *);

/* functions to copy, duplicate and merge hashmaps */
ohm_t *ohm_dup(ohm_t *);
void ohm_cpy(ohm_t *, ohm_t *);

void ohm_merge(ohm_t *, ohm_t *);

/* functions to get items from opaque structures */
int ohm_count(ohm_t *hm);
int ohm_size(ohm_t *hm);

/* default hashing function (modulo of djb2 hash -- not reccomended) */
int ohm_hash(void *, size_t);

#endif /* __OHMIC_H__ */
