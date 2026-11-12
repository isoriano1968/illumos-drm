/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's list_lru: a locked list of items with a count, walked by an
 * isolate callback (TTM's page pools keep their free pages on one). One
 * list for all nodes and no memory cgroups: the nid and memcg arguments
 * are ignored. The callback runs under the list's lock, as in Linux; it
 * returns LRU_REMOVED after list_lru_isolate()/_isolate_move(), or
 * LRU_SKIP/LRU_ROTATE to keep the item (LRU_RETRY is not supported: the
 * lock is never dropped).
 */
#ifndef _LINUX_LIST_LRU_H
#define	_LINUX_LIST_LRU_H
#include <linux/list.h>
#include <linux/spinlock.h>

struct mem_cgroup;

struct list_lru_one {
	struct list_head list;
	long nr_items;
};

struct list_lru {
	spinlock_t lock;
	struct list_head list;
	long nr_items;
};

enum lru_status {
	LRU_REMOVED,
	LRU_REMOVED_RETRY,
	LRU_ROTATE,
	LRU_SKIP,
	LRU_RETRY,
	LRU_STOP,
};

typedef enum lru_status (*list_lru_walk_cb)(struct list_head *item,
    struct list_lru_one *list, void *cb_arg);

static inline int
list_lru_init(struct list_lru *lru)
{
	spin_lock_init(&lru->lock);
	INIT_LIST_HEAD(&lru->list);
	lru->nr_items = 0;
	return (0);
}

static inline void
list_lru_destroy(struct list_lru *lru)
{
}

static inline bool
list_lru_add(struct list_lru *lru, struct list_head *item, int nid,
    struct mem_cgroup *memcg)
{
	bool added = false;

	spin_lock(&lru->lock);
	if (list_empty(item)) {
		list_add_tail(item, &lru->list);
		lru->nr_items++;
		added = true;
	}
	spin_unlock(&lru->lock);
	return (added);
}

static inline bool
list_lru_del(struct list_lru *lru, struct list_head *item, int nid,
    struct mem_cgroup *memcg)
{
	bool deleted = false;

	spin_lock(&lru->lock);
	if (!list_empty(item)) {
		list_del_init(item);
		lru->nr_items--;
		deleted = true;
	}
	spin_unlock(&lru->lock);
	return (deleted);
}

static inline unsigned long
list_lru_count(struct list_lru *lru)
{
	return ((unsigned long)READ_ONCE(lru->nr_items));
}
#define	list_lru_count_node(lru, nid)	list_lru_count(lru)

/*
 * The walk's struct list_lru_one aliases the list_lru's list and count
 * (its first members after the lock), so the isolate helpers update the
 * list_lru itself.
 */
static inline void
list_lru_isolate(struct list_lru_one *list, struct list_head *item)
{
	list_del_init(item);
	list->nr_items--;
}

static inline void
list_lru_isolate_move(struct list_lru_one *list, struct list_head *item,
    struct list_head *head)
{
	list_move(item, head);
	list->nr_items--;
}

static inline unsigned long
list_lru_walk_node(struct list_lru *lru, int nid, list_lru_walk_cb isolate,
    void *cb_arg, unsigned long *nr_to_walk)
{
	struct list_lru_one *one = (struct list_lru_one *)(void *)&lru->list;
	struct list_head *item, *n;
	unsigned long isolated = 0;

	spin_lock(&lru->lock);
	list_for_each_safe(item, n, &lru->list) {
		if (*nr_to_walk == 0)
			break;
		--*nr_to_walk;
		switch (isolate(item, one, cb_arg)) {
		case LRU_REMOVED:
		case LRU_REMOVED_RETRY:
			isolated++;
			break;
		case LRU_ROTATE:
			list_move_tail(item, &lru->list);
			break;
		case LRU_STOP:
			goto out;
		default:
			break;
		}
	}
out:
	spin_unlock(&lru->lock);
	return (isolated);
}

static inline unsigned long
list_lru_walk(struct list_lru *lru, list_lru_walk_cb isolate, void *cb_arg,
    unsigned long nr_to_walk)
{
	return (list_lru_walk_node(lru, 0, isolate, cb_arg, &nr_to_walk));
}
_Static_assert(offsetof(struct list_lru, nr_items) -
    offsetof(struct list_lru, list) == offsetof(struct list_lru_one, nr_items),
    "list_lru_one aliases list_lru's list and count");
#endif
