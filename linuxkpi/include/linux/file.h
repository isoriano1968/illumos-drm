/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's file-descriptor table operations. Not supported: descriptors
 * belong to the DRM core (illumos vnodes), which installs dma-buf and
 * sync-file descriptors itself (dma_buf_fd(), drm_sync_file_install()).
 * get_unused_fd_flags() fails with -ENOSYS, fd_install() and
 * put_unused_fd() do nothing, fget() returns NULL.
 */
#ifndef _LINUX_FILE_H
#define	_LINUX_FILE_H
#include <linux/fs.h>
#define	get_unused_fd_flags(flags)	(-ENOSYS)
#define	put_unused_fd(fd)		((void)(fd))
#define	fd_install(fd, f)		((void)(fd), (void)(f))
#define	fget(fd)			((struct lx_file *)NULL)
#define	O_CLOEXEC_LKPI			0x800000

/*
 * A looked-up descriptor (CLASS(fd, f)(fd)): always empty, as fget(), so
 * a driver taking another process's DRM file by descriptor reports a bad
 * descriptor.
 */
#include <linux/cleanup.h>
struct fd {
	struct lx_file *file;
};
#define	fd_file(f)	((f).file)
#define	fd_empty(f)	((f).file == NULL)
static inline struct fd
fdget(unsigned int fd)
{
	struct fd f = { .file = fget(fd) };

	return (f);
}
#define	fdput(f)	((void)(f))
DEFINE_CLASS(fd, struct fd, fdput(_T), fdget(fd), int fd)
#endif
