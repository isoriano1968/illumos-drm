/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * TTM's backup store (in place of Linux's ttm_backup.c, which writes the
 * pages of buffer objects that a shrinker takes back to a shmem file and
 * reads them back). Not provided on illumos yet: no backup space is
 * available (ttm_backup_bytes_avail() is 0), no backup file can be made
 * (-EOPNOTSUPP), so TTM never backs a page up and the functions that
 * handle backed-up pages are not reached. Pages of TTM objects stay in
 * memory until the objects are freed.
 */
#include <linux/err.h>
#include <linux/errno.h>
#include <drm/ttm/ttm_backup.h>

void
ttm_backup_drop(struct file *backup, pgoff_t handle)
{
	(void) backup;
	(void) handle;
}

int
ttm_backup_copy_page(struct file *backup, struct page *dst, pgoff_t handle,
    bool intr, gfp_t additional_gfp)
{
	(void) backup;
	(void) dst;
	(void) handle;
	(void) intr;
	(void) additional_gfp;
	return (-EOPNOTSUPP);
}

s64
ttm_backup_backup_folio(struct file *backup, struct folio *folio,
    unsigned int order, bool writeback, pgoff_t idx, gfp_t folio_gfp,
    gfp_t alloc_gfp, pgoff_t *nr_pages_backed)
{
	(void) backup;
	(void) folio;
	(void) order;
	(void) writeback;
	(void) idx;
	(void) folio_gfp;
	(void) alloc_gfp;
	if (nr_pages_backed != NULL)
		*nr_pages_backed = 0;
	return (-EOPNOTSUPP);
}

void
ttm_backup_fini(struct file *backup)
{
	(void) backup;
}

u64
ttm_backup_bytes_avail(void)
{
	return (0);
}

struct file *
ttm_backup_shmem_create(loff_t size)
{
	(void) size;
	return (ERR_PTR(-EOPNOTSUPP));
}
