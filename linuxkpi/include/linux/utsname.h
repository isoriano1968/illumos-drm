/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's system name (amdgpu writes the kernel release into its hang
 * dumps): the illumos release and version (utsname of <sys/utsname.h>).
 */
#ifndef _LINUX_UTSNAME_H
#define	_LINUX_UTSNAME_H
struct lkpi_new_utsname {
	const char *sysname, *release, *version, *machine;
};
extern struct lkpi_new_utsname *lkpi_init_utsname(void);
#define	init_utsname()	lkpi_init_utsname()
#define	utsname()	lkpi_init_utsname()
#endif
