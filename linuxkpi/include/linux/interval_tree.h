/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's generic interval tree of unsigned long intervals (lib/
 * interval_tree.c, GPL-2.0; here generated with the layer's
 * INTERVAL_TREE_DEFINE() as static inline functions).
 */
#ifndef _LINUX_INTERVAL_TREE_H
#define	_LINUX_INTERVAL_TREE_H
#include <linux/rbtree.h>
#include <linux/interval_tree_generic.h>

struct interval_tree_node {
	struct rb_node rb;
	unsigned long start;
	unsigned long last;
	unsigned long __subtree_last;
};

#define	LKPI_IT_START(n)	((n)->start)
#define	LKPI_IT_LAST(n)		((n)->last)
INTERVAL_TREE_DEFINE(struct interval_tree_node, rb, unsigned long,
    __subtree_last, LKPI_IT_START, LKPI_IT_LAST, static inline, interval_tree)
#endif
