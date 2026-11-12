/*
 * SPDX-License-Identifier: MIT
 * Copyright 2026 The illumos-drm authors
 *
 * Linux's EFI runtime interface. Drivers see no EFI facilities on illumos:
 * efi_enabled() is false for every feature (the GPU's VBIOS comes from the
 * PCI ROM BAR or ACPI VFCT instead).
 */
#ifndef _LINUX_EFI_H
#define	_LINUX_EFI_H
#include <linux/types.h>
#include <linux/reboot.h>	/* emergency_restart(), as Linux's efi.h */
#define	EFI_BOOT		0
#define	EFI_RUNTIME_SERVICES	3
#define	efi_enabled(feature)	((void)(feature), false)
#endif
