/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's iteration over the fences inside fence chains and arrays (an
 * implementation of the interface). The layer sees no chains
 * (to_dma_fence_chain() is NULL, see <linux/dma-fence-chain.h>), so a
 * walk visits the fence itself or, for an array, its members.
 * dma_fence_unwrap_merge() is the layer's (src/lkpi_fence.c): the
 * unsignalled fences inside the given ones, one per context (the latest),
 * as one fence: the fence itself if one is left, a new fence array of
 * them (a new context) if several, or a signalled stub with the latest
 * timestamp of the signalled ones if none; NULL without memory.
 */
#ifndef _LINUX_DMA_FENCE_UNWRAP_H
#define	_LINUX_DMA_FENCE_UNWRAP_H
#include <linux/dma-fence.h>
#include <linux/dma-fence-chain.h>
#include <linux/dma-fence-array.h>

/*
 * Sorts fences[0..num) by context and keeps the latest fence of each
 * context, putting the others; returns the new count.
 */
extern int dma_fence_dedup_array(struct dma_fence **fences, int num_fences);

struct dma_fence_unwrap {
	struct dma_fence *chain;
	struct dma_fence *array;
	unsigned int index;
};

static inline struct dma_fence *
lkpi_dma_fence_unwrap_array(struct dma_fence_unwrap *c)
{
	c->array = dma_fence_chain_contained(c->chain);
	c->index = 0;
	return (dma_fence_array_first(c->array));
}

static inline struct dma_fence *
dma_fence_unwrap_first(struct dma_fence *head, struct dma_fence_unwrap *c)
{
	c->chain = dma_fence_get(head);
	return (lkpi_dma_fence_unwrap_array(c));
}

static inline struct dma_fence *
dma_fence_unwrap_next(struct dma_fence_unwrap *c)
{
	struct dma_fence *tmp;

	++c->index;
	tmp = dma_fence_array_next(c->array, c->index);
	if (tmp != NULL)
		return (tmp);
	c->chain = dma_fence_chain_walk(c->chain);
	return (c->chain != NULL ? lkpi_dma_fence_unwrap_array(c) : NULL);
}

#define	dma_fence_unwrap_for_each(fence, cursor, head) \
	for ((fence) = dma_fence_unwrap_first((head), (cursor)); (fence) != NULL; \
	    (fence) = dma_fence_unwrap_next(cursor))

extern struct dma_fence *__dma_fence_unwrap_merge(unsigned int num_fences,
    struct dma_fence **fences, struct dma_fence_unwrap *cursors);
#define	dma_fence_unwrap_merge(...) ({ \
	struct dma_fence *__f[] = { __VA_ARGS__ }; \
	struct dma_fence_unwrap __c[ARRAY_SIZE(__f)]; \
	__dma_fence_unwrap_merge(ARRAY_SIZE(__f), __f, __c); })
#endif
