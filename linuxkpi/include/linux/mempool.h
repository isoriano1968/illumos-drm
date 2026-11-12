/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's memory pools, for the kmalloc pools amdgpu's RAS manager
 * creates: an implementation of the interface that keeps min_nr
 * preallocated elements on a free list and falls back to kmalloc().
 * mempool_alloc_preallocated() takes only from the reserve.
 */
#ifndef _LINUX_MEMPOOL_H
#define	_LINUX_MEMPOOL_H
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

typedef struct mempool {
	spinlock_t lock;
	size_t size;
	int min_nr, curr_nr;
	void **elements;
} mempool_t;

static inline mempool_t *
mempool_create_kmalloc_pool(int min_nr, size_t size)
{
	mempool_t *p = kzalloc(sizeof (*p), GFP_KERNEL);
	int i;

	if (p == NULL)
		return (NULL);
	p->elements = kcalloc(min_nr ? min_nr : 1, sizeof (void *), GFP_KERNEL);
	if (p->elements == NULL) {
		kfree(p);
		return (NULL);
	}
	spin_lock_init(&p->lock);
	p->size = size;
	p->min_nr = min_nr;
	for (i = 0; i < min_nr; i++) {
		if ((p->elements[i] = kmalloc(size, GFP_KERNEL)) == NULL)
			break;
		p->curr_nr++;
	}
	return (p);
}

static inline void
mempool_destroy(mempool_t *p)
{
	if (p == NULL)
		return;
	while (p->curr_nr > 0)
		kfree(p->elements[--p->curr_nr]);
	kfree(p->elements);
	kfree(p);
}

static inline void *
mempool_alloc_preallocated(mempool_t *p)
{
	void *e = NULL;
	unsigned long fl;

	spin_lock_irqsave(&p->lock, fl);
	if (p->curr_nr > 0)
		e = p->elements[--p->curr_nr];
	spin_unlock_irqrestore(&p->lock, fl);
	return (e);
}

static inline void *
mempool_alloc(mempool_t *p, gfp_t gfp)
{
	void *e = kmalloc(p->size, gfp);

	return (e != NULL ? e : mempool_alloc_preallocated(p));
}

static inline void
mempool_free(void *e, mempool_t *p)
{
	unsigned long fl;

	if (e == NULL)
		return;
	spin_lock_irqsave(&p->lock, fl);
	if (p->curr_nr < p->min_nr) {
		p->elements[p->curr_nr++] = e;
		e = NULL;
	}
	spin_unlock_irqrestore(&p->lock, fl);
	kfree(e);
}
#endif
