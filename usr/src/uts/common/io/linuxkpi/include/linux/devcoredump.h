/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's device core dumps (amdgpu's GPU hang dumps in sysfs). Not
 * supported: the dump is dropped at once through the caller's free
 * function, as Linux does when devcoredump is disabled.
 */
#ifndef _LINUX_DEVCOREDUMP_H
#define	_LINUX_DEVCOREDUMP_H
#include <linux/types.h>
#include <linux/device.h>
#include <linux/module.h>

static inline void
dev_coredumpm(struct device *dev, struct module *owner, void *data,
    size_t datalen, gfp_t gfp,
    ssize_t (*read)(char *buffer, loff_t offset, size_t count, void *data,
    size_t datalen),
    void (*free)(void *data))
{
	free(data);
}
#endif
