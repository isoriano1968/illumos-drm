/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * One DMA binding for an array of pages (illumos only, no Linux
 * counterpart), for TTM's page arrays. Linux's TTM maps each page (or
 * each higher-order chunk) with dma_map_page(); on illumos every such
 * mapping is a DDI DMA handle of its own, and a rootnex handle is
 * 2600 bytes (preallocated cookies, windows and copy-buffer state), so
 * a handle per 4 KiB page would cost two thirds of the memory it maps.
 * lkpi_dma_map_pages() instead maps the pages into one range of kernel
 * heap (vmap(), write-back: the pages' own cache type), binds that range
 * with one handle and gives every page its bus address from the
 * cookies; lkpi_dma_unmap_pages() undoes both.
 *
 * lkpi_dma_map_pages	maps `count` layer pages for `dev`, both
 *			directions, and fills addrs[0 .. count); 0,
 *			-EINVAL (no pages, too many, no DMA attributes),
 *			-ENOMEM (no kernel VA or DMA resources) or -EIO
 *			(a page outside the device's address range: no
 *			copy buffer is used, as for every streaming
 *			mapping of the layer). May sleep. *out is the
 *			mapping, kept until unmapped.
 * lkpi_dma_unmap_pages	unbinds and unmaps; NULL is a no-op.
 *
 * Mappings the driver left are released by the layer at its fini, with
 * a warning, as for the other DMA mappings. No sync is needed (x86 DMA
 * is cache-coherent and no copy buffer is used).
 */
#ifndef _LKPI_DMA_PAGES_H
#define	_LKPI_DMA_PAGES_H

#include <linux/types.h>
#include <linux/mm_types.h>

struct device;
struct lkpi_dma_pages;

extern int lkpi_dma_map_pages(struct device *dev, struct page **pages,
    unsigned long count, dma_addr_t *addrs, struct lkpi_dma_pages **out);
extern void lkpi_dma_unmap_pages(struct lkpi_dma_pages *dp);
/* The kernel mapping (vmap()) of the pages, valid until the unmap. */
extern void *lkpi_dma_pages_kva(const struct lkpi_dma_pages *dp);

#endif
