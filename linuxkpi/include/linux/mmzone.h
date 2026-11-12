/* SPDX-License-Identifier: MIT */
/* Copyright 2026 The illumos-drm authors. Linux's memory zones: illumos has none; MAX_PAGE_ORDER is Linux's. */
#ifndef _LINUX_MMZONE_H
#define	_LINUX_MMZONE_H
#define	MAX_PAGE_ORDER		10
#define	MAX_ORDER		MAX_PAGE_ORDER
#define	NR_PAGE_ORDERS		(MAX_PAGE_ORDER + 1)
#define	PAGE_ALLOC_COSTLY_ORDER	3
/* One memory node: the layer does not expose illumos lgroups as nodes. */
#define	MAX_NUMNODES	1
#define	nr_node_ids	1
#define	first_online_node	0
#define	num_online_nodes()	1
#define	num_possible_nodes()	1
#define	for_each_node(n)	for ((n) = 0; (n) < 1; (n)++)
#define	for_each_online_node(n)	for_each_node(n)
#define	pfn_to_nid(pfn)		((void)(pfn), 0)

/*
 * The node's memory as one zone of all of physmem (TTM sizes its page
 * pools per node from the zones' managed pages, as si_meminfo_node()).
 */
#define	MAX_NR_ZONES		1
/* Not Linux's struct zone: illumos has a struct zone (its zones). */
struct lkpi_mm_zone {
	int lkpi_zone;		/* layout only */
};
typedef struct pglist_data {
	struct lkpi_mm_zone node_zones[MAX_NR_ZONES];
} pg_data_t;
extern pg_data_t lkpi_node_data;
#define	NODE_DATA(nid)		((void)(nid), &lkpi_node_data)
#define	zone_managed_pages(z)	((void)(z), (unsigned long)physmem_lkpi())
extern unsigned long physmem_lkpi(void);

#endif
