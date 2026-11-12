/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's kobjects, reduced. illumos has no sysfs: a kobject is a name,
 * a parent and a reference count so that structures embedding one keep
 * their layout; kobject_create_and_add() returns an object that sysfs
 * functions accept and ignore, kobject_uevent*() do nothing (return 0).
 * Drivers' sysfs files therefore do not exist on illumos (see
 * <linux/sysfs.h>). An added kobject is also on its parent's list of
 * children (lkpi_children, lkpi_sibling; src/lkpi_kobject.c), so that the
 * layer can detach the children left under a kobject whose memory it
 * frees (lkpi_kobject_orphan_children(), a layer device's kobject).
 *
 * A kset is a kobject with a list of member kobjects, as on Linux: a
 * kobject whose kset is set when it is added joins the kset's list
 * (entry) and, without a parent of its own, gets the kset's kobject as
 * parent; it leaves the list when it is deleted. kset_register() adds
 * the kset's kobject (its preset name, parent and kset), kset_unregister()
 * deletes and puts it. sd (the sysfs node) is always NULL, so drivers
 * that create sysfs files only when their device has one skip them.
 */
#ifndef _LINUX_KOBJECT_H
#define	_LINUX_KOBJECT_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/kref.h>
#include <linux/sysfs.h>
#include <linux/spinlock.h>

struct kobject;
struct kset;
struct kernfs_node;
struct kobj_type {
	void (*release)(struct kobject *kobj);
	const struct sysfs_ops *sysfs_ops;
	const struct attribute_group **default_groups;
};

struct kobject {
	const char *name;
	struct kobject *parent;
	const struct kobj_type *ktype;
	struct kref kref;
	unsigned int state_initialized:1;
	struct list_head entry;		/* on kset->list (kset's list_lock) */
	struct kset *kset;
	struct kernfs_node *sd;		/* always NULL: no sysfs */
	/* illumos: under the layer's kobject lock (src/lkpi_kobject.c). */
	struct list_head lkpi_children;	/* the kobjects added under this */
	struct list_head lkpi_sibling;	/* on the parent's lkpi_children */
};

struct kobj_attribute {
	struct attribute attr;
	ssize_t (*show)(struct kobject *kobj, struct kobj_attribute *attr,
	    char *buf);
	ssize_t (*store)(struct kobject *kobj, struct kobj_attribute *attr,
	    const char *buf, size_t count);
};

enum kobject_action {
	KOBJ_ADD,
	KOBJ_REMOVE,
	KOBJ_CHANGE,
	KOBJ_MOVE,
	KOBJ_ONLINE,
	KOBJ_OFFLINE,
	KOBJ_BIND,
	KOBJ_UNBIND
};

struct kset_uevent_ops;
struct kset {
	struct list_head list;
	spinlock_t list_lock;
	struct kobject kobj;
	const struct kset_uevent_ops *uevent_ops;
};
#define	to_kset(k)	container_of((k), struct kset, kobj)

extern const struct sysfs_ops kobj_sysfs_ops;
extern int kobject_set_name(struct kobject *kobj, const char *fmt, ...)
    __printf(2, 3);
extern int kset_register(struct kset *kset);
extern void kset_unregister(struct kset *kset);
extern void kobject_init(struct kobject *kobj, const struct kobj_type *ktype);
extern int kobject_add(struct kobject *kobj, struct kobject *parent,
    const char *fmt, ...);
extern int kobject_init_and_add(struct kobject *kobj,
    const struct kobj_type *ktype, struct kobject *parent,
    const char *fmt, ...);
extern struct kobject *kobject_create_and_add(const char *name,
    struct kobject *parent);
extern struct kobject *kobject_get(struct kobject *kobj);
extern void kobject_put(struct kobject *kobj);
extern void kobject_del(struct kobject *kobj);
/*
 * illumos: unlinks every child of kobj (child->parent becomes NULL) and
 * drops the reference each child held on kobj; for the owner of kobj,
 * who holds a reference of its own, before it frees kobj's memory.
 * Returns the number of children.
 */
extern int lkpi_kobject_orphan_children(struct kobject *kobj);
#define	kobject_uevent(kobj, action)		((void)(kobj), 0)
#define	kobject_uevent_env(kobj, action, envp)	((void)(kobj), (void)(envp), 0)
#define	kobject_name(kobj)			((kobj)->name)

#endif
