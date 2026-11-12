/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * User-mapping windows (illumos only, no Linux counterpart): a range of
 * kernel heap VA with a ddi_umem cookie on it (gfx_private's
 * gfxp_umem_cookie_init()), for drivers whose objects' pages move (TTM).
 * The DRM core maps an object's umem cookie; for a cookie on kernel VA
 * the devmap framework loads, at each fault, the frame that the kernel
 * VA maps at that moment. A driver gives its object one window, returns
 * the window's cookie from illumos_umem, loads the window with the
 * object's current pages before a fault is served, and unloads it (after
 * unloading the user mappings) when the pages go. User mappings of the
 * window are write-back (the core's attributes), so the window is
 * loaded write-back too: one cache type per frame.
 *
 * lkpi_umem_window_create	reserves `size` bytes (page multiple) of
 *				VA; NULL if the size is bad or VA is short.
 * lkpi_umem_window_cookie	its ddi_umem cookie (always the same).
 * lkpi_umem_window_load	maps `count` pages (size / PAGE_SIZE of
 *				them) write-back, replacing what was
 *				loaded; 0, -EINVAL, or -EFAULT (a page
 *				without a frame: nothing is loaded).
 * lkpi_umem_window_loaded	whether pages are loaded.
 * lkpi_umem_window_unload	unloads the pages (no-op if none).
 * lkpi_umem_window_destroy	unloads, frees the cookie and the VA.
 *
 * The caller serializes calls on one window (TTM: the object's
 * reservation). Thread context.
 */
#ifndef _LKPI_UMEM_WINDOW_H
#define	_LKPI_UMEM_WINDOW_H

#include <linux/types.h>
#include <linux/mm_types.h>
#include <sys/sunddi.h>

struct lkpi_umem_window;

extern struct lkpi_umem_window *lkpi_umem_window_create(size_t size);
extern ddi_umem_cookie_t lkpi_umem_window_cookie(
    const struct lkpi_umem_window *w);
extern int lkpi_umem_window_load(struct lkpi_umem_window *w,
    struct page **pages, unsigned long count);
extern bool lkpi_umem_window_loaded(const struct lkpi_umem_window *w);
extern void lkpi_umem_window_unload(struct lkpi_umem_window *w);
extern void lkpi_umem_window_destroy(struct lkpi_umem_window *w);

#endif
